# Ethernet - IEEE 802.3 Gigabit MAC Driver

## Overview

The Ethernet driver (`cy_ethif` + `cy_ephy`) provides a low-level API for the **MXETH** Gigabit Ethernet
MAC IP (Cadence GEM_GXL). It handles DMA buffer descriptor management, Tx/Rx queueing, MII/RMII/GMII/RGMII
PHY interfaces, and IEEE 1588 hardware timestamping, enabling full-featured wired network connectivity.

---

## Features

- **10 / 100 / 1000 Mbps** Ethernet MAC (Cadence GEM_GXL Gigabit Ethernet core)
- Supports **MII, RMII, GMII, and RGMII** PHY interfaces
- Up to **3 Tx queues** and **3 Rx queues** with configurable DMA buffer descriptors
- Standard and **extended buffer descriptor** modes
- Full Store-and-Forward or **Partial Store-and-Forward** DMA operation modes
- **IEEE 1588 Precision Time Protocol** hardware timestamping (TSU)
- Companion **EPHY driver** (`cy_ephy`) for external PHY configuration via MDIO/MDC
- Callback-based Rx frame delivery and Tx completion notification

---

## When to Use

| Scenario | Details |
|---|---|
| Wired LAN connectivity | Connecting an MCU to a LAN switch or router |
| Industrial Ethernet | EtherNet/IP, PROFINET transport layer |
| IEEE 1588 / PTP | Time synchronization with nanosecond precision |
| High-throughput data transfer | DMA-based bulk data transfer over Ethernet at 100/1000 Mbps |

> **Note**: Wake-on-LAN (WoL) is **not** supported. The Ethernet MAC cannot receive frames when the system
> is in Deep Sleep.

---

## Prerequisites

### Hardware Requirements

- Device with MXETH peripheral
- External Ethernet PHY chip connected via MII/RMII/GMII/RGMII (e.g., DP83867IR, LAN8710AI)
- MDC/MDIO pins for PHY management interface
- Reference clock source (external HSIO clock recommended; internal PLL optional)

### Software Requirements

- ModusToolbox™ 3.x or later
- PDL version 1.x with `cy_ethif.h` and `cy_ephy.h`
- Cadence EMAC core driver (included as a third-party component under `asset/drivers/ethernet/third_party/`)
- Include `cy_pdl.h` or individual headers to access all PDL declarations

### Configure in the Tool

1. Open **Device Configurator** in ModusToolbox.
2. Enable the **Ethernet MAC** peripheral.
3. Assign all required Ethernet pins: TD[0–3] / RD[0–3], TX_CTL, RX_CTL, TX_CLK, RX_CLK, REF_CLK, MDC, MDIO.
4. Select the **PHY interface mode** (MII/RMII/GMII/RGMII) and corresponding link speed.
5. Enable Ethernet MAC interrupt sources (queue 0, 1, 2 IRQs).

---

## Quick Start

### Step-by-Step

1. Configure MAC/PHY GPIO pins (drive strength, HSIOM).
2. Enable Ethernet interrupt vectors with `Cy_SysInt_Init()` and `NVIC_EnableIRQ()`.
3. Fill `cy_stc_ethif_wrapper_config_t` with interface mode and reference clock settings.
4. Optionally fill `cy_stc_ethif_tsu_config_t` for IEEE 1588 timestamping.
5. Fill `cy_stc_ethif_mac_config_t` with DMA settings, queue enables, Rx buffer pools, and callbacks.
6. Call `Cy_ETHIF_Init()` to initialize the MAC.
7. Initialize the external PHY using `Cy_EPHY_Init()` / `Cy_EPHY_Configure()`.
8. Poll `Cy_EPHY_GetLinkStatus()` until the link comes up.
9. Transmit a frame with `Cy_ETHIF_TransmitFrame()`; received frames are delivered via the `rxframecb` callback.

### Sample Code

```c
#include "cy_pdl.h"
#include "cy_ethif.h"
#include "cy_ephy.h"
#include <string.h>

#define PHY_ADDR           0U
#define ETH_FRAME_SIZE     1518U

/* RX buffer pool (32-byte aligned) */
CY_ALIGN(32) uint8_t g_rx_bufs[CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE][CY_ETH_SIZE_MAX_FRAME];
uint8_t *g_rx_buf_ptrs[CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE];

cy_stc_ephy_t phyObj;
static bool frame_received = false;

/* Callbacks */
static void EthTxComplete(ETH_Type *base, uint8_t queueIdx)
{
    (void)base; (void)queueIdx;
    /* Tx complete: release buffer if needed */
}

static void EthTxError(ETH_Type *base, uint8_t queueIdx)
{
    (void)base; (void)queueIdx;
    /* Handle Tx error */
}

static void EthRxFrame(ETH_Type *base, uint8_t *buf, uint32_t len)
{
    (void)base;
    /* Process received frame (len bytes starting at buf) */
    frame_received = true;
}

static void EthRxGetBuffer(ETH_Type *base, uint8_t **buf, uint32_t *len)
{
    (void)base;
    static uint32_t idx = 0U;
    *buf = g_rx_bufs[idx % CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE];
    *len = CY_ETH_SIZE_MAX_FRAME;
    idx++;
}

/* Wrapper config: RMII 100 Mbps, external clock */
cy_stc_ethif_wrapper_config_t wrapperCfg =
{
    .stcInterfaceSel  = CY_ETHIF_CTL_RMII_100,
    .bRefClockSource  = CY_ETHIF_EXTERNAL_HSIO,
    .u8RefClkDiv      = 1U,   /* 25 MHz ref clock / 1 = 25 MHz Tx clock */
};

/* Callbacks structure */
cy_stc_ethif_cb_t ethCallbacks =
{
    .txcompletecb   = EthTxComplete,
    .txerrorcb      = EthTxError,
    .rxframecb      = EthRxFrame,
    .tsuSecondInccb = NULL,
    .rxgetbuff      = EthRxGetBuffer,
};

/* MAC configuration */
cy_stc_ethif_mac_config_t macCfg =
{
    .bintrEnable        = true,
    .dmaDataBurstLen    = CY_ETHIF_DMA_DBUR_LEN_16,
    .mdcPclkDiv         = CY_ETHIF_MDC_DIV_BY_48,
    .u8chkSumOffEn      = 0U,
    .u8rx1536ByteEn     = 1U,
    .u8aw2wMaxPipeline  = 2U,
    .u8ar2rMaxPipeline  = 2U,
    .pstcWrapperConfig  = &wrapperCfg,
    .pstcTSUConfig      = NULL,
    .btxq0enable        = true,
    .btxq1enable        = false,
    .btxq2enable        = false,
    .brxq0enable        = true,
    .brxq1enable        = false,
    .brxq2enable        = false,
    .pRxQbuffPool       = { (cy_ethif_buffpool_t *)g_rx_buf_ptrs, NULL },
};

int main(void)
{
    /* Build RX buffer pointer array */
    for (uint32_t i = 0U; i < CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE; i++)
    {
        g_rx_buf_ptrs[i] = g_rx_bufs[i];
    }

    /* Configure GPIO pins for Ethernet (HSIOM + drive strength) */
    /* ... Cy_GPIO_Pin_Init() calls for TD/RD/CTL/CLK/MDC/MDIO ... */

    /* Enable Ethernet MAC interrupt */
    cy_stc_sysint_t eth_irq = { .intrSrc = CY_GIG_ETH_IRQN0, .intrPriority = 3U };
    Cy_SysInt_Init(&eth_irq, Cy_ETHIF_InterruptHandler);
    NVIC_EnableIRQ(CY_GIG_ETH_IRQN0);

    /* Initialize MAC */
    if (CY_ETHIF_SUCCESS != Cy_ETHIF_Init(CY_GIG_ETH_TYPE, &ethCallbacks, &macCfg))
    { /* handle error */ }

    /* Initialize PHY (vendor-specific via MDIO) */
    Cy_EPHY_Init(&phyObj, Cy_ETHIF_PhyRegRead, Cy_ETHIF_PhyRegWrite);
    Cy_EPHY_Configure(&phyObj, CY_EPHY_SPEED_100, CY_EPHY_DUPLEX_FULL);

    /* Wait for link up */
    while (Cy_EPHY_GetLinkStatus(&phyObj) != CY_EPHY_LINK_UP) { }

    /* Build a minimal Ethernet frame and transmit */
    uint8_t tx_frame[64U] = {0};
    /* Fill destination, source, EtherType, payload ... */
    memset(&tx_frame[0], 0xFF, 6U); /* broadcast destination */
    Cy_ETHIF_TransmitFrame(CY_GIG_ETH_TYPE, tx_frame, 64U, 0U, true, NULL);

    while (!frame_received) { }

    for (;;) { }
}
```

### Expected Outcome

- After `Cy_EPHY_GetLinkStatus()` returns `CY_EPHY_LINK_UP`, the MAC can send and receive frames.
- `EthTxComplete` is called once the transmitted frame is confirmed sent.
- `EthRxFrame` is called for every received frame; `buf` points to a frame buffer from the pool.

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| Link never comes up | PHY not initialized or wrong PHY address | Verify `PHY_ADDR` and PHY ID; read PHY registers with `Cy_ETHIF_PhyRegRead()` |
| No Tx callback fires | Interrupt not enabled or wrong IRQ number | Check `CY_GIG_ETH_IRQN0` constant; ensure `bintrEnable = true` in MAC config |
| No Rx callback fires | Rx buffer pool not configured or misaligned | Ensure `pRxQbuffPool` buffers are **32-byte aligned** (`CY_ALIGN(32)`) |
| `CY_ETHIF_MEMORY_NOT_ENOUGH` from `Init` | Buffer descriptors count too small | Increase `CY_ETH_DEFINE_TOTAL_BD_PER_RXQUEUE` / `CY_ETH_DEFINE_TOTAL_BD_PER_TXQUEUE` |
| Garbled frames at 1000 Mbps | Reference clock not 125 MHz | Configure HSIO or PLL to supply correct reference clock for RGMII mode |
| Driver not compiling | Missing `CY_IP_MXETH` | Target must have the MXETH peripheral |

---

## Related Code Examples

- [PSOC™ Edge MCU: Ethernet MQTT Client](https://github.com/Infineon/mtb-example-psoc-edge-ethernet-mqtt-client)
- [PSOC™ Edge MCU: Ethernet Secure TCP Client](https://github.com/Infineon/mtb-example-psoc-edge-ethernet-secure-tcp-client)
- [PSOC™ Edge MCU: Ethernet Secure TCP Server](https://github.com/Infineon/mtb-example-psoc-edge-ethernet-secure-tcp-server)

## Related Application Notes

- Refer to the device TRM, Ethernet MAC chapter.
- Cadence GEM_GXL Gigabit Ethernet Controller datasheet (third-party IP).

---

## Configuration Parameters Reference

### `cy_stc_ethif_wrapper_config_t`

| Parameter | Type | Description |
|---|---|---|
| `stcInterfaceSel` | `cy_en_ethif_speed_sel_t` | PHY interface and speed: MII-10/100, GMII-1000, RGMII-10/100/1000, RMII-10/100 |
| `bRefClockSource` | `cy_en_ethif_clock_ref_t` | `CY_ETHIF_EXTERNAL_HSIO` (recommended) or `CY_ETHIF_INTERNAL_PLL` |
| `u8RefClkDiv` | `uint8_t` | Reference clock divider; actual divisor = `u8RefClkDiv + 1` |

### `cy_stc_ethif_mac_config_t` (Key Fields)

| Parameter | Type | Description |
|---|---|---|
| `bintrEnable` | `bool` | Enable MAC interrupt generation |
| `dmaDataBurstLen` | `cy_en_ethif_dma_data_buffer_len_t` | AXI DMA burst length (1, 4, 8, or 16) |
| `mdcPclkDiv` | `cy_en_ethif_dma_mdc_clk_div_t` | MDC clock divider from PCLK (÷8 to ÷224) |
| `btxq0/1/2enable` | `bool` | Enable individual Tx queues (Q0–Q2) |
| `brxq0/1/2enable` | `bool` | Enable individual Rx queues (Q0–Q2) |
| `pRxQbuffPool` | `cy_ethif_buffpool_t *` | Pointer to Rx buffer pointer arrays for each enabled Rx queue |
| `pstcWrapperConfig` | `cy_stc_ethif_wrapper_config_t *` | PHY interface and clock configuration |
| `pstcTSUConfig` | `cy_stc_ethif_tsu_config_t *` | IEEE 1588 TSU configuration (NULL to disable) |

### `cy_stc_ethif_tsu_config_t` (IEEE 1588)

| Parameter | Type | Description |
|---|---|---|
| `pstcTimerValue` | `cy_stc_ethif_1588_timer_val_t *` | Initial TSU timer value (seconds + nanoseconds) |
| `pstcTimerIncValue` | `cy_stc_ethif_timer_increment_t *` | Per-clock-cycle increment for nanosecond/sub-nanosecond counters |
| `enTxDescStoreTimeStamp` | `cy_en_ethif_TxTs_mode_t` | Tx timestamping mode (disabled / PTP event / PTP all / all) |
| `enRxDescStoreTimeStamp` | `cy_en_ethif_RxTs_mode_t` | Rx timestamping mode |

### Key API Functions

| Function | Description |
|---|---|
| `Cy_ETHIF_Init()` | Initialize Ethernet MAC with callbacks and config |
| `Cy_ETHIF_TransmitFrame()` | Enqueue a frame for transmission |
| `Cy_ETHIF_SetFilterAddress()` | Set MAC destination / source address filter |
| `Cy_ETHIF_Get1588TimerValue()` | Read current IEEE 1588 TSU timer value |
| `Cy_ETHIF_Set1588TimerValue()` | Set IEEE 1588 TSU timer value |
| `Cy_ETHIF_InterruptHandler()` | Ethernet MAC ISR (call from interrupt vector) |
| `Cy_ETHIF_PhyRegRead()` | Read PHY register via MDIO |
| `Cy_ETHIF_PhyRegWrite()` | Write PHY register via MDIO |
| `Cy_EPHY_Init()` | Initialize the EPHY driver context |
| `Cy_EPHY_Configure()` | Configure PHY speed and duplex mode |
| `Cy_EPHY_GetLinkStatus()` | Poll PHY link status |

---

## Advanced Usage

### IEEE 1588 Precision Time Protocol

Populate `cy_stc_ethif_tsu_config_t` with the clock-cycle increment values calculated from your reference clock frequency. Use `Cy_ETHIF_Get1588TimerValue()` and `Cy_ETHIF_Set1588TimerValue()` to synchronize the TSU. Register `tsuSecondInccb` to handle the once-per-second TSU counter increment event.

### Multiple Tx/RX Queues

Enable `btxq1enable` / `btxq2enable` and/or `brxq1enable` / `brxq2enable` in `cy_stc_ethif_mac_config_t`. Provide separate buffer pools for each enabled Rx queue via `pRxQbuffPool[1]` and `pRxQbuffPool[2]`. Each queue generates its own interrupt vector (`CY_GIG_ETH_IRQN1`, `CY_GIG_ETH_IRQN2`).

### MAC Address Filters

Call `Cy_ETHIF_SetFilterAddress()` with a destination or source MAC address to filter incoming frames. The hardware supports up to 4 independent MAC address filters (`CY_ETHIF_FILTER_NUM_1` through `CY_ETHIF_FILTER_NUM_4`).

---

## Industry Standards

| Standard | Applicability |
|---|---|
| **IEEE 802.3** | Ethernet framing, CSMA/CD, 10/100/1000BASE-T PHY interface |
| **IEEE 1588-2008 (PTP v2)** | Precision Time Protocol hardware timestamping via TSU |
| **RMII Specification 1.2** | Reduced Media Independent Interface for 10/100 Mbps |
| **RGMII Specification 1.3** | Reduced Gigabit Media Independent Interface for 1000 Mbps |

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
