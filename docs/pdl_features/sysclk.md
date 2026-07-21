# SysClk - System Clock Management for Precise Frequency Control

## Overview
The SysClk driver provides a full-featured API for configuring, enabling, and measuring all clock resources on Infineon PSOC/AIROC devices, including PLLs, FLLs, oscillators, clock path muxes, peripheral dividers, and low-frequency sources. It also exposes a low-power callback so clock configurations can be safely adjusted around power-mode transitions.

## Features
- **PLL and FLL configuration**: Automatic (run-time calculated) and manual (pre-calculated) modes for both the Frequency Locked Loop and Phase Locked Loop
- **Rich oscillator support**: IMO, ECO, ILO, PILO, WCO, MFO, and EXTCLK with enable/disable and frequency-query APIs
- **Hierarchical clock tree**: Source → Path → HF clock → Peripheral divider pipeline fully configurable from software
- **Clock measurement and calibration**: Hardware counter-based frequency measurement against a reference clock; ILO/PILO trim functions for ±1.5% accuracy
- **Secure Aware operation**: On ARM TrustZone devices, secured clock resources are automatically accessed via the Secure Request Framework (SRF) when called from non-secure state

## When to Use
- **Startup clock tree initialization**: Configure the PLL or FLL to reach target CPU frequency from the IMO reference
- **ECO-based precision timing**: Drive a PLL from an external crystal for applications requiring accurate RF or USB clocking
- **Dynamic frequency scaling**: Reduce HF clock frequency and switch regulator state when entering ULP mode
- **RTC/WCO backup domain**: Configure the 32.768 kHz WCO or PILO as the LF clock source for always-on timers
- **Clock measurement for calibration**: Periodically measure and trim the ILO to maintain watchdog accuracy

## Prerequisites

### Hardware Requirements
- Any Infineon PSOC or AIROC device supported by the PDL
- Crystal oscillator and load capacitors if using ECO
- For MFO / PILO: available on selected SRSS variants

### Software Requirements
- PDL version ≥ 3.x (`cy_sysclk.h` v3.150)
- `cy_pdl.h` (umbrella include) or `cy_sysclk.h` directly

### Configure in the Tool

| Parameter | Description | Typical Value |
|---|---|---|
| ECO frequency (Hz) | Crystal resonant frequency | `24000000` (24 MHz) |
| ECO load capacitance (pF) | Sum of C0 + Cload | `22`–`33` pF |
| ECO ESR (Ω) | Effective series resistance | `40`–`200` Ω |
| ECO drive level (µW) | Crystal drive power | `500` µW |
| PLL output frequency (Hz) | Desired PLL output | Device maximum (e.g., 150 MHz) |
| FLL output frequency (Hz) | Desired FLL output | `100000000` (100 MHz) |
| Clock path source | Mux input for a given path | `CY_SYSCLK_CLKPATH_IN_IMO` |
| HF clock divider | Divider applied to HF clock output | `1` (no divide) |

## Quick Start

### Step-by-Step
1. Include `cy_pdl.h` (or `cy_sysclk.h`).
2. If using ECO: configure GPIO pins for ECO_IN / ECO_OUT in analog mode, then call `Cy_SysClk_EcoConfigure()` and `Cy_SysClk_EcoEnable()`.
3. Set the clock path source (`Cy_SysClk_ClkPathSetSource()`).
4. Configure the PLL or FLL (`Cy_SysClk_PllConfigure()` / `Cy_SysClk_FllConfigure()`).
5. Enable the PLL/FLL and wait for lock.
6. Switch the HF clock path to the PLL/FLL output.

### Sample Code

```c
#include "cy_pdl.h"

void ClockInit(void)
{
    /* --- 1. Configure and enable the ECO (17.2032 MHz crystal) --- */
    Cy_SysClk_EcoDisable();

    if (CY_SYSCLK_SUCCESS != Cy_SysClk_EcoConfigure(
            17203200UL,   /* frequency: 17.2032 MHz */
            29UL,         /* load capacitance: 7 pF + 22 pF */
            40UL,         /* ESR: 40 Ohm */
            500UL))       /* drive level: 500 uW */
    {
        /* Handle ECO configuration error */
    }

    /* Enable ECO with 3 ms timeout */
    if (CY_SYSCLK_SUCCESS != Cy_SysClk_EcoEnable(3000UL))
    {
        /* Handle ECO enable error */
    }

    /* --- 2. Route ECO to clock path 1 (PLL reference) --- */
    (void)Cy_SysClk_ClkPathSetSource(1UL, CY_SYSCLK_CLKPATH_IN_ECO);

    /* --- 3. Configure PLL0 to produce 150 MHz from ECO --- */
    cy_stc_pll_config_t pllCfg = {
        .inputFreq  = 17203200UL,   /* ECO frequency */
        .outputFreq = 150000000UL,  /* desired output: 150 MHz */
        .lfMode     = false,
        .outputMode = CY_SYSCLK_FLLPLL_OUTPUT_AUTO
    };
    if (CY_SYSCLK_SUCCESS != Cy_SysClk_PllConfigure(0UL, &pllCfg))
    {
        /* Handle PLL configuration error */
    }

    /* Enable PLL and wait for lock (timeout 10 ms) */
    if (CY_SYSCLK_SUCCESS != Cy_SysClk_PllEnable(0UL, 10000UL))
    {
        /* Handle PLL lock error */
    }

    /* --- 4. Switch HF clock 0 to PLL output (path 1) --- */
    (void)Cy_SysClk_ClkHfSetSource(0UL, CY_SYSCLK_CLKHF_IN_CLKPATH1);
    (void)Cy_SysClk_ClkHfSetDivider(0UL, CY_SYSCLK_CLKHF_NO_DIVIDE);
    (void)Cy_SysClk_ClkHfEnable(0UL);
}

int main(void)
{
    ClockInit();
    for (;;) { /* Application loop */ }
}
```

### Expected Outcome
- ECO oscillator starts and reaches stable amplitude within ~3 ms.
- PLL locks to 150 MHz and HF clock 0 begins sourcing the CPU at full speed.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| `Cy_SysClk_EcoEnable()` returns timeout | Crystal not oscillating | Check PCB layout; verify load capacitor values and ESR; confirm GPIO pins are in analog mode with HSIOM_SEL_GPIO |
| `Cy_SysClk_PllEnable()` returns timeout | PLL cannot lock | Verify input/output frequency ratio is within device PLL range; check path source is stable |
| `CY_SYSCLK_ECOSTAT_UNUSABLE` from `EcoGetStatus` | Amplitude too low | Increase drive level or reduce ESR spec in `EcoConfigure`; check crystal quality |
| System crash after HF clock switch | Flash wait-states not updated | Call `Cy_SysLib_SetWaitStates()` before increasing HF clock frequency |
| ILO frequency out of spec | No calibration performed | Use `Cy_SysClk_StartClkMeasurementCounters()` + `Cy_SysClk_IloTrim()` periodically |

## Related Code Examples

- [PSOC™ Edge MCU: Hello World](https://github.com/Infineon/mtb-example-psoc-edge-hello-world)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — System Clocks chapter

## Configuration Parameters Reference

| Parameter / API | Type | Description |
|---|---|---|
| `Cy_SysClk_EcoConfigure()` | Function | Sets ECO oscillator frequency, capacitance, ESR, and drive level |
| `Cy_SysClk_EcoEnable()` | Function | Enables ECO with microsecond timeout |
| `Cy_SysClk_EcoManualConfigure()` | Function | Manual ECO trim config (supported on select SRSS variants) |
| `Cy_SysClk_PllConfigure()` | Function | Auto-calculates PLL feedback dividers for a target frequency |
| `Cy_SysClk_PllManualConfigure()` | Function | Applies pre-calculated PLL divider values |
| `Cy_SysClk_PllEnable()` | Function | Enables PLL and waits for lock |
| `Cy_SysClk_FllConfigure()` | Function | Auto-calculates FLL parameters for a target frequency |
| `Cy_SysClk_FllManualConfigure()` | Function | Applies pre-calculated FLL parameters |
| `Cy_SysClk_FllEnable()` | Function | Enables FLL and waits for lock |
| `Cy_SysClk_ClkPathSetSource()` | Function | Sets the input mux source for a clock path |
| `Cy_SysClk_ClkHfSetSource()` | Function | Assigns a clock path to an HF clock output |
| `Cy_SysClk_ClkHfSetDivider()` | Function | Applies integer post-divider to an HF clock |
| `Cy_SysClk_IloEnable()` | Function | Enables the 32.768 kHz Internal Low-Speed Oscillator |
| `Cy_SysClk_IloHibernateOn()` | Function | Keeps ILO active across Hibernate and POR |
| `Cy_SysClk_PiloEnable()` | Function | Enables the Precision ILO (±250 ppm with calibration) |
| `Cy_SysClk_StartClkMeasurementCounters()` | Function | Starts hardware-based clock frequency measurement |
| `Cy_SysClk_IloTrim()` | Function | Single-step trim of ILO toward nominal 32.768 kHz |
| `cy_stc_pll_config_t` | Struct | PLL auto-configure: inputFreq, outputFreq, lfMode, outputMode |
| `cy_stc_fll_manual_config_t` | Struct | FLL manual configure: multiplier, reference divider, CCO range |

## Advanced Usage

### FLL from WCO Reference
The FLL can accept a 32.768 kHz WCO input (path 0) to produce a high-frequency clock without a crystal at MHz range:

```c
(void)Cy_SysClk_ClkPathSetSource(0UL, CY_SYSCLK_CLKPATH_IN_WCO);
cy_stc_fll_manual_config_t fllCfg;
Cy_SysClk_FllGetConfiguration(&fllCfg);   /* read defaults */
(void)Cy_SysClk_FllConfigure(32768UL, 100000000UL, CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT);
(void)Cy_SysClk_FllEnable(200000UL);      /* 200 ms lock timeout */
```

### Sub-Radio IP Clock Domain
On devices with a sub-radio IP, a separate clock hierarchy may be accessible via a device-specific header (e.g., `cy_sysclk_srip.h`). Refer to the device TRM and PDL documentation for availability.

### Clock Measurement and ILO Calibration

```c
/* Measure ILO frequency relative to IMO reference over 1000 ILO cycles */
Cy_SysClk_StartClkMeasurementCounters(CY_SYSCLK_MEAS_CLK_ILO, 1000UL,
                                       CY_SYSCLK_MEAS_CLK_IMO);
/* Wait for measurement to complete */
while (!Cy_SysClk_ClkMeasurementCountersDone()) {}

uint32_t measuredHz = Cy_SysClk_ClkMeasurementCountersGetFreq(false, CY_SYSCLK_IMO_FREQ);

/* Trim ILO once toward 32.768 kHz nominal */
(void)Cy_SysClk_IloTrim(measuredHz);
```

## Industry Standards
- None specific; clock configuration follows device-family TRM specifications.

## Copyright
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
