package com.m365jbd.monitor

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import java.util.UUID

@SuppressLint("MissingPermission")
object BleManager {

    data class BattData(
        val voltageMv: Int = 0,
        val currentMa: Int = 0,
        val socPct: Int = 0,
        val remainingMah: Int = 0,
        val nominalMah: Int = 0,
        val protStatus: Int = 0,
        val cellsMv: List<Int> = emptyList(),
        val tempsC10: List<Int> = emptyList()
    )

    private val mainHandler = Handler(Looper.getMainLooper())
    private var gatt: BluetoothGatt? = null
    private var scanner: android.bluetooth.le.BluetoothLeScanner? = null
    private var scanning = false

    // BLE operation queue — prevents flooding the stack with simultaneous writes.
    private val opQueue = ArrayDeque<() -> Unit>()
    private var opPending = false

    var onScanResult: ((BluetoothDevice) -> Unit)? = null
    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onDataUpdate: ((BattData) -> Unit)? = null
    var onCmdResponse: ((ByteArray) -> Unit)? = null

    private var battData = BattData()

    // ── Scan ──────────────────────────────────────────────────────────────────

    fun startScan(context: Context) {
        val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
        scanner = adapter.bluetoothLeScanner
        // No service-UUID filter: 128-bit UUIDs overflow the 31-byte adv packet and
        // land in scan-response, which Android ScanFilter ignores. Filter by name instead.
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanning = true
        scanner?.startScan(null, settings, scanCb)
    }

    fun stopScan() {
        if (scanning) {
            scanning = false
            scanner?.stopScan(scanCb)
        }
    }

    // ── Connect / disconnect ──────────────────────────────────────────────────

    fun connect(context: Context, device: BluetoothDevice) {
        stopScan()
        opQueue.clear()
        opPending = false
        gatt = device.connectGatt(context, false, gattCb, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        gatt?.disconnect()
    }

    // ── Send a raw JBD command to the ESP32 ───────────────────────────────────

    fun sendCmd(bytes: ByteArray) {
        val g = gatt ?: return
        val chr = g.getService(BleUuids.SVC_CFG)?.getCharacteristic(BleUuids.CHR_CMD) ?: return
        enqueue {
            @Suppress("DEPRECATION")
            chr.value = bytes
            @Suppress("DEPRECATION")
            g.writeCharacteristic(chr)
        }
    }

    // ── Operation queue ───────────────────────────────────────────────────────

    private fun enqueue(op: () -> Unit) {
        opQueue.addLast(op)
        if (!opPending) dequeue()
    }

    private fun dequeue() {
        if (opQueue.isEmpty()) { opPending = false; return }
        opPending = true
        opQueue.removeFirst().invoke()
    }

    // ── Scan callback ─────────────────────────────────────────────────────────

    private val scanCb = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            // device.name may be null on first sight; scanRecord.deviceName reads the adv packet directly
            val name = result.scanRecord?.deviceName ?: result.device.name ?: return
            if (name == "M365-JBD-Bridge") mainHandler.post { onScanResult?.invoke(result.device) }
        }
    }

    // ── GATT helpers ──────────────────────────────────────────────────────────

    private fun enableNotify(gatt: BluetoothGatt, chr: BluetoothGattCharacteristic) {
        enqueue {
            gatt.setCharacteristicNotification(chr, true)
            val desc = chr.getDescriptor(BleUuids.CCCD)
            if (desc == null) { opPending = false; dequeue(); return@enqueue }
            @Suppress("DEPRECATION")
            desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            @Suppress("DEPRECATION")
            gatt.writeDescriptor(desc)
        }
    }

    private fun subscribeAll(gatt: BluetoothGatt) {
        val battSvc = gatt.getService(BleUuids.SVC_BATT) ?: return
        listOf(
            BleUuids.CHR_VOLT, BleUuids.CHR_CUR, BleUuids.CHR_SOC,
            BleUuids.CHR_REM,  BleUuids.CHR_PROT, BleUuids.CHR_CELLS, BleUuids.CHR_TEMPS
        ).mapNotNull { battSvc.getCharacteristic(it) }
         .forEach { enableNotify(gatt, it) }

        gatt.getService(BleUuids.SVC_CFG)
            ?.getCharacteristic(BleUuids.CHR_CMD)
            ?.let { enableNotify(gatt, it) }
    }

    // ── Parse characteristic value ────────────────────────────────────────────

    private fun parseUpdate(uuid: UUID, value: ByteArray) {
        if (value.isEmpty()) return
        fun u16(b: ByteArray, off: Int = 0) =
            (b[off].toInt() and 0xFF) or ((b[off + 1].toInt() and 0xFF) shl 8)
        fun s16(b: ByteArray, off: Int = 0) =
            u16(b, off).let { if (it >= 0x8000) it - 0x10000 else it }

        battData = when (uuid) {
            BleUuids.CHR_VOLT  -> if (value.size >= 2) battData.copy(voltageMv    = u16(value)) else battData
            BleUuids.CHR_CUR   -> if (value.size >= 2) battData.copy(currentMa    = s16(value)) else battData
            BleUuids.CHR_SOC   -> battData.copy(socPct       = value[0].toInt() and 0xFF)
            BleUuids.CHR_REM   -> if (value.size >= 2) battData.copy(remainingMah = u16(value)) else battData
            BleUuids.CHR_NOM   -> if (value.size >= 2) battData.copy(nominalMah   = u16(value)) else battData
            BleUuids.CHR_PROT  -> if (value.size >= 2) battData.copy(protStatus   = u16(value)) else battData
            BleUuids.CHR_CELLS -> {
                val n = (value[0].toInt() and 0xFF).coerceAtMost((value.size - 1) / 2)
                battData.copy(cellsMv = (0 until n).map { i -> u16(value, 1 + i * 2) })
            }
            BleUuids.CHR_TEMPS -> {
                val n = (value[0].toInt() and 0xFF).coerceAtMost((value.size - 1) / 2)
                battData.copy(tempsC10 = (0 until n).map { i -> s16(value, 1 + i * 2) })
            }
            else -> battData
        }
        mainHandler.post { onDataUpdate?.invoke(battData) }
    }

    private fun handleCharChanged(uuid: UUID, value: ByteArray) {
        if (uuid == BleUuids.CHR_CMD) {
            mainHandler.post { onCmdResponse?.invoke(value) }
        } else {
            parseUpdate(uuid, value)
        }
    }

    // ── GATT callback ─────────────────────────────────────────────────────────

    private val gattCb = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED    -> gatt.discoverServices()
                BluetoothProfile.STATE_DISCONNECTED -> {
                    this@BleManager.gatt = null
                    opQueue.clear(); opPending = false
                    mainHandler.post { onDisconnected?.invoke() }
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                subscribeAll(gatt)
                mainHandler.post { onConnected?.invoke() }
            }
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int
        ) { opPending = false; dequeue() }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt, chr: BluetoothGattCharacteristic, status: Int
        ) { opPending = false; dequeue() }

        // API < 33
        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt, chr: BluetoothGattCharacteristic
        ) { handleCharChanged(chr.uuid, chr.value) }

        // API 33+
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray
        ) { handleCharChanged(chr.uuid, value) }

        // API < 33
        @Deprecated("Deprecated in Java")
        @Suppress("DEPRECATION")
        override fun onCharacteristicRead(
            gatt: BluetoothGatt, chr: BluetoothGattCharacteristic, status: Int
        ) { if (status == BluetoothGatt.GATT_SUCCESS) parseUpdate(chr.uuid, chr.value) }

        // API 33+
        override fun onCharacteristicRead(
            gatt: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray, status: Int
        ) { if (status == BluetoothGatt.GATT_SUCCESS) parseUpdate(chr.uuid, value) }
    }
}
