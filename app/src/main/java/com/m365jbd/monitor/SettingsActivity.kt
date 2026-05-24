package com.m365jbd.monitor

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.m365jbd.monitor.databinding.ActivitySettingsBinding

/**
 * BMS settings screen.
 *
 * Each setting writes a JBD command through the ESP32 to the BMS.
 * The BMS response is shown in tvCmdResponse.
 *
 * VERIFY register addresses and units with JBD PC software before use.
 * Wrong values can damage the battery pack.
 */
class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = "BMS Settings"

        BleManager.onCmdResponse = { bytes -> showResponse(bytes) }
        BleManager.onDisconnected = {
            startActivity(Intent(this, ScanActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            })
            finish()
        }

        // ── Cell overvoltage protection (mV) ─────────────────────────────────
        binding.btnWriteCellOvp.setOnClickListener {
            val mv = binding.etCellOvp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mV")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xE1.toByte(), mv))
            toast("→ Cell OVP: ${mv} mV")
        }

        // ── Cell OVP release (mV) ─────────────────────────────────────────────
        binding.btnWriteCellOvpRel.setOnClickListener {
            val mv = binding.etCellOvpRel.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mV")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xE2.toByte(), mv))
            toast("→ Cell OVP release: ${mv} mV")
        }

        // ── Cell undervoltage protection (mV) ────────────────────────────────
        binding.btnWriteCellUvp.setOnClickListener {
            val mv = binding.etCellUvp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mV")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xE3.toByte(), mv))
            toast("→ Cell UVP: ${mv} mV")
        }

        // ── Cell UVP release (mV) ─────────────────────────────────────────────
        binding.btnWriteCellUvpRel.setOnClickListener {
            val mv = binding.etCellUvpRel.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mV")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xE4.toByte(), mv))
            toast("→ Cell UVP release: ${mv} mV")
        }

        // ── Discharge overcurrent protection (mA, stored as 10 mA units) ─────
        binding.btnWriteDchgOcp.setOnClickListener {
            val ma = binding.etDchgOcp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mA")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xEA.toByte(), ma / 10))
            toast("→ Discharge OCP: ${ma} mA")
        }

        // ── Charge overcurrent protection (mA, stored as 10 mA units) ────────
        binding.btnWriteChgOcp.setOnClickListener {
            val ma = binding.etChgOcp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter a value in mA")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xE9.toByte(), ma / 10))
            toast("→ Charge OCP: ${ma} mA")
        }

        // ── High temp protection (°C, stored as 0.1 K = T×10 + 2731) ─────────
        binding.btnWriteHighTemp.setOnClickListener {
            val c = binding.etHighTemp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter °C")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xEF.toByte(), c * 10 + 2731))
            toast("→ High temp protect: ${c} °C")
        }

        // ── Low temp protection (°C) ──────────────────────────────────────────
        binding.btnWriteLowTemp.setOnClickListener {
            val c = binding.etLowTemp.text.toString().toIntOrNull()
                ?: return@setOnClickListener toast("Enter °C")
            BleManager.sendCmd(JbdPacket.buildWriteUInt16(0xF1.toByte(), c * 10 + 2731))
            toast("→ Low temp protect: ${c} °C")
        }
    }

    override fun onSupportNavigateUp(): Boolean { finish(); return true }

    override fun onDestroy() {
        super.onDestroy()
        BleManager.onCmdResponse  = null
        BleManager.onDisconnected = null
    }

    private fun showResponse(bytes: ByteArray) {
        val hex = bytes.joinToString(" ") { "%02X".format(it) }
        val ok  = bytes.size >= 3 && bytes[2] == 0x00.toByte()
        binding.tvCmdResponse.text = if (ok) "✓ OK   $hex" else "✗ Error   $hex"
    }

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
}
