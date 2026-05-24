/**
 * JBD BMS <-> Xiaomi M365 ESC Bridge  +  WiFi AP battery web dashboard
 * Target: ESP32 (any variant)
 *
 * WiFi:
 *   ESP32 creates an AP with SSID "M365-BMS" (password below).
 *   Connect and open http://192.168.4.1 to view the live battery dashboard.
 *   /data returns a JSON snapshot polled by the page every second.
 *
 * Wiring:
 *   ESP32 GPIO16 (RX1) <-- JBD BMS TX
 *   ESP32 GPIO17 (TX1) --> JBD BMS RX
 *   ESP32 GPIO18 (RX2) <-- M365 ESC BMS-UART TX
 *   ESP32 GPIO19 (TX2) --> M365 ESC BMS-UART RX
 *   Common GND required between ESP32, JBD BMS, and M365 ESC.
 *
 * IMPORTANT — voltage levels:
 *   JBD BMS UART is typically 3.3 V. Direct connection to ESP32 is fine.
 *   M365 ESC BMS UART is 3.3 V. Direct connection to ESP32 is fine.
 *   DO NOT connect 5 V UART directly to ESP32 GPIO without a level shifter.
 *
 * Protocol notes:
 *   JBD: 9600 8N1. Poll with DD A5 xx 00 [crc_hi crc_lo] 77.
 *        Response: DD xx 00 <len> <data*len> <crc_hi crc_lo> 77
 *        CRC = 0x10000 - sum(all bytes between len and last data byte inclusive)
 *
 *   M365: 115200 8N1. Packet: 55 AA <pkt_len> <src> <dst> <cmd> [data] <crc_lo crc_hi>
 *         pkt_len = 3 + dataLen  (src + dst + cmd + data)
 *         CRC = (~sum(pkt_len..last_data_byte)) & 0xFFFF, little-endian
 *         ESC addr: 0x21 | BMS addr: 0x22
 *
 * !! VERIFY before use !!
 *   - M365 BMS payload field order / units depend on ESC firmware version.
 *     Capture traffic from a real M365 BMS with a logic analyzer and compare.
 *   - JBD current sign convention: some firmwares flip positive/negative for
 *     charge vs discharge. Check raw output with JBD PC software first.
 *   - The ESC may require more BMS packet types than just 0x01. Add them if
 *     the ESC shows a BMS-fault LED or won't engage throttle.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ─── Pin config ───────────────────────────────────────────────────────────────
#define JBD_RX  16
#define JBD_TX  17
#define M365_RX 18
#define M365_TX 19

// ─── Timing ──────────────────────────────────────────────────────────────────
#define JBD_POLL_MS      500   // how often to poll the JBD BMS
#define M365_PUSH_MS     100   // how often to push battery status to ESC
#define JBD_TIMEOUT_MS   250   // max wait for JBD response

// ─── M365 addresses ──────────────────────────────────────────────────────────
#define ADDR_ESC  0x21
#define ADDR_BMS  0x22

// ─── UART instances ──────────────────────────────────────────────────────────
HardwareSerial jbdSerial(1);   // UART1 -> JBD BMS
HardwareSerial m365Serial(2);  // UART2 -> M365 ESC

// ─── JBD poll commands ───────────────────────────────────────────────────────
// CRC pre-calculated: 0x10000 - (0x03) = 0xFFFC? No:
//   Basic info cmd 0x03: data = none, so CRC covers [03 00] = sum 3, CRC = 0xFFFD
//   Cell info cmd  0x04: data = none, so CRC covers [04 00] = sum 4, CRC = 0xFFFC
static const uint8_t JBD_CMD_BASIC[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
static const uint8_t JBD_CMD_CELLS[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};

// ─── Battery state ───────────────────────────────────────────────────────────
struct BatteryState {
    uint16_t voltage_mv;       // pack voltage mV
    int16_t  current_ma;       // current mA (+discharge, -charge — verify with your JBD)
    uint16_t remaining_mah;    // remaining capacity mAh
    uint16_t nominal_mah;      // nominal capacity mAh
    uint8_t  soc_pct;          // state of charge 0–100
    uint8_t  cell_count;
    uint16_t cell_mv[20];      // per-cell voltage mV
    int16_t  temp_c10[3];      // temperatures in 0.1 °C, up to 3 NTCs
    uint8_t  ntc_count;
    uint16_t prot_status;      // JBD protection status bitmask
    bool     valid;
};

static BatteryState batt = {};

// ─── RX buffers ──────────────────────────────────────────────────────────────
static uint8_t jbdRxBuf[140];
static uint8_t jbdRxLen = 0;

static uint8_t m365RxBuf[64];
static uint8_t m365RxLen = 0;

// ─── WiFi AP config ──────────────────────────────────────────────────────────
static Preferences prefs;
static char apSsid[33] = "M365-BMS";
static char apPass[64] = "m365batt";  // min 8 chars
static bool apRestartPending = false;

// ─── Web server ──────────────────────────────────────────────────────────────
static WebServer webServer(80);

// Control state
static volatile bool     btEnabled    = true;
static uint8_t           btReg        = 0xE1;   // JBD BT control register (firmware-dependent)
static volatile bool     escKill      = false;   // disable ESC by reporting zero SoC/voltage
static volatile bool     socSpoof     = false;   // report fixed SoC to M365 ESC
static volatile uint8_t  socSpoofVal  = 100;     // spoofed SoC %
static uint32_t          jbdPollMs    = JBD_POLL_MS; // runtime-configurable poll interval
static uint8_t           packCells    = 10;          // series cell count — maps real voltage to M365 10S range

// ─── AP auto-shutdown ────────────────────────────────────────────────────────
// If no client connects within AP_IDLE_MS of boot, disable WiFi AP.
// Bridge continues running on stored settings.
#define AP_IDLE_MS        (2UL * 60UL * 1000UL)  // 2 minutes
static bool     apActive    = true;
static uint32_t apStartMs   = 0;

// ─── Web page ────────────────────────────────────────────────────────────────
// Served at /  — fetches /data JSON every second and updates in-place.
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M365 Battery</title>
<style>
:root{--bg:#111;--card:#1c1c1c;--accent:#00c853;--warn:#ffab00;--err:#e53935;--text:#e0e0e0;--muted:#616161}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px;max-width:600px;margin:0 auto}
h1{font-size:1.1rem;color:var(--accent);letter-spacing:.05em;text-transform:uppercase;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px;margin-bottom:14px}
.card{background:var(--card);border-radius:8px;padding:12px 14px}
.card .lbl{font-size:.65rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.card .val{font-size:1.55rem;font-weight:700;line-height:1.2;margin-top:2px}
.card .unit{font-size:.75rem;color:var(--muted)}
.section{background:var(--card);border-radius:8px;padding:12px 14px;margin-bottom:14px}
.section .lbl{font-size:.65rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;margin-bottom:8px}
.cells{display:grid;grid-template-columns:repeat(auto-fill,minmax(72px,1fr));gap:7px}
.cell{border-radius:6px;padding:7px 8px;text-align:center;font-size:.82rem;font-weight:600;background:#222}
.cell .cidx{font-size:.6rem;color:var(--muted);display:block;margin-bottom:2px}
.flags{display:flex;flex-wrap:wrap;gap:5px}
.flag{padding:3px 9px;border-radius:4px;font-size:.72rem;background:#252525;color:var(--muted)}
.flag.on{background:var(--err);color:#fff}
.foot{display:flex;align-items:center;gap:7px;font-size:.72rem;color:var(--muted)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted);flex-shrink:0}
.dot.ok{background:var(--accent)}.dot.warn{background:var(--warn)}.dot.err{background:var(--err)}
.hdr{position:relative;margin-bottom:16px}
.gear{position:absolute;top:0;right:0;font-size:1.4rem;cursor:pointer;color:var(--muted);user-select:none;line-height:1}
.gear:hover{color:var(--text)}
.overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.75);z-index:10;align-items:center;justify-content:center}
.overlay.open{display:flex}
.modal{background:var(--card);border-radius:12px;padding:20px;width:min(360px,92vw);max-height:85vh;overflow-y:auto}
.mhdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}
.mhdr h2{font-size:.85rem;color:var(--accent);text-transform:uppercase;letter-spacing:.08em;margin:0}
.mcls{cursor:pointer;color:var(--muted);font-size:1.1rem}.mcls:hover{color:var(--text)}
.ssect{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.1em;margin:14px 0 8px;border-top:1px solid #2a2a2a;padding-top:10px}
.ssect.first{margin-top:0;border-top:none}
.srow{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px}
.slbl{font-size:.8rem}.ssub{font-size:.68rem;color:var(--muted);margin-top:1px}
.tgl{position:relative;width:40px;height:22px;flex-shrink:0}
.tgl input{opacity:0;width:0;height:0;position:absolute}
.tgl-sl{position:absolute;inset:0;border-radius:22px;background:#333;cursor:pointer;transition:.2s}
.tgl-sl:before{content:'';position:absolute;width:16px;height:16px;left:3px;bottom:3px;border-radius:50%;background:#fff;transition:.2s}
.tgl input:checked~.tgl-sl{background:var(--accent)}
.tgl input:checked~.tgl-sl:before{transform:translateX(18px)}
.tgl input.red:checked~.tgl-sl{background:var(--err)}
.sinp{background:#252525;border:1px solid #333;border-radius:5px;color:var(--text);padding:5px 8px;font-size:.8rem}
.sinp:focus{outline:none;border-color:var(--accent)}
.sapply{width:100%;margin-top:14px;padding:9px;border-radius:6px;border:none;background:var(--accent);color:#111;font-size:.85rem;font-weight:700;cursor:pointer}
.kbtn{width:100%;padding:10px;border-radius:6px;border:none;cursor:pointer;font-size:.88rem;font-weight:700;transition:background .2s,color .2s}
</style>
</head>
<body>
<div class="hdr">
  <h1 style="margin:0">M365 Battery Monitor</h1>
  <span class="gear" onclick="document.getElementById('ov').classList.add('open')" title="Settings">&#9881;</span>
</div>
<div class="grid">
  <div class="card"><div class="lbl">Voltage</div><div class="val" id="volt">--</div><div class="unit">V</div></div>
  <div class="card"><div class="lbl">Current</div><div class="val" id="cur">--</div><div class="unit">A</div></div>
  <div class="card"><div class="lbl">SoC</div><div class="val" id="soc">--</div><div class="unit">%</div></div>
  <div class="card"><div class="lbl">Remaining</div><div class="val" id="rem">--</div><div class="unit">Ah</div></div>
  <div class="card"><div class="lbl">Nominal</div><div class="val" id="nom">--</div><div class="unit">Ah</div></div>
  <div class="card"><div class="lbl">Temperature</div><div class="val" id="temp">--</div><div class="unit">°C</div></div>
</div>
<div class="section">
  <div class="lbl">Cell Voltages</div>
  <div class="cells" id="cells"></div>
</div>
<div class="section">
  <div class="lbl">Protection Flags</div>
  <div class="flags" id="flags"></div>
</div>
<div class="section" style="margin-bottom:14px">
  <div class="lbl">Controls</div>
  <button id="kbtn" onclick="toggleKill()" class="kbtn" style="background:#2a2a2a;color:var(--muted)">&#9889; ESC Kill: OFF</button>
</div>
<div class="overlay" id="ov" onclick="if(event.target===this)this.classList.remove('open')">
<div class="modal">
  <div class="mhdr"><h2>Settings</h2><span class="mcls" onclick="document.getElementById('ov').classList.remove('open')">&#x2715;</span></div>
  <div class="ssect first">WiFi AP</div>
  <div class="srow" style="flex-direction:column;align-items:flex-start;gap:4px">
    <div class="slbl">Network Name (SSID)</div>
    <input class="sinp" type="text" id="s-ssid" maxlength="32" style="width:100%">
  </div>
  <div class="srow" style="flex-direction:column;align-items:flex-start;gap:4px">
    <div class="slbl">Password <span class="ssub">(min 8 chars &#8212; blank = keep current)</span></div>
    <input class="sinp" type="password" id="s-pass" maxlength="63" placeholder="unchanged" style="width:100%">
  </div>
  <div class="ssect">BMS Protocol</div>
  <select class="sinp" disabled style="width:100%">
    <option selected>JBD / Overkill Solar</option>
    <option>DALY (coming soon)</option>
    <option>JIKONG / ANT (coming soon)</option>
  </select>
  <div class="ssect">BMS Control</div>
  <div class="srow">
    <div><div class="slbl">Bluetooth</div><div class="ssub">JBD onboard BT module</div></div>
    <label class="tgl"><input type="checkbox" id="s-bt" checked><span class="tgl-sl"></span></label>
  </div>
  <div class="srow" style="flex-direction:column;align-items:flex-start;gap:4px">
    <div class="slbl">BT Register <span class="ssub">(hex &#8212; varies by firmware)</span></div>
    <input class="sinp" type="text" id="s-btreg" value="E1" maxlength="2" style="width:64px;font-family:monospace;text-transform:uppercase">
  </div>
  <div class="srow">
    <div><div class="slbl">Poll interval</div><div class="ssub">JBD query rate</div></div>
    <select class="sinp" id="s-poll">
      <option value="200">200 ms</option>
      <option value="500" selected>500 ms</option>
      <option value="1000">1 s</option>
      <option value="2000">2 s</option>
    </select>
  </div>
  <div class="ssect">ESC Control</div>
  <div class="srow">
    <div><div class="slbl">Pack Cells (S)</div><div class="ssub">Real cell count &#8212; voltage mapped to M365 10S range</div></div>
    <select class="sinp" id="s-packcells">
      <option value="7">7S</option><option value="8">8S</option><option value="9">9S</option>
      <option value="10" selected>10S (stock)</option><option value="11">11S</option>
      <option value="12">12S</option><option value="13">13S</option><option value="14">14S</option>
      <option value="15">15S</option><option value="16">16S</option>
    </select>
  </div>
  <div class="srow">
    <div><div class="slbl">ESC Kill</div><div class="ssub">Report fault &#8594; motor disabled</div></div>
    <label class="tgl"><input type="checkbox" id="s-kill" class="red"><span class="tgl-sl"></span></label>
  </div>
  <div class="srow">
    <div><div class="slbl">SoC Override</div><div class="ssub">Report fixed SoC to ESC (affects speed limit)</div></div>
    <label class="tgl"><input type="checkbox" id="s-spoof"><span class="tgl-sl"></span></label>
  </div>
  <div class="srow" style="flex-direction:column;align-items:stretch;gap:4px">
    <div style="display:flex;justify-content:space-between;font-size:.8rem"><span>SoC Value</span><span id="s-spoof-lbl">100%</span></div>
    <input type="range" id="s-spoofval" min="0" max="100" value="100" style="accent-color:var(--accent)">
  </div>
  <button class="sapply" onclick="applySettings()">Apply</button>
</div></div>
<div class="foot"><div class="dot" id="dot"></div><span id="status">connecting&#x2026;</span></div>
<script>
const PROT=['Cell OV','Cell UV','Pack OV','Pack UV','Chg OC','Dis OC','Short','IC Err','SW Lock'];
function cellColor(mv){
  if(mv<3000)return'#e53935';
  if(mv<3300)return'#ff6f00';
  if(mv<4050)return'#00c853';
  if(mv<=4200)return'#ffab00';
  return'#e53935';
}
function fmt(v,d=2){return isNaN(v)?'--':v.toFixed(d);}
function render(d){
  document.getElementById('volt').textContent=fmt(d.voltage_mv/1000);
  const curA=d.current_ma/1000;
  const curEl=document.getElementById('cur');
  curEl.textContent=(curA>=0?'+':'')+fmt(curA);
  curEl.style.color=curA>0?'var(--warn)':curA<0?'#4fc3f7':'var(--text)';
  document.getElementById('soc').textContent=d.soc_pct;
  document.getElementById('rem').textContent=fmt(d.remaining_mah/1000);
  document.getElementById('nom').textContent=fmt(d.nominal_mah/1000);
  const t=d.temps.map(x=>fmt(x/10,1));
  document.getElementById('temp').textContent=t.length?t.join(' / '):'--';
  const ce=document.getElementById('cells');
  ce.innerHTML='';
  d.cells.forEach((mv,i)=>{
    const el=document.createElement('div');
    el.className='cell';
    el.style.borderBottom='3px solid '+cellColor(mv);
    el.innerHTML='<span class="cidx">C'+(i+1)+'</span>'+fmt(mv/1000,3);
    ce.appendChild(el);
  });
  const fl=document.getElementById('flags');
  fl.innerHTML='';
  PROT.forEach((name,i)=>{
    const el=document.createElement('span');
    el.className='flag'+((d.prot_status>>i)&1?' on':'');
    el.textContent=name;
    fl.appendChild(el);
  });
  const dot=document.getElementById('dot');
  const stxt=document.getElementById('status');
  if(!d.valid){dot.className='dot err';stxt.textContent='No BMS data';}
  else if(d.prot_status){dot.className='dot warn';stxt.textContent='Protection active (0x'+d.prot_status.toString(16).toUpperCase()+')';}
  else{dot.className='dot ok';stxt.textContent='OK';}
  syncSettings(d);
}
async function poll(){
  try{
    const r=await fetch('/data');
    if(!r.ok)throw new Error(r.status);
    render(await r.json());
  }catch(e){
    document.getElementById('dot').className='dot err';
    document.getElementById('status').textContent='Connection lost';
  }
  setTimeout(poll,1000);
}
poll();
function syncSettings(d){
  // Always update the quick-access kill button on the main page
  if(d.esc_kill!=null){
    const kb=document.getElementById('kbtn');
    if(kb){kb.textContent=d.esc_kill?'\u26a1 ESC Kill: ON':'\u26a1 ESC Kill: OFF';kb.style.background=d.esc_kill?'var(--err)':'#2a2a2a';kb.style.color=d.esc_kill?'#fff':'var(--muted)';}
  }
  // Don't overwrite modal fields while the modal is open — user may be editing
  if(document.getElementById('ov').classList.contains('open')) return;
  if(d.bt_enabled!=null) document.getElementById('s-bt').checked=d.bt_enabled;
  if(d.bt_reg!=null) document.getElementById('s-btreg').value=d.bt_reg.toString(16).toUpperCase().padStart(2,'0');
  if(d.esc_kill!=null) document.getElementById('s-kill').checked=d.esc_kill;
  if(d.soc_spoof!=null) document.getElementById('s-spoof').checked=d.soc_spoof;
  if(d.soc_spoof_val!=null){const v=d.soc_spoof_val;document.getElementById('s-spoofval').value=v;document.getElementById('s-spoof-lbl').textContent=v+'%';}
  if(d.poll_ms!=null){const sel=document.getElementById('s-poll');for(const o of sel.options)if(parseInt(o.value)===d.poll_ms){o.selected=true;break;}}
  if(d.pack_cells!=null){const sel=document.getElementById('s-packcells');for(const o of sel.options)if(parseInt(o.value)===d.pack_cells){o.selected=true;break;}}
  if(d.wifi_ssid!=null) document.getElementById('s-ssid').value=d.wifi_ssid;
}
async function applySettings(){
  const body=new URLSearchParams({
    bt:document.getElementById('s-bt').checked?'1':'0',
    btReg:document.getElementById('s-btreg').value,
    escKill:document.getElementById('s-kill').checked?'1':'0',
    socSpoof:document.getElementById('s-spoof').checked?'1':'0',
    socSpoofVal:document.getElementById('s-spoofval').value,
    pollMs:document.getElementById('s-poll').value,
    packCells:document.getElementById('s-packcells').value,
    apSsid:document.getElementById('s-ssid').value,
    apPass:document.getElementById('s-pass').value
  });
  try{
    const wifiChanged=document.getElementById('s-pass').value.length>=8||document.getElementById('s-ssid').value!=='';
    const r=await fetch('/settings',{method:'POST',body:body.toString(),headers:{'Content-Type':'application/x-www-form-urlencoded'}});
    const d=await r.json();
    syncSettings(d);
    document.getElementById('ov').classList.remove('open');
    if(wifiChanged&&(document.getElementById('s-pass').value.length>=8))alert('WiFi password changed.\nReconnect to "'+(d.wifi_ssid||document.getElementById('s-ssid').value)+'" with the new password.');
    document.getElementById('s-pass').value='';
  }catch(e){}
}
async function toggleKill(){
  try{const r=await fetch('/kill',{method:'POST'});syncSettings(await r.json());}catch(e){}
}
document.getElementById('s-spoofval').addEventListener('input',function(){
  document.getElementById('s-spoof-lbl').textContent=this.value+'%';
});
</script>
</body>
</html>
)rawliteral";

// Forward declaration
static uint16_t jbdCrc(const uint8_t* data, uint8_t len);

// ─── JBD write helper ───────────────────────────────────────────────────────
// Write format: DD 5A <reg> <len> <data...> <crc_hi> <crc_lo> 77
// CRC covers [reg, len, data...]
static void sendJbdWrite(uint8_t reg, const uint8_t* data, uint8_t len) {
    // Unlock with default password 0x0000
    // CRC of [0x00, 0x02, 0x00, 0x00] = 0xFFFE
    static const uint8_t unlock[] = {0xDD, 0x5A, 0x00, 0x02, 0x00, 0x00, 0xFF, 0xFE, 0x77};
    while (jbdSerial.available()) jbdSerial.read();  // flush
    jbdSerial.write(unlock, sizeof(unlock));
    delay(60);
    while (jbdSerial.available()) jbdSerial.read();  // flush response

    // Build write packet
    uint8_t buf[32];
    buf[0] = 0xDD;
    buf[1] = 0x5A;
    buf[2] = reg;
    buf[3] = len;
    if (len > 0) memcpy(&buf[4], data, len);

    // CRC covers [reg, len, data...]
    uint8_t crcIn[32];
    crcIn[0] = reg;
    crcIn[1] = len;
    if (len > 0) memcpy(&crcIn[2], data, len);
    uint16_t crc = jbdCrc(crcIn, 2 + len);
    buf[4 + len] = (crc >> 8) & 0xFF;
    buf[5 + len] =  crc       & 0xFF;
    buf[6 + len] = 0x77;
    jbdSerial.write(buf, 7 + len);
    delay(60);
    while (jbdSerial.available()) jbdSerial.read();  // flush response
}

// ─── POST /bt — toggle JBD BMS Bluetooth ─────────────────────────────────────
// !! Register 0xE1 is commonly cited for JBD BT control but is firmware-dependent.
// !! Verify with a logic analyser or JBD PC software for your specific BMS.
static void handleBtToggle() {
    btEnabled = !btEnabled;
    uint8_t val = btEnabled ? 0x01 : 0x00;
    sendJbdWrite(btReg, &val, 1);  // 0x01 = BT on, 0x00 = BT off
    Serial.printf("[BT] BMS Bluetooth %s\n", btEnabled ? "ON" : "OFF");
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send(200, "application/json",
        String("{\"bt\":") + (btEnabled ? "true" : "false") + "}");
}

// ─── POST /kill — toggle ESC kill ───────────────────────────────────────────────────
static void buildSettingsJson(String& j) {
    j += "\"bt_enabled\":"    + String(btEnabled    ? "true" : "false") + ",";
    j += "\"bt_reg\":"        + String(btReg)                           + ",";
    j += "\"esc_kill\":"      + String(escKill      ? "true" : "false") + ",";
    j += "\"soc_spoof\":"     + String(socSpoof     ? "true" : "false") + ",";
    j += "\"soc_spoof_val\":" + String(socSpoofVal)                     + ",";
    j += "\"poll_ms\":"       + String(jbdPollMs)        + ",";
    j += "\"pack_cells\":"    + String(packCells)  + ",";
    j += "\"wifi_ssid\":\""   + String(apSsid)     + "\"";
}

static void handleKillToggle() {
    escKill = !escKill;
    Serial.printf("[Kill] ESC Kill %s\n", escKill ? "ON" : "OFF");
    String j = "{";
    buildSettingsJson(j);
    j += "}";
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send(200, "application/json", j);
}

// ─── POST /settings — apply settings from modal ───────────────────────────────────
static void handleSettings() {
    if (webServer.hasArg("bt")) {
        bool newBt = webServer.arg("bt") == "1";
        if (newBt != btEnabled) {
            btEnabled = newBt;
            uint8_t val = btEnabled ? 0x01 : 0x00;
            sendJbdWrite(btReg, &val, 1);
            Serial.printf("[Settings] BT %s\n", btEnabled ? "ON" : "OFF");
        }
    }
    if (webServer.hasArg("btReg")) {
        uint8_t r = (uint8_t)strtol(webServer.arg("btReg").c_str(), nullptr, 16);
        if (r != 0) btReg = r;
    }
    if (webServer.hasArg("escKill"))  escKill     = webServer.arg("escKill")  == "1";
    if (webServer.hasArg("socSpoof")) socSpoof    = webServer.arg("socSpoof") == "1";
    if (webServer.hasArg("socSpoofVal")) {
        int v = webServer.arg("socSpoofVal").toInt();
        socSpoofVal = (uint8_t)constrain(v, 0, 100);
    }
    if (webServer.hasArg("pollMs")) {
        int ms = webServer.arg("pollMs").toInt();
        if (ms >= 200 && ms <= 5000) jbdPollMs = (uint32_t)ms;
    }
    if (webServer.hasArg("packCells")) {
        int c = webServer.arg("packCells").toInt();
        if (c >= 1 && c <= 24) packCells = (uint8_t)c;
    }
    if (webServer.hasArg("apSsid")) {
        String s = webServer.arg("apSsid");
        s.trim();
        if (s.length() >= 1 && s.length() <= 32) {
            strlcpy(apSsid, s.c_str(), sizeof(apSsid));
            prefs.putString("ssid", apSsid);
            apRestartPending = true;
        }
    }
    if (webServer.hasArg("apPass")) {
        String p = webServer.arg("apPass");
        if (p.length() >= 8 && p.length() <= 63) {
            strlcpy(apPass, p.c_str(), sizeof(apPass));
            prefs.putString("pass", apPass);
            apRestartPending = true;
        }
    }
    Serial.printf("[Settings] kill=%d spoof=%d(%d%%) poll=%lums cells=%dS ssid=%s\n",
        escKill, socSpoof, socSpoofVal, jbdPollMs, packCells, apSsid);
    String j = "{";
    buildSettingsJson(j);
    j += "}";
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send(200, "application/json", j);
}

// ─── /data — JSON battery snapshot ───────────────────────────────────────────
static void handleData() {
    String j = "{";
    j += "\"valid\":"         + String(batt.valid ? "true" : "false") + ",";
    j += "\"voltage_mv\":"    + String(batt.voltage_mv)    + ",";
    j += "\"current_ma\":"    + String(batt.current_ma)    + ",";
    j += "\"soc_pct\":"       + String(batt.soc_pct)       + ",";
    j += "\"remaining_mah\":" + String(batt.remaining_mah) + ",";
    j += "\"nominal_mah\":"   + String(batt.nominal_mah)   + ",";
    j += "\"prot_status\":"   + String(batt.prot_status)   + ",";
    j += "\"ntc_count\":"     + String(batt.ntc_count)     + ",";
    j += "\"temps\":[";
    for (uint8_t i = 0; i < batt.ntc_count; i++) {
        if (i) j += ",";
        j += String(batt.temp_c10[i]);
    }
    j += "],\"cell_count\":";
    j += String(batt.cell_count);
    j += ",\"cells\":[";
    uint8_t n = (batt.cell_count > 20) ? 20 : batt.cell_count;
    for (uint8_t i = 0; i < n; i++) {
        if (i) j += ",";
        j += String(batt.cell_mv[i]);
    }
    j += "],";
    buildSettingsJson(j);
    j += "}";
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send(200, "application/json", j);
}

// ─── / — HTML dashboard ───────────────────────────────────────────────────────
static void handleRoot() {
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send_P(200, "text/html", HTML_PAGE);
}

// ─── JBD helpers ─────────────────────────────────────────────────────────────

/**
 * Compute JBD checksum for an outgoing command.
 * Covers: [cmd, 0x00] (the two bytes after 0xA5 for a read command).
 * For commands with data payload the sum covers those bytes too.
 */
static uint16_t jbdCrc(const uint8_t* data, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += data[i];
    return (0x10000 - sum) & 0xFFFF;
}

/**
 * Read JBD response into jbdRxBuf.
 * Returns true when end marker 0x77 is received within timeout.
 */
static bool readJbdResponse() {
    jbdRxLen = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < JBD_TIMEOUT_MS) {
        while (jbdSerial.available() && jbdRxLen < sizeof(jbdRxBuf)) {
            jbdRxBuf[jbdRxLen++] = (uint8_t)jbdSerial.read();
            // End-of-packet marker
            if (jbdRxLen >= 7 && jbdRxBuf[jbdRxLen - 1] == 0x77) return true;
        }
    }
    return false;
}

/**
 * Parse JBD 0x03 basic-info response.
 * Packet layout (after 0xDD start byte):
 *   [0] 0xDD  [1] 0x03  [2] 0x00 (OK)  [3] dataLen
 *   [4..4+dataLen-1] data
 *   [4+dataLen] crc_hi  [4+dataLen+1] crc_lo  [4+dataLen+2] 0x77
 */
static bool parseJbdBasicInfo(const uint8_t* buf, uint8_t len) {
    if (len < 8)           return false;
    if (buf[0] != 0xDD)    return false;
    if (buf[1] != 0x03)    return false;
    if (buf[2] != 0x00)    return false;  // 0x80 = error response

    uint8_t dataLen = buf[3];
    if (len < (uint16_t)(dataLen + 7)) return false;
    if (buf[dataLen + 6] != 0x77)      return false;
    if (dataLen < 24)                  return false;

    const uint8_t* d = &buf[4];

    // Verify CRC
    uint16_t crc_calc = jbdCrc(d, dataLen);
    uint16_t crc_recv = ((uint16_t)d[dataLen] << 8) | d[dataLen + 1];
    if (crc_calc != crc_recv) {
        Serial.printf("[JBD] Basic info CRC mismatch: calc=%04X recv=%04X\n", crc_calc, crc_recv);
        return false;
    }

    // JBD units: voltage = 10 mV, current = 10 mA (signed), capacity = 10 mAh
    batt.voltage_mv    = (uint16_t)(((uint16_t)d[0] << 8) | d[1]) * 10;
    int16_t raw_cur    = (int16_t)(((uint16_t)d[2] << 8) | d[3]);
    batt.current_ma    = (int16_t)((int32_t)raw_cur * 10);
    batt.remaining_mah = (uint16_t)(((uint16_t)d[4] << 8) | d[5]) * 10;
    batt.nominal_mah   = (uint16_t)(((uint16_t)d[6] << 8) | d[7]) * 10;
    batt.prot_status   = ((uint16_t)d[16] << 8) | d[17];
    batt.soc_pct       = d[19];
    batt.cell_count    = d[21];
    batt.ntc_count     = d[22];
    if (batt.ntc_count > 3) batt.ntc_count = 3;

    for (uint8_t i = 0; i < batt.ntc_count; i++) {
        if ((23 + i * 2 + 1) >= dataLen) break;
        uint16_t raw_t = ((uint16_t)d[23 + i * 2] << 8) | d[24 + i * 2];
        batt.temp_c10[i] = (int16_t)(raw_t - 2731);  // 0.1 K -> 0.1 °C
    }

    batt.valid = true;
    return true;
}

/**
 * Parse JBD 0x04 cell-voltage response.
 * Each cell voltage is a big-endian uint16 in mV.
 */
static bool parseJbdCellInfo(const uint8_t* buf, uint8_t len) {
    if (len < 8)           return false;
    if (buf[0] != 0xDD)    return false;
    if (buf[1] != 0x04)    return false;
    if (buf[2] != 0x00)    return false;

    uint8_t dataLen = buf[3];
    if (len < (uint16_t)(dataLen + 7)) return false;

    const uint8_t* d = &buf[4];
    uint8_t cells = dataLen / 2;
    if (cells > 20) cells = 20;
    batt.cell_count = cells;

    for (uint8_t i = 0; i < cells; i++) {
        batt.cell_mv[i] = ((uint16_t)d[i * 2] << 8) | d[i * 2 + 1];
    }
    return true;
}

// ─── M365 packet helpers ─────────────────────────────────────────────────────

/**
 * Compute M365 packet CRC.
 * Input: pointer to pkt_len byte through last data byte.
 * CRC = bitwise NOT of 16-bit sum, little-endian.
 */
static uint16_t m365Crc(const uint8_t* start, uint8_t count) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += start[i];
    return (~sum) & 0xFFFF;
}

/**
 * Build an M365 packet into out[].
 * Format: 55 AA <pkt_len> <src> <dst> <cmd> [data*dataLen] <crc_lo> <crc_hi>
 * pkt_len = 3 + dataLen (src + dst + cmd + data bytes)
 * Returns total byte count.
 */
static uint8_t buildM365Packet(uint8_t* out, uint8_t src, uint8_t dst,
                                uint8_t cmd, const uint8_t* data, uint8_t dataLen) {
    out[0] = 0x55;
    out[1] = 0xAA;
    out[2] = 3 + dataLen;    // pkt_len
    out[3] = src;
    out[4] = dst;
    out[5] = cmd;
    if (dataLen > 0) memcpy(&out[6], data, dataLen);

    uint16_t crc = m365Crc(&out[2], 4 + dataLen);  // from pkt_len byte
    out[6 + dataLen] = crc & 0xFF;
    out[7 + dataLen] = (crc >> 8) & 0xFF;

    return 8 + dataLen;
}

/**
 * Build the standard BMS status packet (BMS->ESC, cmd 0x01).
 *
 * !! Field order / units below are from community reverse engineering !!
 * !! Verify against a logic-analyzer capture of your M365 BMS !!
 *
 * Typical M365 BMS status payload (10 bytes):
 *   [0]    SoC %  (uint8)
 *   [1-2]  Pack voltage in 10 mV units (uint16 LE)
 *   [3-4]  Current in mA signed (int16 LE), negative = charging
 *   [5-6]  Temperature of NTC1 in 0.1 °C (int16 LE)
 *   [7-8]  Remaining capacity mAh (uint16 LE)
 *   [9]    Number of cells (uint8)
 */
static uint8_t buildBmsStatusPacket(uint8_t* out) {
    uint8_t payload[10];

    // Apply ESC kill (zero SoC+voltage) or SoC spoof override
    uint8_t  effSoc = escKill ? 0 : (socSpoof ? socSpoofVal : batt.soc_pct);
    payload[0] = effSoc;

    // Map real pack voltage → 10S Li-ion ESC range (30–42 V).
    // Per-cell: 3.0 V = empty, 4.2 V = full.  packCells is user-configured.
    uint32_t vPkMin  = (uint32_t)packCells * 3000UL;
    uint32_t vPkMax  = (uint32_t)packCells * 4200UL;
    uint32_t vAct    = batt.voltage_mv;
    uint32_t vEscMv;
    if (escKill) {
        vEscMv = 0;
    } else if (vAct <= vPkMin) {
        vEscMv = 30000UL;
    } else if (vAct >= vPkMax) {
        vEscMv = 42000UL;
    } else {
        vEscMv = 30000UL + (vAct - vPkMin) * 12000UL / (vPkMax - vPkMin);
    }
    uint16_t v10mv = (uint16_t)(vEscMv / 10);
    payload[1] = v10mv & 0xFF;
    payload[2] = (v10mv >> 8) & 0xFF;

    // M365 convention: negative current = charging, positive = discharging
    // JBD convention may differ — flip sign if needed
    int16_t cur = batt.current_ma;
    payload[3] = (uint8_t)(cur & 0xFF);
    payload[4] = (uint8_t)((cur >> 8) & 0xFF);

    int16_t temp = (batt.ntc_count > 0) ? batt.temp_c10[0] : 250;  // default 25 °C
    payload[5] = (uint8_t)(temp & 0xFF);
    payload[6] = (uint8_t)((temp >> 8) & 0xFF);

    payload[7] = batt.remaining_mah & 0xFF;
    payload[8] = (batt.remaining_mah >> 8) & 0xFF;

    payload[9] = batt.cell_count;

    return buildM365Packet(out, ADDR_BMS, ADDR_ESC, 0x01, payload, sizeof(payload));
}

/**
 * Build a BMS cell-voltage packet (BMS->ESC, cmd 0x02 — verify cmd byte).
 * Payload: [cell_count] then cell_count * uint16_LE voltages in mV.
 */
static uint8_t buildBmsCellPacket(uint8_t* out) {
    uint8_t payload[1 + 20 * 2];
    uint8_t n = batt.cell_count;
    if (n > 20) n = 20;

    payload[0] = n;
    for (uint8_t i = 0; i < n; i++) {
        payload[1 + i * 2]     = batt.cell_mv[i] & 0xFF;
        payload[1 + i * 2 + 1] = (batt.cell_mv[i] >> 8) & 0xFF;
    }

    return buildM365Packet(out, ADDR_BMS, ADDR_ESC, 0x02, payload, 1 + n * 2);
}

// ─── M365 ESC request handler ─────────────────────────────────────────────────

/**
 * Consume bytes from m365Serial and respond to ESC requests.
 * ESC sends read-request packets addressed to BMS (dst == ADDR_BMS).
 */
static void processM365Input() {
    while (m365Serial.available()) {
        uint8_t b = (uint8_t)m365Serial.read();

        // Sync to header
        if (m365RxLen == 0 && b != 0x55) continue;
        if (m365RxLen == 1 && b != 0xAA) { m365RxLen = 0; continue; }

        if (m365RxLen < sizeof(m365RxBuf)) {
            m365RxBuf[m365RxLen++] = b;
        } else {
            m365RxLen = 0;  // overflow, reset
            continue;
        }

        // Need at least 3 bytes (55 AA pkt_len) before we know total length
        if (m365RxLen < 3) continue;

        uint8_t pkt_len      = m365RxBuf[2];
        uint8_t totalExpected = 2 + 1 + pkt_len + 2;  // 55 AA + pkt_len + payload + 2 crc

        if (m365RxLen < totalExpected) continue;

        // Full packet in buffer — verify CRC
        uint16_t crc_calc = m365Crc(&m365RxBuf[2], 1 + pkt_len);
        uint16_t crc_recv = (uint16_t)m365RxBuf[totalExpected - 2] |
                            ((uint16_t)m365RxBuf[totalExpected - 1] << 8);

        if (crc_calc != crc_recv) {
            Serial.printf("[M365] CRC mismatch: calc=%04X recv=%04X\n", crc_calc, crc_recv);
            m365RxLen = 0;
            continue;
        }

        uint8_t dst = m365RxBuf[4];
        uint8_t cmd = m365RxBuf[5];

        // ESC polling BMS
        if (dst == ADDR_BMS && batt.valid) {
            uint8_t resp[64];
            uint8_t respLen = 0;

            switch (cmd) {
                case 0x01:  // Basic status request
                    respLen = buildBmsStatusPacket(resp);
                    break;
                case 0x02:  // Cell voltage request
                    respLen = buildBmsCellPacket(resp);
                    break;
                default:
                    // Unknown command — send empty ACK so ESC doesn't hang
                    respLen = buildM365Packet(resp, ADDR_BMS, ADDR_ESC, cmd, nullptr, 0);
                    break;
            }

            if (respLen > 0) m365Serial.write(resp, respLen);
        }

        m365RxLen = 0;
    }
}

// ─── WiFi AP + HTTP server init ───────────────────────────────────────────────
static void wifiInit() {
    WiFi.softAP(apSsid, apPass);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] AP '%s' ready — open http://%s\n", apSsid, ip.toString().c_str());

    webServer.on("/",         HTTP_GET,  handleRoot);
    webServer.on("/data",     HTTP_GET,  handleData);
    webServer.on("/bt",       HTTP_POST, handleBtToggle);
    webServer.on("/kill",     HTTP_POST, handleKillToggle);
    webServer.on("/settings", HTTP_POST, handleSettings);
    webServer.begin();
    Serial.println("[WiFi] HTTP server started on port 80");
    apStartMs = millis();  // start idle countdown
}

// ─── Timestamps ──────────────────────────────────────────────────────────────
static uint32_t lastJbdPoll  = 0;
static uint32_t lastM365Push = 0;

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[bridge] JBD<->M365 bridge starting");

    jbdSerial.begin(9600,   SERIAL_8N1, JBD_RX,  JBD_TX);
    m365Serial.begin(115200, SERIAL_8N1, M365_RX, M365_TX);

    memset(&batt, 0, sizeof(batt));
    prefs.begin("bridge", false);
    strlcpy(apSsid, prefs.getString("ssid", "M365-BMS").c_str(), sizeof(apSsid));
    strlcpy(apPass, prefs.getString("pass", "m365batt").c_str(), sizeof(apPass));
    wifiInit();
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── Poll JBD BMS ──
    if (now - lastJbdPoll >= jbdPollMs) {
        lastJbdPoll = now;

        // Read basic info (voltage, current, SoC, temps)
        jbdSerial.write(JBD_CMD_BASIC, sizeof(JBD_CMD_BASIC));
        if (readJbdResponse()) {
            if (!parseJbdBasicInfo(jbdRxBuf, jbdRxLen)) {
                Serial.println("[JBD] Basic info parse failed");
            }
        } else {
            Serial.println("[JBD] Basic info timeout");
            batt.valid = false;
        }

        // Read cell voltages
        jbdSerial.write(JBD_CMD_CELLS, sizeof(JBD_CMD_CELLS));
        if (readJbdResponse()) {
            parseJbdCellInfo(jbdRxBuf, jbdRxLen);
        }

        // Debug output
        if (batt.valid) {
            Serial.printf("[JBD] %umV  %dmA  SoC=%u%%  %umAh/%umAh  T=%d.%d°C\n",
                batt.voltage_mv, batt.current_ma, batt.soc_pct,
                batt.remaining_mah, batt.nominal_mah,
                batt.temp_c10[0] / 10, abs(batt.temp_c10[0] % 10));
        }
    }

    // ── Handle ESC requests ──
    processM365Input();

    // ── AP restart (SSID/pass changed) ──
    if (apRestartPending) {
        apRestartPending = false;
        Serial.printf("[WiFi] Restarting AP as '%s'\n", apSsid);
        WiFi.softAPdisconnect(false);
        delay(100);
        WiFi.softAP(apSsid, apPass);
        apStartMs = millis();
        apActive  = true;
    }

    // ── AP idle-shutdown check ──
    if (apActive && (now - apStartMs >= AP_IDLE_MS)) {
        if (WiFi.softAPgetStationNum() == 0) {
            Serial.println("[WiFi] No client after 2 min — shutting down AP");
            webServer.stop();
            WiFi.softAPdisconnect(true);  // true = also disables radio
            apActive = false;
        } else {
            // At least one client present — keep AP alive indefinitely
            apStartMs = now;  // push check 2 min into future so we don't spin
        }
    }

    // ── Serve web clients ──
    if (apActive) webServer.handleClient();

    // ── Periodically push BMS status to ESC (unsolicited) ──
    if (batt.valid && (now - lastM365Push >= M365_PUSH_MS)) {
        lastM365Push = now;
        uint8_t pkt[64];
        uint8_t len = buildBmsStatusPacket(pkt);
        m365Serial.write(pkt, len);
    }
}
