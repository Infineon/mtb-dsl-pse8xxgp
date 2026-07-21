# LPComp - Ultra-Low-Power Analog Comparator with DeepSleep Wakeup

Provides access to the fixed-function Low-Power Comparator (LPComp) block, enabling fast analog signal comparison of internal and external signals across all system power modes, including Deep Sleep and Hibernate.

# Overview

The **LPComp driver** exposes the two-channel ultra-low-power comparator hardware that can operate in Normal, Low-Power (LP), and Ultra-Low-Power (ULP) modes. It is designed for always-on voltage monitoring, wakeup source generation from Deep Sleep and Hibernate modes, and asynchronous DSI output — all with minimal current consumption.

# Features

- Two independent comparator channels (CHANNEL_0 and CHANNEL_1) with selectable power modes (Normal / LP / ULP)
- Configurable hysteresis (enable/disable) to prevent output chatter on slow-moving signals
- Flexible interrupt trigger: rising edge, falling edge, both edges, or disabled
- Three output modes: pulse (DSI), direct bypass, or synchronised level
- Wakeup source from Deep Sleep and Hibernate power modes via SysPm callback integration
- Input routing to dedicated GPIO pins, AMUXBUSA/AMUXBUSB, or an internal local reference (~0.45 V–0.75 V)

# When to Use

- Detect a voltage threshold crossing (e.g., battery low) without polling in a CPU-intensive loop
- Wake the MCU from Deep Sleep when an analog signal exceeds or drops below a set reference
- Monitor supply rail stability continuously while the system is in a low-power state
- Feed a comparator result asynchronously to the DSI interconnect for hardware-triggered responses
- Implement a simple zero-crossing detector for sinusoidal inputs

# Prerequisites

## Hardware Requirements

- One or two LPCOMP input GPIO pins configured as **High Impedance Analog** drive mode
- Optional output pin configured as **Strong** drive mode
- External reference voltage OR use the built-in local reference
- For AMUXBUS routing, the HSIOM AMUX split switch must be closed

## Software Requirements

- ModusToolbox 3.x with PSOC PDL (`cy_pdl.h`)
- GPIO driver (`cy_gpio`) for pin configuration
- SysPm driver (`cy_syspm`) if low-power callbacks are used

## Configure in the Tool

1. Open the Device Configurator and navigate to the **Peripherals** tab.
2. Enable **LPComp** under the Analog category.
3. Assign GPIO pins to the positive and negative inputs in the **Pins** tab.
4. Configure the personality parameters:

| Parameter | Description | Value for Threshold Wakeup | Value Explanation | Parameter Description |
|---|---|---|---|---|
| Channel | Comparator channel | `CHANNEL_0` | First of two independent channels | Selects CHANNEL_0 or CHANNEL_1 |
| Power Mode | Operating current/speed tradeoff | `CY_LPCOMP_MODE_LP` | Balances speed and current in Deep Sleep | Normal (fastest), LP, or ULP (lowest power) |
| Hysteresis | Prevents chatter on slow signals | `CY_LPCOMP_HYST_ENABLE` | Required if input signal is noisy | Adds ~10 mV hysteresis to the comparator threshold |
| Output Mode | How the result is routed | `CY_LPCOMP_OUT_DIRECT` | Readable via `Cy_LPComp_GetCompare()` | PULSE / DIRECT / SYNC |
| Interrupt Type | Edge triggering | `CY_LPCOMP_INTR_RISING` | Fires when voltage crosses threshold upward | DISABLE / RISING / FALLING / BOTH |
| Positive Input | Source for + terminal | `CY_LPCOMP_SW_GPIO` | External pin | GPIO pin, AMUXBUSA, or AMUXBUSB |
| Negative Input | Source for – terminal | `CY_LPCOMP_SW_LOCAL_VREF` | Built-in ~0.6 V reference | GPIO pin, AMUXBUS, or local VREF |

5. Save the `.modus` file. The configurator generates `cy_stc_lpcomp_config_t` and context structures in `GeneratedSource/`.

# Quick Start

This quick start demonstrates wakeup from Deep Sleep when a GPIO pin voltage rises above the internal local reference.

**Step 1:** Configure the positive input pin as High Impedance Analog via Device Configurator.

**Step 2:** Register the LPComp Deep Sleep callback with SysPm before entering Deep Sleep.

**Step 3:** Enable global interrupts and initialize the NVIC interrupt for the LPComp.

**Step 4:** Add the following code to your `main.c`:

**Expected Outcome:** The device enters Deep Sleep; when the voltage on the positive input pin exceeds the local reference (~0.6 V), the LPComp fires a rising-edge interrupt that wakes the CPU.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

/* LPComp configuration: LP mode, hysteresis on, direct output, rising-edge interrupt */
static const cy_stc_lpcomp_config_t lpcomp_cfg = {
    .outputMode = CY_LPCOMP_OUT_DIRECT,
    .hysteresis  = CY_LPCOMP_HYST_ENABLE,
    .power       = CY_LPCOMP_MODE_LP,
    .intType     = CY_LPCOMP_INTR_RISING,
};

static cy_stc_lpcomp_context_t lpcomp_ctx;

/* SysPm callback parameters */
static cy_stc_syspm_callback_params_t lpcomp_pm_params = { LPCOMP, &lpcomp_ctx };
static cy_stc_syspm_callback_t lpcomp_pm_cb = {
    .callback       = &Cy_LPComp_DeepSleepCallback,
    .type           = CY_SYSPM_DEEPSLEEP,
    .callbackParams = &lpcomp_pm_params,
};

static volatile bool wakeup_flag = false;

void LPComp_ISR(void)
{
    /* Clear the comparator interrupt */
    Cy_LPComp_ClearInterrupt(LPCOMP, CY_LPCOMP_COMP0);
    wakeup_flag = true;
}

int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    __enable_irq();

    /* Initialize LPComp channel 0 */
    Cy_LPComp_Init(LPCOMP, CY_LPCOMP_CHANNEL_0, &lpcomp_cfg, &lpcomp_ctx);

    /* Connect positive input to GPIO, negative to local VREF */
    Cy_LPComp_SetInputs(LPCOMP, CY_LPCOMP_CHANNEL_0,
                        CY_LPCOMP_SW_GPIO,       /* positive: dedicated pin */
                        CY_LPCOMP_SW_LOCAL_VREF);/* negative: ~0.6 V local ref */

    /* Configure and enable the NVIC interrupt */
    static const cy_stc_sysint_t lpcomp_int_cfg = {
        .intrSrc      = lpcomp_interrupt_IRQn,
        .intrPriority = 3U,
    };
    Cy_SysInt_Init(&lpcomp_int_cfg, LPComp_ISR);
    NVIC_EnableIRQ(lpcomp_interrupt_IRQn);

    /* Register the Deep Sleep callback */
    Cy_SysPm_RegisterCallback(&lpcomp_pm_cb);

    /* Enable interrupt mask and power on the comparator */
    Cy_LPComp_SetInterruptMask(LPCOMP, CY_LPCOMP_COMP0);
    Cy_LPComp_Enable(LPCOMP, CY_LPCOMP_CHANNEL_0, &lpcomp_ctx);

    for (;;)
    {
        /* Enter Deep Sleep; the LPComp wakes the device on a rising edge */
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);

        if (wakeup_flag)
        {
            wakeup_flag = false;
            /* Handle the comparator wakeup event here */
        }
    }
}
```

## Expected Outcome

- The MCU enters Deep Sleep after initialization.
- Raise the voltage on the positive input pin above ~0.6 V (local VREF).
- The LPComp fires a rising-edge interrupt; the CPU wakes and `wakeup_flag` is set.
- Lowering the voltage back and raising it again triggers a repeated wakeup.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| No interrupt fires | Interrupt mask not set | Call `Cy_LPComp_SetInterruptMask()` before `Cy_LPComp_Enable()` |
| False triggers / chatter | Noisy input signal with hysteresis disabled | Set `hysteresis = CY_LPCOMP_HYST_ENABLE` |
| Comparator output always low | Pin not in High-Impedance Analog drive mode | Reconfigure the GPIO using `Cy_GPIO_Pin_Init()` with `CY_GPIO_DM_ANALOG` |
| Device does not wake from Deep Sleep | Callback not registered | Call `Cy_SysPm_RegisterCallback(&lpcomp_pm_cb)` before `Cy_SysPm_CpuEnterDeepSleep()` |
| ULP mode: slow response | ULP power mode has ~50 µs startup delay | Use `CY_LPCOMP_MODE_LP` or `CY_LPCOMP_MODE_NORMAL` if fast response is required |

# Related Code Examples

- [PSOC™ Edge MCU: LPComp Hibernate Wakeup](https://github.com/Infineon/mtb-example-psoc-edge-lpcomp-hibernate-wakeup)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — Low-Power Comparator chapter


# Configuration Parameters Reference

| Parameter | API Enum / Field | Values | Description |
|---|---|---|---|
| Power Mode | `cy_en_lpcomp_pwr_t` in `cy_stc_lpcomp_config_t` | `CY_LPCOMP_MODE_NORMAL`, `CY_LPCOMP_MODE_LP`, `CY_LPCOMP_MODE_ULP` | Tradeoff between response speed and current draw. Normal (~400 µA), LP (~10 µA), ULP (~900 nA) |
| Output Mode | `cy_en_lpcomp_out_t` | `CY_LPCOMP_OUT_PULSE`, `CY_LPCOMP_OUT_DIRECT`, `CY_LPCOMP_OUT_SYNC` | Pulse = no bypass DSI output; Direct = bypass to DSI; Sync = level-synchronized to clk |
| Hysteresis | `cy_en_lpcomp_hyst_t` | `CY_LPCOMP_HYST_ENABLE`, `CY_LPCOMP_HYST_DISABLE` | Adds ~10 mV hysteresis band to prevent spurious transitions |
| Interrupt Type | `cy_en_lpcomp_int_t` | `CY_LPCOMP_INTR_DISABLE`, `CY_LPCOMP_INTR_RISING`, `CY_LPCOMP_INTR_FALLING`, `CY_LPCOMP_INTR_BOTH` | Edge selection for the comparator interrupt |
| Positive Input | `Cy_LPComp_SetInputs()` pos arg | `CY_LPCOMP_SW_GPIO`, `CY_LPCOMP_SW_AMUXBUSA`, `CY_LPCOMP_SW_AMUXBUSB` | Signal source for the non-inverting (+) input |
| Negative Input | `Cy_LPComp_SetInputs()` neg arg | `CY_LPCOMP_SW_GPIO`, `CY_LPCOMP_SW_AMUXBUSA`, `CY_LPCOMP_SW_AMUXBUSB`, `CY_LPCOMP_SW_LOCAL_VREF` | Signal source for the inverting (–) input; LOCAL_VREF ≈ 0.45–0.75 V |

# Advanced Usage

- **Hibernate wakeup:** Register `Cy_LPComp_HibernateCallback` with `Cy_SysPm_RegisterCallback()` to use the LPComp as a Hibernate wakeup source. The comparator output is latched in the power-on reset (POR) domain.
- **Both-edges interrupt:** Use `CY_LPCOMP_INTR_BOTH` and read `Cy_LPComp_GetCompare()` inside the ISR to determine the current comparator state.
- **DSI routing:** Set `outputMode = CY_LPCOMP_OUT_PULSE` and connect the DSI output to a trigger mux input to start DMA or TCPWM capture without CPU involvement.
- **Offset trim:** Call `Cy_LPComp_SetTrim()` with the silicon-specific trim values to reduce the input offset voltage.
- **Dual-channel usage:**Both channels share one interrupt vector. In the ISR, call `Cy_LPComp_GetInterruptStatus()` to determine which channel fired before clearing with `Cy_LPComp_ClearInterrupt()`.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
