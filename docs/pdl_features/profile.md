# Profile (MXPROFILE) - Hardware Energy Profiler for Runtime Power Analysis

## Overview

The **Profile** driver provides an API for configuring and reading the MXPROFILE hardware block — a set of dedicated hardware counters that track peripheral signal activity and clock cycles during a defined measurement window. By assigning individual peripherals or clock signals to counters and multiplying raw counts by user-supplied energy weighting factors, firmware can construct a detailed energy consumption profile of the SoC at runtime with zero external instrumentation and minimal CPU overhead.

> **Device Support:** Available on devices that include the MXPROFILE IP (`CY_IP_MXPROFILE`). Profiling is limited to **active run-time** states only; measurements during Deep Sleep, Hibernate, or Off modes are not supported (use an RTC for low-power period timing instead).

---

## Features

- **Multiple independent hardware counters** — Each counter can be assigned a different monitor signal from the device's `en_ep_mon_sel_t` enumeration (e.g., CPU active cycles, flash access events, peripheral bus activity)
- **Event and duration monitoring modes** — *Event mode* counts discrete pulse events (e.g., number of flash reads); *Duration mode* counts clock cycles while a signal is asserted (e.g., BLE TX radio active time)
- **64-bit counter with overflow protection** — Each 32-bit hardware register is extended to 64 bits in software via an overflow counter; the supplied ISR `Cy_Profile_ISR()` services overflow interrupts automatically
- **Weighted energy calculation** — Each counter carries a user-defined weight factor; `Cy_Profile_GetWeightedCount()` multiplies raw counts by the weight; `Cy_Profile_GetSumWeightedCounts()` accumulates all active counters into a single normalized energy estimate
- **Flexible reference clock** — Counters can reference Timer, IMO, ECO, LFCLK, HF, or Peri clock sources for duration measurements
- **Low CPU overhead** — Measurement window is started/stopped by writing single command register values; counters run entirely in hardware

---

## When to Use

- Estimate energy consumed by an RTOS task by counting CM4/CM55 active cycles and flash accesses, then multiplying by the associated power numbers from the device datasheet
- Measure BLE or Wi-Fi radio active time using duration monitoring on the radio control signal, then compute energy as `power × time`
- Benchmark firmware optimization: compare total weighted energy between two software implementations of the same algorithm
- Create continuous system energy monitors by reading counters on-the-fly without stopping the profiling window (`Cy_Profile_GetRawCount()`)
- Build timestamped power profiles by assigning one counter to `PROFILE_ONE` (constant increment) as a reference clock and others to peripheral events

---

## Prerequisites

### Hardware Requirements

- **Device:** Must have MXPROFILE IP block (`CY_IP_MXPROFILE`); consult the device datasheet for the number of available profile counters (`PROFILE_PRFL_CNT_NR`)
- **Interrupt:** The profile interrupt (`profile_interrupt_IRQn`) must be configured using the SysInt driver; use the provided `Cy_Profile_ISR()` handler or a custom ISR that calls it
- **Clock:** Profile counters use the device's existing clock infrastructure; no dedicated external clock required

### Software Requirements

- PDL ≥ 1.30 (included with ModusToolbox™ 3.x)
- Include `cy_pdl.h` or `cy_profile.h`
- SysInt driver for interrupt configuration (`cy_sysint.h` is included transitively via `cy_pdl.h`)

### Configure in the Tool

The Profile driver does not require Device Configurator personality setup. Configuration is performed entirely in firmware:

1. Identify available monitor signals for your device in the `en_ep_mon_sel_t` enum in the device configuration file
2. Assign monitor signals to counters using `Cy_Profile_ConfigureCounter()`
3. Configure the `profile_interrupt_IRQn` interrupt using `Cy_SysInt_Init()` with `Cy_Profile_ISR` as the handler
4. Enable the interrupt and call `Cy_Profile_StartProfiling()`

---

## Quick Start

This quick start measures CM0+ processor active cycles and HF clock duration over a 1-second window.

**Step 1:** Include the PDL header.

**Step 2:** Configure the profile interrupt.

**Step 3:** Initialize the profile hardware, configure counters, start profiling.

**Step 4:** Add the following code to your `main.c` (see [Sample Code](#sample-code) for complete example).

**Expected Outcome:** After the measurement window, the raw and weighted counts for the CM0+ active cycles counter are printed/read, and the sum of all weighted counts gives the normalized energy estimate.

### Sample Code

```c
#include "cy_pdl.h"

/* Weighting factor (normalized; multiply by known power values for real energy) */
#define WEIGHT_CM0_ACTIVE   1000000UL
#define WEIGHT_HF_CLOCK     500000UL

/* Counter handles */
static cy_stc_profile_ctr_ptr_t cntCm0, cntHf;

/* Profile interrupt configuration */
static const cy_stc_sysint_t profileIntrCfg = {
    .intrSrc      = profile_interrupt_IRQn,
    .intrPriority = 3U
};

int main(void)
{
    uint64_t rawCount, weightedCount, totalEnergy;

    /* Step 1: Configure and enable the profile interrupt */
    Cy_SysInt_Init(&profileIntrCfg, Cy_Profile_ISR);
    NVIC_EnableIRQ(profile_interrupt_IRQn);

    /* Step 2: Initialize the profile hardware */
    Cy_Profile_Init();
    Cy_Profile_ClearConfiguration();
    Cy_Profile_ClearCounters();

    /* Step 3: Allocate counters
       - CM0+ active cycles (event mode, HF reference clock) */
    cntCm0 = Cy_Profile_ConfigureCounter(
                  CPUSS_MONITOR_CM0,       /* CM0+ active monitor signal */
                  CY_PROFILE_DURATION,     /* measure active time in clock cycles */
                  CY_PROFILE_CLK_HF,       /* HF reference clock */
                  WEIGHT_CM0_ACTIVE);      /* weighting factor */

    /* - HF clock ticks (continuous reference using PROFILE_ONE) */
    cntHf  = Cy_Profile_ConfigureCounter(
                  PROFILE_ONE,             /* always 1 — counts every clock */
                  CY_PROFILE_DURATION,
                  CY_PROFILE_CLK_HF,
                  WEIGHT_HF_CLOCK);

    /* Step 4: Enable each counter */
    Cy_Profile_EnableCounter(cntCm0);
    Cy_Profile_EnableCounter(cntHf);

    /* Step 5: Start the profiling window */
    Cy_Profile_StartProfiling();

    /* --- Code under measurement --- */
    Cy_SysLib_Delay(1000U); /* 1 second measurement window */
    /* --- End of measured section --- */

    /* Step 6: Stop profiling and disable interrupt */
    NVIC_DisableIRQ(profile_interrupt_IRQn);
    Cy_Profile_StopProfiling();

    /* Step 7: Read results */
    Cy_Profile_GetRawCount(cntCm0, &rawCount);
    Cy_Profile_GetWeightedCount(cntCm0, &weightedCount);

    cy_stc_profile_ctr_ptr_t ctrs[2] = {cntCm0, cntHf};
    totalEnergy = Cy_Profile_GetSumWeightedCounts(ctrs, 2U);

    /* Step 8: Release counters and clean up */
    Cy_Profile_FreeCounter(cntCm0);
    Cy_Profile_FreeCounter(cntHf);
    Cy_Profile_DeInit();

    for (;;) { /* results in rawCount, weightedCount, totalEnergy */ }
}
```

### Expected Outcome

- `rawCount` contains the number of HF clock cycles during which CM0+ was active over the 1-second window
- `weightedCount` = `rawCount × WEIGHT_CM0_ACTIVE`
- `totalEnergy` = sum of weighted counts from both counters
- No assertion fires — all Profile APIs return `CY_PROFILE_SUCCESS`

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `Cy_Profile_ConfigureCounter()` returns NULL | All hardware counters already allocated | Free unused counters with `Cy_Profile_FreeCounter()` before allocating new ones; check `PROFILE_PRFL_CNT_NR` for device limit |
| Counter value does not increment | Counter not enabled or profile window not started | Confirm `Cy_Profile_EnableCounter()` and `Cy_Profile_StartProfiling()` are called before the measurement |
| `Cy_Profile_GetRawCount()` returns stale value | Profiling window still running | Call `Cy_Profile_StopProfiling()` before reading final counter values |
| Overflow counter not incrementing | Profile interrupt not configured or NVIC not enabled | Configure the interrupt with `Cy_SysInt_Init()` and call `NVIC_EnableIRQ(profile_interrupt_IRQn)` |
| Energy measurement during Deep Sleep is zero | MXPROFILE does not count during low-power states | Use an RTC timestamp before/after Deep Sleep entry; multiply the elapsed time by the Deep Sleep power number from the device datasheet |
| `CY_PROFILE_BAD_PARAM` returned | Invalid monitor signal index or clock source | Verify the `en_ep_mon_sel_t` value is within `CY_EP_MONITOR_COUNT` for the target device |

---

## Related Code Examples

- [PSOC™ Edge MCU: Power Measurements](https://github.com/Infineon/mtb-example-psoc-edge-power-measurements)

## Related Application Notes

- Device Technical Reference Manual (TRM) — Profile hardware chapter (for full list of `en_ep_mon_sel_t` monitor signals per device)

---

## Configuration Parameters Reference

### Enumerations

| Enum | Values | Description |
|------|--------|-------------|
| `cy_en_profile_duration_t` | `CY_PROFILE_EVENT` (0), `CY_PROFILE_DURATION` (1) | Counter mode: count discrete events (edge) or clock cycles while signal is high (level) |
| `cy_en_profile_ref_clk_t` | `CY_PROFILE_CLK_TIMER` … `CY_PROFILE_CLK_PERI` | Reference clock used for duration counting (6 clock sources available) |
| `cy_en_profile_status_t` | `CY_PROFILE_SUCCESS`, `CY_PROFILE_BAD_PARAM` | API return status |

### Data Structures

| Structure | Members | Description |
|-----------|---------|-------------|
| `cy_stc_profile_ctr_ctl_t` | `cntDuration`, `refClkSel`, `monSel` | Counter control register settings (mode, clock, monitor signal) |
| `cy_stc_profile_ctr_t` | `ctrNum`, `used`, `ctlRegVals`, `cntAddr`, `ctlReg`, `cntReg`, `overflow`, `weight` | Complete counter state including 32-bit hardware registers and 32-bit software overflow extension |

### Reference Clock Sources

| Enum Value | Clock | Typical Use |
|-----------|-------|-------------|
| `CY_PROFILE_CLK_TIMER` | Timer clock | High-precision timing measurements |
| `CY_PROFILE_CLK_IMO` | Internal Main Oscillator | Default always-available clock |
| `CY_PROFILE_CLK_ECO` | External Crystal Oscillator | Accurate absolute time reference |
| `CY_PROFILE_CLK_LF` | Low-Frequency Clock | Measuring long slow events |
| `CY_PROFILE_CLK_HF` | High-Frequency clock (HFCLK0) | CPU cycle counting and bus monitoring |
| `CY_PROFILE_CLK_PERI` | Peripheral clock | Peripheral bus activity duration |

### Key API Summary

| API | Group | Description |
|-----|-------|-------------|
| `Cy_Profile_Init()` | General | Initialize and enable the profile hardware block |
| `Cy_Profile_DeInit()` | General | Clear interrupt mask and disable profile hardware |
| `Cy_Profile_StartProfiling()` | General | Start the measurement window (all enabled counters begin counting) |
| `Cy_Profile_StopProfiling()` | General | Stop the measurement window |
| `Cy_Profile_IsProfiling()` | General | Returns 1 if profiling window is active, 0 otherwise |
| `Cy_Profile_ConfigureCounter()` | Counter | Allocate and configure a counter with monitor signal, mode, clock, and weight |
| `Cy_Profile_EnableCounter()` | Counter | Enable a configured counter to count during the active window |
| `Cy_Profile_DisableCounter()` | Counter | Disable a counter |
| `Cy_Profile_FreeCounter()` | Counter | Release a counter back to the available pool |
| `Cy_Profile_ClearConfiguration()` | Counter | Clear all counter configurations |
| `Cy_Profile_ClearCounters()` | Counter | Reset all hardware counter registers to 0 |
| `Cy_Profile_GetRawCount()` | Calculation | Read the 64-bit raw count (hardware + overflow) for a counter |
| `Cy_Profile_GetWeightedCount()` | Calculation | Read the 64-bit weighted count (`raw × weight`) for a counter |
| `Cy_Profile_GetSumWeightedCounts()` | Calculation | Sum the weighted counts of an array of counters (total energy estimate) |
| `Cy_Profile_ISR()` | Interrupt | Sample ISR — increments overflow counters; use as handler for `profile_interrupt_IRQn` |

### Macro Reference

| Macro | Value | Description |
|-------|-------|-------------|
| `CY_PROFILE_START_TR` | 1 | Command value to write to PROFILE_CMD to start profiling |
| `CY_PROFILE_STOP_TR` | 2 | Command value to write to PROFILE_CMD to stop profiling |
| `CY_PROFILE_CLR_ALL_CNT` | 0x100 | Command value to clear all counter registers |
| `PROFILE_PRFL_CNT_NR` | device-specific | Total number of available profile counter hardware instances |

---

## Advanced Usage

### Continuous On-the-Fly Reads

Read counter values without stopping the profiling window to create a continuous monitor or timestamped log:

```c
uint64_t snapshot;
/* Window is still running */
Cy_Profile_GetRawCount(cntCm0, &snapshot);
printf("CM0 active cycles so far: %llu\n", (unsigned long long)snapshot);
```

### Reference Timestamp with PROFILE_ONE

Assign one counter to `PROFILE_ONE` (a signal that is always asserted) to get an absolute time reference in clock cycles:

```c
cy_stc_profile_ctr_ptr_t refCnt = Cy_Profile_ConfigureCounter(
    PROFILE_ONE, CY_PROFILE_DURATION, CY_PROFILE_CLK_HF, 1UL);
Cy_Profile_EnableCounter(refCnt);
/* After window: divide raw count by HF clock frequency to get seconds */
uint64_t cycles;
Cy_Profile_GetRawCount(refCnt, &cycles);
double seconds = (double)cycles / CY_CLK_HFCLK0_FREQ_HZ;
```

### Multi-Window Sequential Measurements

Clear counters and restart without re-initializing the hardware:

```c
Cy_Profile_StopProfiling();
Cy_Profile_ClearCounters();          /* reset counters to 0 */
Cy_Profile_StartProfiling();         /* start next window */
```

### Energy Calculation Using Power Numbers

Combine profile results with power numbers from the device datasheet:

```
Energy_mJ = (CM4_active_cycles / HF_freq_MHz) × CM4_active_power_mW
           + (Flash_accesses × Flash_access_energy_nJ / 1e6)
```

Use `Cy_Profile_GetSumWeightedCounts()` by setting the `weight` parameter of each counter proportional to the associated power coefficient.

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
