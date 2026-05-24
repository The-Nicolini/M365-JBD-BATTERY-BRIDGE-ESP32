package com.m365jbd.monitor

import java.util.UUID

/** UUIDs must match the ESP32 firmware exactly. */
object BleUuids {
    val SVC_BATT  = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c3319100")
    val CHR_VOLT  = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2600")
    val CHR_CUR   = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2601")
    val CHR_SOC   = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2602")
    val CHR_REM   = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2603")
    val CHR_NOM   = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2604")
    val CHR_PROT  = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2605")
    val CHR_CELLS = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2606")
    val CHR_TEMPS = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2607")
    val SVC_CFG   = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c3319200")
    val CHR_CMD   = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b2610")
    val CCCD      = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}
