# RRAM (Resistive Random Access Memory) - Non-Volatile NVM Storage with OTP and Write Protection

## Overview

The **RRAM driver** provides read/write access to Resistive RAM (RRAM), the non-volatile memory technology that replaces NOR flash and OTP memory on supported devices. The driver supports NVM region read/write, OTP (one-time programmable) area access, sector-level write protection, sleep mode control, and ECC error reporting.

## Features

- **Word- and byte-array NVM read/write:** Write and read arbitrary byte arrays in 16-byte block granularity to the MAIN, Em_EEPROM, Supervisory, and PROTECTED NVM sub-regions
- **OTP (One-Time Programmable) support:** Write-once byte/word/block access to Protected OTP and General OTP sub-regions with irreversible bit-set semantics
- **Hardware ECC:** Built-in ECC detection and correction; single-bit errors corrected silently; uncorrectable errors reported via `cy_en_rram_verr_status_t`
- **Sector-level write protection:** Lock individual NVM sectors against further writes; lock state itself can be permanently committed via `Cy_RRAM_SetProtLock()`
- **Sleep mode control:** `Cy_RRAM_EnableSleep()` / `Cy_RRAM_DisableSleep()` for power optimization during low-activity periods
- **Tearing-safe write APIs:** `Cy_RRAM_TSWriteByteArray()` / `Cy_RRAM_TSReadByteArray()` guarantee data consistency across unexpected power-loss events

## When to Use

- **Application code and data storage:** Store firmware images, configuration blobs, or calibration data in RRAM MAIN region
- **Em_EEPROM emulation:** Use the Em_EEPROM sub-region for high-endurance key-value data (replaces external EEPROM)
- **Provisioning and lifecycle management:** Write device lifecycle state, security keys, and identity data to OTP sub-regions at manufacturing time
- **Secure boot configuration:** Program BootRow and UDS (Unique Device Secret) into Protected OTP for secure boot chain
- **Write-protected firmware partitions:** Lock sectors after programming to prevent field modification

## Prerequisites

### Hardware Requirements

- Device with `CY_IP_MXS22RRAMC` IP block
- For indirect write operations (OTP, PROTECTED regions): PC lock must be acquired; BootRow must be pre-programmed with the PC_LOCK value (see note in `cy_rram.h`)
- Secure access (Cortex-M33 TrustZone secure state) required for Protected OTP and Protected NVM regions

### Software Requirements

- Include `cy_pdl.h` (includes `cy_rram.h` and `cy_v2_rram.h`)

### Configure in the Tool

No Device Configurator personality is required. The RRAM driver operates directly on the RRAMC register base (`RRAMC0`). No external configuration files are generated.

## Quick Start

This quick start demonstrates writing an 8-byte array to the RRAM MAIN NVM region and reading it back.

**Step 1:** Include `cy_pdl.h` in your project.

**Step 2:** Identify the target address — use `CY_RRAM_MAIN_HOST_NS_START_ADDRESS` (non-secure) or `CY_RRAM_MAIN_HOST_S_START_ADDRESS` (secure) as the base.

**Step 3:** Ensure the target region is not write-protected (default state after power-on is unlocked).

**Step 4:** Add the following code to your `main.c`:

**Expected Outcome:** 8 bytes are written and read back successfully, confirming RRAM access is functional.

### Sample Code

#### Bare Metal Example — NVM Write/Read (main.c)

```c
#include "cy_pdl.h"

int main(void)
{
    __enable_irq();

    RRAMC_Type *base  = RRAMC0;
    cy_en_rram_status_t status;

    /* Target address in MAIN NVM region (non-secure) */
    uint32_t addr = CY_RRAM_MAIN_HOST_NS_START_ADDRESS + 0x20UL;

    /* Data to write — must be 16-byte aligned for best performance */
    uint8_t writeData[8U] = {0x01U, 0x02U, 0x03U, 0x04U,
                              0x05U, 0x06U, 0x07U, 0x08U};
    uint8_t readData[8U]  = {0};

    /* Write byte array to NVM */
    status = Cy_RRAM_WriteByteArray(base, addr, writeData, sizeof(writeData));
    if (CY_RRAM_SUCCESS != status) { CY_ASSERT(0); }

    /* Read back */
    status = Cy_RRAM_ReadByteArray(base, addr, readData, sizeof(readData));
    if (CY_RRAM_SUCCESS != status) { CY_ASSERT(0); }

    /* Verify */
    for (uint32_t i = 0; i < sizeof(writeData); i++)
    {
        if (readData[i] != writeData[i]) { CY_ASSERT(0); }
    }

    /* Success */
    for (;;) {}
}
```

#### Sample Code — OTP Write (One-Time Programmable)

```c
#include "cy_pdl.h"

void otp_write_example(void)
{
    #if defined(COMPONENT_SECURE_DEVICE)
    RRAMC_Type *base = RRAMC0;

    /* General OTP start address — secure access required */
#if (CY_IP_MXS22RRAMC_VERSION == 1u)
    uint32_t addr = CY_RRAM_PROTECTED_OTP_PROTECTED_S_START_ADDRESS + 0x10UL;
#else
    uint32_t addr = CY_RRAM_GENERAL_OTP_MMIO_S_START_ADDRESS + 0x10UL;
#endif

    uint32_t data = 0x01020304UL;

    /* OTP write: bits can only be changed from 0→1, never 1→0 */
    cy_en_rram_status_t status = Cy_RRAM_OtpWriteWord(base, addr, data);
    if (CY_RRAM_SUCCESS != status) { CY_ASSERT(0); }
    #endif
}
```

### Expected Outcome

- `Cy_RRAM_WriteByteArray()` returns `CY_RRAM_SUCCESS`
- `Cy_RRAM_ReadByteArray()` returns `CY_RRAM_SUCCESS`
- `readData` matches `writeData` byte-for-byte
- No ECC errors reported

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---------|-------------|------------|
| `CY_RRAM_ACQUIRE_PC_LOCK_FAIL` | PC lock not acquired; BootRow not programmed | Program BootRow with required value before OTP/indirect operations |
| `CY_RRAM_WPLOCK_ENABLED` | Target sector is write-protected | Verify sector protection state with `Cy_RRAM_GetWpLockState()`; protection is permanent once locked |
| `CY_RRAM_ECC_FAIL` | Uncorrectable multi-bit ECC error | RRAM cell may be defective; check `Cy_RRAM_GetVERRStatus()` for details |
| `CY_RRAM_OPERATION_TIME_OUT_ERROR` | RRAM controller busy timeout | Retry; ensure no conflicting RRAM access from another core |
| OTP write silently ignored | Attempting to write `1→0` (not allowed) | OTP is one-way; bits can only transition `0→1` |
| HardFault on Protected OTP read | Access from non-secure context | Use secure alias address (`_S_START_ADDRESS`) or configure MPC appropriately |
| `CY_RRAM_INIT_FAIL` | Config space in extra area corrupt | Likely first power-on; RRAM IP performs forming on first use; retry |

## Related Code Examples

- [PSOC™ Edge MCU: NVM Read/Write](https://github.com/Infineon/mtb-example-psoc-edge-nvm-read-write)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) for RRAMC register descriptions
- Refer to the device datasheet for RRAM endurance, retention, and operating conditions

## Configuration Parameters Reference

No personality configuration is required. All parameters are supplied programmatically. Key runtime parameters:

| Parameter / Function | Description | Valid Values | Notes |
|----------------------|-------------|--------------|-------|
| `base` | Pointer to RRAMC hardware instance | `RRAMC0` | Always `RRAMC0` on current devices |
| `addr` (NVM write/read) | Target byte address | Within region bounds | Use provided `CY_RRAM_*_START_ADDRESS` macros |
| `numBytes` (write/read) | Number of bytes to transfer | > 0, ≤ NVM size | Hardware operates in 16-byte blocks internally |
| `regionSize` (write protect) | Number of 8 KB units to protect | ≤ `CY_RRAM_PROTECTED_LOCK_REGION_LIMIT` | Set before calling `Cy_RRAM_SetProtLock()` |
| `cy_en_rram_vmode_t` | Voltage/frequency mode | `CY_RRAM_VMODE_ULP`, `LP`, `HP` | Must match actual operating conditions |
| `cy_en_rram_temperature_t` | Junction temperature range | `CY_RRAM_TEMP_*` enum | Required for correct write timing calibration |

## Advanced Usage

### Tearing-Safe Write

Use `Cy_RRAM_TSWriteByteArray()` instead of `Cy_RRAM_WriteByteArray()` for data that must remain consistent across unexpected power failures (e.g., configuration sectors). The tearing-safe variant uses a shadow-write protocol at the hardware level.

```c
cy_en_rram_status_t status =
    Cy_RRAM_TSWriteByteArray(base, addr, data, numBytes);
```

### Write Protection Workflow

```c
/* Lock 100 KB of PROTECTED_NVM as lockable */
uint32_t regionSize = 100U; /* in KB / (8 KB per unit) → 12-13 units */
if (Cy_RRAM_GetProtLockState(base) == CY_RRAM_PROTECTED_UNLOCK)
{
    status = Cy_RRAM_SetProtLockableRegion(base, regionSize);
}
if (status == CY_RRAM_SUCCESS)
{
    Cy_RRAM_SetProtLock(base);   /* Permanently commit — IRREVERSIBLE */
}
```

### Sleep Mode

```c
Cy_RRAM_EnableSleep(base);   /* Enter RRAM sleep; reduces leakage current */
/* ... other MCU work ... */
Cy_RRAM_DisableSleep(base);  /* Wake up before next NVM access */
```

### ECC Error Handling

After any read operation, check for AHB errors:

```c
cy_en_rram_hresp_t ahbErr = Cy_RRAM_GetAHBError(base);
if (ahbErr != CY_RRAM_AHB_NO_ERROR)
{
    /* Handle ECC or access violation */
}
```

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
