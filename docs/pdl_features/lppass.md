# LPPASS (Autonomous Analog) - Low-Power Analog Sensing Subsystem for Always-On Applications

Provides access to the Autonomous Analog block (LPPASS), a fully reconfigurable mixed-signal sense, condition, and respond subsystem that includes a SAR ADC, CTB op-amps, a DAC, a programmable threshold comparator (PTC), and a hardware Finite-State Machine controller — operational in all power modes except Hibernate.

# Overview

The **LPPASS (Autonomous Analog / `cy_autanalog`) driver** configures and controls the low-power analog subsystem. The subsystem is entirely governed by a hardware Autonomous Controller (AC) Finite-State Machine that executes a user-programmed State Transition Table (STT), allowing sensing, conversion, comparison, and decision-making to occur autonomously in Deep Sleep — without waking the CPU.

# Features

- **Autonomous Controller (AC):** Hardware FSM that sequences analog operations from a State Transition Table, eliminating CPU polling in low-power modes
- **SAR ADC:** Up to 12-bit successive-approximation ADC with single-ended and differential input support, FIR digital filtering, and DMA-ready FIFO output
- **Continuous Time Block (CTB):** Two independent op-amp blocks usable as PGA, TIA, voltage follower, or comparator
- **Programmable Threshold Comparator (PTC):** Autonomous threshold detection with range-crossing interrupts, suitable for battery and supply monitoring
- **CT-DAC:** Continuous-time digital-to-analog converter for reference generation and signal output, supporting waveform and lookup-table modes
- **Full DeepSleep operation:** All subsystems remain powered and functional in Deep Sleep; the AC timer runs from CLK_LF (32 kHz) to schedule periodic conversions

# When to Use

- Continuously sample a battery voltage or a sensor in Deep Sleep without waking the CPU
- Implement an always-on microphone or audio front-end using the SAR ADC in LP mode
- Perform multi-channel sequential scanning with the AC state machine and buffer results in the FIFO
- Generate a precision reference voltage with the CT-DAC while the device is in a low-power state
- Use the PTC for autonomous range detection (e.g., temperature or current limits) and generate a CPU interrupt only on threshold crossings

# Prerequisites

## Hardware Requirements

- LPPASS analog supply (VDDA ≤ 1.8 V max for Autonomous Analog subsystem)
- GPIO pins reserved for analog input channels (High Impedance Analog drive mode)
- Peripheral clock Clk_HF9 routed to LPPASS in Active mode (max 80 MHz); CLK_LF (32 kHz) for the AC wakeup timer
- Optional external Vref pin (must not exceed VDDA)

## Software Requirements

- ModusToolbox 3.x with PSOC PDL (`cy_pdl.h`)
- SysPm driver for power-mode transitions

## Configure in the Tool

1. Open the Device Configurator → **Analog** → enable **Autonomous Analog**.
2. In the **Clocks** tab, route **Clk_HF9** through a Peri Clock divider to the LPPASS peripheral clock.
3. In the **Pins** tab, assign analog input pins and set their drive mode to **High Impedance Analog**.
4. Enable and configure the required sub-blocks (AC, SAR, CTB, PTC, DAC) in the personality pane.

| Parameter | Description | Value for LP SAR Scan | Value Explanation | Parameter Description |
|---|---|---|---|---|
| AC First State | FSM start condition | `Wait Blocks Ready` | Ensures all sub-blocks are initialized before the first conversion | Must be set to "Wait Blocks Ready" when any sub-block other than AC is used |
| SAR Operating Mode | Speed vs. power | `LP` | Low-power mode for battery-operated applications | HS (High Speed) or LP (Low Power); LP allows DeepSleep operation |
| SAR Resolution | ADC bit width | `12` | Highest accuracy for general sensing | 8–12 bits; higher bits increase conversion time |
| SAR Reference | Voltage reference source | Internal PRB | No external pin needed | Internal bandgap PRB or external Vref via pin |
| AC Timer Source | Clock for wakeup period | `CLK_LF` | 32 kHz ILO; runs in Deep Sleep | CLK_LF for DeepSleep-capable timers |
| FIFO Enable | Buffer ADC results | Enabled | Read results in bulk when the CPU wakes | Stores multiple ADC results; generates interrupt when threshold reached |

5. Save the `.modus` file; generated structures appear in `GeneratedSource/`.

# Quick Start

This quick start demonstrates single-channel SAR ADC conversion controlled by the Autonomous Controller in LP mode.

**Step 1:** Enable the Autonomous Analog in Device Configurator and configure one SAR channel connected to the target GPIO.

**Step 2:** Include the generated initialization headers and call the generated `init_cycfg_all()` function.

**Step 3:** Start the Autonomous Controller; the AC FSM will execute the STT and trigger SAR conversions autonomously.

**Step 4:** Add the following code to your `main.c`:

**Expected Outcome:** The AC starts SAR conversions autonomously. The CPU reads the result from the FIFO after conversion completes and prints the raw ADC count.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"   /* Generated by Device Configurator */

/* Interrupt flag */
static volatile bool sar_eos_flag = false;

/* LPPASS interrupt handler */
void AutoAnalog_ISR(void)
{
    uint32_t cause = Cy_AutoAnalog_GetInterruptStatus(LPPASS);

    if (cause & CY_AUTANALOG_INT_SAR0_EOS)
    {
        sar_eos_flag = true;
    }
    Cy_AutoAnalog_ClearInterrupt(LPPASS, cause);
}

int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals (calls generated code) */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    __enable_irq();

    /* Initialize all configured peripherals from Device Configurator */
    init_cycfg_all();

    /* Register and enable the LPPASS interrupt */
    static const cy_stc_sysint_t lppass_int_cfg = {
        .intrSrc      = lppass_interrupt_IRQn,
        .intrPriority = 3U,
    };
    Cy_SysInt_Init(&lppass_int_cfg, AutoAnalog_ISR);
    NVIC_EnableIRQ(lppass_interrupt_IRQn);

    /* Enable the EOS interrupt for SAR channel 0 */
    Cy_AutoAnalog_SetInterruptMask(LPPASS, CY_AUTANALOG_INT_SAR0_EOS);

    /* Start the Autonomous Controller */
    Cy_AutoAnalog_AC_Enable(LPPASS);

    for (;;)
    {
        if (sar_eos_flag)
        {
            sar_eos_flag = false;

            /* Read the latest SAR result (raw count) */
            uint16_t raw = (uint16_t)Cy_AutoAnalog_SAR_GetResult(LPPASS, 0U);

            /* Convert to millivolts (example: VDDA=1800 mV, 12-bit) */
            uint32_t mv = ((uint32_t)raw * 1800UL) / 4096UL;
            (void)mv; /* Use the value as needed */
        }

        /* Optionally enter Deep Sleep; the AC wakeup timer will restart conversions */
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }
}
```

## Expected Outcome

- The Autonomous Controller starts and executes the programmed STT.
- The SAR ADC performs a conversion on the configured input channel.
- On End-of-Scan (EOS), the interrupt fires and `sar_eos_flag` is set.
- The CPU reads the raw 12-bit result and can convert it to millivolts.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| AC does not start | AC first state not set to "Wait Blocks Ready" | Set the first STT state to `CY_AUTANALOG_STT_CONDITION_WAIT_BLOCKS_READY` |
| SAR result is always 0 | Analog input pin not set to High Impedance Analog | Set the GPIO drive mode to `CY_GPIO_DM_ANALOG` |
| No EOS interrupt | Interrupt mask not configured | Call `Cy_AutoAnalog_SetInterruptMask()` with `CY_AUTANALOG_INT_SAR0_EOS` |
| Conversion clock too fast | Clk_HF9 divider not set correctly | Ensure the LPPASS peripheral clock ≤ 80 MHz via the Peri Clock divider |
| Device won't enter Deep Sleep with AC running | Active-only subblocks enabled | Confirm SAR is set to LP mode (not HS) and AC timer uses CLK_LF |
| FIFO overflow | CPU reads too slowly | Reduce FIFO threshold or increase read frequency; enable DMA for bulk transfers |

# Related Code Examples

- [PSOC™ Edge MCU: ADC Basic](https://github.com/Infineon/mtb-example-psoc-edge-adc-basic)

# Related Application Notes

- AN235175 — Getting Started with Autonomous Analog

# Configuration Parameters Reference

| Parameter | API / Structure Field | Values / Range | Description |
|---|---|---|---|
| AC Mode | `cy_en_autanalog_ac_mode_t` | `CY_AUTANALOG_AC_MODE_ACTIVE`, `CY_AUTANALOG_AC_MODE_DEEPSLEEP` | Operating power mode of the Autonomous Controller |
| SAR Operating Mode | `cy_en_autanalog_sar_mode_t` | `CY_AUTANALOG_SAR_HS`, `CY_AUTANALOG_SAR_LP` | HS: up to 80 MHz clock, faster conversion; LP: DeepSleep-capable |
| SAR Resolution | Resolution field in SAR config | 8–12 bits | ADC bit depth; 12 bits = highest accuracy, longest conversion time |
| STT State Table | `cy_stc_autanalog_stt_t[]` | User-defined array | The "program" for the AC FSM; defines state conditions, actions, and transitions |
| AC Timer Period | `cy_stc_autanalog_timer_t.period` | 0–65535 CLK_LF ticks | Wakeup interval for periodic conversion; 1 tick ≈ 30.5 µs at 32.768 kHz |
| FIFO Threshold | SAR FIFO config | 1–32 entries | Number of results to buffer before raising an interrupt or triggering DMA |
| PRB Reference | `cy_stc_autanalog_prb_cfg_t` | Internal bandgap / external pin | Sets the voltage reference for ADC full-scale |

# Advanced Usage

- **DMA integration:** Connect the SAR FIFO to an AXI DMA channel so results are transferred to SRAM without CPU wakeup, enabling fully autonomous, continuous monitoring.
- **Multi-channel scanning:** Build a multi-state STT that cycles through multiple SAR channels, stores results in the FIFO, and wakes the CPU only when all channels are done.
- **CTB as PGA:** Configure the CTB op-amp as a Programmable Gain Amplifier ahead of the SAR input to boost weak sensor signals (e.g., from a thermocouple).
- **PTC for autonomous threshold crossing:** The Programmable Threshold Comparator generates a range interrupt without any AC state machine involvement, useful for overvoltage or undervoltage events.
- **DAC waveform generation:** Load a lookup table (LUT) into the CT-DAC to generate arbitrary waveforms (sine, sawtooth) autonomously, even in Deep Sleep.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
