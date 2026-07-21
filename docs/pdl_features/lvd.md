# LVD - Low-Voltage Detector for Supply Monitoring and Interrupt/Fault Generation

Monitors the VDDD supply rail against a software-configurable threshold and triggers an interrupt or fault response when the observed voltage crosses the set level, protecting the application from under-voltage conditions.

# Overview

The **LVD (Low-Voltage Detector) driver** configures the single-channel hardware voltage comparator inside the SRSS (System Resources Subsystem) that continuously monitors VDDD. When the voltage drops below (or rises above) the configured threshold, the block raises an interrupt or triggers a fault, allowing the firmware to take protective action — such as flushing data to non-volatile memory, disabling peripherals, or signalling a safe shutdown.

# Features

- Configurable voltage threshold (range varies by IP variant)
- Selectable interrupt trigger: rising edge, falling edge, or both edges
- Optional fault-trigger action (supported on some IP variants): configurable to generate a system fault instead of (or in addition to) an interrupt
- SysPm Deep Sleep callback included (`Cy_LVD_DeepSleepCallback`) for safe low-power transitions
- Inline (zero-overhead) API for enable/disable, threshold setting, and interrupt management
- Selectable voltage source on some variants (VDDD, AMUXBUSA, AMUXBUSB, VDDIO)

# When to Use

- Detect a low-battery condition and trigger a safe shutdown or data flush before power is lost
- Monitor an external supply rail (via AMUXBUSA/B on devices with multi-source LVD support) for overvoltage or undervoltage events
- Generate an interrupt when VDDD recovers (rising edge) after a brown-out event
- Protect flash write operations by disabling them when VDDD falls below the minimum write voltage

# Prerequisites

## Hardware Requirements

- VDDD supply rail connected to the device; no external components required
- For AMUXBUS monitoring (where supported): signal routed to AMUXBUSA or AMUXBUSB

## Software Requirements

- ModusToolbox 3.x with PSOC PDL (`cy_pdl.h`)
- SysPm driver (`cy_syspm`) if Deep Sleep is used

## Configure in the Tool

The LVD block is part of the SRSS and does not require a Device Configurator personality — configure it entirely through the API. No `.modus` file entry is needed.

# Quick Start

This quick start sets up the LVD to trigger an interrupt when VDDD falls below a threshold.

**Step 1:** Determine the desired threshold from `cy_en_lvd_tripsel_t` (e.g., `CY_LVD_THRESHOLD_3_15_V`).

**Step 2:** Disable the LVD and clear the interrupt mask before changing the threshold (prevents false triggers).

**Step 3:** Configure the interrupt edge, enable the LVD, wait 20 µs for stabilization, then enable the interrupt mask.

**Step 4:** Add the following code to your `main.c`:

**Expected Outcome:** When VDDD drops below the configured threshold, `LVD_ISR` fires. The `lvd_event_flag` variable is set and the application can respond accordingly.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

static volatile bool lvd_event_flag = false;

/* LVD interrupt handler */
void LVD_ISR(void)
{
    /* Clear the LVD interrupt */
    Cy_LVD_ClearInterrupt();
    lvd_event_flag = true;
}

int main(void)
{
    cy_rslt_t result;

    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    __enable_irq();

    /* Step 1: Disable LVD and clear interrupt mask before changing threshold */
    Cy_LVD_Disable();
    Cy_LVD_ClearInterruptMask();

    /* Step 2: Set detection threshold — see cy_en_lvd_tripsel_t for supported values */
    Cy_LVD_SetThreshold(CY_LVD_THRESHOLD_3_15_V);

    /* Step 3: Configure interrupt edge — fire on falling edge (supply dropping below threshold) */
    Cy_LVD_SetInterruptConfig(CY_LVD_INTR_FALLING);

    /* Step 4: Configure the NVIC interrupt */
    static const cy_stc_sysint_t lvd_int_cfg = {
        .intrSrc      = srss_interrupt_IRQn,
        .intrPriority = 3U,
    };
    Cy_SysInt_Init(&lvd_int_cfg, LVD_ISR);
    NVIC_EnableIRQ(srss_interrupt_IRQn);

    /* Step 5: Enable LVD and wait ≥20 µs for circuit stabilization */
    Cy_LVD_Enable();
    Cy_SysLib_DelayUs(20U);

    /* Step 6: Clear any false interrupt that may have occurred during startup,
     * then enable the interrupt mask */
    Cy_LVD_ClearInterrupt();
    Cy_LVD_SetInterruptMask();

    for (;;)
    {
        if (lvd_event_flag)
        {
            lvd_event_flag = false;
            /* VDDD has dropped below the threshold — take protective action here */
            /* e.g., flush NVM, disable write operations, signal safe shutdown */
        }

        /* Check current status polled (no interrupt needed) */
        cy_en_lvd_status_t status = Cy_LVD_GetStatus();
        if (status == CY_LVD_STATUS_BELOW)
        {
            /* Supply is currently below threshold */
        }
    }
}
```

## Expected Outcome

- After initialization the LVD monitors VDDD continuously.
- When VDDD falls below the configured threshold, the `LVD_ISR` fires within one LVD reaction time.
- `lvd_event_flag` is set and the application loop can handle the under-voltage event.
- `Cy_LVD_GetStatus()` can be polled at any time to read the current supply status without waiting for an interrupt.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Spurious interrupt at startup | Threshold set while LVD was active | Always call `Cy_LVD_Disable()` + `Cy_LVD_ClearInterruptMask()` before `Cy_LVD_SetThreshold()` |
| No interrupt fires | Interrupt mask not set | Call `Cy_LVD_SetInterruptMask()` after `Cy_LVD_Enable()` and the 20 µs delay |
| Interrupt fires immediately after enable | False trigger during startup transient | Add `Cy_SysLib_DelayUs(20U)` + `Cy_LVD_ClearInterrupt()` before `Cy_LVD_SetInterruptMask()` |
| LVD does not wake from Deep Sleep | LVD only monitors in LP/ULP modes; Deep Sleep may need periodic wakeup | Configure a periodic wakeup source (e.g., WDT) to bring the device to LP mode for LVD checks |
| Threshold enum mismatch | Using threshold values outside the supported range for the IP variant | Consult `cy_en_lvd_tripsel_t` in the device header for the valid threshold enumeration |

# Related Code Examples

- [PSOC™ Edge MCU: Switching Power Modes](https://github.com/Infineon/mtb-example-psoc-edge-switching-power-modes)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — LVD chapter


# Configuration Parameters Reference

| Parameter | API | Values | Description |
|---|---|---|---|
| Threshold | `Cy_LVD_SetThreshold(cy_en_lvd_tripsel_t)` | Enumerated values in `cy_en_lvd_tripsel_t` (range varies by IP variant) | Voltage trip point for VDDD monitoring |
| Interrupt Edge | `Cy_LVD_SetInterruptConfig(cy_en_lvd_intr_config_t)` | `CY_LVD_INTR_DISABLE`, `CY_LVD_INTR_RISING`, `CY_LVD_INTR_FALLING`, `CY_LVD_INTR_BOTH` | Selects which supply transition triggers the interrupt |
| Action (where supported) | `Cy_LVD_SetActionConfig(cy_en_lvd_action_config_t)` | `CY_LVD_ACTION_INTERRUPT`, `CY_LVD_ACTION_FAULT` | Determines whether the LVD event generates an interrupt or a system fault |
| Voltage Source (where supported) | `Cy_LVD_SetSourceVoltage(cy_en_lvd_source_t)` | `CY_LVD_SOURCE_VDDD`, `CY_LVD_SOURCE_AMUXBUSA`, `CY_LVD_SOURCE_AMUXBUSB`, `CY_LVD_SOURCE_VDDIO` | Selects which supply rail the LVD monitors (on IP variants with multi-source support) |

# Advanced Usage

- **Fault mode (where supported):** Call `Cy_LVD_SetActionConfig(CY_LVD_ACTION_FAULT)` to route the LVD event to the System Fault handler (`Cy_SysFault`) instead of the NVIC. This is useful for safety-critical applications where a supply drop must trigger a system reset.
- **Both-edges monitoring:** Use `CY_LVD_INTR_BOTH` and read `Cy_LVD_GetStatus()` inside the ISR to determine whether VDDD is currently above or below the threshold, enabling debounced supply monitoring.
- **Deep Sleep strategy:** The LVD circuit is only active in Low Power (LP) and Ultra Low Power (ULP) modes. In Deep Sleep, schedule periodic wakeups via the WDT or RTC alarm to allow the LVD to perform a voltage check each interval.
- **AMUXBUS monitoring (where supported):** Route an external signal to AMUXBUSA via the HSIOM switch matrix, then call `Cy_LVD_SetSourceVoltage(CY_LVD_SOURCE_AMUXBUSA)` to monitor the external supply rail through the LVD.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
