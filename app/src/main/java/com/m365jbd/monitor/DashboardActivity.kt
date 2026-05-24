package com.m365jbd.monitor

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.m365jbd.monitor.databinding.ActivityDashboardBinding

class DashboardActivity : AppCompatActivity() {

    private lateinit var binding: ActivityDashboardBinding
    // Assume BMS BT is on when we first connect. Toggled by the user.
    private var bmsBtOn = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityDashboardBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.btnSettings.setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }

        binding.btnDisconnect.setOnClickListener {
            BleManager.disconnect()
            finish()
        }

        binding.btnBmsBt.setOnClickListener {
            bmsBtOn = !bmsBtOn
            BleManager.sendCmd(JbdPacket.buildBmsBtCmd(bmsBtOn))
            updateBmsBtButton()
        }

        BleManager.onCmdResponse = { bytes ->
            val ok  = bytes.size >= 3 && bytes[2] == 0x00.toByte()
            val hex = bytes.joinToString(" ") { "%02X".format(it) }
            Toast.makeText(this, if (ok) "BMS BT: OK" else "BMS BT: Error $hex", Toast.LENGTH_SHORT).show()
        }

        BleManager.onDataUpdate  = { data -> updateUi(data) }
        BleManager.onDisconnected = {
            startActivity(Intent(this, ScanActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            })
            finish()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        BleManager.onDataUpdate   = null
        BleManager.onDisconnected = null
        BleManager.onCmdResponse  = null
    }

    private fun updateBmsBtButton() {
        binding.btnBmsBt.text = if (bmsBtOn) "BMS BT ON" else "BMS BT OFF"
    }

    private fun updateUi(d: BleManager.BattData) {
        binding.tvSoc.text      = "${d.socPct}%"
        binding.tvVoltage.text  = "%.2f V".format(d.voltageMv / 1000.0)
        binding.tvCurrent.text  = "%.2f A".format(d.currentMa / 1000.0)
        binding.tvCapacity.text = "${d.remainingMah} mAh  /  ${d.nominalMah} mAh"

        if (d.tempsC10.isNotEmpty()) {
            binding.tvTemps.text = d.tempsC10.mapIndexed { i, t ->
                "T${i + 1}: %.1f °C".format(t / 10.0)
            }.joinToString("   ")
        }

        if (d.cellsMv.isNotEmpty()) {
            val min   = d.cellsMv.min()
            val max   = d.cellsMv.max()
            val avg   = d.cellsMv.average()
            val delta = max - min
            binding.tvCellSummary.text =
                "n=%d   Min %.3fV   Max %.3fV   Avg %.3fV   Δ %d mV".format(
                    d.cellsMv.size, min / 1000.0, max / 1000.0, avg / 1000.0, delta
                )
            binding.tvCellDetail.text = d.cellsMv.mapIndexed { i, mv ->
                "C${i + 1}: %.3f".format(mv / 1000.0)
            }.joinToString("  ")
        }

        binding.tvProtStatus.text = protStatusText(d.protStatus)
    }

    private fun protStatusText(status: Int): String {
        if (status == 0) return "OK"
        return listOf(
            0  to "Cell OVP",    1  to "Cell UVP",
            2  to "Pack OVP",    3  to "Pack UVP",
            4  to "Chg OCP",     5  to "Dchg OCP",
            6  to "Chg OTP",     7  to "Dchg OTP",
            8  to "Chg UTP",     9  to "Dchg UTP",
            10 to "Short",       11 to "IC Fault",
            12 to "Soft Lock"
        ).filter { (bit, _) -> (status shr bit) and 1 != 0 }
         .joinToString(", ") { (_, name) -> name }
    }
}
