# Smart I/O Driver (PDL) - Zero-CPU Programmable Logic at the GPIO Port

# Overview

The **Smart I/O driver** configures the programmable logic fabric located between the GPIO pins and the HSIOM mux on select device ports, enabling hardware-level combinatorial and sequential signal manipulation — AND, OR, invert, flip-flop, counter, shift-register — without CPU intervention. It is ideal for glue-logic, signal level-shifting, pulse-width conditioning, and Deep-Sleep-capable I/O processing that must continue when the CPU is powered down.

# Features

- **8 independently configurable Look-Up Tables (LUT)**: each 3-input, 8-output combinatorial or sequential (flip-flop / gated) element
- **1 multi-function Data Unit (DU)**: 8-bit simplified ALU supporting increment/decrement, shift, rotate, and comparison operations for counters and shift registers
- **Flexible routing fabric**: LUT outputs and chip/io terminal signals can be freely cross-connected; each LUT output drives the corresponding io/chip terminal
- **Multiple clock sources**: asynchronous, divided clock (Active / Deep-Sleep / Hibernate), LFCLK, or any io/chip terminal signal — enabling operation in all power modes
- **Deep-Sleep capable operation**: hold-override mode preserves logic states during chip Deep-Sleep; signals must be < 1 MHz
- **Bypass mode per channel**: channels not requiring logic can pass signals through unmodified, minimizing routing complexity

# When to Use

- Invert, combine, or gate signals from one peripheral pin to another without CPU involvement
- Generate hardware Enable/Disable pulses conditioned on multiple digital inputs
- Implement a pulse counter or shift register that runs during Deep-Sleep
- Pre-process high-frequency digital signals (e.g., encoder pulses) before they reach a TCPWM peripheral
- Adapt incompatible signal polarities (active-high vs. active-low) at the GPIO port level
- Reduce firmware complexity by offloading simple combinatorial glue-logic to hardware

# Prerequisites

## Hardware Requirements

- Device with Smart I/O-capable port (check device header `IOSS_SMARTIO_SMARTIO_MASK` for port availability)
- Pins connected to the Smart I/O port must be configured via the GPIO driver first; Smart I/O modifies signals between the pin pad and the HSIOM, not the pin drive mode itself
- For Deep-Sleep operation: all signals in the block (including clock) must be ≤ 1 MHz

## Software Requirements

- PDL available via `#include "cy_pdl.h"`
- Smart I/O Configurator (bundled with ModusToolbox) is strongly recommended for complex LUT configurations; it generates the `cy_stc_smartio_config_t` structure automatically

## Configure in the Tool

1. Open the **Device Configurator** and enable the **Smart I/O** personality on the desired port
2. Click **Launch Smart I/O Configurator** to open the graphical LUT/DU configuration tool
3. Wire io/chip terminal inputs to LUT inputs and configure the truth table for each active LUT
4. Enable **Hold Override** if the block must function during Deep-Sleep
5. Save — generated config structures are added to `GeneratedSource/`
6. In application code call `Cy_SmartIO_Init()` then `Cy_SmartIO_Enable()` using the generated structure

| Parameter | Description | Value for Signal Invert | Value Explanation | Parameter Description |
|-----------|-------------|------------------------|-------------------|----------------------|
| Clock Source (`clkSrc`) | Fabric synchronous-element clock | `CY_SMARTIO_CLK_ASYNC` | No clock needed for purely combinatorial logic | Async, LFCLK, Div-Active/DS/Hib, or io/chip terminal |
| Bypass Mask (`bypassMask`) | Channels that pass through unchanged | `CY_SMARTIO_CHANNEL4 \| ...7` | Only channels 0-3 used; 4-7 bypass | Per-channel bit mask `CY_SMARTIO_CHANNEL0` – `CHANNEL7` |
| LUT Op-code | LUT output mode | `CY_SMARTIO_LUTOPC_COMB` | Pure combinatorial; no flip-flop | COMB / GATED_TR2 / GATED_OUT / ASYNC_SR |
| LUT Mapping (`lutMap`) | 8-bit truth table | `0x01` | Output = 1 only when all three inputs = 0 (invert) | Bit N of lutMap = output for input combination N |
| Hold Override (`hldOvr`) | Preserve state in Deep-Sleep | `false` | Not needed for combinatorial | Set `true` when entering Deep-Sleep with Smart I/O active |

# Quick Start

This quick start demonstrates using Smart I/O to invert a signal from chip terminal 0 (peripheral output) to io terminal 0 (physical pin), so an active-high peripheral signal drives an active-low external component.

**Step 1:** Configure the GPIO pins for the target port using the GPIO driver or Device Configurator.

**Step 2:** In Device Configurator, enable Smart I/O on the same port and launch the Smart I/O Configurator to wire `chip0 → LUT0 (all 3 inputs) → io0` with LUT mapping `0x01` (invert).

**Step 3:** Save to generate the config structure.

**Step 4:** Add the initialization code below (see [Sample Code](#sample-code)).

**Expected Outcome:** A logic HIGH from the peripheral appears as logic LOW on the physical pin, and vice versa — with zero CPU cycles consumed.

## Sample Code

**Configuration:** Uses structures generated by the Device Configurator / Smart I/O Configurator. Do not write `cy_stc_smartio_config_t` manually for complex designs.

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"

/* LUT0 configuration: all three inputs sourced from io1 terminal,
   truth table = 0x01 (output=1 only when all inputs=0, i.e., invert) */
static const cy_stc_smartio_lutcfg_t lut0_cfg =
{
    .tr0    = CY_SMARTIO_LUTTR_IO1,
    .tr1    = CY_SMARTIO_LUTTR_IO1,
    .tr2    = CY_SMARTIO_LUTTR_IO1,
    .opcode = CY_SMARTIO_LUTOPC_COMB,
    .lutMap = 0x01u,    /* Output = 1 when all inputs are 0 (invert) */
};

/* Smart I/O port 8 configuration */
static const cy_stc_smartio_config_t smartio_cfg =
{
    .clkSrc     = CY_SMARTIO_CLK_ASYNC,
    /* Bypass channels 4-7; channels 0-3 go through LUTs */
    .bypassMask = CY_SMARTIO_CHANNEL4 | CY_SMARTIO_CHANNEL5 |
                  CY_SMARTIO_CHANNEL6 | CY_SMARTIO_CHANNEL7,
    .ioSyncEn   = 0u,
    .chipSyncEn = 0u,
    .lutCfg0    = &lut0_cfg,
    .lutCfg1    = NULL,
    .lutCfg2    = NULL,
    .lutCfg3    = NULL,
    .lutCfg4    = NULL,
    .lutCfg5    = NULL,
    .lutCfg6    = NULL,
    .lutCfg7    = NULL,
    .duCfg      = NULL,
    .hldOvr     = false,
};

int main(void)
{
    __enable_irq();

    /* Initialize Smart I/O on port 8 */
    if (CY_SMARTIO_SUCCESS != Cy_SmartIO_Init(SMARTIO_PRT8, &smartio_cfg))
    {
        CY_ASSERT(0); /* Init failed */
    }

    /* Enable the Smart I/O block */
    Cy_SmartIO_Enable(SMARTIO_PRT8);

    for (;;)
    {
        /* Smart I/O performs signal inversion autonomously;
           the CPU can enter low-power modes here */
    }
}
```

### Reconfiguring Clock Source at Runtime

```c
#include "cy_pdl.h"

void switch_to_lfclk(void)
{
    /* Must disable before reconfiguring clock source */
    Cy_SmartIO_Disable(SMARTIO_PRT8);
    Cy_SmartIO_SetClock(SMARTIO_PRT8, CY_SMARTIO_CLK_LFCLK);
    Cy_SmartIO_Enable(SMARTIO_PRT8);
}
```

### Enabling Deep-Sleep Hold Override

```c
#include "cy_pdl.h"

void enter_deepsleep_prep(void)
{
    /* Enable hold-override so Smart I/O maintains state in Deep-Sleep */
    Cy_SmartIO_Disable(SMARTIO_PRT8);
    Cy_SmartIO_HoldOverride(SMARTIO_PRT8, true);
    Cy_SmartIO_Enable(SMARTIO_PRT8);

    /* Now safe to call Cy_SysPm_CpuEnterDeepSleep() */
}

void exit_deepsleep(void)
{
    /* Disable hold-override after waking */
    Cy_SmartIO_Disable(SMARTIO_PRT8);
    Cy_SmartIO_HoldOverride(SMARTIO_PRT8, false);
    Cy_SmartIO_Enable(SMARTIO_PRT8);
}
```

## Expected Outcome

- Signal inversion is instantaneous and continuous — no CPU polling required.
- If a logic analyzer is connected: the io0 signal is the logical inverse of the chip0 signal with propagation delay only.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Smart I/O output is always 0 | LUT truth table incorrect | Remember LUT mapping bit N is output for input combination `{TR2,TR1,TR0} = N`; check your truth table |
| Block not operational after init | `Cy_SmartIO_Enable()` not called | Always call Enable after Init |
| Oscillations on output | Combinatorial feedback loop (LUT chaining without flip-flop) | Insert a sequential LUT (GATED_OUT mode) to break the feedback |
| Sequential LUT output is wrong | Signal not synchronized to fabric clock | Enable io/chip terminal synchronizer with `Cy_SmartIO_SetIoSync()` / `Cy_SmartIO_SetChipSync()` |
| Smart I/O doesn't work in Deep-Sleep | Hold override not enabled / clock too fast | Call `Cy_SmartIO_HoldOverride(port, true)` before deep sleep; ensure all signals ≤ 1 MHz |
| Reconfiguring LUT has no effect | Block was not disabled before reconfiguration | Always call `Cy_SmartIO_Disable()` before any `Set*` API call; re-enable after |
| Single-input LUT gives wrong output | Only one input connected but other two are floating | All three LUT inputs must be driven; tie unused inputs to the same signal as the active input |

# Related Code Examples

- [PSOC™ Edge MCU: Smart I/O SGPIO Target](https://github.com/Infineon/mtb-example-psoc-edge-smartio-sgpio-target)
- [PSOC™ Edge MCU: Smart I/O I2S](https://github.com/Infineon/mtb-example-psoc-edge-smartio-i2s)
- [PSOC™ Edge MCU: Smart I/O Ramping LED](https://github.com/Infineon/mtb-example-psoc-edge-smartio-ramping-led)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — Smart I/O chapter


# Configuration Parameters Reference

| Parameter | Description | Values |
|-----------|-------------|--------|
| `clkSrc` | Clock source for sequential elements | `CY_SMARTIO_CLK_ASYNC`, `CY_SMARTIO_CLK_LFCLK`, `CY_SMARTIO_CLK_DIVACT`, `CY_SMARTIO_CLK_DIVDS`, `CY_SMARTIO_CLK_DIVHIB`, `CY_SMARTIO_CLK_IO0`–`IO7`, `CY_SMARTIO_CLK_CHIP0`–`CHIP7` |
| `bypassMask` | Bitmask of channels to bypass | OR of `CY_SMARTIO_CHANNEL0` … `CY_SMARTIO_CHANNEL7` |
| `ioSyncEn` | Enable input synchronizer per io terminal | Bitmask; 1 = synchronize terminal to fabric clock |
| `chipSyncEn` | Enable input synchronizer per chip terminal | Bitmask |
| `hldOvr` | Hold override for Deep-Sleep | `true` / `false` |
| `lutCfg0`–`lutCfg7` | LUT configuration pointer (NULL = disable LUT) | Pointer to `cy_stc_smartio_lutcfg_t` |
| LUT `.opcode` | LUT operating mode | `CY_SMARTIO_LUTOPC_COMB`, `GATED_TR2`, `GATED_OUT`, `ASYNC_SR` |
| LUT `.lutMap` | 8-bit truth table | 0x00–0xFF; bit N = output for input `{tr2,tr1,tr0}=N` |
| LUT `.tr0/tr1/tr2` | Input signal source for each LUT input | `CY_SMARTIO_LUTTR_IO0`–`IO7`, `CHIP0`–`CHIP7`, `LUT0_OUT`–`LUT7_OUT`, `DU_OUT`, `INVALID` |
| `duCfg` | Data Unit configuration pointer | Pointer to `cy_stc_smartio_ducfg_t` or NULL |

# Advanced Usage and Examples

- **Multi-LUT chaining**: connect LUT output `CY_SMARTIO_LUTTR_LUT1_OUT` as an input to LUT2 to build multi-level logic functions (up to 3 stages in LUTs 0-3, another 3 stages in LUTs 4-7).
- **Data Unit counter**: use `CY_SMARTIO_DUOPC_INCR_WRAP` with `CY_SMARTIO_DUSIZE_8` to implement a hardware 8-bit wrapping counter clocked by a LUT output — no CPU timer needed.
- **Shift register**: wire DU with `CY_SMARTIO_DUOPC_ROTR` or `ROTL` and clock from an io terminal to implement serial-to-parallel conversion at the port.
- **LUTs 0-3 / 4-7 fabric split**: LUTs 0–3 accept io/chip signals from channels 0–3 only; LUTs 4–7 accept from channels 4–7 only. Cross-half routing is only possible via LUT outputs.
- **Avoiding race conditions with clock-sourced io/chip terminals**: if an io or chip terminal is selected as the clock source for the fabric, that same terminal **cannot** also be used as a LUT input — a race condition will result.

# Industry Standards and Compliance

No specific external standards apply to this internal logic fabric component.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
