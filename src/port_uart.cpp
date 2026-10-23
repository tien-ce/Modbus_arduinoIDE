#include <Arduino.h>
#include <HardwareSerial.h>
#include <port.h>
#include <mbport.h>
#include <mbrtu.h>
#include <mbinst.h>

/**
 * @brief Direct mapping from RTU mode macros to Arduino SerialConfig.
 * @param rtu_mode Custom RTU macro (e.g., RTU_8N1, RTU_8E1).
 * @return Corresponding Arduino SerialConfig value.
 */
static SerialConfig get_arduino_serial_config(ModbusSerialConfig_t rtu_mode) 
{
    switch (rtu_mode) {
        case RTU_8N1: return SERIAL_8N1;
        case RTU_8N2: return SERIAL_8N2;
        case RTU_8E1: return SERIAL_8E1;
        case RTU_8E2: return SERIAL_8E2;
        case RTU_8O1: return SERIAL_8O1;
        case RTU_8O2: return SERIAL_8O2;

        case RTU_7N1: return SERIAL_7N1;
        case RTU_7N2: return SERIAL_7N2;
        case RTU_7E1: return SERIAL_7E1;
        case RTU_7E2: return SERIAL_7E2;
        case RTU_7O1: return SERIAL_7O1;
        case RTU_7O2: return SERIAL_7O2;

        default:      return SERIAL_8N1;
    }
}

/**
 * @brief Opens mb->serial with the baud rate, serial config, TX/RX pins and
 * optional RS485 driver-enable pin stored in the instance. The receive
 * callback (invoked from ISR context on byte reception) feeds the RTU
 * receive FSM of the same instance.
 */
bool xMBPortUartInit( ModbusRS485 *mb )
{
    HardwareSerial *serial = mb->serial;
    if (serial == NULL) return false;

    serial->begin(mb->baudrate, get_arduino_serial_config(mb->serial_config), mb->rxPin, mb->txPin);
    if (!(*serial))
    {
        return false;
    }

    /* Register callback */
    serial->onReceive([mb]() { xMBRTUReceiveFSM(mb); });

    if (mb->enPin >= 0)
    {
        pinMode(mb->enPin, OUTPUT);
        digitalWrite(mb->enPin, LOW); /* Default to receive mode */
    }

    return true;
}

void vMBPortUartDeinit( ModbusRS485 *mb )
{
    if (mb->serial == NULL) return;

    mb->serial->end();
}

void vMBPortUartRead (ModbusRS485 *mb, uint8_t *buffer, int *length)
{
    HardwareSerial *serial = mb->serial;
    if (serial == NULL) return;

    *length = serial->available();
    for (int i = 0; i < *length; i++)
    {
        buffer[i] = (uint8_t)serial->read();
    }
}

void vMBPortUartWrite (ModbusRS485 *mb, uint8_t *buffer, uint16_t length)
{
    HardwareSerial *serial = mb->serial;
    if (serial == NULL) return;

    if (mb->enPin >= 0)
    {
        digitalWrite(mb->enPin, HIGH); /* Switch RS485 transceiver to transmit */
    }

    serial->write(buffer, length);

    if (mb->enPin >= 0)
    {
        serial->flush(); /* Wait for the last byte to physically leave the UART */
        digitalWrite(mb->enPin, LOW); /* Switch back to receive */
    }
}
