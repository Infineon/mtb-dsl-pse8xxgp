# SysFault - Centralized System Fault Capture and Reporting

## Overview
The SysFault driver provides an API to configure the hardware Fault Report Structures, which capture and expose system-wide fault events (memory protection violations, ECC errors, bus faults, peripheral errors) through a single set of MMIO registers without requiring a search across multiple peripheral status registers.

## Features
- **Centralized fault capture**: All fault sources (MPU/SMPU/PPC violations, SRAM/Flash ECC errors, bus timeouts, TCM errors) reported through a single MMIO register set
- **Multiple fault report structures**: Each `FAULT_STRUCT` captures one fault; devices typically provide `FAULT_STRUCT0` and `FAULT_STRUCT1`
- **Configurable fault actions**: Each structure can be configured to trigger a reset, output pulse, interrupt, or a combination
- **Fault source masking**: Enable or disable individual fault sources via mask registers (up to 96 fault IDs across three 32-bit pending sets)
- **Fault data capture**: Up to four 32-bit data registers (DATA0–DATA3) carry fault-specific diagnostic information alongside the fault source index
- **Pending fault visibility**: `Cy_SysFault_GetPendingFault()` exposes all outstanding faults not yet captured by a structure

## When to Use
- **Security hardening**: Detect and respond to SMPU/PPC protection violations in safety and security-aware applications
- **Memory integrity monitoring**: Capture SRAM or Flash ECC single/double-bit errors for production diagnostics
- **System diagnostics during bring-up**: Enable all fault sources and log fault data to identify unexpected bus transactions
- **IEC 60730 / ISO 26262 safety monitors**: Use fault structures as independent hardware observers for safety-critical code paths
- **MCWDT fault detection**: Monitor watchdog counter overflow faults alongside regular MCWDT interrupts

## Prerequisites

### Hardware Requirements
- Device with hardware fault report structures (check device TRM for `FAULT_NR` parameter)
- `FAULT_NR` design-time parameter ≥ 1 (typically 2 structures)

### Software Requirements
- PDL version ≥ 1.30 (`cy_sysfault.h` v1.30)
- `cy_pdl.h` (umbrella include) or `cy_sysfault.h` directly
- `cy_sysint.h` for interrupt configuration

### Configure in the Tool

| Parameter | Description | Typical Value |
|---|---|---|
| `ResetEnable` | Assert system reset on fault capture | `false` (use interrupt instead) |
| `OutputEnable` | Assert output trigger signal on fault capture | `true` |
| `TriggerEnable` | Enable trigger output to Trigger Mux | `false` |
| Fault mask source | Which fault ID to monitor | `CY_SYSFAULT_SRSS_MCWDT0` or device-specific enum |
| IRQ priority | NVIC priority for the fault interrupt | `2` |

## Quick Start

### Step-by-Step
1. Include `cy_pdl.h`.
2. Clear any stale fault status with `Cy_SysFault_ClearStatus()`.
3. Enable the desired fault source with `Cy_SysFault_SetMaskByIdx()`.
4. Set the interrupt mask with `Cy_SysFault_SetInterruptMask()`.
5. Initialize the fault structure with `Cy_SysFault_Init()`.
6. Configure the NVIC using `Cy_SysInt_Init()` and `NVIC_EnableIRQ()`.
7. In the ISR, read the fault source and data, then clear the interrupt.

### Sample Code

```c
#include "cy_pdl.h"

/* Fault ISR */
void FaultInterruptHandler(void)
{
    /* Read interrupt and mask status */
    uint32_t intrSrc    = Cy_SysFault_GetInterruptStatus(FAULT_STRUCT0);
    uint32_t intrMasked = Cy_SysFault_GetInterruptStatusMasked(FAULT_STRUCT0);
    (void)intrSrc;
    (void)intrMasked;

    /* Identify fault source */
    cy_en_SysFault_source_t faultSrc = Cy_SysFault_GetErrorSource(FAULT_STRUCT0);

    if (faultSrc != CY_SYSFAULT_NO_FAULT)
    {
        if (faultSrc == CY_SYSFAULT_SRSS_MCWDT0)
        {
            /* Read auxiliary fault data for MCWDT0 fault */
            uint32_t faultData = Cy_SysFault_GetFaultData(FAULT_STRUCT0, CY_SYSFAULT_DATA0);
            (void)faultData;
        }
    }

    /* Clear interrupt mask and flag */
    Cy_SysFault_ClearInterruptMask(FAULT_STRUCT0);
    Cy_SysFault_ClearInterrupt(FAULT_STRUCT0);
}

int main(void)
{
    __enable_irq();

    /* --- Fault structure configuration --- */
    cy_stc_SysFault_t faultCfg = {
        .ResetEnable   = false,
        .OutputEnable  = true,
        .TriggerEnable = false,
    };

    /* --- Interrupt configuration --- */
    cy_stc_sysint_t irqCfg = {
        .intrSrc = (NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) |
                   cpuss_interrupts_fault_0_IRQn,
        .intrPriority = 2UL
    };

    /* Clear existing status, enable MCWDT0 fault source */
    Cy_SysFault_ClearStatus(FAULT_STRUCT0);
    Cy_SysFault_SetMaskByIdx(FAULT_STRUCT0, CY_SYSFAULT_SRSS_MCWDT0);
    Cy_SysFault_SetInterruptMask(FAULT_STRUCT0);

    /* Initialize fault structure */
    (void)Cy_SysFault_Init(FAULT_STRUCT0, &faultCfg);

    /* Register and enable the fault ISR */
    Cy_SysInt_Init(&irqCfg, FaultInterruptHandler);
    NVIC_SetPriority((IRQn_Type)NvicMux3_IRQn, 2UL);
    NVIC_EnableIRQ((IRQn_Type)NvicMux3_IRQn);

    for (;;) { /* Application loop */ }
}
```

### Expected Outcome
- When an MCWDT0 fault fires, `FaultInterruptHandler` executes, reads the fault source and DATA0 diagnostic register, and clears the interrupt so the system continues running.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| ISR never fires for an expected fault | Fault source mask not enabled | Call `Cy_SysFault_SetMaskByIdx()` for the specific fault ID before init |
| `Cy_SysFault_GetErrorSource()` returns `CY_SYSFAULT_NO_FAULT` | Status register was cleared before reading | Read fault source before calling `ClearInterrupt` or `ClearStatus` |
| Unexpected system reset on fault | `ResetEnable = true` in config | Set `ResetEnable = false` when debugging; use interrupt mode instead |
| Fault DATA registers read as zero | Fault type does not populate DATA registers | Refer to TRM table of fault sources and their associated data fields |
| Second fault not captured | First fault not cleared | Call `Cy_SysFault_ClearStatus()` after processing to allow capture of the next fault |

## Related Code Examples

- [PSOC™ Edge MCU: Hello World](https://github.com/Infineon/mtb-example-psoc-edge-hello-world)

## Related Application Notes
## Configuration Parameters Reference

| Parameter / API | Type | Description |
|---|---|---|
| `Cy_SysFault_Init()` | Function | Writes ResetEnable / OutputEnable / TriggerEnable to the fault structure control register |
| `Cy_SysFault_ClearStatus()` | Function | Clears the captured fault validity bit so a new fault can be recorded |
| `Cy_SysFault_SetMaskByIdx()` | Function | Enables a fault source by its `cy_en_SysFault_source_t` index |
| `Cy_SysFault_GetPendingFault()` | Function | Returns the bitmask of all pending (but not yet captured) faults in a given set (SET0/SET1/SET2) |
| `Cy_SysFault_GetErrorSource()` | Function | Returns the `cy_en_SysFault_source_t` of the currently captured fault |
| `Cy_SysFault_GetFaultData()` | Function | Reads one of the four DATA0–DATA3 diagnostic registers |
| `Cy_SysFault_SetInterruptMask()` | Function | Arms the interrupt output for the fault structure |
| `Cy_SysFault_ClearInterruptMask()` | Function | Disarms the interrupt output |
| `Cy_SysFault_GetInterruptStatus()` | Function | Returns the raw interrupt status bit |
| `Cy_SysFault_GetInterruptStatusMasked()` | Function | Returns interrupt status ANDed with mask (use in ISR) |
| `Cy_SysFault_ClearInterrupt()` | Function | Clears the interrupt pending bit (call at end of ISR) |
| `Cy_SysFault_SetInterrupt()` | Function | Software-triggers a fault interrupt (testing only) |
| `cy_stc_SysFault_t` | Struct | Configuration structure: ResetEnable, OutputEnable, TriggerEnable |
| `cy_en_SysFault_source_t` | Enum | Device-specific fault source IDs (generated from IP headers) |
| `cy_en_SysFault_Set_t` | Enum | SET0 (IDs 0–31), SET1 (IDs 32–63), SET2 (IDs 64–95) |
| `cy_en_SysFault_Data_t` | Enum | DATA0, DATA1, DATA2, DATA3 register selectors |
| `cy_en_SysFault_status_t` | Enum | SUCCESS, BAD_PARAM return codes |

## Advanced Usage

### Monitoring Multiple Fault Sources
Enable more than one fault source before calling `Cy_SysFault_Init()`:

```c
Cy_SysFault_ClearStatus(FAULT_STRUCT0);
Cy_SysFault_SetMaskByIdx(FAULT_STRUCT0, CY_SYSFAULT_SRSS_MCWDT0);
Cy_SysFault_SetMaskByIdx(FAULT_STRUCT0, CY_SYSFAULT_SRSS_MCWDT1);

/* Route SMPU violations to the second structure */
Cy_SysFault_ClearStatus(FAULT_STRUCT1);
Cy_SysFault_SetMaskByIdx(FAULT_STRUCT1, CY_SYSFAULT_PERI_0_PERI_MS0_PPC_VIO);
```

### Reading Pending Faults (Not Yet Captured)
When a fault structure is already occupied, subsequent faults are not captured but remain visible in the pending registers:

```c
/* Check for any pending faults in the first 32 fault IDs */
uint32_t pending = Cy_SysFault_GetPendingFault(FAULT_STRUCT0, CY_SYSFAULT_SET0);
if (pending != 0UL)
{
    /* At least one fault is pending; clear the current fault and re-evaluate */
    Cy_SysFault_ClearStatus(FAULT_STRUCT0);
}
```

## Industry Standards
- IEC 60730 Class B — Software self-test requirements (memory integrity monitoring via ECC fault reporting)
- ISO 26262 ASIL B/D — Fault detection and diagnostic coverage (hardware fault capture mechanisms)

## Copyright
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
