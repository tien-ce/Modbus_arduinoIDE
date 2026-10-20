/*
 * TwoBuses
 *
 * Runs two independent Modbus RTU masters at the same time, one per RS485
 * bus / HardwareSerial port. Every bus has its own ModbusRS485 instance that
 * holds all of its state (locks, buffers, RTU state machine, T3.5 timer and
 * worker task), so the two never interfere with each other.
 *
 * Bus 1: Serial1, 9600 8N1, polls slave 0x01 input register 0x03E8.
 * Bus 2: Serial2, 19200 8E1, polls slave 0x02 holding register 0x0000.
 *
 * Adjust the pins to your board. Use -1 for the enable pin if the RS485
 * transceiver auto-directions itself.
 */
#include <Arduino.h>
#include "modbus.h"

// Bus 1
#define BUS1_TX_PIN   17
#define BUS1_RX_PIN   18
#define BUS1_EN_PIN   21

// Bus 2
#define BUS2_TX_PIN   4
#define BUS2_RX_PIN   5
#define BUS2_EN_PIN   -1

// One instance per bus; must outlive the stack, so keep them global/static.
static ModbusRS485 bus1;
static ModbusRS485 bus2;

// Reads one register from a slave on the given bus and prints it.
static void pollRegister(const char *name, ModbusRS485 &bus, UCHAR slave, UCHAR function, USHORT addr)
{
    SendRetType ret = ModbusSend(bus, slave, function, addr, 1, 200);

    if (ret != ESEND_NOERR) {
        Serial.printf("[%s] Modbus Send Error: %s\n", name, ModbusErrToStr(ret));
        return;
    }

    UCHAR rx_buf[256];
    USHORT rx_len = 0;
    ModbusReceive(bus, rx_buf, &rx_len);

    if (rx_len >= 2) {
        // ModbusReceive() returns register bytes in Modbus (big-endian) order
        uint16_t value = (rx_buf[0] << 8) | rx_buf[1];
        Serial.printf("[%s] Register value: %u\n", name, value);
    }
}

void setup() {
    Serial.begin(115200);

    if (!ModbusInit(bus1, Serial1, 9600, RTU_8N1, BUS1_TX_PIN, BUS1_RX_PIN, BUS1_EN_PIN) ||
        !ModbusStart(bus1)) {
        Serial.println("Init bus 1 failed!");
        return;
    }

    if (!ModbusInit(bus2, Serial2, 19200, RTU_8E1, BUS2_TX_PIN, BUS2_RX_PIN, BUS2_EN_PIN) ||
        !ModbusStart(bus2)) {
        Serial.println("Init bus 2 failed!");
        return;
    }

    Serial.println("Both Modbus buses started");
}

void loop() {
    pollRegister("bus1", bus1, 0x01, 0x04, 0x03E8);  // Read Input Register
    pollRegister("bus2", bus2, 0x02, 0x03, 0x0000);  // Read Holding Register
    delay(1000);
}
