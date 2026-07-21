# Protection (MPC / PPC / MS_CTL) - TrustZone-M Memory and Peripheral Protection for All Devices

## Overview

The **Protection** driver group provides hardware-enforced memory and peripheral access control for supported devices, enabling secure system partitioning compliant with ARM TrustZone-M. It comprises three complementary sub-drivers: the **Memory Protection Controller (MPC)** for per-block secure/non-secure classification of RAM and flash; the **Peripheral Protection Controller (PPC)** for fine-grained peripheral access policies based on protection contexts and privilege levels; and the **Master Security Controller (MS_CTL)** for configuring bus master attributes and switching active protection contexts. Together with the **Secure-Aware PDL (SRF)** layer, these drivers allow transparent non-secure to secure transitions for applications that span TrustZone partitions.

---

## Features

- **Memory Protection Controller (MPC)** — Configures RAM and flash blocks (32 B to 1 MB granularity) as secure or non-secure with per-protection-context (PC) read/write access; supports Root-of-Trust (ROT) lock to freeze security settings after boot
- **Peripheral Protection Controller (PPC)** — Protects up to 1024 peripheral regions with secure/non-secure and privilege-level attributes; supports PC masks; generates bus error or read-zero/write-ignore on violations; lockable per system reset
- **Master Security Controller (MS_CTL)** — Sets security attributes (secure/non-secure, privileged/unprivileged) and PC masks for up to 32 bus masters (CPUs, DMA controllers); provides `Cy_Ms_Ctl_SetActivePC()` to switch the active protection context at runtime
- **Secure-Aware PDL (SRF)** — Automatically routes PDL API calls through the Secure Request Framework when a hardware resource is marked secure and the caller is in non-secure CPU state, making security transparent to application code
- **Classic PROT driver** — MPU (per-master, covers SRAM regions), SMPU (shared memory protection units), and PPU (peripheral protection units) for TrustZone-less devices via `Cy_Prot_*` API
- **Violation responses** — Configurable per MPC/PPC instance: Bus Error (`CY_MPC_BUS_ERR` / `CY_PPC_BUS_ERR`) or Read-Zero/Write-Ignore (`CY_MPC_RZWI` / `CY_PPC_RZWI`)

---

## When to Use

- Partition a TrustZone-M device into secure (bootloader, key storage, attestation) and non-secure (application code, connectivity) worlds during secure boot
- Protect sensitive SRAM regions (cryptographic keys, secure state) from non-secure or unprivileged access using MPC
- Restrict peripheral access (e.g., allow only trusted firmware to access the Cryptolite block) using PPC protection contexts
- Implement multi-context OS security where different RTOS tasks run under different protection contexts set via `Cy_Ms_Ctl_SetActivePC()`
- Build Root-of-Trust (ROT) configurations where security settings are locked after provisioning using `Cy_Mpc_RotLock()` / `Cy_Ppc_Lock()`
- Develop dual-core or multi-application SoC designs requiring memory isolation between CPU subsystems

---

## Prerequisites

### Hardware Requirements

- **Devices with TrustZone-M (MPC/PPC/MS_CTL):** Devices with `CY_IP_M33SYSCPUSS`
- **Devices without TrustZone-M (classic PROT):** Devices with `CY_IP_MXPERI` (MPU/SMPU/PPU via `Cy_Prot_*` API)
- **Clock:** Protection hardware is always powered; no separate clock configuration required
- **Privilege:** MPC ROT struct configuration and PPC lock operations should be performed in secure privileged mode (typically at boot/provisioning time)

### Software Requirements

- PDL ≥ 1.1 for MPC; PDL ≥ 1.10 for PPC; PDL ≥ 1.2 for MS_CTL (all included with ModusToolbox™ 3.x)
- Include `cy_pdl.h`, or individual headers: `cy_mpc.h`, `cy_ppc.h`, `cy_ms_ctl.h`
- For Secure-Aware PDL: `cy_pdl_srf.h`; requires Secure Request Framework (SRF) middleware initialized before `cy_pdl_srf_module_register()`

### Configure in the Tool

Protection configuration is performed both in the Device Configurator and in firmware:

1. Open **Device Configurator** → **Security** tab
2. Assign peripheral regions to secure or non-secure via the PPC configurator
3. Assign memory regions to secure or non-secure contexts via the MPC configurator
4. Save the `.modus` file; Device Configurator generates `cycfg_ppc.h` and `cycfg_mpc.h` with pre-built configuration structures
5. In firmware, reference the generated structures and call the MPC/PPC initialization APIs at system startup (before any application code accesses protected resources)

> **Note:** MPC and PPC settings should be applied before transitioning to non-secure CPU state. Attempt to configure protection from non-secure state will fault.

---

## Quick Start

This quick start demonstrates configuring a SRAM region as non-secure read/write using the MPC driver, and restricting a peripheral to secure access using the PPC driver.

**Step 1:** Include the PDL headers.

**Step 2:** Configure the MPC to mark a SRAM region non-secure.

**Step 3:** Configure the PPC to mark a peripheral as secure-only.

**Step 4:** Add the following code to your secure-side startup (see [Sample Code](#sample-code) for complete example).

**Expected Outcome:** The SRAM region at the specified offset is accessible from non-secure code; the protected peripheral returns a bus error when accessed from non-secure state.

### Sample Code

```c
#include "cy_pdl.h"
#include "cy_mpc.h"
#include "cy_ppc.h"
#include "cy_ms_ctl.h"

int main(void)
{
    cy_en_mpc_status_t  mpcStatus;
    cy_en_ppc_status_t  ppcStatus;
    cy_en_ms_ctl_status_t mscStatus;

    /* --- MPC: Mark 4 KB of SRAM0 as Non-Secure for all protection contexts --- */
    cy_stc_mpc_cfg_t ramCfg = {
        .secure = CY_MPC_NON_SECURE
    };
    /* Configure SRAM0 MPC: offset 0, size 4096 bytes, non-secure */
    mpcStatus = Cy_Mpc_ConfigMpcStruct((MPC_Type*)RAMC0_MPC0,
                                        0U,         /* byte offset within MPC */
                                        4096U,      /* size in bytes */
                                        &ramCfg);
    CY_ASSERT(CY_MPC_SUCCESS == mpcStatus);

    /* --- PPC: Protect a peripheral (UART0) — secure access only --- */
    cy_stc_ppc_attribute_t ppcAttr = {
        .secAttribute  = CY_PPC_SECURE,
        .secPriv       = CY_PPC_PRIV_PLUS_UNPRIV,
        .nsPriv        = CY_PPC_NO_PRIV,           /* no non-secure access */
        .pcMask        = CY_PPC_PC_MASK_0           /* accessible in PC0 only */
    };

    PPC_Type *ppcBase = (PPC_Type*)PERI_MAIN_PPU_PR0; /* device-specific PPC base */
    ppcStatus = Cy_Ppc_SetPpcSecureAttribute(ppcBase,
                                              CY_PPC_DRV_REGION_EXTRACT(PERI_MS_PPU_PR_SCB0),
                                              &ppcAttr);
    CY_ASSERT(CY_PPC_SUCCESS == ppcStatus);

    /* --- MS_CTL: Allow CM33 non-secure to operate under PC0 --- */
    mscStatus = Cy_Ms_Ctl_ConfigBusMaster(CY_MS_CTL_MASTER_CM33_0,
                                           false,  /* non-secure */
                                           false,  /* unprivileged */
                                           CY_MS_CTL_PCMASK0);
    CY_ASSERT(CY_MS_CTL_SUCCESS == mscStatus);

    /* Transition to non-secure world here (e.g., call TZ_StartNS_NS) */
    for (;;) {}
}
```

### Expected Outcome

- The SRAM0 region (first 4 KB) is accessible from non-secure state without a bus error
- Accessing the PPC-protected SCB0 peripheral from non-secure state triggers the configured violation response (bus error by default)
- CM33 non-secure unprivileged code operates under PC0 context

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Bus fault when configuring MPC/PPC | MPC/PPC configuration attempted from non-secure state | Perform all protection setup in the secure firmware image before `TZ_StartNS_NS()` |
| `Cy_Mpc_Lock()` has no effect | Already locked (one-way; clears only on system reset) | Lock is a one-way operation; reset the device to change locked settings |
| Non-secure code causes bus fault on permitted region | Auto-increment not enabled; MPC block index not advancing | Enable auto-increment with `Cy_Mpc_AutoInc(base, 1)` when configuring contiguous blocks |
| PPC protection not taking effect | Wrong PPC instance base address or region number | Cross-reference device TRM for correct PPC base address and region numbers per peripheral |
| `Cy_Ms_Ctl_SetActivePC()` returns error | Target PC not in the allowed PC mask for this master | Call `Cy_Ms_Ctl_ConfigBusMaster()` first to add the target PC to the allowed mask |
| Non-secure app can still access secure peripheral | PPC response configured as RZWI instead of BUS_ERR | Call `Cy_Ppc_SetViolationResponse()` to set `CY_PPC_BUS_ERR` on the PPC instance |
| SRF module not registered | `cy_pdl_srf_module_register()` not called on secure side | Call `cy_pdl_srf_module_register()` during secure boot before switching to non-secure |

---

## Related Code Examples

- [PSOC™ Edge MCU: Basic Secure Application](https://github.com/Infineon/mtb-example-psoc-edge-basic-secure-app)
- [PSOC™ Edge MCU: Secure / Non-Secure GPIO](https://github.com/Infineon/mtb-example-psoc-edge-secure-nonsecure-gpio)

## Related Application Notes

- [ARM TrustZone for ARMv8-M Architecture Reference Manual](https://developer.arm.com/documentation/100690/latest/)

---

## Configuration Parameters Reference

### MPC Parameters

| Parameter / API | Type | Description |
|----------------|------|-------------|
| `cy_en_mpc_sec_attr_t` | enum | `CY_MPC_SECURE` or `CY_MPC_NON_SECURE` — security classification for the memory region |
| `cy_en_mpc_access_attr_t` | enum | `CY_MPC_ACCESS_DISABLED`, `CY_MPC_ACCESS_R`, `CY_MPC_ACCESS_W`, `CY_MPC_ACCESS_RW` — access permissions |
| `cy_en_mpc_prot_context_t` | enum | Protection context selector: `CY_MPC_PC_0` … `CY_MPC_PC_7` |
| `cy_en_mpc_size_t` | enum | Block granularity: 32 B to 1 MB (`CY_MPC_SIZE_32B` … `CY_MPC_SIZE_1MB`) |
| `cy_en_mpc_resp_cfg_t` | enum | `CY_MPC_RZWI` (Read Zero/Write Ignore) or `CY_MPC_BUS_ERR` on access violation |

### MPC Key APIs

| API | Description |
|-----|-------------|
| `Cy_Mpc_ConfigMpcStruct()` | Configure a non-ROT MPC region (security only) |
| `Cy_Mpc_ConfigRotMpcStruct()` | Configure a ROT MPC region (security + per-PC access) |
| `Cy_Mpc_GetBlockAttr()` | Read current non-ROT block security setting |
| `Cy_Mpc_GetRotBlockAttr()` | Read current ROT block security + access setting |
| `Cy_Mpc_Lock()` | Lock non-ROT MPC configuration (one-way until reset) |
| `Cy_Mpc_RotLock()` | Lock ROT MPC configuration (one-way until reset) |
| `Cy_Mpc_AutoInc()` | Enable/disable block ID auto-increment for bulk configuration |
| `Cy_Mpc_SetViolationResponse()` | Set bus error or RZWI response on MPC violation |

### PPC Parameters

| Parameter / API | Type | Description |
|----------------|------|-------------|
| `cy_stc_ppc_attribute_t` | struct | Per-region security, privilege, and PC mask attributes |
| `cy_en_ppc_sec_attr_t` | enum | `CY_PPC_SECURE` or `CY_PPC_NON_SECURE` |
| `cy_en_ppc_priv_attr_t` | enum | `CY_PPC_PRIV_PLUS_UNPRIV` (both allowed) or `CY_PPC_NO_PRIV` (blocked) |
| `cy_stc_ppc_pc_mask_t` | struct | Bitmask of allowed protection contexts for the peripheral region |

### PPC Key APIs

| API | Description |
|-----|-------------|
| `Cy_Ppc_SetPpcSecureAttribute()` | Configure security, privilege, and PC mask for a peripheral region |
| `Cy_Ppc_GetPpcSecureAttribute()` | Read current security attributes for a region |
| `Cy_Ppc_Lock()` | Lock PPC configuration (one-way until reset) |
| `Cy_Ppc_SetViolationResponse()` | Set bus error or RZWI response on PPC violation |

### MS_CTL Key APIs

| API | Description |
|-----|-------------|
| `Cy_Ms_Ctl_ConfigBusMaster()` | Configure security/privilege attributes and allowed PC mask for a bus master |
| `Cy_Ms_Ctl_SetActivePC()` | Switch the active protection context for the calling master |
| `Cy_Ms_Ctl_GetActivePC()` | Query the current protection context of a master |
| `Cy_Ms_Ctl_SetPcHandler()` | Register a handler for PC change events |

---

## Advanced Usage

### ROT (Root-of-Trust) MPC Configuration

ROT MPC structs add per-protection-context read/write access control on top of security classification. Configure them at secure boot time before locking:

```c
cy_stc_mpc_rot_cfg_t rotCfg = {
    .pc     = CY_MPC_PC_1,
    .secure = CY_MPC_SECURE,
    .access = CY_MPC_ACCESS_RW
};
Cy_Mpc_ConfigRotMpcStruct((MPC_Type*)RAMC0_MPC0, 0U, 4096U, &rotCfg);
Cy_Mpc_RotLock((MPC_Type*)RAMC0_MPC0); /* irreversible */
```

### Dynamic Protection Context Switching

Use `Cy_Ms_Ctl_SetActivePC()` to implement task-level isolation in a bare-metal scheduler:

```c
/* Before task A runs in PC1 */
Cy_Ms_Ctl_SetActivePC(CY_MS_CTL_MASTER_CM33_0, CY_MS_CTL_PC1);
run_task_A();

/* Before task B runs in PC2 */
Cy_Ms_Ctl_SetActivePC(CY_MS_CTL_MASTER_CM33_0, CY_MS_CTL_PC2);
run_task_B();
```

### Secure-Aware PDL

On devices with TrustZone-M, many PDL drivers are marked *Secure-Aware*. When their hardware resource is secured in the PPC and the caller is in non-secure CPU state, the PDL automatically submits the operation via the SRF middleware:

```c
/* Secure-side boot: register PDL operations with SRF */
cy_pdl_srf_module_register(&cybsp_secure_context);
```

Non-secure application code then calls the same PDL API without any change; the SRF handles the secure transition transparently.

### Bulk Region Configuration with Auto-Increment

For configuring large contiguous memory areas:

```c
Cy_Mpc_AutoInc((MPC_Type*)RAMC0_MPC0, 1);  /* enable auto-increment */
cy_stc_mpc_cfg_t cfg = { .secure = CY_MPC_NON_SECURE };
for (uint32_t offset = 0U; offset < totalSize; offset += blockSize)
{
    Cy_Mpc_ConfigMpcStruct((MPC_Type*)RAMC0_MPC0, offset, blockSize, &cfg);
}
Cy_Mpc_AutoInc((MPC_Type*)RAMC0_MPC0, 0);  /* disable auto-increment */
```

---

## Industry Standards

| Standard | Description |
|----------|-------------|
| **ARM TrustZone-M** | ARMv8-M security extension — secure/non-secure CPU state partitioning |
| **ARM SAU / IDAU** | Security Attribution Unit / Implementation Defined Attribution Unit (complementary to MPC) |
| **IEC 62443** | Industrial automation cybersecurity — memory isolation recommended for secure partitions |
| **PSA Certified** | Platform Security Architecture — requires hardware memory and peripheral isolation |

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
