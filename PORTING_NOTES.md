# Linux Kernel → Arduino (ESP32-S3) Porting Notes

Target: `esp32-s3-devkitc-1`, `framework = arduino` (see `platformio.ini`).

## Architecture: layers vs. platform

The codebase is split into two axes that are easy to conflate but should stay
separate:

- **Layers** = the protocol logic. Platform-independent, expressed only in
  terms of the `port.h` typedefs and the porting-contract functions declared
  in `Include/mbport.h`/`Include/mbrtu.h`. This is the code that would be
  identical whether it runs on Linux, Arduino, or bare metal.
- **Platform** = how a layer's porting contract is actually implemented for
  one target. This is where `pthread_*`, `esp_timer_*`, `Serial2`,
  `xTaskCreate`, `#if defined(ESP32)`, etc. are allowed to appear.

### The layers

1. **Master / application layer — `modbus.cpp`**
   Owns the LightModbus master instance, the public `Modbus*` API
   (`ModbusInit`/`Start`/`Send`/`Receive`/`Destroy`/`Run`), and the polling
   state machine `eMBMasterPoll()`. Talks to the layer below only through
   `eMBRTU*()` functions and the event mailbox (`xMBPortEvent*()`) — never
   touches timers or UART directly.

2. **RTU link layer — `mbrtu.cpp`**
   Owns framing, CRC16, and the byte-timing receive FSM
   (`xMBRTUReceiveFSM`, `xMBRTUTimerT35Expired`). This is the *only* layer
   that calls the Timer and UART porting functions
   (`xMBPortTimersInit/Start/Cancel`, `xMBPortUartInit/Read/Write`) — the
   master layer never sees them. It also posts into the event mailbox
   (`xMBPortEventPost(EV_FRAME_RECEIVED)`, `EV_FRAME_SENT`, `EV_READY`) to
   notify the master layer that something happened.

3. **Porting contract — `Include/mbport.h`**
   Not an implementation, just the interface every platform must provide,
   split into three independent resources:
   - **Event** (`xMBPortEventInit/Post/Get`, `vMBPortEventDeinit`) — an
     async mailbox/notification primitive, *not* a hardware resource. It's
     the seam between whatever produces events (RTU layer, running in an
     ISR or timer-callback context) and whatever consumes them (the
     master's poll loop, running in task context). Both the RTU layer and
     the master layer legitimately depend on it directly — that's not a
     layering violation, it's the intended producer/consumer contract.
   - **Timers** (`xMBPortTimersInit`, `vMBPortTimersStart/Cancel`) — RTU's
     t1.5/t3.5 silence timing. Only `mbrtu.cpp` depends on this.
   - **UART** (`xMBPortUartInit/Read/Write`) — byte-level serial I/O. Only
     `mbrtu.cpp` depends on this.

### The platform implementations (per-target, swappable)

- `port_event.cpp` — ESP32/Arduino: a FreeRTOS task (`prvModbusWorkerTask`)
  blocked on `ulTaskNotifyTake`-style wake, given from `xMBPortEventPost`
  (ISR-safe via `vTaskNotifyGiveFromISR` when called from interrupt
  context). A Linux platform would implement the same `xMBPortEvent*`
  contract with a pthread mutex/condvar queue instead.
- `port_timer.cpp` — two platform implementations selected by
  `#if defined(ESP32)`: a hardware `esp_timer` one-shot timer for ESP32,
  and a `micros()`-based cooperative-polling fallback
  (`vMBPortTimersPoll()`, meant to be called from `loop()`) for plain
  AVR/Arduino boards without `esp_timer`.
- `port_uart.cpp` — ESP32/Arduino `Serial2` hardware UART.
- `Include/port.h` — cross-cutting platform primitives shared by every
  layer: logging (`PR_INFO`/`PR_ERR`, `Serial.printf` vs. `printf`),
  mutex/condvar macros (`DEFINE_MUTEX`/`LOCK_MUTEX`/`UNLOCK_MUTEX`,
  `COND_WAIT_TIMEOUT`/`COND_SIGNAL` — currently pthread-backed, see "Known
  remaining issues" below for the FreeRTOS port), and
  `ENTER_CRITICAL_SECTION`/`EXIT_CRITICAL_SECTION`.

### Call graph (how a request actually flows through the layers)

```
ModbusSend()                              [master]
  -> send_and_wait_logic()                [master]
       -> xMBPortEventPost(EV_MASTER_SEND_REQUEST)   [platform: event]
       -> COND_WAIT_TIMEOUT(...)                     [platform: port.h]
                                    (worker task wakes)
prvModbusWorkerTask -> ModbusRun() -> eMBMasterPoll() [master]
  case EV_MASTER_SEND_REQUEST:
    -> eMBRTUSend()                        [RTU link]
         -> vMBPortUartWrite()             [platform: uart]

(byte arrives on the wire, UART ISR fires)
Serial2 receive callback -> xMBRTUReceiveFSM()   [RTU link, ISR context]
  -> vMBPortUartRead()                     [platform: uart]
  -> vMBPortTimersStart()                  [platform: timer]

(t3.5 silence timer expires)
esp_timer callback -> xMBRTUTimerT35Expired()    [RTU link]
  -> xMBPortEventPost(EV_FRAME_RECEIVED)   [platform: event]
                                    (worker task wakes)
eMBMasterPoll() case EV_FRAME_RECEIVED:    [master]
  -> eMBRTUReceive()                       [RTU link]
  -> COND_SIGNAL(send_cond)                [platform: port.h]
                                    (wakes send_and_wait_logic's waiter)
```

## Done

### Logging macros (`pr_info` / `pr_err` → `PR_INFO` / `PR_ERR`)

- Declared in `Include/port.h`:
  ```c
  #ifdef ARDUINO
  #include <Arduino.h>
  #define PR_INFO(fmt, ...)   Serial.printf("[INFO] " fmt, ##__VA_ARGS__)
  #define PR_ERR(fmt, ...)    Serial.printf("[ERROR] " fmt, ##__VA_ARGS__)
  #else
  #include <stdio.h>
  #define PR_INFO(fmt, ...)   printf("[INFO] " fmt, ##__VA_ARGS__)
  #define PR_ERR(fmt, ...)    printf("[ERROR] " fmt, ##__VA_ARGS__)
  #endif
  ```
- All call sites in `modbus.c` and `mbrtu.c` updated from `pr_info(...)`/`pr_err(...)`
  to `PR_INFO(...)`/`PR_ERR(...)` (including two `mbrtu.c` calls that had a stray
  space before the parenthesis: `pr_info (...)`).

## Files touched this session

- `Include/port.h` — added `PR_INFO`/`PR_ERR` macros.
- `modbus.c` — `pr_info`/`pr_err` → `PR_INFO`/`PR_ERR`.
- `mbrtu.c` — `pr_info`/`pr_err` → `PR_INFO`/`PR_ERR`.
