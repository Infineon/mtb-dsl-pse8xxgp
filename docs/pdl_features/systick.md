# SysTick Driver (PDL) - ARM System Timer for Periodic Interrupts

# Overview

The **SysTick driver** provides a vendor-specific API for the ARM Cortex-M SysTick 24-bit down-counter, enabling precise periodic interrupt generation with a selectable clock source (CPU clock, IMO, ECO, LFCLK, or SRSS timer). It extends the standard CMSIS SysTick interface with a multi-callback registration mechanism (up to 5 simultaneous callbacks) and flexible clock-source switching, making it ideal for general-purpose periodic task scheduling, software timekeeping, and OS tick generation when no dedicated hardware timer is available.

# Features

- **24-bit down-counter** with automatic reload, clocked from CPU clock, IMO, ECO, LFCLK, or SRSS clk_timer
- **Periodic interrupt generation** with hardware-accurate reload interval (no CPU polling required)
- **5 simultaneous callback slots** — register multiple periodic tasks on a single SysTick interrupt using `Cy_SysTick_SetCallback()`
- **Flexible clock source switching** at runtime — change source without reinitializing; call `Cy_SysTick_SetReload()` after switching to compensate for the new frequency
- **Enable/disable granularity** — independent control of the SysTick counter and its interrupt
- **TrustZone support** — separate Non-Secure SysTick enable/disable APIs on CM33 devices with Security Extension (`Cy_NsSysTick_Enable()` / `Cy_NsSysTick_Disable()`)

# When to Use

- Implement a periodic task scheduler tick (e.g., 1 ms OS tick) without allocating a TCPWM counter
- Generate a software heartbeat timer for LED blink or watchdog-kick scheduling
- Measure elapsed time in embedded firmware when a hardware profiling timer is unavailable
- Provide a coarse real-time tick during Deep-Sleep phases using LFCLK
- Multiplex up to 5 independent periodic callbacks on a single hardware interrupt

# Prerequisites

## Hardware Requirements

- Any Infineon device with `M4CPUSS`, `M33SYSCPUSS`, `M7CPUSS`, or `M55APPCPUSS` IP
- The selected clock source for SysTick must be enabled before calling `Cy_SysTick_Init()` (e.g., IMO is on by default; ECO requires separate ECO initialization)
- The SysTick counter is 24 bits wide — maximum reload value is `0xFFFFFF` (16,777,215 ticks)

## Software Requirements

- PDL available via `#include "cy_pdl.h"`
- Include `<stddef.h>` if callback functions are used (for `NULL`)

## Configure in the Tool

SysTick does not have a Device Configurator personality. Use `Cy_SysTick_Init()` directly in application code. Calculate the `interval` (reload value) as:

```
interval = (desired_period_us / 1,000,000.0) * clock_frequency_Hz
```

For example, for 1 ms interval at 8 MHz IMO: `interval = 0.001 × 8,000,000 = 8000`.

# Quick Start

This quick start demonstrates a 1 ms SysTick interrupt using the CPU clock, with a callback that increments a millisecond counter.

**Step 1:** Include `cy_pdl.h`.

**Step 2:** Ensure the CPU clock is running at a known frequency (e.g., after `cybsp_init()`).

**Step 3:** Define a callback function.

**Step 4:** Call `Cy_SysTick_Init()` with the calculated interval and register the callback.

**Expected Outcome:** The callback fires every 1 ms; a global counter increments at 1 kHz.

## Sample Code

### Bare Metal Example — 1 ms Periodic Callback (main.c)

```c
#include "cy_pdl.h"
#include <stddef.h>

/* Global millisecond counter updated in the SysTick callback */
static volatile uint32_t g_ms_counter = 0U;

/* SysTick callback — called every SysTick interrupt */
static void systick_ms_callback(void)
{
    g_ms_counter++;
}

int main(void)
{
    __enable_irq();

    /* Configure SysTick for 1 ms interval using CPU clock.
     * interval = (1ms / 1s) * CPU_FREQ_HZ
     * Example: 100 MHz CPU -> interval = 100000
     * Adjust to match your system clock frequency.             */
    uint32_t interval = SystemCoreClock / 1000U; /* 1 ms */

    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU, interval);

    /* Register the callback in slot 0 (slots 0–4 available) */
    Cy_SysTick_SetCallback(0U, systick_ms_callback);

    for (;;)
    {
        /* Read the counter — add a critical section if 32-bit read
           is not atomic on your target core */
        uint32_t now = g_ms_counter;

        /* Example: blink LED every 500 ms */
        if ((now % 500U) == 0U)
        {
            /* Toggle LED */
            Cy_GPIO_Inv(LED_RED_PORT, LED_RED_PIN);
        }
    }
}
```

### Multiple Callbacks Example

```c
#include "cy_pdl.h"

static void task_1ms(void)  { /* Fast 1 ms task */ }
static void task_5ms(void)
{
    static uint8_t cnt = 0U;
    if (++cnt >= 5U) { cnt = 0U; /* 5 ms task */ }
}

int main(void)
{
    __enable_irq();

    uint32_t interval = SystemCoreClock / 1000U;
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU, interval);

    /* Register two callbacks in different slots */
    Cy_SysTick_SetCallback(0U, task_1ms);
    Cy_SysTick_SetCallback(1U, task_5ms);

    for (;;) {}
}
```

### Switching Clock Source at Runtime

```c
#include "cy_pdl.h"

void switch_systick_to_lfclk(void)
{
    /* Disable while reconfiguring */
    Cy_SysTick_Disable();

    /* Switch to LFCLK (32.768 kHz) */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_LF);

    /* Recalculate reload for 100 ms interval at 32768 Hz */
    uint32_t new_interval = (uint32_t)((100.0 / 1000.0) * 32768U);
    Cy_SysTick_SetReload(new_interval);
    Cy_SysTick_Clear();

    Cy_SysTick_Enable();
}
```

## Expected Outcome

- The callback executes every 1 ms (±1 CPU clock cycle).
- `g_ms_counter` increments at exactly 1 kHz (verified with a logic analyzer on a toggled GPIO inside the callback).
- Callbacks registered in all 5 slots execute sequentially within each SysTick ISR.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| SysTick never fires | Selected clock source not running | Enable the clock source before calling `Cy_SysTick_Init()`; IMO is always on, others need initialization |
| SysTick fires at wrong rate | Wrong `interval` calculation | `interval = desired_period_s × clock_Hz`; verify `SystemCoreClock` is up-to-date after clock init |
| Rate changes after `SetClockSource()` | Reload value not updated for new clock frequency | Always call `Cy_SysTick_SetReload()` after changing clock source |
| Callback not called | `Cy_SysTick_SetCallback()` not called, or slot out of range | Valid slot numbers are `0` to `CY_SYS_SYST_NUM_OF_CALLBACKS - 1` (0–4) |
| Overflow / reload value > 24 bits | `interval > 0xFFFFFF` | Use a lower-frequency clock source or divide the task into multiple ticks |
| Callbacks disturb timing | Callbacks take too long | Keep all 5 callback functions short; offload heavy work to main loop via flags |

# Related Code Examples

- [PSOC™ Edge MCU: Empty App (FreeRTOS)](https://github.com/Infineon/mtb-example-psoc-edge-empty-app)
- [PSOC™ Edge MCU: Hello World](https://github.com/Infineon/mtb-example-psoc-edge-hello-world)

# Related Application Notes

- Refer to the ARM Cortex-M SysTick chapter in the relevant device Technical Reference Manual (TRM).

# Configuration Parameters Reference

SysTick is configured via direct API calls (no Device Configurator personality):

| Parameter | API | Description | Valid Range |
|-----------|-----|-------------|-------------|
| Clock Source | `Cy_SysTick_SetClockSource()` | Selects the counter clock | `CY_SYSTICK_CLOCK_SOURCE_CLK_LF`, `CLK_IMO`, `CLK_ECO`, `CLK_TIMER`, `CLK_CPU` |
| Reload Value (interval) | `Cy_SysTick_SetReload()` | Down-counter reload value | 1 – 0xFFFFFF (24-bit) |
| Callback Slot | `Cy_SysTick_SetCallback(n, fn)` | Register/replace callback | Slot 0–4; `CY_SYS_SYST_NUM_OF_CALLBACKS = 5` |

Clock source descriptions:

| `cy_en_systick_clock_source_t` | Clock | Typical Frequency | Power Mode |
|-------------------------------|-------|-------------------|------------|
| `CY_SYSTICK_CLOCK_SOURCE_CLK_LF` | clk_lf | 32.768 kHz | All modes |
| `CY_SYSTICK_CLOCK_SOURCE_CLK_IMO` | Internal main oscillator | 8 MHz | Active/Sleep |
| `CY_SYSTICK_CLOCK_SOURCE_CLK_ECO` | External crystal | Varies | Active/Sleep |
| `CY_SYSTICK_CLOCK_SOURCE_CLK_TIMER` | SRSS clk_timer | Varies | Active/Sleep |
| `CY_SYSTICK_CLOCK_SOURCE_CLK_CPU` | CPU clock | Varies (device-dependent) | Active/Sleep |

# Advanced Usage and Examples

- **RTOS tick source**: many RTOSes (FreeRTOS, ThreadX) override the SysTick handler; do not register PDL callbacks when using RTOS-managed SysTick — use the RTOS timer API instead.
- **Low-power periodic wake**: use `CY_SYSTICK_CLOCK_SOURCE_CLK_LF` with LFCLK enabled in Deep-Sleep to tick at 32.768 kHz while the CPU is in Sleep mode (not Deep-Sleep — SysTick does not wake from Deep-Sleep).
- **Removing a callback**: call `Cy_SysTick_SetCallback(slot, NULL)` to unregister a slot; the previous function pointer is returned for reference.
- **Counting overflow flag**: `Cy_SysTick_GetCountFlag()` returns 1 if the counter has wrapped since the last read — use for non-interrupt polling of elapsed time.
- **TrustZone (CM33)**: on devices with Security Extension and `CY_PDL_TZ_ENABLED`, use `Cy_NsSysTick_Enable()` / `Cy_NsSysTick_Disable()` to control the Non-Secure SysTick from secure code.

# Industry Standards and Compliance

The SysTick peripheral is part of the ARM Cortex-M architecture (ARM DDI0403E — ARMv7-M Architecture Reference Manual) and the CMSIS-Core specification. The PDL driver is a vendor extension to the standard CMSIS SysTick interface.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
