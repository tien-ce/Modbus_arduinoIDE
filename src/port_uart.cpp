#include <Arduino.h>
#include <port.h>
#include <mbport.h>

#define MODBUS_TX   1
#define MODBUS_RX   2

bool xMBPortUartInit( uint32_t baudrate, bool (*recive_callback)(void) )
{
    if (Serial2)
    {
        return false;
    }

    Serial2.begin(baudrate, SERIAL_8N1, RX, TX);
    if (!Serial2)
    {
        return false;
    }
    /* Register call back */
    Serial2.onReceive(recive_callback);
    return true;
}

void vMBPortUartRead (uint8_t *buffer, int *length)
{
    *length = Serial2.available();
    for (int i = 0; i < *length; i++)
    {
        buffer[i] = (uint8_t)Serial2.read();
    }
}

void vMBPortUartWrite (uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        Serial2.write(buffer[i]);
    }
}
