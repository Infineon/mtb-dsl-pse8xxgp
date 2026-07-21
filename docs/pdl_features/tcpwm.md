# TCPWM - Versatile Timer/Counter/PWM/QuadDec Engine

## Overview
The Timer/Counter/PWM (TCPWM) driver provides a unified API for the `mxtcpwm` hardware block, enabling
Timer/Counter, PWM signal generation, Quadrature Decoding, and Shift Register functions in a single
multifunction peripheral available on all supported devices.

---

## Features
- **16- or 32-bit** counter/timer with programmable period and compare registers (double-buffered)
- **PWM modes**: Left/Right/Center/Asymmetric aligned, Pseudo-Random, Dead-Time insertion (PWMDT)
- **Quadrature Decoder**: x1/x2/x4 resolution, Range-0/1 modes, index input for absolute position
- **Shift Register**: Serial-in / parallel-out and parallel-in / serial-out data conversion
- **Capture**: Time-stamp on external events with buffered capture register (CC0 / CC1)
- **Trigger outputs** (trig_out0/trig_out1): Overflow, Underflow, Terminal Count, Compare Match (V2+)
- Up to 2048 counters per block (TCPWM v3); counter groups of 16-bit and 32-bit
- Debug Freeze / Debug Suspend support (V2+ / V3+)

---

## When to Use

| Scenario | Mode |
|---|---|
| Periodic ISR, event counting, pulse-width measurement | **Timer/Counter** |
| LED brightness, motor drive, arbitrary square waves | **PWM** |
| Motor encoder position / velocity / direction | **Quadrature Decoder** |
| FSK detection, serial-to-parallel conversion | **Shift Register** |

---

## Prerequisites

### Hardware Requirements
- `mxtcpwm` IP block on the target device
- Clock source (any peripheral clock divider via `cy_sysclk`)
- For PWM: GPIO pin routed to `line_out` / `line_compl_out`
- For QuadDec: Two GPIO pins for phiA / phiB encoder signals; optional Index pin

### Software Requirements
- `cy_pdl.h` (includes `cy_tcpwm_counter.h`, `cy_tcpwm_pwm.h`, `cy_tcpwm_quaddec.h`, `cy_tcpwm_shiftreg.h`)
- Clock assigned via `Cy_SysClk_PeriphAssignDivider()` / `Cy_SysClk_PeriphEnableDivider()`

### Configure in the Tool (ModusToolbox Device Configurator)
The ModusToolbox Device Configurator generates a `cy_stc_tcpwm_*_config_t` structure automatically.
The table below lists the key personality parameters shared across sub-modes.

| Parameter | Description | Typical Values |
|---|---|---|
| `Counter Width` | Selects 16-bit or 32-bit counter | `16`, `32` |
| `Run Mode` | Continuous or One-Shot | `CY_TCPWM_*_CONTINUOUS`, `_ONE_SHOT` |
| `Clock Prescaler` | Divides input clock inside the block | `DIVBY_1` … `DIVBY_128` |
| `Period` | Terminal count value | Device-specific |
| `Compare / Capture` | Compare mode or Capture mode (Counter) | `CY_TCPWM_COUNTER_MODE_COMPARE` |
| `Interrupt Source` | TC, CC0, CC1 (V2+) | `CY_TCPWM_INT_ON_TC` |
| `PWM Mode` | Left/Right/Center/Asymmetric/PR/DT | `CY_TCPWM_PWM_MODE_PWM` |
| `Dead Time` | Clocks of dead time (PWMDT only) | 0–255 |
| `Resolution` (QuadDec) | x1 / x2 / x4 / Up-Down (V2+) | `CY_TCPWM_QUADDEC_X4` |

---

## Quick Start

### Sub-Mode 1 — Timer / Counter

**Step 1.** Fill `cy_stc_tcpwm_counter_config_t`.
**Step 2.** Call `Cy_TCPWM_Counter_Init()`.
**Step 3.** Enable the counter with `Cy_TCPWM_Counter_Enable()`.
**Step 4.** Start counting with `Cy_TCPWM_TriggerStart_Single()`.

```c
#include "cy_pdl.h"

#define MY_CNT_NUM  (0UL)

/* TCPWM V2 Counter – count up, period 100, compare at 33 */
cy_stc_tcpwm_counter_config_t counterCfg =
{
    .period            = 99UL,
    .clockPrescaler    = CY_TCPWM_COUNTER_PRESCALER_DIVBY_4,
    .runMode           = CY_TCPWM_COUNTER_CONTINUOUS,
    .countDirection    = CY_TCPWM_COUNTER_COUNT_UP,
    .compareOrCapture  = CY_TCPWM_COUNTER_MODE_COMPARE,
    .compare0          = 33UL,
    .compare1          = 66UL,
    .enableCompareSwap = true,
    .interruptSources  = CY_TCPWM_INT_NONE,
    .captureInputMode  = CY_TCPWM_INPUT_RISINGEDGE,
    .captureInput      = CY_TCPWM_INPUT_0,
    .reloadInputMode   = CY_TCPWM_INPUT_RISINGEDGE,
    .reloadInput       = CY_TCPWM_INPUT_0,
    .startInputMode    = CY_TCPWM_INPUT_RISINGEDGE,
    .startInput        = CY_TCPWM_INPUT_0,
    .stopInputMode     = CY_TCPWM_INPUT_RISINGEDGE,
    .stopInput         = CY_TCPWM_INPUT_0,
    .countInputMode    = CY_TCPWM_INPUT_LEVEL,
    .countInput        = CY_TCPWM_INPUT_1,
};

int main(void)
{
    __enable_irq();

    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(TCPWM0, MY_CNT_NUM, &counterCfg))
    {
        /* Handle error */
    }
    Cy_TCPWM_Counter_Enable(TCPWM0, MY_CNT_NUM);
    Cy_TCPWM_TriggerStart_Single(TCPWM0, MY_CNT_NUM);

    for (;;)
    {
        uint32_t count = Cy_TCPWM_Counter_GetCounter(TCPWM0, MY_CNT_NUM);
        (void)count; /* use count value */
    }
}
```

**Expected Outcome:** Counter increments from 0 to 99, wraps, and generates a terminal-count event each cycle at 1/4 of the input clock frequency.

---

### Sub-Mode 2 — PWM

**Step 1.** Fill `cy_stc_tcpwm_pwm_config_t` (pwmMode, period0, compare0 …).
**Step 2.** Call `Cy_TCPWM_PWM_Init()`.
**Step 3.** `Cy_TCPWM_PWM_Enable()` then `Cy_TCPWM_TriggerStart_Single()`.

```c
#include "cy_pdl.h"

#define MY_PWM_NUM  (0UL)

cy_stc_tcpwm_pwm_config_t pwmCfg =
{
    .pwmMode           = CY_TCPWM_PWM_MODE_PWM,
    .clockPrescaler    = CY_TCPWM_PWM_PRESCALER_DIVBY_4,
    .pwmAlignment      = CY_TCPWM_PWM_LEFT_ALIGN,
    .deadTimeClocks    = 0UL,
    .runMode           = CY_TCPWM_PWM_CONTINUOUS,
    .period0           = 99UL,   /* 100-count period */
    .period1           = 199UL,
    .enablePeriodSwap  = false,
    .compare0          = 33UL,   /* 33% duty cycle */
    .compare1          = 66UL,
    .enableCompareSwap = false,
    .interruptSources  = CY_TCPWM_INT_NONE,
    .invertPWMOut      = 0UL,
    .invertPWMOutN     = 0UL,
    .killMode          = CY_TCPWM_PWM_STOP_ON_KILL,
    .swapInputMode     = CY_TCPWM_INPUT_RISINGEDGE,
    .swapInput         = CY_TCPWM_INPUT_0,
    .reloadInputMode   = CY_TCPWM_INPUT_RISINGEDGE,
    .reloadInput       = CY_TCPWM_INPUT_0,
    .startInputMode    = CY_TCPWM_INPUT_RISINGEDGE,
    .startInput        = CY_TCPWM_INPUT_0,
    .killInputMode     = CY_TCPWM_INPUT_RISINGEDGE,
    .killInput         = CY_TCPWM_INPUT_0,
    .countInputMode    = CY_TCPWM_INPUT_LEVEL,
    .countInput        = CY_TCPWM_INPUT_1,
};

int main(void)
{
    __enable_irq();

    if (CY_TCPWM_SUCCESS != Cy_TCPWM_PWM_Init(TCPWM0, MY_PWM_NUM, &pwmCfg))
    {
        /* Handle error */
    }
    Cy_TCPWM_PWM_Enable(TCPWM0, MY_PWM_NUM);
    Cy_TCPWM_TriggerStart_Single(TCPWM0, MY_PWM_NUM);

    for (;;) { }
}
```

**Expected Outcome:** A 33% duty-cycle PWM signal on the configured GPIO pin at `Fclk / (prescaler × (period+1))`.

---

### Sub-Mode 3 — Quadrature Decoder

**Step 1.** Fill `cy_stc_tcpwm_quaddec_config_t` with phiA/phiB inputs.
**Step 2.** `Cy_TCPWM_QuadDec_Init()` → `Cy_TCPWM_QuadDec_Enable()` → `Cy_TCPWM_TriggerReloadOrIndex_Single()`.

```c
#include "cy_pdl.h"

#define MY_QD_NUM  (0UL)

cy_stc_tcpwm_quaddec_config_t qdCfg =
{
    .resolution       = CY_TCPWM_QUADDEC_X4,
    .interruptSources = CY_TCPWM_INT_ON_TC,
    .indexInputMode   = CY_TCPWM_INPUT_RISINGEDGE,
    .indexInput       = CY_TCPWM_INPUT_0,
    .stopInputMode    = CY_TCPWM_INPUT_RISINGEDGE,
    .stopInput        = CY_TCPWM_INPUT_0,
    .phiAInput        = CY_TCPWM_INPUT_TRIG(0),
    .phiBInput        = CY_TCPWM_INPUT_TRIG(1),
    .phiAInputMode    = CY_TCPWM_INPUT_LEVEL,
    .phiBInputMode    = CY_TCPWM_INPUT_LEVEL,
    .quadmode         = CY_TCPWM_QUADDEC_MODE_RANGE0,
};

int main(void)
{
    __enable_irq();

    if (CY_TCPWM_SUCCESS != Cy_TCPWM_QuadDec_Init(TCPWM0, MY_QD_NUM, &qdCfg))
    {
        /* Handle error */
    }
    Cy_TCPWM_QuadDec_Enable(TCPWM0, MY_QD_NUM);
    Cy_TCPWM_TriggerReloadOrIndex_Single(TCPWM0, MY_QD_NUM);

    for (;;)
    {
        uint32_t position = Cy_TCPWM_QuadDec_GetCounter(TCPWM0, MY_QD_NUM);
        (void)position; /* use encoder position */
    }
}
```

**Expected Outcome:** Counter value tracks encoder shaft position; increments on CW rotation, decrements on CCW.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `Cy_TCPWM_*_Init()` returns `CY_TCPWM_BAD_PARAM` | Invalid counter index or NULL config pointer | Verify the counter index is within the group range; check config pointer |
| Counter never starts | Clock not connected | Assign and enable a peripheral clock divider before init |
| PWM output pin stuck | GPIO not configured for TCPWM function | Route the pin using `Cy_GPIO_Pin_Init()` with the correct HSIOM selection |
| QuadDec counts wrong direction | phiA/phiB swapped | Swap the `phiAInput` and `phiBInput` assignments |
| Interrupt never fires | Interrupt mask not set / NVIC not enabled | Call `Cy_TCPWM_SetInterruptMask()` and `NVIC_EnableIRQ()` |
| Dead-time produces glitch | `deadTimeClocks` too small for load | Increase `deadTimeClocks`; verify complementary output polarity |

---

## Related Code Examples

- [PSOC™ Edge MCU: PWM Timer](https://github.com/Infineon/mtb-example-psoc-edge-pwm-timer)
- [PSOC™ Edge MCU: PWM Square Wave](https://github.com/Infineon/mtb-example-psoc-edge-pwm-square-wave)
- [PSOC™ Edge MCU: TCPWM Frequency Measurement](https://github.com/Infineon/mtb-example-psoc-edge-tcpwm-frequency-measurement)
- [PSOC™ Edge MCU: PWM Dual Compare/Capture](https://github.com/Infineon/mtb-example-psoc-edge-pwm-dual-compare-capture)

## Related Application Notes
- Device TRM: *TCPWM Block* chapter

---

## Configuration Parameters Reference

### Timer / Counter (`cy_stc_tcpwm_counter_config_t`)

| Field | Type | Description |
|---|---|---|
| `period` | `uint32_t` | Terminal count value (counter wraps at this value) |
| `clockPrescaler` | `uint32_t` | Input clock prescaler (1x–128x) |
| `runMode` | `uint32_t` | `CONTINUOUS` or `ONE_SHOT` |
| `countDirection` | `uint32_t` | Up / Down / Up-Down |
| `compareOrCapture` | `uint32_t` | Compare mode or Capture mode |
| `compare0` / `compare1` | `uint32_t` | Compare values (compare1 is buffer) |
| `enableCompareSwap` | `bool` | Auto-swap CC0/CC1 on compare event |
| `interruptSources` | `uint32_t` | TC, CC0, CC1 interrupt sources |
| `captureInputMode` / `captureInput` | `uint32_t` | Capture trigger edge/source |
| `trigger0Event` / `trigger1Event` | `uint32_t` | Output trigger event select (V2+) |

### PWM (`cy_stc_tcpwm_pwm_config_t`)

| Field | Type | Description |
|---|---|---|
| `pwmMode` | `uint32_t` | PWM / PWMDT / PWMPR |
| `pwmAlignment` | `uint32_t` | Left / Right / Center / Asymmetric |
| `period0` / `period1` | `uint32_t` | PWM period (period1 is swap buffer) |
| `compare0` / `compare1` | `uint32_t` | Duty cycle compare values |
| `deadTimeClocks` | `uint32_t` | Dead-time insertion (PWMDT) |
| `killMode` | `uint32_t` | Stop / Asynchronous / No kill |
| `invertPWMOut` / `invertPWMOutN` | `uint32_t` | Output polarity |
| `tapsEnabled` | `uint32_t` | PRBS polynomial taps (PWMPR, V2+) |

### Quadrature Decoder (`cy_stc_tcpwm_quaddec_config_t`)

| Field | Type | Description |
|---|---|---|
| `resolution` | `uint32_t` | x1 / x2 / x4 / Up-Down rotary |
| `quadmode` | `uint32_t` | Range0 / Range0-Compare / Range1-Capture / Range1-Compare (V2+) |
| `phiAInput` / `phiBInput` | `uint32_t` | Encoder channel input selection |
| `indexInput` | `uint32_t` | Index / zero-reference input |
| `captureOnIndex` | `uint32_t` | Capture counter value on index event (V2+) |
| `period0` | `uint32_t` | Range limit (V2+) |

---

## Advanced Usage

### Synchronized Multi-Counter Start (V1)
```c
/* Enable counters 1, 5, 6 simultaneously for phase-coherent operation */
#define MASK  ((1UL<<1)|(1UL<<5)|(1UL<<6))
Cy_TCPWM_Enable_Multiple(TCPWM0, MASK);
Cy_TCPWM_TriggerStart(TCPWM0, MASK);
```

### Dynamic Duty Cycle Update
```c
/* Swap compare value on the fly (PWM continues without glitch) */
Cy_TCPWM_PWM_SetCompare0BufVal(TCPWM0, MY_PWM_NUM, newDuty);
/* On next terminal count the buffer value becomes active */
```

### Debug Freeze (V2+)
```c
Cy_TCPWM_SetDebugFreeze(TCPWM0, MY_CNT_NUM, true);
/* Counter halts when tr_dbg_freeze is asserted by the debugger */
```

### High Resolution PWM / HRPWM (TCPWM v3.1)
The HRPWM analog interpolator shifts `line_out` / `line_compl_out` edges in sub-clock steps.
Use the lower `GRP_HRPWM_WIDTH` bits of `compare0` / `period0` for fractional control;
the remaining upper bits drive the digital TCPWM counter.

---

## Industry Standards
Not directly applicable. PWM dead-time generation supports IEC 61800-5-1 functional-safety drive designs when combined with appropriate system-level measures.

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
