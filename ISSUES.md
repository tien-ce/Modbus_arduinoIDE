# Known Issues — modbus_controller

Tracking list from the full read-through on 2026-07-28. Check items off as they're
resolved. Ordered so that fixing top-to-bottom keeps the code buildable at each step.

**2026-07-29: library restructured** into a proper PlatformIO layout —
`lib/src/modbus_controller` → `lib/modbus_controller`, split into `include/`
(public: `modbus.h` only) and `src/` (private: everything else, headers
flattened out of the old `Include/` subfolder). Older entries below still say
`Include/foo.h`/`port_uart.c`-style paths from when they were written — read
those as `src/foo.h` now; not rewriting history, just noting the path shift.

## 0. Licensing conflict (found 2026-07-29, not resolved)

- [ ] **`src/lightmodbus/` (vendored liblightmodbus) is GPLv3, not MIT.**
      `src/lightmodbus/lightmodbus.h`'s own header says
      "Copyright (C) 2021 Jacek Wieczorek ... GNU General Public License ...
      version 3", but every other file in this library carries an MIT
      header (`modbus.cpp`, `port_uart.cpp`, `port_timer.cpp`, the new
      `LICENSE`/`library.json`). Statically linking GPLv3 code into an
      MIT-licensed library and distributing the result is a real license
      conflict, not a style nit — the combined work is bound by GPLv3 terms
      whether or not the wrapper claims MIT. Needs an actual decision:
      relicense this library as GPLv3, replace liblightmodbus with something
      permissively licensed, or get separate permission from its author.
      Flagged in the new `LICENSE` file; not fixed there since it's a
      licensing/product decision, not a code change.

## 1. Won't compile at all

- [x] **`Include/port.h:41`** — `DEFINE_MUTEX` macro space fixed: now
      `#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER`. ✅

- [x] **`port_uart.c`, `port_event.c`, `port_timer.c`** renamed to `.cpp`. ✅

- [x] **`port_uart.cpp`**: `#include <Include/mbport>` → fixed to `<Include/mbport.h>`. ✅

- [x] **`port_uart.cpp`**: `vMBPortUartWrite` — `word` bug fixed (now `buffer[i]`),
      and both uart functions switched to pointer-style params instead of C++
      references (references are gone). ✅ (see 1b for what this switch left broken)

- [ ] ~~`port_uart.c`: vMBPortUartRead reference/dereference mismatch~~ — superseded,
      see 1b below (signature changed to pointer style, but body/callers not
      fully updated to match).

- [ ] ~~Header/impl signature mismatches~~ — partially addressed, see 1b below for
      what's still inconsistent.

### 1b. Regressions / leftovers from the compiler-fix pass (checked 2026-07-28)

- [x] `Include/mbport.h:103` unbalanced parens — fixed, now
      `BOOL xMBPortUartInit( int baudrate, BOOL (*receive_callback)(void));`,
      and the callback type was changed to `BOOL (*)(void)`, which now matches
      `xMBRTUReceiveFSM`'s actual return type. ✅
- [x] `Include/mbport.h:124` conflicting `xMBRTUReceiveFSM` declaration — removed. ✅
- [x] `port_uart.cpp:28` (`vMBPortUartRead`) loop — now correctly uses `*lenght`. ✅
- [x] `mbrtu.cpp:109` — bogus `(void(*)(int))` cast removed; `xMBRTUReceiveFSM` is
      passed directly now, matching the header's `BOOL (*)(void)` callback type. ✅
- [x] `vMBPortUartWrite` header/impl pointer-vs-value mismatch — both now use
      plain `int` for the length param. ✅

### 1c. Fixed in round 3 (checked 2026-07-28, 2nd pass)

- [x] `port_uart.cpp:37` (`vMBPortUartWrite`) — `lenght`/`length` mismatch fixed. ✅
- [x] `mbrtu.cpp:109` — now uses its own `ulBaudRate` parameter instead of the
      out-of-scope `baudrate` global. ✅ (also resolves the section-2 duplicate below)
- [x] `xMBPortUartInit` parameter type mismatch — resolved by the type-convention
      split in round 4 (see 1d): header uses `ULONG`, impl uses `uint32_t` —
      identical underlying type, so they match. ✅

### 1d. Type-convention pass (2026-07-28, round 4)

Per request: the portable core (`Include/mbport.h`, `modbus.cpp`, `mbrtu.cpp`)
now consistently uses the `port.h` typedefs (`BOOL`, `UCHAR`, `USHORT`, `ULONG`,
`INT`, `CHAR`, `UINT8`) — never bare `bool`/`uint8_t`/`uint16_t`/`uint32_t`/`int`/
`char`. The hardware porting layer (`port_uart.cpp`, `port_event.cpp`,
`port_timer.cpp`) now consistently uses native types instead, since those files
talk directly to Arduino/FreeRTOS/ESP-IDF APIs. Both sides still match exactly
at the `mbport.h` boundary, since the typedefs are plain aliases of the native
types.

- [x] `vMBPortUartRead`/`vMBPortUartWrite` — were declared with `char *buffer` in
      the header while `mbrtu.cpp` passes `UCHAR*` buffers (an invalid pointer
      conversion in C++). Header now declares `UCHAR *buffer`; `port_uart.cpp`
      implements with `uint8_t *buffer` (identical type). ✅
- [x] **New: `port_uart.cpp` called `ModbusInit(...)` with no declaration in
      scope.** In C this only warned (implicit declaration); as `.cpp` it's a
      hard "not declared in this scope" error. Added
      `extern bool ModbusInit(uint32_t baud);` at the top of `port_uart.cpp` as
      a stopgap — the layering issue itself (uart layer calling back into the
      master) is still tracked separately below. ✅
- [x] **New: `SendRetType`/`ESEND_NOERR`/`ESEND_RQINVAL`/`ESEND_TIMEOUT`/
      `ESEND_RPINVAL` were used in `modbus.cpp` but never defined anywhere.**
      Added a `SendRetType` enum, and changed `ModbusSend`'s local `ret_val`
      from `int` to `SendRetType` to match the function's own return type. ✅
- [x] `modbus.cpp`, `mbrtu.cpp` — swept remaining bare `int`/`char`/`unsigned char`/
      `uint8_t`/`uint16_t` locals and signatures (`ModbusInit`, `ModbusStart`,
      `ModbusSend`, `ModbusReceive`, `buildreq`, `send_and_wait_logic`,
      `eMBMasterPoll` locals, loop counters) over to `BOOL`/`UCHAR`/`USHORT`/
      `ULONG`/`INT`/`CHAR`. ✅

- [ ] **New, not yet fixed: `mbrtu.cpp:137` (`eMBRTUStop`) calls `timer_remove();`**
      — this function doesn't exist anywhere in the codebase (not in
      `port_timer.cpp`, not an Arduino/FreeRTOS/ESP-IDF API). Looks like a
      leftover from the original Linux-kernel timer API that was never ported.
      Almost certainly should be `vMBPortTimersCancel();`. Whether this actually
      breaks the *build* depends on the toolchain's dead-code elimination (it
      will surface as "undefined reference to `timer_remove`" once anything
      calls `ModbusDestroy()`/`eMBRTUStop()` and isn't stripped) — fix it either
      way since it's clearly wrong.

## 2. Logic bugs / remaining compile blocker

- [x] **`modbus.cpp` `send_and_wait_logic()` condition-variable path (fixed
      2026-07-29)** — rewritten
      using `DEFINE_COND`/`LOCK_MUTEX`/`UNLOCK_MUTEX` plus new
      `COND_WAIT_TIMEOUT`/`COND_SIGNAL` macros (`Include/port.h`), which wrap
      `pthread_cond_timedwait`/`pthread_cond_signal` and the absolute-timeout
      `clock_gettime`/`struct timespec` math (added `<time.h>`/`<errno.h>` to
      `port.h`). `modbus.cpp` no longer calls pthread/time APIs directly, so a
      future FreeRTOS port (binary semaphore instead of pthread cond) only
      touches `port.h`. Also fixed: the loop referenced `g_master_state`
      (undeclared — real variable is `master_state`), and the **inverted return
      value** (`return (rc == ETIMEDOUT) ? MB_ENOERR : MB_ETIMEDOUT;` had
      success/timeout swapped, so a real timeout was swallowed and treated as
      success, parsing stale `pucMBFrame` data). Now
      `return timed_out ? MB_ETIMEDOUT : MB_ENOERR;`. ✅
      Still not FreeRTOS — the macros wrap pthread, tracked in section 4 below.

- [x] ~~`mbrtu.c:109` uses global `baudrate` instead of `ulBaudRate`~~ — fixed,
      see 1c. ✅

- [ ] **`Include/mbport.h:74`** — corrupted enum value `MB_PEV_FRAME_RECEIVEDAR_ODD`
      (looks like a mangled `MB_PAR_ODD` from a bad find/replace).

- [ ] **`port_uart.cpp`** calls `ModbusInit(baud)` from inside the UART init layer —
      layering inversion; re-initializes the master a second time (once by the app
      explicitly, once implicitly here). Should be removed from this layer (the
      `extern bool ModbusInit(...)` forward declaration added in round 4 is a
      stopgap to unblock compilation, not a fix for this).

- [ ] **New, found during architecture review (2026-07-29):
      `port_event.cpp`'s `prvModbusWorkerTask` calls `ModbusRun()` with no
      declaration in scope at all** — not even a stopgap `extern` like
      `port_uart.cpp` has for `ModbusInit()`. In C++ this is a hard
      "not declared in this scope" compile error, not a warning. Same class
      of bug as the `ModbusInit()` issue above, and same root cause below.

- [ ] **New, found during architecture review (2026-07-29): there is no
      public header for the master layer's own API** (`ModbusInit`,
      `ModbusStart`, `ModbusSend`, `ModbusReceive`, `ModbusRun`,
      `ModbusDestroy` — all defined in `modbus.cpp`, none declared in
      `Include/`, unlike the RTU layer's `mbrtu.h` or the porting contract's
      `mbport.h`). Every platform file that needs to call up into the master
      (`port_uart.cpp`, `port_event.cpp`) has to invent its own ad hoc
      `extern` declaration — `port_uart.cpp` did, `port_event.cpp` didn't,
      hence the compile error above. A `modbus.h` declaring the public
      `Modbus*` API would fix both the missing declaration and prevent the
      next platform file from making the same mistake.

- [x] ~~`mbrtu.cpp:137` — `timer_remove()` doesn't exist~~ — user removed the
      call from `eMBRTUStop()` directly instead. ✅

- [x] **Found via user question, fixed: `port_timer.cpp` `vMBPortTimersStart()`
      (ESP32 branch) called `esp_timer_start_once()` without stopping a
      still-running timer first.** ESP-IDF's `esp_timer_start_once()` returns
      `ESP_ERR_INVALID_STATE` (and does nothing) if the timer is already
      running — it does not implicitly restart. Since `vMBPortTimersStart()` is
      called on every received byte to reset the t3.5 silence timer, and the
      previous timer is almost always still running when the next byte
      arrives, the restart was silently failing after the first byte of each
      frame — t3.5 would fire based on the first byte's timing instead of the
      latest one, breaking frame-boundary detection. Fixed by calling
      `esp_timer_stop()` before `esp_timer_start_once()` each time. ✅

## 3. Declared but never defined (link-time failures if referenced)

- [x] `xMBRTUReceiveTrigger`, `pxMBFrameCBByteReceived`, `pxMBFrameCBTransmitterEmpty`,
      `pxMBPortCBTimerExpired`, `vMBPortTimersDelay` — leftover FreeModbus
      ASCII/TCP-support declarations, unused/unassigned in this RTU-only port.
      Removed from `Include/mbport.h`/`Include/mbrtu.h` (2026-07-29). ✅

- [ ] `xMBPortTimersClose` (`Include/mbport.h:91`) — kept intentionally (thought
      likely useful later), but still has no implementation in
      `port_timer.cpp`. Dead API surface until it's implemented; will be an
      "undefined reference" if anything ever calls it.

## 4. Already tracked in `PORTING_NOTES.md`

- [ ] `port.h` still `#include <pthread.h>` and mutex macros aren't ported to
      FreeRTOS primitives (`SemaphoreHandle_t` / `xSemaphoreCreateMutex` / etc).
- [ ] `ENTER_CRITICAL_SECTION()` / `EXIT_CRITICAL_SECTION()` are no-ops — RTU
      timing-sensitive paths are currently unprotected.

## Not yet deeply reviewed

- `light_modbus/` vendor library (lightmodbus) — assumed correct/untouched, not
  audited in this pass.
