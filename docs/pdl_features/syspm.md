# SysPm - System Power Management for Ultra-Low Power Designs

## Overview
The SysPm driver provides a comprehensive API for managing system and CPU power modes on Infineon PSOC/AIROC devices, enabling significant reduction in average power consumption. It supports transitioning between Active, Sleep, DeepSleep, Hibernate, and extended low-power profiles, and provides a callback framework for safe power-mode sequencing.

## Features
- **Multiple system power modes**: LP, ULP, LPACTIVE/LPSLEEP, DeepSleep, DeepSleep-RAM, DeepSleep-OFF, and Hibernate
- **Callback registration framework**: Ordered callbacks that execute before and after each power-mode transition, enabling drivers and application code to safely prepare for low-power entry
- **Per-CPU power control**: Independently query and manage CM0+, CM4, CM33, and CM55 CPU states on multi-core devices
- **Power domain management**: ARM Power Policy Unit (PPU) and Power Dependency Control Matrix (PDCM) for fine-grained domain control on supported devices
- **BTSS subsystem power control**: Host-side API to power-manage the Bluetooth Sub-System independently of the main MCU on supported devices
- **PMIC, backup domain, and regulator management**: LDO and Buck regulator voltage control for LP/ULP transitions on supported devices

## When to Use
- **Battery-powered IoT devices**: Duty-cycle the application into DeepSleep or Hibernate between sensor wakeup events to extend battery life
- **Multi-core coordination**: Use callbacks to park one CPU while the other enters a shared low-power mode
- **Brownout recovery**: Enter ULP mode when Vdd is marginal, then promote to LP when conditions improve
- **Bluetooth/Wi-Fi applications**: Gate the BTSS power rail when RF is not required
- **Safety-critical firmware**: Register fault-tolerant callbacks that veto a power transition if the system is not in a safe state

## Prerequisites

### Hardware Requirements
- Any Infineon PSOC or AIROC device supported by the PDL
- ILO or WCO present for DeepSleep wakeup timer operation
- For Hibernate wakeup: LPCOMP, WDT, GPIO pin, or RTC alarm source configured

### Software Requirements
- PDL version ≥ 5.x (`cy_syspm.h` v5.180)
- `cy_pdl.h` (umbrella include) or `cy_syspm.h` directly

### Configure in the Tool

| Parameter | Description | Typical Value |
|---|---|---|
| `enableCallback` | Register a power-mode callback during BSP init | `true` |
| `callbackType` | Which power mode(s) the callback handles | `CY_SYSPM_DEEPSLEEP` |
| `callbackMode` | When within the sequence to invoke callback | `CY_SYSPM_CHECK_READY \| CY_SYSPM_BEFORE_TRANSITION \| CY_SYSPM_AFTER_TRANSITION` |
| `skipMode` | Modes to skip (for this callback entry) | `0` (none) |

## Quick Start

### Step-by-Step
1. Include `cy_pdl.h` (or `cy_syspm.h`).
2. Define and register a callback (optional but recommended).
3. Call the appropriate `Cy_SysPm_*` transition API (`CpuEnterSleep`, `CpuEnterDeepSleep`, `SystemEnterHibernate`, etc.).
4. On wakeup, check `Cy_SysLib_GetResetReason()` if returning from Hibernate/DeepSleep-RAM/DeepSleep-OFF.

### Sample Code

```c
#include "cy_pdl.h"

/* User callback invoked before/after DeepSleep transitions */
cy_stc_syspm_callback_params_t callbackParams = { NULL, NULL };

cy_en_syspm_status_t MyDeepSleepCallback(
    cy_stc_syspm_callback_params_t *params,
    cy_en_syspm_callback_mode_t     mode)
{
    cy_en_syspm_status_t result = CY_SYSPM_SUCCESS;
    (void)params;

    switch (mode)
    {
        case CY_SYSPM_CHECK_READY:
            /* Verify it is safe to enter DeepSleep */
            break;
        case CY_SYSPM_BEFORE_TRANSITION:
            /* Save any non-retained state here */
            break;
        case CY_SYSPM_AFTER_TRANSITION:
            /* Restore state after wakeup */
            break;
        default:
            result = CY_SYSPM_FAIL;
            break;
    }
    return result;
}

cy_stc_syspm_callback_t deepSleepCb = {
    .callback       = MyDeepSleepCallback,
    .type           = CY_SYSPM_DEEPSLEEP,
    .skipMode       = 0u,
    .callbackParams = &callbackParams,
    .prevItm        = NULL,
    .nextItm        = NULL,
    .order          = 0u
};

int main(void)
{
    __enable_irq();

    /* Register the DeepSleep callback */
    if (!Cy_SysPm_RegisterCallback(&deepSleepCb))
    {
        /* Handle registration error */
    }

    /* Check current system power mode */
    if (Cy_SysPm_IsSystemLp())
    {
        /* System is in LP mode; safe to use full clock speed */
    }

    /* Enter CPU DeepSleep (wakeup via interrupt) */
    if (CY_SYSPM_SUCCESS != Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT))
    {
        /* Handle transition error */
    }

    for (;;) { /* Application loop */ }
}
```

### Expected Outcome
- The registered callback executes CHECK_READY → BEFORE_TRANSITION just before the WFI instruction; the CPU halts until an interrupt fires.
- After wakeup, AFTER_TRANSITION fires and execution continues from the next line.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| `Cy_SysPm_CpuEnterDeepSleep` returns `CY_SYSPM_FAIL` | A registered callback returned `CY_SYSPM_FAIL` during CHECK_READY | Identify which callback vetoed the transition; check peripheral ready states |
| Device resets immediately after DeepSleep entry | WDT not cleared before transition | Feed (clear) the WDT interrupt before calling the DeepSleep API |
| System stuck in Hibernate | No wakeup source configured | Ensure LPCOMP, WDT, or GPIO hibernate wakeup pin is enabled before calling `Cy_SysPm_SystemEnterHibernate()` |
| UDB state lost after DeepSleep | Non-retained UDB registers not saved | Use `Cy_SysPm_SaveRegisters()` / `Cy_SysPm_RestoreRegisters()` (CAT1A only) |
| CM4 / CM33 does not enter DeepSleep | Other CPU still active | On multi-core devices, both CPUs must call DeepSleep; the last CPU actually performs the transition |

## Related Code Examples

- [PSOC™ Edge MCU: Switching Power Modes](https://github.com/Infineon/mtb-example-psoc-edge-switching-power-modes)
- [PSOC™ Edge MCU: Power Measurements](https://github.com/Infineon/mtb-example-psoc-edge-power-measurements)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — Power Modes chapter

## Configuration Parameters Reference

| Parameter / API | Type | Description |
|---|---|---|
| `Cy_SysPm_RegisterCallback()` | Function | Registers a callback into the ordered callback list |
| `Cy_SysPm_UnregisterCallback()` | Function | Removes a previously registered callback |
| `Cy_SysPm_CpuEnterSleep()` | Function | Puts the calling CPU into Sleep mode (WFI/WFE) |
| `Cy_SysPm_CpuEnterDeepSleep()` | Function | Puts the calling CPU (and system, if all CPUs comply) into DeepSleep |
| `Cy_SysPm_SystemEnterHibernate()` | Function | Shuts down regulators; only wakeup sources can resume (via reset) |
| `Cy_SysPm_SystemEnterLp()` | Function | Transitions system to LP active mode (1.1 V nominal, ≤150 MHz) |
| `Cy_SysPm_SystemEnterUlp()` | Function | Transitions system to ULP active mode (0.9 V nominal, ≤50 MHz) |
| `Cy_SysPm_IsSystemLp()` | Function | Returns true when system is in LP mode |
| `Cy_SysPm_IsSystemUlp()` | Function | Returns true when system is in ULP mode |
| `Cy_SysPm_IsSystemHp()` | Function | Returns true when system is in HP mode |
| `Cy_SysPm_SetHibernateWakeupSource()` | Function | Configures the Hibernate wakeup trigger source |
| `cy_stc_syspm_callback_t` | Struct | Callback list node containing function pointer, type, mode mask |
| `cy_en_syspm_callback_mode_t` | Enum | CHECK_READY, CHECK_FAIL, BEFORE_TRANSITION, AFTER_TRANSITION |
| `cy_en_syspm_status_t` | Enum | SUCCESS, FAIL, CANCELED, TIMEOUT |

## Advanced Usage

### DeepSleep-RAM
DeepSleep-RAM retains a configurable SRAM window while powering down the CPU and most peripherals. On wakeup, code executes a reduced reset sequence and boots from the retained SRAM entry point rather than from flash.

```c
/* Enter DeepSleep-RAM; ensure retained SRAM region is configured in linker script */
Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_DEEPSLEEP_RAM);
```

### System LP ↔ HP Transitions
On devices that support HP mode, the system supports HP → LP → ULP transitions with intermediate MF (Medium Frequency) states:

```c
/* Check current state and step down */
if (Cy_SysPm_IsSystemHp())
{
    Cy_SysPm_SystemEnterLp();   /* HP → LP */
}
if (Cy_SysPm_IsSystemLp())
{
    Cy_SysPm_SystemEnterUlp();  /* LP → ULP */
}
```

### BTSS Power Control
On devices with a Bluetooth Sub-System, the `cy_syspm_btss.h` header exposes the host-side API for powering the BTSS:

```c
#include "cy_syspm_btss.h"

/* Power on the BTSS from the MCU host side */
Cy_BTSS_PowerDep(true);
```

## Industry Standards
- ARM Power Control Architecture (LPACTIVE/LPSLEEP, DEEPSLEEP profiles)
- ARM Cortex-M WFI / WFE low-power instructions (AMBA / IHI0042)

## Copyright
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
