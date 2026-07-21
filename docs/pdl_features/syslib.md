# SysLib Driver (PDL) - Core System Utilities for Embedded C Applications

# Overview

The **SysLib driver** provides a collection of essential system-level utilities for Infineon PSOC and AIROC devices, including calibrated software delay functions, register access macros, data manipulation utilities, reset cause interrogation, unique device ID retrieval, critical-section helpers, and fault-frame capture for debugging hard faults. It is a foundational dependency of the PDL itself and is available on all supported CPU cores (CM0+, CM4, CM7, CM33, CM55).

# Features

- **Calibrated software delays** — `Cy_SysLib_Delay()` (ms), `Cy_SysLib_DelayUs()` (µs), `Cy_SysLib_DelayCycles()` (exact CPU cycles) — with CPU-frequency-aware calibration
- **Register access macros** — `CY_GET_REG8/16/32`, `CY_SET_REG8/16/32`, `_FLD2VAL`, `_VAL2FLD`, `_FLD2BOOL`, `_CLR_SET_FLDxxU` for type-safe peripheral register read-modify-write
- **Data manipulation macros** — `CY_LO8/HI8`, `CY_LO16/HI16`, `CY_SWAP_ENDIAN16/32/64` for portable byte-order and bit-field operations
- **Reset cause and silicon ID** — `Cy_SysLib_GetResetReason()`, `Cy_SysLib_GetUniqueId()`, `Cy_SysLib_GetDevice()`, `Cy_SysLib_GetDeviceRevision()` for runtime device identification
- **Fault handler support** — `Cy_SysLib_FaultHandler()`, `cy_stc_fault_frame_t` structure captures CPU registers (R0–R12, LR, PC, PSR, CFSR, HFSR, BFAR, MMFAR) on hard fault
- **Critical section helpers** — `Cy_SysLib_EnterCriticalSection()` / `Cy_SysLib_ExitCriticalSection()` for interrupt-safe atomic operations

# When to Use

- Add precise software delays where a hardware timer is not available or too heavyweight
- Read/write peripheral registers safely with field-extraction macros instead of raw bit shifts
- Determine why the device reset (watchdog, power-on, software, external) at startup
- Retrieve the silicon unique 64-bit ID for device authentication or serial number logging
- Capture the full CPU register state on a hard fault for post-mortem debugging
- Protect a short critical section from interrupts without disabling the NVIC globally

# Prerequisites

## Hardware Requirements

- Any Infineon PSOC or AIROC device with a supported CPU core (M4CPUSS, M33SYSCPUSS, M7CPUSS, or M55APPCPUSS)

## Software Requirements

- PDL available via `#include "cy_pdl.h"`
- The [Infineon Core Library (core-lib)](https://github.com/Infineon/core-lib) — provides `CY_ASSERT`, `CY_ASSERT_L1/L2/L3` macros and `cy_utils.h`; normally included transitively through the PDL

## Read Documentation First

- [Device Technical Reference Manual (TRM)](https://www.infineon.com) — for delay accuracy considerations and wait-state requirements on specific memory types

## Configure in the Tool

The SysLib driver has no Device Configurator personality. It requires no configuration beyond ensuring the correct clock frequency is applied to `cy_delayFreqHz` before calling delay functions. This is typically done by `cybsp_init()` or the system clock initialization code.

# Quick Start

This quick start demonstrates using delay functions, reset cause checking, and a critical section.

**Step 1:** Include `cy_pdl.h`.

**Step 2:** After system clock initialization, call delay functions directly — no additional setup is required.

**Step 3:** Check the reset reason at startup to differentiate power-on-reset from watchdog or software reset.

**Expected Outcome:** Delay functions produce accurate timing; reset reason is printed or stored for diagnostics.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"

int main(void)
{
    __enable_irq();

    /* --- Reset cause diagnostics --- */
    uint32_t resetReason = Cy_SysLib_GetResetReason();
    if (resetReason & CY_SYSLIB_RESET_SOFT)
    {
        /* Software-triggered reset; application-specific handling */
    }
    else if (resetReason & CY_SYSLIB_RESET_HWWDT)
    {
        /* Watchdog reset — possible firmware hang */
    }
    /* Clear the reset reason register for next boot */
    Cy_SysLib_ClearResetReason();

    /* --- Unique silicon ID --- */
    uint64_t uid = Cy_SysLib_GetUniqueId();
    (void)uid; /* Use for serial number, authentication, etc. */

    /* --- Calibrated delay --- */
    Cy_SysLib_Delay(100U);     /* 100 ms */
    Cy_SysLib_DelayUs(50U);    /* 50 µs  */
    Cy_SysLib_DelayCycles(200U); /* exactly 200 CPU cycles */

    /* --- Register field read-modify-write --- */
    /* Example: clear the TX_REQ interrupt bit in SMIF */
    SMIF0->CTL = _CLR_SET_FLD32U(SMIF0->CTL, SMIF_INTR_TR_TX_REQ, 0u);

    /* --- Critical section --- */
    uint32_t intrStatus = Cy_SysLib_EnterCriticalSection();
    /* Atomic multi-step operation here */
    Cy_SysLib_ExitCriticalSection(intrStatus);

    /* --- Data manipulation --- */
    uint32_t regVal = 0xF1F2F3F4u;
    uint16_t lo = (uint16_t)CY_LO16(regVal); /* 0xF3F4 */
    uint16_t hi = (uint16_t)CY_HI16(regVal); /* 0xF1F2 */
    (void)lo; (void)hi;

    for (;;)
    {
        Cy_SysLib_Delay(500U); /* Main loop heartbeat delay */
    }
}
```

### Fault Handler Setup (startup / main.c)

```c
#include "cy_pdl.h"

/* Place this in the hard-fault handler (defined in the linker/startup or overridden here) */
void Cy_SysLib_ProcessingFault(void)
{
    /* cy_faultFrame is populated by Cy_SysLib_FaultHandler() with CPU register state */
    /* Access: cy_faultFrame.r0, .pc, .cfsr.cfsrBits.iaccViol, etc. */
    /* Add breakpoint here or log registers via UART for post-mortem analysis */
    while (1) {} /* Halt */
}
```

### Backup Domain Reset with WCO Trim Preservation

```c
#include "cy_pdl.h"

void reset_backup_domain_safe(void)
{
#if defined(CY_IP_MXS40SRSS)
    uint32_t wcoTrim = Cy_SysLib_GetWcoTrim(); /* Save WCO calibration */
#endif

    if (CY_SYSLIB_SUCCESS != Cy_SysLib_ResetBackupDomain())
    {
        Cy_SysLib_DelayUs(1U);
        if (CY_SYSLIB_SUCCESS != Cy_SysLib_GetResetStatus())
        {
            /* Reset bit not cleared — check CLK_BAK configuration */
        }
    }

#if defined(CY_IP_MXS40SRSS)
    Cy_SysLib_SetWcoTrim(wcoTrim); /* Restore WCO calibration */
#endif
}
```

## Expected Outcome

- `Cy_SysLib_Delay(500)` produces a 500 ms delay accurate to within 1–2% when executed from fast memory (I-Cache or SRAM).
- `Cy_SysLib_GetResetReason()` returns a bitmask of `CY_SYSLIB_RESET_*` flags reflecting the actual reset cause.
- On a hard fault, `cy_faultFrame` captures the full CPU register state for debugger inspection.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Delay is longer or shorter than expected | Wrong clock frequency in `cy_delayFreqHz` | Ensure `SystemCoreClockUpdate()` and BSP clock init (`cybsp_init()`) are called before delay functions |
| Delay inaccurate on CM4/CM0+ without cache | Code executing from slow flash | Place delay functions in SRAM using `CY_SECTION_RAMFUNC_BEGIN` / `CY_SECTION_RAMFUNC_END` |
| `Cy_SysLib_GetUniqueId()` returns 0 | Device or IP not supported | Supported only on devices with a compatible eFuse or RRAMC unique ID path |
| Hard fault handler not capturing registers | `Cy_SysLib_FaultHandler()` not installed | Override the `HardFault_Handler` weak symbol to call `Cy_SysLib_FaultHandler()` |
| `CY_ASSERT()` hangs during debugging | Assert level too strict for hardware variant | Compile with `-D CY_ASSERT_LEVEL=CY_ASSERT_CLASS_1` or `CLASS_2` to relax assertions |
| Critical section is too long | `Cy_SysLib_EnterCriticalSection()` disables all maskable interrupts | Keep critical sections to the minimum necessary; avoid calling any blocking API inside |

# Related Code Examples

- [PSOC™ Edge MCU: Hello World](https://github.com/Infineon/mtb-example-psoc-edge-hello-world)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — System Resources chapter


# Configuration Parameters Reference

SysLib has no Device Configurator personality. Key compile-time options:

| Macro | Description | Default | Options |
|-------|-------------|---------|---------|
| `CY_ASSERT_LEVEL` | Assert class level enabled | `CY_ASSERT_CLASS_3` (all) | `CY_ASSERT_CLASS_1`, `CLASS_2`, `CLASS_3` |
| `CY_ARM_FAULT_DEBUG` | Enable fault frame capture | `CY_ARM_FAULT_DEBUG_ENABLED` (1) | 0 to disable |
| `CY_SYSLIB_DELAY_CALIBRATION_FACTOR` | Delay loop calibration multiplier | `1U` (CM0P/CM33/CM4) | `2U` for CM7 (branch prediction) |

Key API summary:

| Function | Description |
|----------|-------------|
| `Cy_SysLib_Delay(ms)` | Software delay in milliseconds |
| `Cy_SysLib_DelayUs(us)` | Software delay in microseconds |
| `Cy_SysLib_DelayCycles(cycles)` | Exact CPU cycle delay |
| `Cy_SysLib_GetResetReason()` | Returns reset cause bitmask |
| `Cy_SysLib_ClearResetReason()` | Clears reset status register |
| `Cy_SysLib_GetUniqueId()` | Returns 64-bit silicon unique ID |
| `Cy_SysLib_GetDevice()` | Returns device family ID |
| `Cy_SysLib_GetDeviceRevision()` | Returns silicon revision |
| `Cy_SysLib_EnterCriticalSection()` | Disables interrupts, returns saved state |
| `Cy_SysLib_ExitCriticalSection(s)` | Restores interrupt enable state |
| `Cy_SysLib_FaultHandler(faultStackAddr)` | Populates `cy_faultFrame` from fault stack |
| `Cy_SysLib_ResetBackupDomain()` | Resets the backup power domain |
| `Cy_SysLib_ClearFlashCacheAndBuffer()` | Invalidates flash cache (CM4 / M4CPUSS) |

# Advanced Usage and Examples

- **Placement in fast memory**: On devices without I-Cache (CM0+, some CM4 variants), place the delay functions in SRAM with `CY_SECTION_RAMFUNC_BEGIN` / `CY_SECTION_RAMFUNC_END` section attributes to achieve the specified ±1-cycle accuracy. On CM33/CM55/CM7 devices, the I-Cache handles this automatically.
- **Assert levels in production**: set `CY_ASSERT_LEVEL=CY_ASSERT_CLASS_1` (or omit entirely with `NDEBUG`) to strip most assert checks from production builds while keeping critical boundary checks.
- **Fault frame usage**: define `CY_ARM_FAULT_DEBUG=1` (default) and override `Cy_SysLib_ProcessingFault()` to add a UART log before halting; the `cy_faultFrame` global holds the full CPU state from the faulting context.
- **Life Cycle Stage (LCS)**: call `Cy_SysLib_GetDeviceLCS()` to determine the provisioning state (VIRGIN, NORMAL, SECURE, RMA) for conditional behavior in bootloaders.
- **Endianness macros**: `CY_SWAP_ENDIAN16/32/64` use compiler intrinsics where available (`__REV`, `__REV16`) for zero-overhead byte swapping on ARM cores.

# Industry Standards and Compliance

No specific external protocol standards apply to SysLib. The fault-frame structure is compatible with ARM Cortex-M architecture exception frame definitions (ARM DDI0403E).

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
