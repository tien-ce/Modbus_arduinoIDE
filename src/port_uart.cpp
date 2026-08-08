#include <Arduino.h>
#include <HardwareSerial.h>
#include <port.h>
#include <mbport.h>
static HardwareSerial *pMBSerial = NULL;
static int enablePin = -1; /* RS485 driver-enable (DE/RE) pin; -1 = transceiver auto-directions itself */
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
 * @brief Initializes UART2 with the given baud rate, serial config, TX/RX pins
 * and optional RS485 driver-enable pin.
 * @param baudrate Baud rate in bits per second.
 * @param rtu_mode Custom RTU macro (e.g., RTU_8N1, RTU_8E1).
 * @param txPin UART TX pin connected to the RS485 transceiver.
 * @param rxPin UART RX pin connected to the RS485 transceiver.
 * @param enPin RS485 transceiver driver-enable (DE/RE) pin, or -1 if the
 * transceiver auto-directions itself and no pin needs to be toggled.
 * @param recive_callback Callback invoked from ISR context on byte reception.
 */
bool xMBPortUartInit( uint32_t baudrate, ModbusSerialConfig_t rtu_mode, int txPin, int rxPin, int enPin, void (*recive_callback)(void) )
{
    pMBSerial = &Serial2;

    pMBSerial->begin(baudrate, get_arduino_serial_config(rtu_mode), rxPin, txPin);
    if (!(*pMBSerial))
    {
        return false;
    }

    /* Register callback */
    pMBSerial->onReceive(recive_callback);

    enablePin = enPin;
    if (enablePin >= 0)
    {
        pinMode(enablePin, OUTPUT);
        digitalWrite(enablePin, LOW); /* Default to receive mode */
    }

    return true;
}

void vMBPortUartRead (uint8_t *buffer, int *length)
{
    if (pMBSerial == NULL) return;

    *length = pMBSerial->available();
    for (int i = 0; i < *length; i++)
    {
        buffer[i] = (uint8_t)pMBSerial->read();
    }
}

void vMBPortUartWrite (uint8_t *buffer, uint16_t length)
{
    if (pMBSerial == NULL) return;

    if (enablePin >= 0)
    {
        digitalWrite(enablePin, HIGH); /* Switch RS485 transceiver to transmit */
    }

    pMBSerial->write(buffer, length);

    if (enablePin >= 0)
    {
        pMBSerial->flush(); /* Wait for the last byte to physically leave the UART */
        digitalWrite(enablePin, LOW); /* Switch back to receive */
    }
}