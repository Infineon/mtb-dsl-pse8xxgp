# MCWDT - Multi-Counter Watchdog Timer for Flexible Timing and Watchdog Applications

## Overview
The MCWDT driver manages a Multi-Counter Watchdog Timer block that contains two independent 16-bit counters (C0, C1) and one 32-bit counter (C2), all clocked from the low-frequency LFCLK (nominally 32 kHz ILO). Each counter can independently generate periodic interrupts, trigger a watchdog reset, or operate as a free-running timer, and adjacent counters can be cascaded for extended time intervals.

## Features
- **Three independent sub-counters**: C0 and C1 are 16-bit; C2 is a 32-bit toggle-bit counter — each individually configurable for interrupt, reset, or free-running operation
- **Cascade mode**: C0→C1 and C1→C2 cascade options allow construction of a 48-bit or 64-bit effective timer from a single 32 kHz source
- **Configurable clear-on-match**: C0 and C1 support `ClearOnMatch` so the match value directly programs the interrupt period without requiring ISR counter management
- **Multiple watchdog modes per counter**: `NONE` (free-running), `INT` (interrupt only), `RESET` (reset only), or `INT_RESET` (interrupt then reset)
- **Register write protection**: `Cy_MCWDT_Lock()` / `Cy_MCWDT_Unlock()` protect all MCWDT configuration registers from accidental writes
- **Simultaneous multi-counter enable/disable**: Single API call with bitmask (`CY_MCWDT_CTR0 | CY_MCWDT_CTR1`) starts or stops multiple counters in one step

## When to Use
- **Periodic interrupts**: Use C0 or C1 with `ClearOnMatch` and `INT` mode for deterministic tick generation (e.g., 100 ms system tick from 32 kHz)
- **Free-running elapsed time measurement**: Monitor event timestamps using `Cy_MCWDT_GetCount()` on a free-running C2 without consuming a TCPWM resource
- **Watchdog with interrupt warning**: Configure C0 in `INT_RESET` mode so the ISR receives an advance warning before the watchdog resets the device
- **Long-interval timers**: Cascade C0→C1→C2 to achieve periods exceeding 24 hours from the 32 kHz clock
- **Real-time clocking fallback**: Use MCWDT as an always-on interval source when the RTC is not available or PILO is not present

## Prerequisites

### Hardware Requirements
- Device with hardware MCWDT supported by the PDL
- LFCLK must be routed and running (ILO by default); MCWDT requires LFCLK input

### Software Requirements
- PDL version ≥ 1.x (`cy_mcwdt.h` v1.90)
- `cy_pdl.h` (umbrella include) or `cy_mcwdt.h` directly
- `cy_sysint.h` for interrupt configuration

### Configure in the Tool

| Parameter | Description | Typical Value |
|---|---|---|
| `c0Match` | C0 match / clear-on-match value | `327` (10 ms at 32.768 kHz) |
| `c1Match` | C1 match / clear-on-match value | `0` |
| `c2ToggleBit` | Bit position in C2 that toggles to generate interrupt | `15` (≈1 s period) |
| `c0Mode` | Counter 0 operating mode | `CY_MCWDT_MODE_INT` |
| `c1Mode` | Counter 1 operating mode | `CY_MCWDT_MODE_NONE` |
| `c2Mode` | Counter 2 operating mode | `CY_MCWDT_MODE_NONE` |
| `c0ClearOnMatch` | Auto-reset C0 when match fires | `true` |
| `c1ClearOnMatch` | Auto-reset C1 when match fires | `false` |
| `c1c2Cascade` | Enable C1 clocked by C0 overflow | `false` |
| `c2c1Cascade` | Enable C2 clocked by C1 overflow | `false` |

## Quick Start

### Step-by-Step
1. Include `cy_pdl.h`.
2. Configure `cy_stc_mcwdt_config_t` with the desired match values, modes, and cascade settings.
3. Set the interrupt mask before enabling: `Cy_MCWDT_SetInterruptMask()`.
4. Initialize the MCWDT block: `Cy_MCWDT_Init()`.
5. Register the ISR and enable the NVIC interrupt.
6. Enable the desired counters: `Cy_MCWDT_Enable()`.
7. In the ISR, check which counter fired, handle the event, then clear the interrupt with `Cy_MCWDT_ClearInterrupt()`.

### Sample Code

```c
#include "cy_pdl.h"

#define MCWDT_HW           MCWDT_STRUCT0
#define LF_CLK_HZ          (32000UL)
#define INTERVAL_MS        (500UL)
#define MCWDT_MATCH_C0     ((LF_CLK_HZ * INTERVAL_MS) / 1000UL)  /* 16000 counts = 500 ms */
#define MCWDT_STARTUP_DELAY (94U)   /* ~3 LFCLK cycles */

volatile uint32_t g_mcwdtTick = 0U;

/* MCWDT ISR */
void MCWDT_InterruptHandler(void)
{
    uint32_t isrMask = Cy_MCWDT_GetInterruptStatus(MCWDT_HW);

    if ((isrMask & CY_MCWDT_CTR0) != 0U)
    {
        g_mcwdtTick++;
        Cy_MCWDT_ClearInterrupt(MCWDT_HW, CY_MCWDT_CTR0);
    }
}

int main(void)
{
    __enable_irq();

    /* MCWDT C0: 500 ms periodic interrupt with ClearOnMatch */
    cy_stc_mcwdt_config_t mcwdtCfg = {
        .c0Match        = (uint16_t)MCWDT_MATCH_C0,
        .c1Match        = 0U,
        .c0Mode         = CY_MCWDT_MODE_INT,
        .c1Mode         = CY_MCWDT_MODE_NONE,
        .c2ToggleBit    = 16U,          /* C2 free-running; toggle at bit 16 */
        .c2Mode         = CY_MCWDT_MODE_NONE,
        .c0ClearOnMatch = true,
        .c1ClearOnMatch = false,
        .c1c2Cascade    = false,
        .c2c1Cascade    = false
    };

    /* Enable C0 interrupt before init */
    Cy_MCWDT_SetInterruptMask(MCWDT_HW, CY_MCWDT_CTR0);

    /* Initialize and enable counter 0 */
    (void)Cy_MCWDT_Init(MCWDT_HW, &mcwdtCfg);
    Cy_MCWDT_Enable(MCWDT_HW, CY_MCWDT_CTR0, MCWDT_STARTUP_DELAY);

    /* Configure NVIC */
    cy_stc_sysint_t mcwdtIrqCfg = {
        .intrSrc      = (IRQn_Type)srss_interrupt_mcwdt_0_IRQn,
        .intrPriority = 4U
    };
    Cy_SysInt_Init(&mcwdtIrqCfg, MCWDT_InterruptHandler);
    NVIC_EnableIRQ((IRQn_Type)srss_interrupt_mcwdt_0_IRQn);

    for (;;)
    {
        /* Application work; g_mcwdtTick increments every 500 ms */
    }
}
```

### Expected Outcome
- Counter C0 increments at 32 kHz; every 16000 counts (500 ms) an interrupt fires, `g_mcwdtTick` is incremented, and C0 auto-clears back to zero (ClearOnMatch = true).

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| ISR never fires | `Cy_MCWDT_SetInterruptMask()` not called before `Enable` | Call `SetInterruptMask` before or during initialization |
| Counter does not start | LFCLK not running | Ensure ILO/WCO/PILO is enabled and routed to LFCLK before initializing MCWDT |
| Unexpected reset | Counter mode set to `INT_RESET` or `RESET` | Change mode to `INT` if only interrupts are needed; verify match value gives enough time to service the ISR |
| MCWDT register writes silently ignored | MCWDT is locked | Call `Cy_MCWDT_Unlock()` before modifying MCWDT registers |
| Short delay (`MCWDT_Disable` has no effect) | Counter still running due to synchronizer latency | Provide the two-LFCLK-cycle wait parameter to `Cy_MCWDT_Disable()` |
| C2 period unexpected | Toggle bit position miscalculated | C2 interrupt period = 2^(ToggleBit+1) / LFCLK_Hz seconds |

## Related Code Examples

- [PSOC™ Edge MCU: MCWDT](https://github.com/Infineon/mtb-example-psoc-edge-mcwdt)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — MCWDT chapter

## Configuration Parameters Reference

| Parameter / API | Type | Description |
|---|---|---|
| `Cy_MCWDT_Init()` | Function | Applies the `cy_stc_mcwdt_config_t` structure to the MCWDT hardware |
| `Cy_MCWDT_DeInit()` | Function | Resets all MCWDT registers to hardware defaults |
| `Cy_MCWDT_Enable()` | Function | Enables selected counters (bitmask); accepts startup delay in LFCLK cycles |
| `Cy_MCWDT_Disable()` | Function | Disables selected counters; accepts synchronizer wait delay |
| `Cy_MCWDT_GetEnabledStatus()` | Function | Returns true when the specified counter is running |
| `Cy_MCWDT_Lock()` | Function | Write-protects all MCWDT configuration registers |
| `Cy_MCWDT_Unlock()` | Function | Removes write protection |
| `Cy_MCWDT_GetLockedStatus()` | Function | Returns true when MCWDT is locked |
| `Cy_MCWDT_SetMode()` | Function | Sets counter mode: NONE, INT, RESET, or INT_RESET |
| `Cy_MCWDT_GetMode()` | Function | Returns the current counter mode |
| `Cy_MCWDT_SetClearOnMatch()` | Function | Enables auto-reset of C0/C1 on match (C2 not supported) |
| `Cy_MCWDT_GetClearOnMatch()` | Function | Returns ClearOnMatch setting |
| `Cy_MCWDT_SetCascade()` | Function | Configures C0→C1 and/or C1→C2 cascade |
| `Cy_MCWDT_GetCascade()` | Function | Returns the current cascade configuration |
| `Cy_MCWDT_SetMatch()` | Function | Sets match value for C0 or C1; accepts post-write synchronization delay |
| `Cy_MCWDT_GetMatch()` | Function | Returns current match value |
| `Cy_MCWDT_SetToggleBit()` | Function | Sets the C2 toggle bit position (0–31) |
| `Cy_MCWDT_GetToggleBit()` | Function | Returns current C2 toggle bit position |
| `Cy_MCWDT_GetCount()` | Function | Returns current count value for any sub-counter |
| `Cy_MCWDT_ResetCounters()` | Function | Force-resets selected counters; accepts synchronization delay |
| `Cy_MCWDT_GetInterruptStatus()` | Function | Returns pending interrupt bits for all three counters |
| `Cy_MCWDT_ClearInterrupt()` | Function | Clears pending interrupt bits (bitmask) |
| `Cy_MCWDT_SetInterruptMask()` | Function | Enables the interrupt for selected counters |
| `Cy_MCWDT_GetInterruptMask()` | Function | Returns the current interrupt mask |
| `Cy_MCWDT_GetInterruptStatusMasked()` | Function | Returns interrupt status ANDed with mask (use in ISR) |
| `Cy_MCWDT_SetInterrupt()` | Function | Software-triggers an interrupt for testing |
| `cy_stc_mcwdt_config_t` | Struct | Full MCWDT configuration: matches, modes, cascade, clearOnMatch |
| `cy_en_mcwdtctr_t` | Enum | COUNTER0, COUNTER1, COUNTER2 |
| `cy_en_mcwdtmode_t` | Enum | MODE_NONE, MODE_INT, MODE_RESET, MODE_INT_RESET |
| `cy_en_mcwdtcascade_t` | Enum | CASCADE_NONE, CASCADE_C0C1, CASCADE_C1C2, CASCADE_BOTH |

## Advanced Usage

### Cascaded 48-Bit Timer (C0 → C1)
By cascading C0 into C1, C1 increments only when C0 overflows (every 65535 LFCLK cycles ≈ 2 seconds). The combined counter spans ~131,070 seconds (≈36 hours):

```c
cy_stc_mcwdt_config_t cascadeCfg = {
    .c0Match        = 0U,       /* C0 free-running; overflow clocks C1 */
    .c1Match        = 100U,     /* C1 interrupt at 100 * 65535 LFCLK cycles */
    .c0Mode         = CY_MCWDT_MODE_NONE,
    .c1Mode         = CY_MCWDT_MODE_INT,
    .c2ToggleBit    = 0U,
    .c2Mode         = CY_MCWDT_MODE_NONE,
    .c0ClearOnMatch = false,
    .c1ClearOnMatch = true,
    .c1c2Cascade    = false,
    .c2c1Cascade    = false     /* Set c0c1Cascade below via SetCascade */
};
(void)Cy_MCWDT_Init(MCWDT_HW, &cascadeCfg);
Cy_MCWDT_SetCascade(MCWDT_HW, CY_MCWDT_CASCADE_C0C1);
Cy_MCWDT_Enable(MCWDT_HW, CY_MCWDT_CTR0 | CY_MCWDT_CTR1, MCWDT_STARTUP_DELAY);
```

### Watchdog Reset with Interrupt Warning (INT_RESET Mode)

```c
/* C0 in INT_RESET mode: interrupt fires first, reset fires on next match */
Cy_MCWDT_Unlock(MCWDT_HW);
Cy_MCWDT_SetMode(MCWDT_HW, CY_MCWDT_COUNTER0, CY_MCWDT_MODE_INT_RESET);
Cy_MCWDT_SetMatch(MCWDT_HW, CY_MCWDT_COUNTER0, WDT_MATCH, 0U);
Cy_MCWDT_Lock(MCWDT_HW);
/* Main loop must call Cy_MCWDT_ClearInterrupt() to prevent the reset */
```

The `MCWDT_STRUCT_Type` macro resolves to the appropriate hardware type for the target device, maintaining source-level compatibility across MCWDT hardware variants.

## Industry Standards
- IEC 60730 Class B — Multi-counter watchdog as a CPU program flow monitoring mechanism
- ISO 26262 — Watchdog timer as a hardware safety mechanism (multi-channel monitoring via C0 + C1 cascade)

## Copyright
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
