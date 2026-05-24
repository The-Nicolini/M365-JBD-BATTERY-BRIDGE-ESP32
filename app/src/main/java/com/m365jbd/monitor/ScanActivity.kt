package com.m365jbd.monitor

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.m365jbd.monitor.databinding.ActivityScanBinding
import com.m365jbd.monitor.databinding.ItemDeviceBinding

class ScanActivity : AppCompatActivity() {

    private lateinit var binding: ActivityScanBinding
    private val devices = mutableListOf<BluetoothDevice>()
    private val deviceAdapter = DeviceAdapter()
    private var isScanning = false
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        (getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.values.all { it }) startScan()
        else binding.tvStatus.text = "Permissions denied — cannot scan"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityScanBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.tvVersion.text = "v${BuildConfig.VERSION_NAME}"

        binding.rvDevices.layoutManager = LinearLayoutManager(this)
        binding.rvDevices.adapter = deviceAdapter

        binding.btnScan.setOnClickListener {
            if (isScanning) stopScan() else checkPermsAndScan()
        }

        BleManager.onScanResult = { device ->
            if (devices.none { it.address == device.address }) {
                devices.add(device)
                deviceAdapter.notifyItemInserted(devices.size - 1)
            }
        }

        BleManager.onConnected = {
            stopScan()
            startActivity(Intent(this, DashboardActivity::class.java))
        }

    }

    override fun onDestroy() {
        super.onDestroy()
        BleManager.onScanResult = null
        BleManager.onConnected  = null
        BleManager.stopScan()
    }

    private fun checkPermsAndScan() {
        val needed = if (Build.VERSION.SDK_INT >= 31) {
            listOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        } else {
            listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }.filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }

        if (needed.isEmpty()) startScan() else permLauncher.launch(needed.toTypedArray())
    }

    private fun startScan() {
        if (bluetoothAdapter?.isEnabled == false) {
            binding.tvStatus.text = "Bluetooth is off — enable it first"
            return
        }
        devices.clear()
        deviceAdapter.notifyDataSetChanged()
        isScanning = true
        binding.btnScan.text = "Stop"
        binding.tvStatus.text = "Scanning for JBD-Bridge…"
        BleManager.startScan(this)
    }

    private fun stopScan() {
        isScanning = false
        binding.btnScan.text = "Scan"
        binding.tvStatus.text = "Stopped. ${devices.size} device(s) found."
        BleManager.stopScan()
    }

    // ── RecyclerView adapter ──────────────────────────────────────────────────

    inner class DeviceAdapter : RecyclerView.Adapter<DeviceAdapter.VH>() {

        inner class VH(val b: ItemDeviceBinding) : RecyclerView.ViewHolder(b.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) =
            VH(ItemDeviceBinding.inflate(LayoutInflater.from(parent.context), parent, false))

        override fun getItemCount() = devices.size

        @SuppressLint("MissingPermission")
        override fun onBindViewHolder(holder: VH, position: Int) {
            val dev = devices[position]
            holder.b.tvName.text    = dev.name ?: "Unknown"
            holder.b.tvAddress.text = dev.address
            holder.b.root.setOnClickListener {
                binding.tvStatus.text = "Connecting to ${dev.name ?: dev.address}…"
                BleManager.connect(this@ScanActivity, dev)
            }
        }
    }
}
