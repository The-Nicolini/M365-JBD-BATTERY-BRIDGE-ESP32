#include "webui.h"
#include <WiFi.h>
#include "wifi_network.h"
#include "persistence.h"
#include "bms.h"
#include <Update.h>

String escapeHtml(const String &input) {
    String escaped = input;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

void handleRoot() {
    String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>M365toJBD BMS Data</title>
  <style>
    :root { color-scheme: dark; }
    body { margin:0; min-height:100vh; font-family:Inter, system-ui, sans-serif; background:#09121F; color:#E8F1FF; }
    header { position:relative; padding:28px 22px; background:radial-gradient(circle at top left,#2C4D8B 0%,#0D1A31 45%,#07101E 100%); border-bottom:1px solid rgba(255,255,255,.06); }
    header h1 { margin:0; font-size:2.1rem; letter-spacing:.08em; font-weight:700; }
    header p { margin:.5rem 0 0; color:#A7B7D9; font-size:.95rem; }
    .page-links { margin-top:16px; display:flex; gap:12px; flex-wrap:wrap; }
    .page-links a, .page-links a:visited { color:#E8F1FF; text-decoration:none; padding:.5rem 1rem; border-radius:999px; background:rgba(255,255,255,.08); border:1px solid rgba(255,255,255,.12); transition:background .2s ease; }
    .page-links a:hover { background:rgba(255,255,255,.15); }
    .page-links .active { background:rgba(255,255,255,.18); color:inherit; }
    .settings-link, .theme-toggle { position:absolute; top:24px; width:44px; height:44px; border-radius:50%; display:inline-flex; align-items:center; justify-content:center; background:rgba(255,255,255,.08); color:#E8F1FF; text-decoration:none; font-size:1.2rem; border:1px solid rgba(255,255,255,.12); transition:background .2s ease, transform .15s ease; }
    .settings-link:hover, .theme-toggle:hover { background:rgba(255,255,255,.16); transform:translateY(-1px); }
    .settings-link { right:22px; }
    .theme-toggle { right:78px; }
    body.light-mode { background:#F3F6FF; color:#0F172A; }
    body.light-mode header { background:radial-gradient(circle at top left,#EAF0FF 0%,#DCE5F5 45%,#F8FBFF 100%); border-bottom:1px solid rgba(15,23,42,.08); }
    body.light-mode .settings-link, body.light-mode .theme-toggle { background:rgba(15,23,42,.06); color:#0F172A; border-color:rgba(15,23,42,.12); }
    body.light-mode .page-links a, body.light-mode .page-links a:visited { color:#0F172A; background:rgba(15,23,42,.08); border-color:rgba(15,23,42,.12); }
    body.light-mode .page-links a:hover { background:rgba(15,23,42,.12); }
    body.light-mode .page-links .active { background:rgba(15,23,42,.14); color:#0F172A; }
    body.light-mode .card { background:#FFFFFF; border-color:rgba(15,23,42,.08); box-shadow:0 24px 80px rgba(15,23,42,.08); }
    body.light-mode .card h2 { color:#0F172A; }
    body.light-mode .card p, body.light-mode .card pre { color:#334155; }
    body.light-mode .tile { background:rgba(241,245,255,.94); border-color:rgba(15,23,42,.08); }
    body.light-mode .tile span { color:#475569; }
    body.light-mode .tile strong { color:#0F172A; }
    body.light-mode .status-pill { background:#E2E8F0; color:#0F172A; }
    body.light-mode .status-pill .status-dot { border-color:rgba(15,23,42,.25); }
    main { padding:22px; display:grid; gap:18px; grid-template-columns:repeat(auto-fit,minmax(260px,1fr)); }
    .card { background:rgba(13,20,35,.88); border:1px solid rgba(145,187,255,.14); border-radius:22px; padding:22px; box-shadow:0 24px 80px rgba(0,0,0,.2); backdrop-filter:blur(12px); }
    .card h2 { margin:0 0 14px; font-size:1.05rem; letter-spacing:.03em; color:#F8FAFF; }
    .card p, .card pre { margin:0; line-height:1.7; color:#D7E3FF; }
    .card pre { white-space:pre-wrap; word-break:break-word; font-family:ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace; }
    .tiles { display:grid; gap:14px; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); }
    .tile { padding:16px; border-radius:18px; background:rgba(24,38,63,.94); border:1px solid rgba(255,255,255,.08); }
    .tile span { display:block; font-size:.78rem; color:#8FA7CA; margin-bottom:8px; }
    .tile strong { display:block; font-size:1.2rem; color:#F8FAFF; }
    .status-pill { display:inline-flex; align-items:center; gap:10px; padding:.5rem 1rem; border-radius:999px; font-size:.95rem; background:#15253D; color:#D1E1FF; white-space:nowrap; }
    .status-pill .status-dot { width:10px; height:10px; border-radius:50%; display:inline-block; background:#3DDC84; flex-shrink:0; border:2px solid rgba(255,255,255,0.35); box-sizing:border-box; }
    .grid-wide { grid-column:1/-1; }
    footer { text-align:center; padding:18px 12px 28px; color:#7D8EA6; font-size:.92rem; }
    @media (max-width:640px) { main { padding:16px; } }
  </style>
</head>
<body>
  <header>
    <h1>M365toJBD</h1>
    <p>BMS data</p>
    <div class="page-links">
      <a class="active" href="/">JBD-BMS</a>
      <a href="/m365-scooter">M365-SCOOTER</a>
    </div>
    <button class="theme-toggle" type="button" id="themeToggle" title="Toggle theme">☀</button>
    <a class="settings-link" href="/settings" title="Settings">⚙</a>
  </header>
  <main>
    <section class="card grid-wide">
      <h2>Bridge Status</h2>
      <div class="status-pill"><span class="status-dot"></span><span id="connection">Starting...</span></div>
    </section>
    <section class="card grid-wide">
      <div class="tiles">
        <div class="tile"><span>Voltage</span><strong id="voltage">-- V</strong></div>
        <div class="tile"><span>Current</span><strong id="current">-- A</strong></div>
        <div class="tile"><span>SOC</span><strong id="soc">-- %</strong></div>
        <div class="tile"><span>Cycles</span><strong id="cycles">--</strong></div>
      </div>
    </section>
    <section class="card">
      <h2>Pack summary</h2>
      <pre id="overview">Loading status…</pre>
    </section>
    <section class="card">
      <h2>Model</h2>
      <p id="model">Loading status…</p>
    </section>
    <section class="card">
      <h2>Protection</h2>
      <p id="protections">Loading status…</p>
    </section>
    <section class="card">
      <h2>Balancing</h2>
      <p id="balance">Loading status…</p>
    </section>
    <section class="card grid-wide">
      <h2>Cell voltages</h2>
      <pre id="cells">Waiting for cell data…</pre>
    </section>
    <section class="card">
      <h2>NTC temps</h2>
      <pre id="ntcs">Waiting for NTC data…</pre>
    </section>
  </main>
  <footer>Updated every 1.5 seconds.</footer>
  <script>
    function setTheme(mode) {
      const body = document.body;
      const button = document.getElementById('themeToggle');
      if (mode === 'light') {
        body.classList.add('light-mode');
        if (button) {
          button.textContent = '🌙';
          button.title = 'Switch to dark mode';
        }
      } else {
        body.classList.remove('light-mode');
        if (button) {
          button.textContent = '☀';
          button.title = 'Switch to light mode';
        }
      }
      localStorage.setItem('theme', mode);
    }
    function toggleTheme() {
      setTheme(document.body.classList.contains('light-mode') ? 'dark' : 'light');
    }
    function initTheme() {
      const saved = localStorage.getItem('theme');
      setTheme(saved === 'light' ? 'light' : 'dark');
      const button = document.getElementById('themeToggle');
      if (button) {
        button.addEventListener('click', toggleTheme);
      }
    }
    async function updateStatus() {
      try {
        const res = await fetch('/status');
        if (!res.ok) throw new Error('fetch failed');
        const data = await res.json();
        document.getElementById('connection').textContent = data.valid ? 'Bridge online' : 'Waiting for BMS';
        document.getElementById('voltage').textContent = data.total_voltage.toFixed(2) + ' V';
        document.getElementById('current').textContent = data.current_a.toFixed(2) + ' A';
        document.getElementById('soc').textContent = data.rsoc + ' %';
        document.getElementById('cycles').textContent = data.cycles;
        document.getElementById('model').textContent = data.model || 'Unknown';
        document.getElementById('protections').textContent = data.protection_text || 'None';
        document.getElementById('balance').textContent = data.balance_text || 'None';
        const overview = [];
        overview.push(`Cells: ${data.cell_count}`);
        overview.push(`NTCs: ${data.ntc_count}`);
        overview.push(`FET: ${data.fet ? 'ON' : 'OFF'}`);
        overview.push(`Last update: ${new Date(data.last_update_ms).toLocaleTimeString()}`);
        overview.push(`Manufactured: ${data.production_year}-${String(data.production_month).padStart(2,'0')}-${String(data.production_day).padStart(2,'0')}`);
        document.getElementById('overview').textContent = overview.join('\n');

        const cells = data.cell_voltages.map((v, i) => `C${i+1}: ${v.toFixed(3)} V`).join('\n');
        document.getElementById('cells').textContent = cells || 'No cell data yet';

        const ntcs = data.ntc_temps.map((t, i) => `NTC${i+1}: ${t.toFixed(1)} °C`).join('\n');
        document.getElementById('ntcs').textContent = ntcs || 'No NTC data yet';
      } catch (err) {
        console.log('Status update failed', err);
      }
    }
    initTheme();
    updateStatus();
    setInterval(updateStatus, 1500);
  </script>
</body>
</html>)rawliteral";
    html.replace("setInterval(updateStatus, 1500);", "setInterval(updateStatus, " + String(wifiSettings.status_refresh_interval_ms) + ");");
    server.send(200, "text/html", html);
}

void handleStatus() {
    server.send(200, "application/json", buildStatusJson());
}

void handleSettings() {
    String escapedApSsid = escapeHtml(current_ssid);
    String escapedApPassword = escapeHtml(current_password);
    String escapedStationSsid = escapeHtml(station_ssid);
    String escapedStationPassword = escapeHtml(station_password);
    String escapedDeviceName = escapeHtml(String(wifiSettings.device_name));
    String escapedStaticIp = escapeHtml(String(wifiSettings.static_ip));
    String escapedGateway = escapeHtml(String(wifiSettings.gateway));
    String escapedSubnet = escapeHtml(String(wifiSettings.subnet));
    String escapedDns = escapeHtml(String(wifiSettings.dns));
    String checkedUseStaticIp = wifiSettings.use_static_ip ? " checked" : "";
    String checkedOtaAccessOnlyOnAp = wifiSettings.ota_access_only_on_ap ? " checked" : "";
    String checkedVerbose = wifiSettings.verbose_logging ? " checked" : "";
    String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\"><title>Settings</title>";
    html += "<style>body{margin:0;font-family:Inter,system-ui,sans-serif;background:#07101F;color:#E8F1FF;}header{position:relative;padding:20px 24px 16px;background:#0E1B32;border-bottom:1px solid rgba(255,255,255,.08);}h1{margin:0;font-size:1.8rem;}main{padding:24px;}form{display:grid;gap:18px;max-width:520px;}label{display:grid;gap:8px;font-size:.95rem;color:#A7B8D6;}input[type=text],input[type=password],input[type=number],select{width:100%;padding:12px 14px;border-radius:14px;border:1px solid rgba(255,255,255,.12);background:rgba(13,24,39,.95);color:#E8F1FF;}option{background:#FFFFFF;color:#000000;}button{border:none;padding:12px 16px;border-radius:14px;background:#3C6DE0;color:#fff;font-weight:700;cursor:pointer;}button:hover{background:#5A82F5;} .theme-toggle{position:absolute;top:24px;right:24px;width:44px;height:44px;border-radius:50%;display:inline-flex;align-items:center;justify-content:center;background:rgba(255,255,255,.08);color:#E8F1FF;font-size:1.2rem;border:1px solid rgba(255,255,255,.12);transition:background .2s ease, transform .15s ease;} .theme-toggle:hover{background:rgba(255,255,255,.16);transform:translateY(-1px);} .section-title{margin:24px 0 8px;font-size:1rem;font-weight:700;color:#E8F1FF;border-bottom:1px solid rgba(255,255,255,.12);padding-bottom:8px;} .toggle{display:flex;align-items:center;gap:12px;} .toggle input{width:18px;height:18px;} .scan-bar{display:flex;gap:12px;align-items:center;} .scan-bar button{flex:0 0 auto;} .scan-bar select{flex:1 1 auto;} .note{color:#8FA5D1;font-size:.92rem;} body.light-mode{background:#F3F6FF;color:#0F172A;} body.light-mode header{background:radial-gradient(circle at top left,#EAF0FF 0%,#DCE5F5 45%,#F8FBFF 100%);border-bottom:1px solid rgba(15,23,42,.08);} body.light-mode label{color:#0F172A;} body.light-mode input[type=text],body.light-mode input[type=password],body.light-mode input[type=number],body.light-mode select{background:#FFFFFF;color:#0F172A;border:1px solid rgba(15,23,42,.12);} body.light-mode .note{color:#475569;} body.light-mode .theme-toggle{background:rgba(15,23,42,.06);color:#0F172A;border-color:rgba(15,23,42,.12);} body.light-mode .section-title{color:#0F172A;}</style>";
    html += "</head><body><header><h1>Settings</h1><button class=\"theme-toggle\" type=\"button\" id=\"themeToggle\" title=\"Toggle theme\">☀</button></header><main><form id=\"settingsForm\" method=\"POST\" action=\"/save-settings\" onsubmit=\"return confirmNetworkChange()\"><div class=\"section-title\">General</div><label>Device name / hostname<input type=\"text\" name=\"device_name\" value=\"" + escapedDeviceName + "\"></label><label>Wi-Fi AP SSID<input type=\"text\" name=\"ssid\" value=\"" + escapedApSsid + "\"></label><label>Wi-Fi AP Password<input type=\"password\" name=\"password\" value=\"" + escapedApPassword + "\"></label><label>AP visibility timeout (seconds)<input type=\"number\" name=\"ap_timeout_seconds\" min=\"0\" value=\"" + String(wifiSettings.ap_timeout_seconds) + "\"></label><p class=\"note\">Set how long the access point remains visible after boot when no client connects. Use 0 to keep it visible indefinitely.</p><div class=\"section-title\">Network</div><label class=\"toggle\"><span>Use static IP for station connection</span><input type=\"checkbox\" name=\"use_static_ip\"" + checkedUseStaticIp + "></label><label>Static IP address<input type=\"text\" name=\"static_ip\" value=\"" + escapedStaticIp + "\"></label><label>Gateway<input type=\"text\" name=\"gateway\" value=\"" + escapedGateway + "\"></label><label>Subnet mask<input type=\"text\" name=\"subnet\" value=\"" + escapedSubnet + "\"></label><label>DNS server<input type=\"text\" name=\"dns\" value=\"" + escapedDns + "\"></label><label>Station SSID<select id=\"stationSelect\" name=\"station_ssid\" data-current=\"" + escapedStationSsid + "\"><option value=\"\">Choose network</option></select></label><div class=\"scan-bar\"><button type=\"button\" id=\"scanButton\">Scan</button></div><label>Station reconnect interval (seconds)<input type=\"number\" name=\"station_reconnect_interval\" min=\"1\" value=\"" + String(wifiSettings.station_reconnect_interval) + "\"></label><label>Station max reconnect attempts<input type=\"number\" name=\"max_reconnect_attempts\" min=\"1\" max=\"20\" value=\"" + String(wifiSettings.max_reconnect_attempts) + "\"></label><label>Wi-Fi scan timeout (seconds)<input type=\"number\" name=\"scan_timeout_seconds\" min=\"1\" max=\"60\" value=\"" + String(wifiSettings.scan_timeout_seconds) + "\"></label><label>Status refresh interval (ms)<input type=\"number\" name=\"status_refresh_interval_ms\" min=\"200\" max=\"10000\" value=\"" + String(wifiSettings.status_refresh_interval_ms) + "\"></label><div class=\"section-title\">OTA & logging</div><label>Existing Wi-Fi Password<input type=\"password\" name=\"station_password\" id=\"stationPassword\" value=\"" + escapedStationPassword + "\"></label><label>OTA password<input type=\"password\" name=\"ota_password\" value=\"" + escapeHtml(String(wifiSettings.ota_password)) + "\"></label><label class=\"toggle\"><span>Allow OTA only while AP is active</span><input type=\"checkbox\" name=\"ota_access_only_on_ap\"" + checkedOtaAccessOnlyOnAp + "></label><label class=\"toggle\"><span>Verbose serial logging</span><input type=\"checkbox\" name=\"verbose_logging\"" + checkedVerbose + "></label><label class=\"toggle\"><span>Connect to existing network</span><input type=\"checkbox\" name=\"use_existing\"" + String(use_existing_network ? " checked" : "") + "></label><label class=\"toggle\"><span>Enable OTA updates</span><input type=\"checkbox\" name=\"ota\"" + String(ota_enabled ? " checked" : "") + "\"></label><div style=\"display:grid;grid-template-columns:1fr 1fr;gap:12px;\"><button type=\"button\" style=\"background:#2C3C62;color:#fff;border:none;padding:12px 16px;border-radius:14px;cursor:pointer;\" onclick=\"window.location.href='/'\">Cancel</button><button type=\"submit\">Save settings</button></div></form>";
    if (ota_enabled) {
        html += "<p style=\"margin-top:20px;font-size:.95rem;\"><a href=\"/ota\" style=\"color:#7CA8FF;text-decoration:none;\">Open OTA upload page</a></p>";
    }
    html += "<p class=\"note\">Press Scan to discover available Wi-Fi networks, then select one and enter the password.</p>";
    html += "<script>function scanNetworks(){var sel=document.getElementById('stationSelect');var preserved=sel.dataset.current||sel.value||'';sel.innerHTML='<option>Scanning...</option>';try{fetch('/scan').then(function(res){return res.json();}).then(function(list){var selectedFound=false;sel.innerHTML='<option value=\"\">Choose network</option>';list.forEach(function(n){var opt=document.createElement('option');opt.value=n.ssid;opt.textContent=n.ssid+' ('+n.rssi+' dBm)';if(preserved && n.ssid===preserved){opt.selected=true;selectedFound=true;}sel.appendChild(opt);});if(preserved && !selectedFound){var opt=document.createElement('option');opt.value=preserved;opt.textContent=preserved+' (saved)';opt.selected=true;sel.appendChild(opt);} }).catch(function(e){sel.innerHTML='<option value=\"\">Scan failed</option>';console.log(e);});}catch(e){sel.innerHTML='<option value=\"\">Scan failed</option>';console.log(e);} } function confirmNetworkChange(){var useExisting=document.querySelector('input[name=\"use_existing\"]'); var stationSelect=document.getElementById('stationSelect'); if(useExisting && useExisting.checked && stationSelect && stationSelect.value){return confirm('You are about to connect the bridge to a selected Wi-Fi network.\\n\\nThis will move the device off its current AP, so you will need to reconnect your browser to the device after it joins the new network.\\n\\nContinue?');} return true;} function setTheme(mode){var body=document.body;var btn=document.getElementById('themeToggle'); if(mode==='light'){body.classList.add('light-mode');btn.textContent='🌙';btn.title='Switch to dark mode';}else{body.classList.remove('light-mode');btn.textContent='☀';btn.title='Switch to light mode';}localStorage.setItem('theme',mode);} function toggleTheme(){setTheme(document.body.classList.contains('light-mode')?'dark':'light');} function initTheme(){var saved=localStorage.getItem('theme');setTheme(saved==='light'?'light':'dark');var btn=document.getElementById('themeToggle'); if(btn){btn.addEventListener('click',toggleTheme);} } document.addEventListener('DOMContentLoaded', function(){var btn=document.getElementById('scanButton'); if(btn){btn.addEventListener('click', scanNetworks);} initTheme();});</script>";
    html += "</main></body></html>";
    server.send(200, "text/html", html);
}

void handleSaveSettings() {
    if (server.hasArg("ssid")) {
        current_ssid = server.arg("ssid");
    }
    if (server.hasArg("password")) {
        current_password = server.arg("password");
    }
    if (server.hasArg("station_ssid")) {
        station_ssid = server.arg("station_ssid");
    }
    if (server.hasArg("station_password")) {
        station_password = server.arg("station_password");
    }
    if (server.hasArg("device_name")) {
        String deviceName = server.arg("device_name");
        deviceName.trim();
        memset(wifiSettings.device_name, 0, sizeof(wifiSettings.device_name));
        strncpy(wifiSettings.device_name, deviceName.c_str(), sizeof(wifiSettings.device_name) - 1);
    }
    if (server.hasArg("static_ip")) {
        String value = server.arg("static_ip");
        value.trim();
        memset(wifiSettings.static_ip, 0, sizeof(wifiSettings.static_ip));
        strncpy(wifiSettings.static_ip, value.c_str(), sizeof(wifiSettings.static_ip) - 1);
    }
    if (server.hasArg("gateway")) {
        String value = server.arg("gateway");
        value.trim();
        memset(wifiSettings.gateway, 0, sizeof(wifiSettings.gateway));
        strncpy(wifiSettings.gateway, value.c_str(), sizeof(wifiSettings.gateway) - 1);
    }
    if (server.hasArg("subnet")) {
        String value = server.arg("subnet");
        value.trim();
        memset(wifiSettings.subnet, 0, sizeof(wifiSettings.subnet));
        strncpy(wifiSettings.subnet, value.c_str(), sizeof(wifiSettings.subnet) - 1);
    }
    if (server.hasArg("dns")) {
        String value = server.arg("dns");
        value.trim();
        memset(wifiSettings.dns, 0, sizeof(wifiSettings.dns));
        strncpy(wifiSettings.dns, value.c_str(), sizeof(wifiSettings.dns) - 1);
    }
    if (server.hasArg("use_static_ip")) {
        wifiSettings.use_static_ip = 1;
    } else {
        wifiSettings.use_static_ip = 0;
    }
    if (server.hasArg("station_reconnect_interval")) {
        int interval = server.arg("station_reconnect_interval").toInt();
        if (interval < 1) {
            interval = 60;
        }
        wifiSettings.station_reconnect_interval = (uint16_t)interval;
    }
    if (server.hasArg("max_reconnect_attempts")) {
        int attempts = server.arg("max_reconnect_attempts").toInt();
        if (attempts < 1) {
            attempts = 1;
        }
        if (attempts > 20) {
            attempts = 20;
        }
        wifiSettings.max_reconnect_attempts = (uint8_t)attempts;
    }
    if (server.hasArg("scan_timeout_seconds")) {
        int timeout = server.arg("scan_timeout_seconds").toInt();
        if (timeout < 1) {
            timeout = 10;
        }
        wifiSettings.scan_timeout_seconds = (uint16_t)timeout;
    }
    if (server.hasArg("status_refresh_interval_ms")) {
        int interval = server.arg("status_refresh_interval_ms").toInt();
        if (interval < 200) {
            interval = 1500;
        }
        if (interval > 10000) {
            interval = 10000;
        }
        wifiSettings.status_refresh_interval_ms = (uint16_t)interval;
    }
    if (server.hasArg("ota_password")) {
        String otaPassword = server.arg("ota_password");
        otaPassword.trim();
        memset(wifiSettings.ota_password, 0, sizeof(wifiSettings.ota_password));
        strncpy(wifiSettings.ota_password, otaPassword.c_str(), sizeof(wifiSettings.ota_password) - 1);
    }
    if (server.hasArg("ota_access_only_on_ap")) {
        wifiSettings.ota_access_only_on_ap = 1;
    } else {
        wifiSettings.ota_access_only_on_ap = 0;
    }
    if (server.hasArg("verbose_logging")) {
        wifiSettings.verbose_logging = 1;
    } else {
        wifiSettings.verbose_logging = 0;
    }
    if (server.hasArg("ap_timeout_seconds")) {
        int timeout = server.arg("ap_timeout_seconds").toInt();
        if (timeout < 0) {
            timeout = 0;
        }
        wifiSettings.ap_timeout_seconds = (uint16_t)timeout;
    }
    use_existing_network = server.hasArg("use_existing");
    ota_enabled = server.hasArg("ota");
    if (use_existing_network && station_ssid.length() > 0) {
        if (!tryStationConnect()) {
            startAccessPoint();
        }
    } else {
        startAccessPoint();
    }
    if (ota_enabled) {
        setupOTA();
    }

    strncpy(wifiSettings.ap_ssid, current_ssid.c_str(), sizeof(wifiSettings.ap_ssid) - 1);
    strncpy(wifiSettings.ap_password, current_password.c_str(), sizeof(wifiSettings.ap_password) - 1);
    strncpy(wifiSettings.station_ssid, station_ssid.c_str(), sizeof(wifiSettings.station_ssid) - 1);
    strncpy(wifiSettings.station_password, station_password.c_str(), sizeof(wifiSettings.station_password) - 1);
    wifiSettings.use_existing = use_existing_network ? 1 : 0;
    wifiSettings.ota_enabled = ota_enabled ? 1 : 0;
    saveWifiSettings();

    String response = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"2;url=/settings\"><title>Saved</title></head><body style=\"background:#07101F;color:#E8F1FF;font-family:Inter,system-ui,sans-serif;padding:24px;\"><h1>Settings saved</h1><p>Wi-Fi and OTA settings updated. Redirecting back to settings...</p></body></html>";
    server.send(200, "text/html", response);
}

void handleToggleM365Overtemp() {
    bool enable = server.hasArg("enable") && server.arg("enable") == "1";
    g_M365OvertempTest = enable;
    String response = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"0;url=/m365-scooter\"><title>Overtemp Test</title></head><body></body></html>";
    server.send(200, "text/html", response);
}

void handleM365Scooter() {
    String serialValue = escapeHtml(String(g_Settings.serial));
    String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>M365-SCOOTER</title>";
    html += "<style>body{margin:0;font-family:Inter,system-ui,sans-serif;background:#07101F;color:#E8F1FF;}header{padding:24px 22px;background:#0E1B32;border-bottom:1px solid rgba(255,255,255,.08);}header h1{margin:0;font-size:2rem;}main{padding:24px;}form{display:grid;gap:16px;max-width:520px;}label{display:grid;gap:8px;font-size:.95rem;color:#A7B8D6;}input[type=text],select{width:100%;padding:12px 14px;border-radius:14px;border:1px solid rgba(255,255,255,.12);background:rgba(255,255,255,.05);color:#E8F1FF;}button{border:none;padding:12px 16px;border-radius:14px;background:#3C6DE0;color:#fff;font-weight:700;cursor:pointer;}button:hover{background:#5A82F5;}a{color:#7CA8FF;text-decoration:none;} .theme-toggle{position:absolute;top:24px;right:24px;width:44px;height:44px;border-radius:50%;display:inline-flex;align-items:center;justify-content:center;background:rgba(255,255,255,.08);color:#E8F1FF;font-size:1.2rem;border:1px solid rgba(255,255,255,.12);transition:background .2s ease, transform .15s ease;} .theme-toggle:hover{background:rgba(255,255,255,.16);transform:translateY(-1px);} .section-title{margin:24px 0 8px;font-size:1rem;font-weight:700;color:#E8F1FF;border-bottom:1px solid rgba(255,255,255,.12);padding-bottom:8px;} .meta{margin-top:20px;color:#8FA5D1;font-size:.95rem;} body.light-mode{background:#F3F6FF;color:#0F172A;} body.light-mode header{background:radial-gradient(circle at top left,#EAF0FF 0%,#DCE5F5 45%,#F8FBFF 100%);border-bottom:1px solid rgba(15,23,42,.08);} body.light-mode input[type=text],body.light-mode select{background:#FFFFFF;color:#0F172A;border:1px solid rgba(15,23,42,.12);} body.light-mode .theme-toggle{background:rgba(15,23,42,.06);color:#0F172A;border-color:rgba(15,23,42,.12);} body.light-mode a{color:#2563EB;} body.light-mode .section-title{color:#0F172A;}</style>";
    html += "</head><body><header><h1>M365-SCOOTER</h1><button class=\"theme-toggle\" type=\"button\" id=\"themeToggle\" title=\"Toggle theme\">☀</button></header><main>";
    html += "<form id=\"m365SettingsForm\" method=\"POST\" action=\"/save-m365-settings\">";
    html += "<div class=\"section-title\">Bridge settings</div>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">These settings are preset for a standard 10s battery pack used by M365 scooters. Adjust only if your battery configuration differs.</p>";
    html += "<label>Battery serial number<input type=\"text\" name=\"serial\" maxlength=\"13\" value=\"" + serialValue + "\"></label>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">Enter the battery pack serial exactly as printed on the pack label. This serial often encodes the pack identity and battery specifications, so use the full label value and avoid spaces or extra characters unless they are part of the printed serial.</p>";
    html += "<label>Bridge UART baudrate<select name=\"baudrate\">";
    html += "<option value=\"9600\"" + String(M365BaudRate == 9600 ? " selected" : "") + ">9600</option>";
    html += "<option value=\"19200\"" + String(M365BaudRate == 19200 ? " selected" : "") + ">19200</option>";
    html += "<option value=\"38400\"" + String(M365BaudRate == 38400 ? " selected" : "") + ">38400</option>";
    html += "<option value=\"57600\"" + String(M365BaudRate == 57600 ? " selected" : "") + ">57600</option>";
    html += "<option value=\"115200\"" + String(M365BaudRate == 115200 ? " selected" : "") + ">115200 (standard)</option>";
    html += "<option value=\"230400\"" + String(M365BaudRate == 230400 ? " selected" : "") + ">230400</option>";
    html += "</select></label>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">This baudrate configures the M365 controller on UART1. Use 115200 for the standard controller UART1 speed unless your scooter specifically requires a different rate.</p>";
    html += "<div class=\"section-title\">BMS polling</div>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">These values are tuned for a standard 10s battery pack. Keep the poll interval above 200ms to avoid overloading the scooter controller.</p>";
    html += "<label>Battery poll interval (ms)<input type=\"number\" name=\"bms_poll_interval_ms\" min=\"200\" value=\"" + String(g_Settings.bms_poll_interval_ms) + "\"></label>";
    html += "<label>Command retry count<input type=\"number\" name=\"bms_command_retry_count\" min=\"1\" max=\"10\" value=\"" + String(g_Settings.bms_command_retry_count) + "\"></label>";
    html += "<div class=\"section-title\">Battery warnings</div>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">Standard 10s battery thresholds are a warning at 3.1V per cell and critical at 2.9V per cell. These values are per-cell voltages in millivolts, not pack voltage. Change them only if your pack requires different cutoffs.</p>";
    html += "<label>Warning voltage per cell (mV)<input type=\"number\" name=\"warning_voltage\" min=\"2500\" value=\"" + String(g_Settings.warning_voltage) + "\"></label>";
    html += "<label>Critical voltage per cell (mV)<input type=\"number\" name=\"critical_voltage\" min=\"2500\" value=\"" + String(g_Settings.critical_voltage) + "\"></label>";
    html += "<label>Warning charge current (mA)<input type=\"number\" name=\"warning_charge_current\" min=\"0\" value=\"" + String(g_Settings.warning_charge_current) + "\"></label>";
    html += "<label>Warning discharge current (mA)<input type=\"number\" name=\"warning_discharge_current\" min=\"0\" value=\"" + String(g_Settings.warning_discharge_current) + "\"></label>";
    html += "<label class=\"toggle\"><span>Map larger packs to a 10-cell equivalent</span><input type=\"checkbox\" name=\"map_10_cell\"" + String(g_Settings.map_to_10_cells ? " checked" : "") + "></label>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">When enabled, packs with more than 10 cells will present only the first 10 cell voltages and a mapped total voltage so the M365 controller sees a normal 10-cell pack.</p>";
    html += "<div class=\"section-title\">Display test</div>";
    html += "<p style=\"margin:0 0 16px;color:#9BB4D6;font-size:.95rem;\">Temporarily simulate an overtemperature condition for display testing. This does not modify the real BMS protection logic.</p>";
    html += "<div style=\"display:grid;gap:12px;\"><button type=\"button\" onclick=\"window.location.href='/toggle-overtemp?enable=1'\">Enable overtemp test</button><button type=\"button\" onclick=\"window.location.href='/toggle-overtemp?enable=0'\">Disable overtemp test</button></div>";
    html += "<p class=\"meta\">Overtemp test: <strong>" + String(g_M365OvertempTest ? "enabled" : "disabled") + "</strong></p>";
    html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:12px;\"><button type=\"button\" onclick=\"window.location.href='/'\">Back</button><button type=\"submit\">Save settings</button></div>";
    html += "</form>";
    html += "<button type=\"button\" style=\"margin-top:16px;background:#D64545;color:#fff;border:none;padding:12px 16px;border-radius:14px;cursor:pointer;\" onclick=\"if(confirm('Reset all stored settings to default and restart the device?')) window.location.href='/reset-settings';\">Reset defaults</button>";
    html += "<p class=\"meta\">Current bridge serial: <strong>" + serialValue + "</strong><br>Current UART baud: <strong>" + String(M365BaudRate) + "</strong></p>";
    html += "<script>function setTheme(mode){var body=document.body;var button=document.getElementById('themeToggle');if(mode==='light'){body.classList.add('light-mode'); if(button){button.textContent='🌙';button.title='Switch to dark mode';}}else{body.classList.remove('light-mode'); if(button){button.textContent='☀';button.title='Switch to light mode';}}localStorage.setItem('theme',mode);}function toggleTheme(){setTheme(document.body.classList.contains('light-mode')?'dark':'light');}function initTheme(){var saved=localStorage.getItem('theme');setTheme(saved==='light'?'light':'dark');var btn=document.getElementById('themeToggle');if(btn){btn.addEventListener('click',toggleTheme);}}if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',initTheme);}else{initTheme();}</script>";
    html += "</main></body></html>";
    server.send(200, "text/html", html);
}

void handleSaveM365Settings() {
    if (server.hasArg("serial")) {
        String serialValue = server.arg("serial");
        serialValue.trim();
        if (serialValue.length() > 13) {
            serialValue = serialValue.substring(0, 13);
        }
        strncpy(g_Settings.serial, serialValue.c_str(), sizeof(g_Settings.serial) - 1);
        g_Settings.serial[sizeof(g_Settings.serial) - 1] = '\0';
    }
    if (server.hasArg("baudrate")) {
        uint32_t baud = server.arg("baudrate").toInt();
        if (baud == 9600 || baud == 19200 || baud == 38400 || baud == 57600 || baud == 115200 || baud == 230400) {
            g_Settings.m365_baud = baud;
            M365BaudRate = baud;
            M365Serial.begin(M365BaudRate, SERIAL_8N1, M365_UART_RX_PIN, M365_UART_TX_PIN);
        }
    }
    if (server.hasArg("bms_poll_interval_ms")) {
        int interval = server.arg("bms_poll_interval_ms").toInt();
        if (interval >= 200) {
            g_Settings.bms_poll_interval_ms = (uint16_t)interval;
        }
    }
    if (server.hasArg("bms_command_retry_count")) {
        int retries = server.arg("bms_command_retry_count").toInt();
        if (retries < 1) {
            retries = 1;
        }
        if (retries > 10) {
            retries = 10;
        }
        g_Settings.bms_command_retry_count = (uint8_t)retries;
    }
    if (server.hasArg("warning_voltage")) {
        int volts = server.arg("warning_voltage").toInt();
        g_Settings.warning_voltage = volts > 0 ? (uint16_t)volts : g_Settings.warning_voltage;
    }
    if (server.hasArg("critical_voltage")) {
        int volts = server.arg("critical_voltage").toInt();
        g_Settings.critical_voltage = volts > 0 ? (uint16_t)volts : g_Settings.critical_voltage;
    }
    if (server.hasArg("warning_charge_current")) {
        int amps = server.arg("warning_charge_current").toInt();
        g_Settings.warning_charge_current = amps > 0 ? (uint16_t)amps : g_Settings.warning_charge_current;
    }
    if (server.hasArg("warning_discharge_current")) {
        int amps = server.arg("warning_discharge_current").toInt();
        g_Settings.warning_discharge_current = amps > 0 ? (uint16_t)amps : g_Settings.warning_discharge_current;
    }
    g_Settings.map_to_10_cells = server.hasArg("map_10_cell") ? 1 : 0;
    saveSettings();
    updateM365Data();
    String response = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"1;url=/m365-scooter\"><title>Saved</title></head><body style=\"background:#07101F;color:#E8F1FF;font-family:Inter,system-ui,sans-serif;padding:24px;\"><h1>Saved</h1><p>M365 scooter settings updated. Redirecting back...</p></body></html>";
    server.send(200, "text/html", response);
}

void handleResetSettings() {
    resetSettings();
    current_ssid = String(wifiSettings.ap_ssid);
    current_password = String(wifiSettings.ap_password);
    station_ssid = String(wifiSettings.station_ssid);
    station_password = String(wifiSettings.station_password);
    use_existing_network = wifiSettings.use_existing;
    ota_enabled = wifiSettings.ota_enabled;
    M365BaudRate = g_Settings.m365_baud;
    M365Serial.begin(M365BaudRate, SERIAL_8N1, M365_UART_RX_PIN, M365_UART_TX_PIN);
    startAccessPoint();

    String response = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"2;url=/settings\"><title>Reset</title></head><body style=\"background:#07101F;color:#E8F1FF;font-family:Inter,system-ui,sans-serif;padding:24px;\"><h1>Reset complete</h1><p>Stored settings have been reset to defaults. Redirecting back to settings...</p></body></html>";
    server.send(200, "text/html", response);
}

void handleScanNetworks() {
    bool hadApOnly = WiFi.getMode() == WIFI_AP;
    if (hadApOnly) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    int count = WiFi.scanNetworks();
    if (count < 0) {
        count = 0;
    }
    String json = "[";
    for (int i = 0; i < count; ++i) {
        if (i) json += ",";
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + "}";
    }
    json += "]";
    if (hadApOnly) {
        WiFi.mode(WIFI_AP);
        delay(100);
    }
    server.send(200, "application/json", json);
}

void handleResetSettings();

void registerWebRoutes() {
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/save-settings", HTTP_POST, handleSaveSettings);
    server.on("/scan", HTTP_GET, handleScanNetworks);
    server.on("/m365-scooter", handleM365Scooter);
    server.on("/toggle-overtemp", HTTP_GET, handleToggleM365Overtemp);
    server.on("/save-m365-settings", HTTP_POST, handleSaveM365Settings);
    server.on("/reset-settings", HTTP_GET, handleResetSettings);
    server.on("/ota", HTTP_GET, handleOtaPage);
    server.on("/ota", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        if (Update.hasError()) {
            server.send(500, "text/plain", "OTA update failed");
        } else {
            server.send(200, "text/plain", "OTA update complete, rebooting...");
            delay(100);
            ESP.restart();
        }
    }, handleOtaUpload);
}
