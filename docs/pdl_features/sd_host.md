# SD Host (SDHC) - SD/eMMC/SDIO Card Interface with DMA Support for Embedded C Applications

## Overview

The **SD Host driver** (`cy_sd_host.h`) provides a complete software interface to the MXSDHC hardware block, enabling communication with SD memory cards (SDSC/SDHC/SDXC), eMMC embedded flash storage, and SDIO peripheral devices. The driver offers both a high-level API for simple card read/write/erase operations and a low-level API for direct register access and custom command sequences. DMA modes (SDMA, ADMA2, ADMA3) offload data transfers from the CPU, and the driver is RTOS-friendly — high-level functions start a transaction and return immediately so FreeRTOS delays can be inserted between transfer start and completion checks.

## Features

- **Multi-card type support:** SD (SDSC/SDHC/SDXC), eMMC (1-bit/4-bit/8-bit), SDIO, and Combo (SD+SDIO) cards — all detected and initialized automatically by `Cy_SD_Host_InitCard()`
- **Multiple DMA modes:** CPU (polling), SDMA, ADMA2, and ADMA3 for flexible throughput vs. latency tradeoffs; configurable block size (1–65 535 bytes)
- **Speed modes:** Default Speed (DS), High Speed (HS), SDR12, SDR25, SDR50, DDR50 for SD; Legacy and High Speed SDR for eMMC; UHS-I for SD
- **RTOS-friendly design:** High-level `Cy_SD_Host_Read()` / `Cy_SD_Host_Write()` start non-blocking transfers; completion detected via interrupt or flag polling
- **Card presence detection:** Card insert and removal events via `Cy_SD_Host_GetNormalInterruptStatus()` with `CY_SD_HOST_CARD_INSERTION` / `CY_SD_HOST_CARD_REMOVAL` flags
- **CRC and timeout protection:** Hardware CRC checking on command and data packets; configurable packet timeouts

## When to Use

- **Removable SD card storage:** File system storage for data logging, audio recording, firmware images, or configuration files
- **eMMC boot and storage:** Embedded non-removable storage for industrial and automotive applications with 8-bit bus and high-speed modes
- **SDIO peripheral expansion:** Attach Wi-Fi (e.g., CYW43xx), BT, or GPS modules via the SDIO interface
- **Firmware update (FOTA):** Buffer large firmware images on an SD card before programming internal flash/RRAM
- **Data acquisition:** High-throughput sensor data logging to SD card using ADMA2/ADMA3 DMA without CPU involvement

## Prerequisites

### Hardware Requirements

- Device with `CY_IP_MXSDHC` IP block
- Dedicated SDHC I/O pins connected to the SD/eMMC card (CLK, CMD, DAT0–DAT3 for 4-bit; DAT4–DAT7 for eMMC 8-bit mode)
- Optional card control pins: `CARD_DETECT_N`, `CARD_MECH_WRITE_PROT`, `IO_VOLT_SEL`, `CARD_IF_PWR_EN`, `LED_CTRL`, `CARD_EMMC_RESET_N`
- CLK_HF clock source configured at **100 MHz** (typical) for SDHC block
- SD card connector with card detect and write-protect switches (for removable SD cards)

### Software Requirements

- `cy_pdl.h` (includes `cy_sd_host.h`)
- GPIO driver (`cy_gpio.h`) for pin configuration
- SysClk driver (`cy_sysclk.h`) for clock configuration

### Configure in the Tool

Configure SDHC pins in the Device Configurator → SDHC peripheral:

| Parameter | Description | Typical Value | Value Explanation | Parameter Description |
|-----------|-------------|---------------|-------------------|-----------------------|
| DMA type (`dmaType`) | DMA engine selection | `CY_SD_HOST_DMA_ADMA2` | ADMA2 supports scatter-gather; best general-purpose choice | `CY_SD_HOST_DMA_SDMA`, `ADMA2`, `ADMA3`, or CPU mode |
| eMMC mode (`emmc`) | Enable eMMC (vs. SD) mode | `false` for SD, `true` for eMMC | eMMC uses different initialization and DAT4-DAT7 pins | Bool in `cy_stc_sd_host_init_config_t` |
| LED control (`enableLedControl`) | Drive card-activity LED via SDHC | `false` (typically) | Set true if LED connected to SDHC LED_CTRL pin | Bool in `cy_stc_sd_host_init_config_t` |
| CLK_HF source | Clock for SDHC block | CLK_HF at 100 MHz | SDHC prescales internally; 100 MHz gives full speed range | Configure via `Cy_SysClk_ClkHfSetSource()` |
| Pin drive mode | GPIO drive mode for all SDHC pins | `CY_GPIO_DM_STRONG` | Required: strong drive, input buffer enabled | Set via `Cy_GPIO_SetDrivemode()` |

## Quick Start

This quick start demonstrates initializing the SDHC block, initializing an SD card, and performing a DMA-based write/read operation using ADMA2.

**Step 1:** Configure SDHC pins (CLK, CMD, DAT0–DAT3) with HSIOM and strong drive mode.

**Step 2:** Configure and enable the 100 MHz CLK_HF clock for the SDHC block.

**Step 3:** Initialize the SD Host driver and initialize the SD card.

**Step 4:** Add the following code to `main.c`:

**Expected Outcome:** Write and read operations complete without error; read data matches write data.

### Sample Code

#### Bare Metal Example — SD Card Write/Read with ADMA2 (main.c)

```c
#include <string.h>
#include "cy_pdl.h"

/* Allocate SD Host context */
cy_stc_sd_host_context_t sdHostContext;

#define BLOCK_SIZE   (512U)
#define BLOCK_ADDR   (0U)   /* First block on card */

/* DMA transfer buffers — must be cache-aligned on CM7 */
CY_ALIGN(32) static uint8_t writeBuf[BLOCK_SIZE];
CY_ALIGN(32) static uint8_t readBuf[BLOCK_SIZE];

static void configure_pins(void)
{
    /* Connect SDHC function to dedicated pins */
    Cy_GPIO_SetHSIOM(P12_0_PORT, P12_0_NUM, P12_0_SDHC1_CARD_EMMC_RESET_N);
    Cy_GPIO_SetHSIOM(P12_1_PORT, P12_1_NUM, P12_1_SDHC1_CARD_DETECT_N);
    Cy_GPIO_SetHSIOM(P12_4_PORT, P12_4_NUM, P12_4_SDHC1_CARD_CMD);
    Cy_GPIO_SetHSIOM(P12_5_PORT, P12_5_NUM, P12_5_SDHC1_CLK_CARD);
    Cy_GPIO_SetHSIOM(P13_0_PORT, P13_0_NUM, P13_0_SDHC1_CARD_DAT_3TO00);
    Cy_GPIO_SetHSIOM(P13_1_PORT, P13_1_NUM, P13_1_SDHC1_CARD_DAT_3TO01);
    Cy_GPIO_SetHSIOM(P13_2_PORT, P13_2_NUM, P13_2_SDHC1_CARD_DAT_3TO02);
    Cy_GPIO_SetHSIOM(P13_3_PORT, P13_3_NUM, P13_3_SDHC1_CARD_DAT_3TO03);

    /* Set all SDHC pins to strong drive with input buffer */
    Cy_GPIO_SetDrivemode(P12_4_PORT, P12_4_NUM, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetDrivemode(P12_5_PORT, P12_5_NUM, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetDrivemode(P13_0_PORT, P13_0_NUM, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetDrivemode(P13_1_PORT, P13_1_NUM, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetDrivemode(P13_2_PORT, P13_2_NUM, CY_GPIO_DM_STRONG);
    Cy_GPIO_SetDrivemode(P13_3_PORT, P13_3_NUM, CY_GPIO_DM_STRONG);
}

static void configure_clock(void)
{
    /* Configure CLK_HF2 at 100 MHz for SDHC1 */
    Cy_SysClk_ClkHfSetSource(2u, CY_SYSCLK_CLKHF_IN_CLKPATH0);
    Cy_SysClk_ClkHfSetDivider(2u, CY_SYSCLK_CLKHF_NO_DIVIDE);
    Cy_SysClk_ClkHfEnable(2u);
}

int main(void)
{
    __enable_irq();

    configure_pins();
    configure_clock();

    /* Initialize SD Host */
    const cy_stc_sd_host_init_config_t sdHostCfg =
    {
        .dmaType          = CY_SD_HOST_DMA_ADMA2,
        .enableLedControl = false,
        .emmc             = false,  /* SD card mode */
    };
    cy_en_sd_host_status_t status =
        Cy_SD_Host_Init(SDHC1, &sdHostCfg, &sdHostContext);
    if (CY_SD_HOST_SUCCESS != status) { CY_ASSERT(0); }

    /* Enable SDHC block */
    Cy_SD_Host_Enable(SDHC1);

    /* Initialize SD card (auto-detects SD / SDIO / Combo) */
    cy_stc_sd_host_sd_card_config_t cardCfg =
    {
        .lowVoltageSignaling = false,
        .isEmmc              = false,
        .busWidth            = CY_SD_HOST_BUS_WIDTH_4_BIT,
        .cardType            = NULL,
        .rca                 = NULL,
        .cardCapacity        = NULL,
    };
    status = Cy_SD_Host_InitCard(SDHC1, &cardCfg, &sdHostContext);
    if (CY_SD_HOST_SUCCESS != status) { CY_ASSERT(0); }

    /* Prepare write buffer */
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) { writeBuf[i] = (uint8_t)i; }

    /* --- ADMA2 Write --- */
    cy_stc_sd_host_write_read_config_t wrCfg =
    {
        .data              = writeBuf,
        .address           = BLOCK_ADDR,
        .numberOfBlocks    = 1U,
        .autoCommand       = CY_SD_HOST_AUTO_CMD_AUTO,
        .dataTimeout       = 0x0EU,
        .enReliableWrite   = false,
        .enableDma         = true,
    };
    status = Cy_SD_Host_Write(SDHC1, &wrCfg, &sdHostContext);
    if (CY_SD_HOST_SUCCESS != status) { CY_ASSERT(0); }

    /* Wait for write complete */
    uint32_t normalInt;
    do {
        normalInt = Cy_SD_Host_GetNormalInterruptStatus(SDHC1);
    } while (0U == (normalInt & CY_SD_HOST_XFER_COMPLETE));
    Cy_SD_Host_ClearNormalInterruptStatus(SDHC1, CY_SD_HOST_XFER_COMPLETE);

    /* --- ADMA2 Read --- */
    cy_stc_sd_host_write_read_config_t rdCfg =
    {
        .data           = readBuf,
        .address        = BLOCK_ADDR,
        .numberOfBlocks = 1U,
        .autoCommand    = CY_SD_HOST_AUTO_CMD_AUTO,
        .dataTimeout    = 0x0EU,
        .enableDma      = true,
    };
    status = Cy_SD_Host_Read(SDHC1, &rdCfg, &sdHostContext);
    if (CY_SD_HOST_SUCCESS != status) { CY_ASSERT(0); }

    do {
        normalInt = Cy_SD_Host_GetNormalInterruptStatus(SDHC1);
    } while (0U == (normalInt & CY_SD_HOST_XFER_COMPLETE));
    Cy_SD_Host_ClearNormalInterruptStatus(SDHC1, CY_SD_HOST_XFER_COMPLETE);

    /* Verify */
    if (0 != memcmp(writeBuf, readBuf, BLOCK_SIZE)) { CY_ASSERT(0); }

    /* Success */
    for (;;) {}
}
```

### Expected Outcome

- `Cy_SD_Host_Init()` and `Cy_SD_Host_InitCard()` return `CY_SD_HOST_SUCCESS`
- Write and read transfer-complete interrupts fire
- `readBuf` content matches `writeBuf` (0x00, 0x01, … 0xFF, repeating)
- No assertion fires

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---------|-------------|------------|
| `Cy_SD_Host_InitCard()` returns error | No card inserted / incorrect pin configuration | Verify card is present; check HSIOM assignments and GPIO drive modes |
| Transfer complete flag never set | Clock not running / interrupt not cleared | Verify CLK_HF at 100 MHz; ensure previous interrupt status is cleared before new transfer |
| Data compare failure | Cache coherency issue (CM7) | Invalidate/clean data cache before DMA read; use `CY_ALIGN(32)` buffers |
| Card detect interrupt fires repeatedly | DAT pins left floating when no card present | Set DAT pins to `CY_GPIO_DM_HIGHZ` on card removal; restore to `STRONG` on insertion |
| eMMC initialization fails | `emmc` flag not set in `cy_stc_sd_host_init_config_t` | Set `.emmc = true`; ensure DAT4–DAT7 configured for 8-bit mode if needed |
| SDIO card not recognized | Card type detection order | `Cy_SD_Host_InitCard()` auto-detects; check SDIO function enable sequence after init |
| Bus stuck after error | Command or data timeout | Clear error and normal interrupt status; call `Cy_SD_Host_InitCard()` again |
| Low throughput | Using CPU DMA mode (polling) | Switch to `CY_SD_HOST_DMA_ADMA2` or `ADMA3` for DMA-driven transfers |

## Related Code Examples

- [PSOC™ Edge MCU: Filesystem with LittleFS (FreeRTOS)](https://github.com/Infineon/mtb-example-psoc-edge-filesystem-littlefs-freertos)
- [PSOC™ Edge MCU: Filesystem with emFile (FreeRTOS)](https://github.com/Infineon/mtb-example-psoc-edge-filesystem-emfile-freertos)

## Related Application Notes

- [SD Specifications Part 1: Physical Layer Simplified Specification v6.0](https://www.sdcard.org/downloads/pls/) — SD bus protocol
- [JEDEC JESD84 — eMMC Standard v5.1](https://www.jedec.org/standards-documents/docs/jesd84-b51) — eMMC bus specification
- Refer to the device TRM for MXSDHC register descriptions and timing requirements

## Configuration Parameters Reference

| Parameter / Field | Description | Valid Values | Notes |
|-------------------|-------------|--------------|-------|
| `cy_stc_sd_host_init_config_t.dmaType` | DMA engine to use | `CY_SD_HOST_DMA_SDMA`, `ADMA2`, `ADMA3`, CPU | ADMA2/3 preferred for performance; CPU for simple/debug |
| `cy_stc_sd_host_init_config_t.emmc` | Enable eMMC mode | `true` / `false` | Must match attached card type |
| `cy_stc_sd_host_init_config_t.enableLedControl` | Drive LED via SDHC | `true` / `false` | Hardware-controlled activity indicator |
| `cy_stc_sd_host_sd_card_config_t.busWidth` | SD/eMMC bus width | `CY_SD_HOST_BUS_WIDTH_1_BIT`, `4_BIT`, `8_BIT` | 8-bit for eMMC only; 4-bit for SD |
| `cy_stc_sd_host_sd_card_config_t.lowVoltageSignaling` | Use 1.8 V I/O signaling | `true` / `false` | Required for UHS-I (SDR50, DDR50) |
| `cy_stc_sd_host_write_read_config_t.numberOfBlocks` | Transfer block count | 1–65 535 | Each block = 512 bytes (SD/eMMC standard) |
| `cy_stc_sd_host_write_read_config_t.autoCommand` | Auto-CMD12/23 selection | `CY_SD_HOST_AUTO_CMD_NONE`, `AUTO_CMD12`, `AUTO_CMD23`, `AUTO` | `AUTO` recommended; issues stop-transmission automatically |
| `cy_stc_sd_host_write_read_config_t.dataTimeout` | Data timeout counter | 0x00–0x0E | `0x0E` is maximum (~1 second); increase for slow cards |

## Advanced Usage

### ADMA3 Multi-Block Transfer

ADMA3 chains multiple descriptor entries for scatter-gather DMA without CPU intervention between blocks:

```c
/* Initialize with ADMA3 descriptor table */
cy_stc_sd_host_write_read_config_t wrCfg = {
    .data           = (uint8_t *)adma3DescTable,  /* ADMA3 descriptor table */
    .numberOfBlocks = totalBlocks,
    .enableDma      = true,
};
Cy_SD_Host_InitDataTransfer(SDHC1, &wrCfg);
Cy_SD_Host_SendCommand(SDHC1, &cmdConfig);
```

### Deep Sleep / Low Power

SD Host does not operate in Hibernate or Deep Sleep but can resume read/write after waking from Deep Sleep:

```c
/* Before Deep Sleep */
Cy_SD_Host_DisableSdClk(SDHC1);

/* After wakeup */
Cy_SD_Host_EnableSdClk(SDHC1);
```

SDIO card interrupt (`CY_SD_HOST_SDIO_INTERRUPT`) and card insert/remove events are available even when the SD clock is stopped.

### Card Removal and Reinsertion

```c
uint32_t normalStatus = Cy_SD_Host_GetNormalInterruptStatus(SDHC1);

if (normalStatus & CY_SD_HOST_CARD_REMOVAL)
{
    Cy_SD_Host_ClearNormalInterruptStatus(SDHC1, CY_SD_HOST_CARD_REMOVAL);
    /* Float DAT lines to avoid contention */
    Cy_GPIO_SetDrivemode(P13_0_PORT, P13_0_NUM, CY_GPIO_DM_HIGHZ);
    /* ... repeat for DAT1-DAT3 ... */
}

if (normalStatus & CY_SD_HOST_CARD_INSERTION)
{
    Cy_SD_Host_ClearNormalInterruptStatus(SDHC1, CY_SD_HOST_CARD_INSERTION);
    /* Restore strong drive before re-initializing */
    Cy_GPIO_SetDrivemode(P13_0_PORT, P13_0_NUM, CY_GPIO_DM_STRONG);
    Cy_SD_Host_InitCard(SDHC1, &cardCfg, &sdHostContext);
}
```

## Industry Standards

- **SD Specifications Part 1 Physical Layer v6.0** — SD, SDHC, SDXC protocol (SDSC up to 2 GB, SDHC up to 32 GB, SDXC up to 2 TB)
- **SDIO Specification v4.10** — SDIO function interface for peripheral devices
- **JEDEC JESD84-B51 (eMMC 5.1)** — Embedded MultiMediaCard protocol
- **SD Host Controller Simplified Specification v4.20** — Register interface implemented by MXSDHC

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
