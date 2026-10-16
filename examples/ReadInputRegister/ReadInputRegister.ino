/*
 * ReadInputRegister
 *
 * Polls a Modbus RTU slave over RS485 for 1 Input Register (function code
 * 0x04) once a second and prints the value read back over USB Serial.
 *
 * Wiring: adjust RS485_TX_PIN/RS485_RX_PIN to whichever UART pins your board
 * routes to the RS485 transceiver, and RS485_EN_PIN to the transceiver's
 * driver-enable (DE/RE) pin if it has one - pass -1 to ModbusInit() instead
 * if your transceiver auto-directions itself and has no enable pin to drive.
 */
#include <Arduino.h>
#include "modbus.h"

#define SLAVE_ID          0x01   // Modbus slave address of the device being polled
#define INPUT_REG_ADDR    0x03E8 // Input register address to read
#define READ_INPUT_REG    0x04   // Function code 04 (Read Input Registers)

#define RS485_TX_PIN      17
#define RS485_RX_PIN      18
#define RS485_EN_PIN      21     // -1 if the transceiver has no DE/RE pin to control

void setup() {
    Serial.begin(115200);

    bool ok = ModbusInit(Serial2, 9600, RTU_8N1, RS485_TX_PIN, RS485_RX_PIN, RS485_EN_PIN);
    if (!ok) {
        Serial.println("Init Modbus failed!");
        return;
    }

    Serial.println("Init Modbus successfully");
    (void)ModbusStart();
}

void loop() {
    // Send request to read 1 Input Register with a 200ms response timeout
    SendRetType ret = ModbusSend(SLAVE_ID, READ_INPUT_REG, INPUT_REG_ADDR, 1, 200);

    if (ret == ESEND_NOERR) {
        UCHAR rx_buf[256];
        USHORT rx_len = 0;
        ModbusReceive(rx_buf, &rx_len);

        if (rx_len >= 2) {
            // ModbusReceive() returns register bytes in Modbus (big-endian) order
            uint16_t value = (rx_buf[0] << 8) | rx_buf[1];

            Serial.print("Register value: ");
            Serial.println(value);
        }
    } else {
        Serial.print("Modbus Send Error: ");
        Serial.println(ModbusErrToStr(ret));
    }

    delay(1000);
}
