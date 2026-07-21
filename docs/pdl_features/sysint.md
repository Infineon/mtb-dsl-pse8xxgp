# SysInt Driver (PDL) - Peripheral Interrupt Configuration for Embedded C Applications

# Overview

The **SysInt driver** provides a lightweight API to configure, enable, and manage device peripheral interrupts across all supported CPU cores (CM0+, CM4, CM7, CM33, CM55), complementing the ARM CMSIS NVIC API. It abstracts core- and device-specific interrupt routing differences — including the CM0+ NVIC multiplexer, the CM7 system-to-CPU-IRQ mapping, and software-triggered interrupts — enabling portable interrupt initialization code across all supported Infineon PSOC and AIROC devices.

# Features

- **Single initialization call** (`Cy_SysInt_Init()`) configures interrupt source, priority, and ISR vector in one step
- **RAM vector table support**: ISR addresses can be set or replaced at runtime with `Cy_SysInt_SetVector()` / `Cy_SysInt_GetVector()`
- **NMI source routing**: connect any peripheral interrupt to the Non-Maskable Interrupt via `Cy_SysInt_SetNmiSource()`
- **Software interrupt trigger** (`Cy_SysInt_SoftwareTrig()`): trigger any interrupt from code for testing or IPC
- **CM0+ NVIC multiplexer** management: route any of up to 1023 system interrupts to one of 8–32 NVIC channels on CM0+
- **Multi-core capable**: each core has independent NVIC; the driver works on whichever core compiles the code

# When to Use

- Initialize a peripheral interrupt (UART, GPIO, TCPWM, SAR, etc.) and connect it to a specific ISR
- Dynamically replace an ISR vector at runtime (e.g., reconfigure peripheral in a state machine)
- Route a peripheral interrupt to the NMI for highest-priority fault handling
- Use the CM0+ NVIC multiplexer to share hardware NVIC channels across multiple system interrupt sources
- Trigger interrupts from software for unit testing or inter-core signaling

# Prerequisites

## Hardware Requirements

- Any Infineon PSOC or AIROC device with a supported CPU core (M4CPUSS, M33SYSCPUSS, M7CPUSS, or M55APPCPUSS)
- The interrupt source must be a valid `IRQn_Type` defined in the device header (e.g., `ioss_interrupts_gpio_0_IRQn`)

## Software Requirements

- PDL available via `#include "cy_pdl.h"`
- A **relocated RAM vector table** (`__ramVectors[]`) is required for `Cy_SysInt_SetVector()` to work; the startup code normally sets this up
- CMSIS Core `NVIC_EnableIRQ()` / `NVIC_DisableIRQ()` are used to enable/disable after `Cy_SysInt_Init()`

## Configure in the Tool

The SysInt driver does not have a Device Configurator personality; interrupt sources are configured using the `cy_stc_sysint_t` structure directly in application code, referencing IRQ names from the device header.

# Quick Start

This quick start demonstrates initializing a GPIO port-0 interrupt and routing it to a CPU ISR.

**Step 1:** Include `cy_pdl.h` in your source file.

**Step 2:** Define the `cy_stc_sysint_t` configuration structure with the interrupt source and priority.

**Step 3:** Implement the ISR function.

**Step 4:** Call `Cy_SysInt_Init()` and `NVIC_EnableIRQ()`, then enable global interrupts.

**Expected Outcome:** When pin P0.x toggles (rising edge), the ISR executes, allowing you to clear the interrupt and take action.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"

/* GPIO port 0 ISR */
static void gpio_port0_isr(void)
{
    /* Read and clear all pending interrupts on port 0 */
    uint32_t intrStatus = Cy_GPIO_GetInterruptStatusMasked(GPIO_PRT0);
    Cy_GPIO_ClearInterrupt(GPIO_PRT0, 3U); /* Clear pin 3 interrupt */

    /* User action here */

    /* Final register flush to ensure the write reaches the peripheral */
    (void)Cy_GPIO_GetInterruptStatusMasked(GPIO_PRT0);
}

/* Interrupt configuration structure */
static const cy_stc_sysint_t gpio_irq_cfg =
{
#if defined(CY_IP_M7CPUSS)
    /* CM7: encode system IRQ in bits[15:0], CPU IRQ in bits[31:16] */
    .intrSrc      = (IRQn_Type)((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) |
                                 ioss_interrupts_gpio_dpslp_9_IRQn),
#elif (CY_CPU_CORTEX_M0P) && defined(CY_IP_M4CPUSS)
    /* CM0+: NVIC mux channel + system interrupt source */
    .intrSrc      = NvicMux7_IRQn,
    .cm0pSrc      = ioss_interrupts_gpio_0_IRQn,
#else
    /* CM4 / CM33 / CM55: direct mapping */
    .intrSrc      = ioss_interrupts_gpio_0_IRQn,
#endif
    .intrPriority = 3UL,
};

int main(void)
{
    __enable_irq();

    /* Initialize the interrupt with the ISR address */
    Cy_SysInt_Init(&gpio_irq_cfg, gpio_port0_isr);

    /* Enable the NVIC line */
#if defined(CY_IP_M7CPUSS)
    NVIC_EnableIRQ(NvicMux3_IRQn);
#else
    NVIC_EnableIRQ(gpio_irq_cfg.intrSrc);
#endif

    for (;;)
    {
        /* Application code */
    }
}
```

### NMI Configuration Example

```c
#include "cy_pdl.h"

/* Route the WDT interrupt to NMI for highest-priority watchdog handling */
void configure_wdt_nmi(void)
{
#if (CY_CPU_CORTEX_M0P) || (CY_CPU_CORTEX_M7)
    if (disconnected_IRQn == Cy_SysInt_GetNmiSource(CY_SYSINT_NMI1))
#else
    if (unconnected_IRQn == Cy_SysInt_GetNmiSource(CY_SYSINT_NMI1))
#endif
    {
        Cy_SysInt_SetNmiSource(CY_SYSINT_NMI1, srss_interrupt_wdt_IRQn);
    }
}
```

### Software Interrupt Trigger

```c
#include "cy_pdl.h"

/* Trigger GPIO port 0 interrupt from software (CM4/CM33 only, privileged mode) */
void trigger_gpio_irq_from_sw(void)
{
    /* Must be in privileged mode */
    __set_CONTROL(0);
    Cy_SysInt_SoftwareTrig(ioss_interrupts_gpio_0_IRQn);
    /* Return to user mode if desired */
    __set_CONTROL(1);
}
```

### Replacing an ISR Vector at Runtime

```c
#include "cy_pdl.h"

static void new_isr(void) { /* new handler */ }

void replace_isr_at_runtime(void)
{
    /* Requires RAM vector table to be active */
    Cy_SysInt_SetVector(ioss_interrupts_gpio_0_IRQn, new_isr);
}
```

## Expected Outcome

- The ISR executes each time the configured interrupt source fires.
- Using `Cy_SysLib_Delay()` in an ISR is possible but discouraged; keep ISRs short.
- `Cy_SysInt_GetVector()` should return the address of the ISR function after initialization.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| ISR never executes | `NVIC_EnableIRQ()` not called | Call `NVIC_EnableIRQ(intrSrc)` after `Cy_SysInt_Init()` |
| ISR never executes | Global interrupts disabled | Call `__enable_irq()` in `main()` |
| `Cy_SysInt_SetVector()` has no effect | Vector table still in flash (not relocated to RAM) | Ensure startup code has relocated the vector table; check `SCB->VTOR` |
| CM0+ interrupt fires for wrong source | `cm0pSrc` mismatch or NVIC mux not configured | Verify `intrSrc = NvicMuxN_IRQn` and `cm0pSrc = <system_irq>` match |
| Interrupt fires immediately on enable | Interrupt source already pending | Clear the peripheral interrupt source before calling `NVIC_EnableIRQ()` |
| NMI not triggering | Wrong NMI instance number | Use `CY_SYSINT_NMI1`; check device TRM for available NMI lines |
| Duplicate interrupt executions | ISR clears the wrong register | Write to the peripheral's interrupt-clear register and then **read it back** to flush the write bus buffer |

# Related Code Examples

- [PSOC™ Edge MCU: GPIO Interrupt](https://github.com/Infineon/mtb-example-psoc-edge-gpio-interrupt)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — System Interrupts chapter


# Configuration Parameters Reference

The SysInt driver is configured programmatically via `cy_stc_sysint_t`:

| Field | Description | CM4/CM33/CM55 | CM0+ (M4CPUSS) | CM7 |
|-------|-------------|---------------|-----------------|-----|
| `intrSrc` | NVIC IRQ number (or encoded value) | `ioss_interrupts_gpio_0_IRQn` | `NvicMux7_IRQn` | `(NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) \| sys_irq` |
| `cm0pSrc` | CM0+ system interrupt source (M4CPUSS only) | N/A | `ioss_interrupts_gpio_0_IRQn` | N/A |
| `intrPriority` | NVIC interrupt priority | 0 (highest) – 7 (lowest) | 0–7 | 0–7 |

# Advanced Usage and Examples

- **Deep-Sleep interrupts**: on devices where the CM0+ supports Deep-Sleep capable NVIC channels (`CPUSS_CM0_DPSLP_IRQ_NR`), system interrupts must be connected to `NvicMux0`–`NvicMuxN-1` to be able to wake from Deep Sleep.
- **CPUSS_ver2 shared NVIC channels (CM0+)**: multiple system interrupts can be connected to one NVIC channel. Inside the ISR, call `Cy_SysInt_GetInterruptActive()` to determine which source fired, then service it.
- **Multi-core interrupt management**: each CPU core has its own NVIC. Connecting the same system interrupt to two cores is possible but requires careful priority and masking management to avoid race conditions.
- **Priority considerations**: lower numerical value = higher priority. CMSIS functions `NVIC_SetPriority()` and `NVIC_GetPriority()` can adjust priority after init.
- **ISR write-flush pattern**: always read a peripheral's interrupt status register after writing to clear it; this ensures the write has been flushed through the AHB bus and prevents spurious re-entry.

# Industry Standards and Compliance

The driver complies with the ARM CMSIS-Core NVIC interface convention and is compatible with CMSIS-RTOS interrupt management APIs.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
