package com.m365jbd.monitor

/**
 * Build JBD BMS command packets.
 *
 * JBD read  command:  DD A5 <reg> 00 <crc_hi> <crc_lo> 77
 * JBD write command:  DD 5A <reg> <data_len> <data...> <crc_hi> <crc_lo> 77
 * CRC = (0x10000 - sum(crc_region)) & 0xFFFF, big-endian
 *
 * IMPORTANT — register map and units vary by JBD firmware version.
 * Verify with JBD PC software before writing settings to your BMS.
 *
 * Common registers (verify against your firmware):
 *   0xE1 Cell OVP     (mV, uint16)    0xE2 Cell OVP release (mV, uint16)
 *   0xE3 Cell UVP     (mV, uint16)    0xE4 Cell UVP release (mV, uint16)
 *   0xE9 Charge OCP   (10 mA, uint16) 0xEA Discharge OCP   (10 mA, uint16)
 *   0xEF High-T prot  (0.1 K, uint16) 0xF0 High-T release  (0.1 K, uint16)
 *   0xF1 Low-T prot   (0.1 K, uint16) 0xF2 Low-T release   (0.1 K, uint16)
 */
object JbdPacket {

    /** Build a read-register command for [reg]. */
    fun buildRead(reg: Byte): ByteArray {
        val body = byteArrayOf(reg, 0x00)
        val crc = crc(body)
        return byteArrayOf(0xDD.toByte(), 0xA5.toByte()) + body +
               byteArrayOf((crc shr 8).toByte(), (crc and 0xFF).toByte(), 0x77)
    }

    /** Build a write-register command sending [data] to [reg]. */
    fun buildWrite(reg: Byte, data: ByteArray): ByteArray {
        val body = byteArrayOf(reg, data.size.toByte()) + data
        val c = crc(body)
        return byteArrayOf(0xDD.toByte(), 0x5A.toByte()) + body +
               byteArrayOf((c shr 8).toByte(), (c and 0xFF).toByte(), 0x77)
    }

    /** Convenience: write a single big-endian uint16 value to [reg]. */
    fun buildWriteUInt16(reg: Byte, value: Int): ByteArray =
        buildWrite(reg, byteArrayOf((value shr 8).toByte(), (value and 0xFF).toByte()))

    /**
     * Enable or disable the JBD BMS built-in Bluetooth module.
     * !! Register 0x18 is common but FIRMWARE-DEPENDENT — verify with JBD PC software !!
     * Other known registers: 0x2B, 0xC5, 0xE0 (bit 0) depending on firmware version.
     */
    fun buildBmsBtCmd(enable: Boolean): ByteArray =
        buildWriteUInt16(0x18.toByte(), if (enable) 0x0001 else 0x0000)

    private fun crc(data: ByteArray): Int {
        var sum = 0
        for (b in data) sum += b.toInt() and 0xFF
        return (0x10000 - sum) and 0xFFFF
    }
}
