# WDT - Watchdog Timer for Reliable CPU Fault Recovery

## Overview
The WDT driver manages the 16-bit free-running ILO-clocked Watchdog Timer, which can reset the device when firmware fails to service it within the configured timeout period. It also supports generating periodic interrupts and operates across all power modes including Hibernate.

## Features
- **CPU and firmware crash recovery**: Automatically resets the device when the watchdog is not cleared within the match window
- **Configurable match value**: 16-bit match register with adjustable ignore bits to create effective counter widths from 1 to 16 bits
- **Interrupt before reset**: Generates an interrupt on the first counter match, allowing the firmware to respond or log a fault before the reset occurs on the third match
- **All power modes coverage**: Operates in Active, Sleep, DeepSleep, and Hibernate; in Hibernate, reset fires on the first match
- **Register write protection**: `Cy_WDT_Lock()` / `Cy_WDT_Unlock()` prevent accidental corruption of WDT, ILO, and clock-select registers
- **Reset cause detection**: Works with `Cy_SysLib_GetResetReason()` to distinguish a WDT reset (WRES) from other reset sources at startup

## When to Use
- **Safety watchdog**: Any application where a stuck firmware loop must be detected and recovered automatically
- **Brownout protection**: Enable the WDT to guarantee reset if the CPU hangs during low-voltage startup transients
- **Hibernate wakeup**: Configure the WDT as a periodic wakeup source from Hibernate mode for ultra-low-power duty cycling
- **ILO-driven periodic events**: Generate a ~1 second interval interrupt from a 32 kHz ILO source when MCWDTs are not available

## Prerequisites

### Hardware Requirements
- Any device with a hardware watchdog timer supported by the PDL
- ILO must be enabled before starting the WDT (ILO is always-on by default on most devices)

### Software Requirements
- PDL version ≥ 1.x (`cy_wdt.h` v1.90)
- `cy_pdl.h` (umbrella include) or `cy_wdt.h` directly
- `cy_sysint.h` when using WDT interrupt mode

### Configure in the Tool

| Parameter | Description | Typical Value |
|---|---|---|
| Match value | Counter value that triggers interrupt/reset | `32000` (~1 s at 32 kHz ILO) |
| Ignore bits | Number of MSBs to ignore (reduces effective period) | `0` (full 16-bit counter) |
| Interrupt enable | Route WDT interrupt to CPU (unmask) | `true` for interrupt mode |
| Lock after config | Lock WDT registers after initialization | Recommended in production |

## Quick Start

### Step-by-Step
1. Include `cy_pdl.h`.
2. Ensure ILO is enabled (default on most devices; enable with `Cy_SysClk_IloEnable()` if not).
3. Unlock WDT registers with `Cy_WDT_Unlock()`.
4. Disable WDT while configuring: `Cy_WDT_Disable()`.
5. Set match value with `Cy_WDT_SetMatch()`.
6. Optionally configure ignore bits with `Cy_WDT_SetIgnoreBits()`.
7. Unmask the interrupt with `Cy_WDT_UnmaskInterrupt()` to avoid accidental reset during early firmware.
8. Enable WDT with `Cy_WDT_Enable()`.
9. Lock registers with `Cy_WDT_Lock()`.
10. In the main loop (not the ISR), periodically clear the interrupt with `Cy_WDT_ClearInterrupt()`.

### Sample Code

```c
#include "cy_pdl.h"

/* WDT match value: ~1 second at nominal 32 kHz ILO */
#define WDT_MATCH_COUNT     (32000U)

/* WDT ISR — used only as a timer tick; clearing is done in the main loop */
void WDT_InterruptHandler(void)
{
    /* Update match for next interval */
    uint32_t nextMatch = Cy_WDT_GetMatch() + WDT_MATCH_COUNT;
    Cy_WDT_Unlock();
    Cy_WDT_SetMatch((uint16_t)(nextMatch & 0xFFFFU));
    Cy_WDT_Lock();
}

int main(void)
{
    __enable_irq();

    /* ILO is enabled by default; configure WDT */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();
    Cy_WDT_SetMatch(WDT_MATCH_COUNT);
    Cy_WDT_SetIgnoreBits(0U);       /* Use full 16-bit counter */
    Cy_WDT_UnmaskInterrupt();       /* Allow interrupt, prevent premature reset */
    Cy_WDT_Enable();
    Cy_WDT_Lock();

    /* Configure WDT interrupt (device-specific IRQn) */
    cy_stc_sysint_t wdtIrqCfg = {
        .intrSrc      = (IRQn_Type)srss_interrupt_wdt_IRQn,
        .intrPriority = 4U
    };
    Cy_SysInt_Init(&wdtIrqCfg, WDT_InterruptHandler);
    NVIC_EnableIRQ((IRQn_Type)srss_interrupt_wdt_IRQn);

    for (;;)
    {
        /* === Feed the watchdog in the main loop === */
        Cy_WDT_ClearInterrupt();    /* Must clear before 3rd match to avoid reset */

        /* Application work here */
    }
}
```

### Expected Outcome
- The WDT interrupt fires approximately every 1 second; the main loop clears the interrupt before the third match. If the main loop hangs, the device resets within ~3 seconds.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| Unexpected device reset after ~3 WDT periods | `Cy_WDT_ClearInterrupt()` not called in main loop | Move the clear call out of the ISR and into the main application body |
| WDT register writes ignored | WDT is locked | Call `Cy_WDT_Unlock()` before any WDT register modification |
| WDT fires faster than expected | ILO frequency varies ±30% | Add margin to the match value; consider periodic ILO calibration via SysClk driver |
| Infinite reset loop after WDT reset | Startup code does not feed the WDT | Check `Cy_SysLib_GetResetReason()` at startup; increase match value or add early `ClearInterrupt` call |
| WDT locked after DeepSleep wakeup | Hardware re-locks WDT on wakeup | Re-call `Cy_WDT_Unlock()` after each DeepSleep wakeup before modifying match |

## Related Code Examples

- [PSOC™ Edge MCU: WDT](https://github.com/Infineon/mtb-example-psoc-edge-wdt)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — WDT chapter

## Configuration Parameters Reference

| Parameter / API | Type | Description |
|---|---|---|
| `Cy_WDT_Init()` | Function | Sets the WDT to its reset-default state (match = 0, ignore = 0) |
| `Cy_WDT_Enable()` | Function | Enables the WDT counter |
| `Cy_WDT_Disable()` | Function | Disables the WDT counter (must be unlocked) |
| `Cy_WDT_IsEnabled()` | Function | Returns true when WDT is active |
| `Cy_WDT_Lock()` | Function | Write-protects WDT, ILO, and LFCLK select registers |
| `Cy_WDT_Unlock()` | Function | Removes write protection |
| `Cy_WDT_SetMatch()` | Function | Sets the 16-bit counter match value |
| `Cy_WDT_GetMatch()` | Function | Returns the current match value |
| `Cy_WDT_GetCount()` | Function | Returns the current counter value |
| `Cy_WDT_SetIgnoreBits()` | Function | Specifies how many MSBs to mask out of the 16-bit counter |
| `Cy_WDT_GetIgnoreBits()` | Function | Returns the current ignore-bits setting |
| `Cy_WDT_ClearInterrupt()` | Function | Clears the unhandled interrupt count; "feeds" the watchdog |
| `Cy_WDT_MaskInterrupt()` | Function | Masks the WDT interrupt at the interrupt controller level |
| `Cy_WDT_UnmaskInterrupt()` | Function | Unmasks the WDT interrupt so it reaches the CPU |

## Advanced Usage

### WDT as Hibernate Wakeup Source
On devices where the WDT can wake from Hibernate, configure it as a hibernate wakeup source via SysPm:

```c
/* Allow WDT to fire in Hibernate */
Cy_SysClk_IloHibernateOn();

/* Configure WDT for next hibernate period */
Cy_WDT_Unlock();
Cy_WDT_SetMatch(WDT_MATCH_COUNT);
Cy_WDT_Enable();
Cy_WDT_Lock();

/* Set WDT as Hibernate wakeup and enter Hibernate */
Cy_SysPm_SetHibernateWakeupSource(CY_SYSPM_HIBERNATE_WDT);
Cy_SysPm_SystemEnterHibernate();
/* Execution resumes from reset vector after WDT fires */
```

### WDT Reset Detection at Startup

```c
/* Early in main(), check for WDT-caused reset */
uint32_t resetReason = Cy_SysLib_GetResetReason();
if (resetReason & CY_SYSLIB_RESET_HWWDT)
{
    /* WDT reset occurred — log, recover, or escalate */
}
Cy_SysLib_ClearResetReason();
```

### Multi-Instance WDT
On devices with more than one WDT instance (`SRSS_NUM_WDT_A > 1`), each WDT instance (`WDT_STRUCT0`, `WDT_STRUCT1`) is addressed as a hardware structure pointer:

```c
/* Use second WDT instance */
Cy_WDT_Unlock_Ex(WDT_STRUCT1);
Cy_WDT_SetMatch_Ex(WDT_STRUCT1, WDT_MATCH_COUNT);
Cy_WDT_Enable_Ex(WDT_STRUCT1);
Cy_WDT_Lock_Ex(WDT_STRUCT1);
```

On single-instance devices the `_Ex` suffix is not required; the standard `Cy_WDT_*` APIs address the default hardware instance.

## Industry Standards
- IEC 60730 Class B — Watchdog timer as a required CPU monitoring element
- ISO 26262 — Hardware watchdog as a safety mechanism for CPU monitoring

## Copyright
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
