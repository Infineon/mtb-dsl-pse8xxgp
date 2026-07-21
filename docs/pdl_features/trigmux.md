# TrigMux - Hardware Trigger Routing Between Peripherals

## Overview
The Trigger Multiplexer (TrigMux) driver (`cy_trigmux.h`) provides software control over the `MXSPERI` /
`MXPERI` trigger interconnect, enabling zero-CPU routing of trigger signals between any two peripheral
blocks — for example, connecting a TCPWM overflow to a DMA channel start — and supporting software-initiated
triggers for test and debug purposes.

---

## Features
- **Hardware trigger routing** between any source peripheral and any destination peripheral
- **Single-call connection** for PERI v2+ (`Cy_TrigMux_Connect()`) or two-call for PERI v1
- **One-to-one trigger lines** (`Cy_TrigMux_Select()`) for dedicated point-to-point connections (PERI v2+)
- **Software trigger** (`Cy_TrigMux_SwTrigger()`) — pulse for 2 cycles or hold indefinitely
- **Signal inversion** on any routed trigger
- **Edge or Level** trigger type selection per connection
- **Debug Freeze** support — freeze trigger lines when the debugger halts the CPU
- Constants for all trigger signals are defined in the device configuration header

---

## When to Use
- Connecting TCPWM overflow output → DMA channel trigger (sensor-driven DMA without CPU)
- Routing ADC conversion-complete signal → DMA start or another timer capture
- Chaining peripherals in a signal-processing pipeline entirely in hardware
- Software-triggering a DMA channel during development/test
- Setting up synchronised multi-peripheral start using a single SW trigger

---

## Prerequisites

### Hardware Requirements
- `MXSPERI` or `MXPERI` IP block on the target device
- Source peripheral must have a trigger output; destination peripheral must have a trigger input

### Software Requirements
- `cy_pdl.h` (includes `cy_trigmux.h`)
- Device configuration header (provides `TRIG_IN_MUX_*` / `TRIG_OUT_MUX_*` constants)

### Configure in the Tool (ModusToolbox Device Configurator)
TrigMux connections are typically configured automatically by the Device Configurator when you
enable peripheral features. For manual routing the following parameters are relevant:

| Parameter | Description |
|---|---|
| `inTrig` | Encoded input trigger line (`TRIG_IN_MUX_<grp>_<src>`) |
| `outTrig` | Encoded output trigger line (`TRIG_OUT_MUX_<grp>_<dst>`) |
| `invert` | Invert the signal polarity (`true`/`false`) |
| `trigType` | `TRIGGER_TYPE_EDGE` or `TRIGGER_TYPE_LEVEL` |

---

## Quick Start

### PERI v2+

One call to `Cy_TrigMux_Connect()` links a source peripheral output to a destination peripheral input.

**Step 1.** Identify the trigger group and signal for the source (e.g. TCPWM overflow).
**Step 2.** Identify the matching output trigger for the destination (e.g. DW0 channel 0).
**Step 3.** Call `Cy_TrigMux_Connect()`.

```c
#include "cy_pdl.h"

int main(void)
{
    __enable_irq();

    /* Route TCPWM0 counter-0 overflow → DW0 channel 0 trigger input (PERI v2+) */
    cy_en_trigmux_status_t status =
        Cy_TrigMux_Connect(TRIG_IN_MUX_0_TCPWM0_TR_OVERFLOW0,
                           TRIG_OUT_MUX_0_PDMA0_TR_IN0,
                           false,              /* do not invert */
                           TRIGGER_TYPE_EDGE); /* edge-sensitive */
    if (CY_TRIGMUX_SUCCESS != status)
    {
        /* Handle error */
    }

    /* Optionally observe the trigger on GPIO (for oscilloscope probing) */
    Cy_TrigMux_Connect(TRIG_IN_MUX_4_TCPWM0_TR_OVERFLOW0,
                       TRIG_OUT_MUX_4_HSIOM_TR_IO_OUTPUT0,
                       false, TRIGGER_TYPE_EDGE);
    Cy_GPIO_Pin_FastInit(GPIO_PRT0, 4UL, CY_GPIO_DM_STRONG_IN_OFF, 0UL,
                         P0_4_PERI_TR_IO_OUTPUT0);

    for (;;) { }
}
```

### PERI v1 (two-call flow)

```c
/* Step 1: reduction mux — TCPWM output into intermediate group 11 */
Cy_TrigMux_Connect(TRIG11_IN_TCPWM0_TR_OVERFLOW0,
                   TRIG11_OUT_TR_GROUP0_INPUT9,
                   false, TRIGGER_TYPE_LEVEL);

/* Step 2: distribution mux — intermediate group 11 output to DW0 channel 0 */
Cy_TrigMux_Connect(TRIG0_IN_TR_GROUP11_OUTPUT0,
                   TRIG0_OUT_CPUSS_DW0_TR_IN0,
                   false, TRIGGER_TYPE_EDGE);
```

### Software Trigger

```c
/* Fire a 2-cycle software pulse on DW0 channel 0 (PERI v2+) */
if (CY_TRIGMUX_SUCCESS !=
    Cy_TrigMux_SwTrigger(PERI_0_TRIG_OUT_MUX_0_PDMA0_TR_IN0, CY_TRIGGER_TWO_CYCLES))
{
    /* Handle error */
}
```

### One-to-One Connection (PERI v2+)

```c
/* Dedicated wire: SCB0 TX → DW0 channel 16 */
if (CY_TRIGMUX_SUCCESS !=
    Cy_TrigMux_Select(TRIG_OUT_1TO1_0_SCB0_TX_TO_PDMA0_TR_IN16,
                      false, TRIGGER_TYPE_LEVEL))
{
    /* Handle error */
}

/* Disconnect when done */
Cy_TrigMux_Deselect(TRIG_OUT_1TO1_0_SCB0_TX_TO_PDMA0_TR_IN16);
```

### Expected Outcome
After `Cy_TrigMux_Connect()` succeeds, each time the source peripheral generates its trigger output
the destination peripheral automatically receives the trigger input without any CPU involvement.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `CY_TRIGMUX_BAD_PARAM` | Invalid `inTrig` or `outTrig` constant | Use only constants from the device configuration header matching the target device |
| `CY_TRIGMUX_INVALID_STATE` | SW trigger already active when calling `SwTrigger` | Deactivate with `CY_TRIGGER_DEACTIVATE` before re-triggering |
| Destination peripheral never fires | Wrong trigger group or direction bit | Verify `inTrig` has the input-direction bit clear and `outTrig` has the output-direction bit set |
| Two-call PERI v1 does not work | Intermediate group mismatch | Ensure both calls use a compatible intermediate group (step 3 and 5 in the PERI v1 procedure) |
| Trigger works but direction is reversed | Signal not inverted when expected | Pass `invert = true` to `Cy_TrigMux_Connect()` |
| Debug Freeze not freezing trigger | Connection not configured for freeze | Call `Cy_TrigMux_SetDebugFreeze(outTrig, true)` and route `cti_tr_out` via TrigMux group 7 |

---

## Related Code Examples

- [PSOC™ Edge MCU: UART Transmit and Receive with DMA](https://github.com/Infineon/mtb-example-psoc-edge-uart-transmit-receive-dma)
- [PSOC™ Edge MCU: PWM Timer](https://github.com/Infineon/mtb-example-psoc-edge-pwm-timer)

## Related Application Notes
- Device TRM: *Trigger Multiplexer (TrigMux)* chapter
- Device TRM: *Peripheral Interconnect (PERI)* chapter

---

## Configuration Parameters Reference

### `Cy_TrigMux_Connect()`

| Parameter | Type | Description |
|---|---|---|
| `inTrig` | `uint32_t` | Encoded input trigger signal. Bits[12:8] = trigger group; Bits[7:0] = signal index within group |
| `outTrig` | `uint32_t` | Encoded output trigger signal. Bit[30] = output-direction; Bits[12:8] = group; Bits[7:0] = index |
| `invert` | `bool` | Invert trigger polarity |
| `trigType` | `en_trig_type_t` | `TRIGGER_TYPE_EDGE` or `TRIGGER_TYPE_LEVEL` |

### `Cy_TrigMux_SwTrigger()`

| Parameter | Type | Description |
|---|---|---|
| `trigLine` | `uint32_t` | Trigger line (input or output direction, encoded same as above) |
| `cycles` | `uint32_t` | `CY_TRIGGER_TWO_CYCLES` (2 Clk_Peri cycles, PERI v2) · `CY_TRIGGER_INFINITE` (hold until deactivated) · `CY_TRIGGER_DEACTIVATE` (release infinite trigger) |

### `Cy_TrigMux_Select()` / `Cy_TrigMux_Deselect()` (PERI v2+)

| Parameter | Type | Description |
|---|---|---|
| `outTrig` | `uint32_t` | One-to-one output trigger line constant (`TRIG_OUT_1TO1_*`) |
| `invert` | `bool` | Invert polarity |
| `trigType` | `en_trig_type_t` | Edge or Level |

### `Cy_TrigMux_SetDebugFreeze()` (PERI v2+)

| Parameter | Type | Description |
|---|---|---|
| `outTrig` | `uint32_t` | Output trigger line to freeze |
| `enable` | `bool` | `true` to freeze when `tr_dbg_freeze` is asserted |

### Status Codes

| Code | Meaning |
|---|---|
| `CY_TRIGMUX_SUCCESS` | Operation succeeded |
| `CY_TRIGMUX_BAD_PARAM` | One or more invalid parameters |
| `CY_TRIGMUX_INVALID_STATE` | Trigger already active / not active as expected |

---

## Advanced Usage

### Debug Freeze (PERI v2+)
```c
/* Freeze DW0 ch0 trigger when the debugger halts */
Cy_TrigMux_SetDebugFreeze(TRIG_OUT_MUX_0_PDMA0_TR_IN0, true);

/* Route CTI output 0 → Debug Freeze group */
Cy_TrigMux_Connect(TRIG_IN_MUX_7_CTI_TR_OUT0,
                   TRIG_OUT_MUX_7_DEBUG_FREEZE_TR_IN,
                   false, TRIGGER_TYPE_LEVEL);

/* Activate manually during test */
Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_7_DEBUG_FREEZE_TR_IN, CY_TRIGGER_INFINITE);
/* ... inspect system state ... */
Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_7_DEBUG_FREEZE_TR_IN, CY_TRIGGER_DEACTIVATE);
```

### Dual MXSPERI Instance (devices with two PERI blocks)
Some devices have `CY_IP_MXSPERI_INSTANCES == 2`. Use `PERI_INSTANCE_1_IDENT_Msk` in the
trigger constant to address the second instance. The driver handles routing automatically when the
correct device-header constant is used.

### Complete Peripheral Pipeline
```c
/* TCPWM overflow → DW0 ch0 → (DW0 ch0 output) → GPIO observable */
Cy_TrigMux_Connect(TRIG_IN_MUX_0_TCPWM0_TR_OVERFLOW0,
                   TRIG_OUT_MUX_0_PDMA0_TR_IN0,    /* DMA trigger */
                   false, TRIGGER_TYPE_EDGE);

Cy_TrigMux_Connect(TRIG_IN_MUX_4_PDMA0_TR_OUT0,
                   TRIG_OUT_MUX_4_HSIOM_TR_IO_OUTPUT1, /* GPIO output */
                   false, TRIGGER_TYPE_EDGE);
```

---

## Industry Standards
Not directly applicable. TrigMux is used as an integration component in safety-oriented peripheral
pipelines — refer to device safety manuals for requirements on trigger-path validation
(IEC 61508, ISO 26262).

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
