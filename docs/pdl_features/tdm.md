# TDM - Full-Duplex Multi-Channel Audio Interface

## Overview

The TDM driver (`cy_tdm`) configures the **MXTDM IP** as a Time-Division Multiplexing (TDM) or I2S audio
interface. It provides full-duplex transmit and receive paths with independent master/slave clocking,
programmable channel and word sizes, and 128-entry TX/RX FIFOs with DMA and interrupt support.

---

## Features

- **TDM mode** (multi-channel) and **I2S mode** (2-channel stereo) selectable per interface
- **Master and slave** operation with independent TX/RX clock control
- **Full-duplex**: simultaneous transmit and receiver operation
- Up to **32 channels** per TDM frame
- Programmable **channel size** and **word size** (8, 10, 12, 14, 16, 18, 20, 24, 32 bits)
- Left-aligned and **right-aligned** (or sign-extended) sample formatting
- **128-entry TX FIFO** and **128-entry RX FIFO** with per-FIFO interrupt and DMA trigger
- Configurable **SCK/FSYNC polarity** and **channel delay** (0 or 1 bit)
- Delayed sampling support for slave receivers

---

## When to Use

| Scenario | Details |
|---|---|
| Stereo I2S audio output | Driving a DAC/amplifier (e.g., AIC26) in I2S master mode with 16-bit stereo |
| Multi-channel TDM audio | 8-channel 32-bit TDM bus connecting multiple codecs |
| Audio loopback | TX master driving RX slave on same device for self-test |
| DMA-based audio streaming | CPU-free audio with DMA triggered by TX/RX FIFO threshold |

---

## Prerequisites

### Hardware Requirements

- Device with MXTDM peripheral
- Audio codec or DAC/ADC connected via I2S/TDM pins (SCK, FSYNC, SDO, SDI)
- External clock or SRSS clock routed to TDM block

### Software Requirements

- ModusToolbox™ 3.x or later
- PDL version 1.x with `cy_tdm.h`
- Include `cy_pdl.h` to access all PDL declarations

### Configure in the Tool

1. Open **Device Configurator** in ModusToolbox.
2. Enable the **TDM** peripheral and select the TDM structure (e.g., `TDM_STRUCT0`).
3. Assign **SCK**, **FSYNC**, **SDO** (TX data), and **SDI** (RX data) pins.
4. Configure the clock divider and master/slave role for TX and RX independently.
5. If using DMA, connect a DMA channel trigger to the TX/RX FIFO trigger output.

---

## Quick Start

### Step-by-Step

1. Fill `cy_stc_tdm_config_tx_t` and `cy_stc_tdm_config_rx_t` for TX and RX respectively.
2. Combine them into `cy_stc_tdm_config_t`.
3. Call `Cy_AudioTDM_Init()` to initialize the TDM block.
4. Clear and enable TX/RX interrupt masks with `Cy_AudioTDM_ClearTxInterrupt()` / `Cy_AudioTDM_SetTxInterruptMask()`.
5. Enable RX with `Cy_AudioTDM_EnableRx()`, then TX with `Cy_AudioTDM_EnableTx()`.
6. Pre-fill the TX FIFO via `Cy_AudioTDM_WriteTxData()` to avoid underflow on the first frame.
7. Activate TX with `Cy_AudioTDM_ActivateTx()` and RX with `Cy_AudioTDM_ActivateRx()`.

### Sample Code

```c
#include "cy_pdl.h"

#define LOOPBACK_COUNT  512U
#define FIFO_TX_LEVEL   36U
#define FIFO_RX_LEVEL   32U

volatile bool    tx_eod = false;
volatile uint32_t rx_count = 0U;
uint16_t lpbk_rxdata[LOOPBACK_COUNT];

/* 16-bit stereo I2S loopback: TX master, RX slave */
cy_stc_tdm_config_tx_t tdm_tx_config =
{
    .enable          = true,
    .masterMode      = CY_TDM_DEVICE_MASTER,
    .wordSize        = CY_TDM_SIZE_16,
    .format          = CY_TDM_LEFT,
    .clkDiv          = 16U,
    .clkSel          = CY_TDM_SEL_SRSS_CLK0,
    .sckPolarity     = CY_TDM_CLK,
    .fsyncPolarity   = CY_TDM_SIGN,
    .fsyncFormat     = CY_TDM_CH_PERIOD,
    .channelNum      = 2U,
    .channelSize     = 16U,
    .fifoTriggerLevel = FIFO_TX_LEVEL,
    .chEn            = 0x3U,
    .signalInput     = 0U,
    .i2sMode         = true,
};

cy_stc_tdm_config_rx_t tdm_rx_config =
{
    .enable          = true,
    .masterMode      = CY_TDM_DEVICE_SLAVE,
    .wordSize        = CY_TDM_SIZE_16,
    .signExtend      = CY_ZERO_EXTEND,
    .format          = CY_TDM_LEFT,
    .clkDiv          = 16U,
    .clkSel          = CY_TDM_SEL_SRSS_CLK0,
    .sckPolarity     = CY_TDM_CLK,
    .fsyncPolarity   = CY_TDM_SIGN,
    .lateSample      = false,
    .fsyncFormat     = CY_TDM_CH_PERIOD,
    .channelNum      = 2U,
    .channelSize     = 16U,
    .fifoTriggerLevel = FIFO_RX_LEVEL,
    .chEn            = 0x3U,
    .signalInput     = 0U,
    .i2sMode         = true,
};

const cy_stc_tdm_config_t tdm_config =
{
    .tx_config = &tdm_tx_config,
    .rx_config = &tdm_rx_config,
};

void TDM_TX_ISR(void)
{
    uint32_t intr = Cy_AudioTDM_GetTxInterruptStatus(TDM_STRUCT0_TX);
    if (intr & CY_TDM_INTR_TX_FIFO_TRIGGER)
    {
        /* Refill TX FIFO with audio data */
        for (uint32_t i = 0U; i < (128U - FIFO_TX_LEVEL); i++)
        {
            Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, 0x0000FFFFU); /* example tone */
        }
        Cy_AudioTDM_ClearTxInterrupt(TDM_STRUCT0_TX, CY_TDM_INTR_TX_FIFO_TRIGGER);
    }
}

void TDM_RX_ISR(void)
{
    uint32_t intr = Cy_AudioTDM_GetRxInterruptStatus(TDM_STRUCT0_RX);
    if (intr & CY_TDM_INTR_RX_FIFO_TRIGGER)
    {
        for (uint32_t i = 0U; i < FIFO_RX_LEVEL && rx_count < LOOPBACK_COUNT; i++)
        {
            lpbk_rxdata[rx_count++] = (uint16_t)Cy_AudioTDM_ReadRxData(TDM_STRUCT0_RX);
        }
        Cy_AudioTDM_ClearRxInterrupt(TDM_STRUCT0_RX, CY_TDM_INTR_RX_FIFO_TRIGGER);
    }
}

int main(void)
{
    /* Initialize TDM block */
    if (CY_TDM_SUCCESS != Cy_AudioTDM_Init(TDM_STRUCT0, &tdm_config))
    { /* handle error */ }

    /* Clear pending interrupts */
    Cy_AudioTDM_ClearTxInterrupt(TDM_STRUCT0_TX, CY_TDM_INTR_TX_MASK);
    Cy_AudioTDM_ClearRxInterrupt(TDM_STRUCT0_RX, CY_TDM_INTR_RX_MASK);

    /* Enable interrupt masks */
    Cy_AudioTDM_SetTxInterruptMask(TDM_STRUCT0_TX, CY_TDM_INTR_TX_FIFO_TRIGGER);
    Cy_AudioTDM_SetRxInterruptMask(TDM_STRUCT0_RX, CY_TDM_INTR_RX_FIFO_TRIGGER);

    /* Register ISRs */
    cy_stc_sysint_t tx_irq = { .intrSrc = tdm_0_interrupts_tx_0_IRQn, .intrPriority = 4U };
    cy_stc_sysint_t rx_irq = { .intrSrc = tdm_0_interrupts_rx_0_IRQn, .intrPriority = 4U };
    Cy_SysInt_Init(&tx_irq, TDM_TX_ISR);
    Cy_SysInt_Init(&rx_irq, TDM_RX_ISR);
    NVIC_EnableIRQ(tdm_0_interrupts_tx_0_IRQn);
    NVIC_EnableIRQ(tdm_0_interrupts_rx_0_IRQn);

    /* Enable and activate RX then TX */
    Cy_AudioTDM_EnableRx(TDM_STRUCT0_RX);
    Cy_AudioTDM_EnableTx(TDM_STRUCT0_TX);

    /* Prime TX FIFO before activating */
    for (uint32_t i = 0U; i < 4U; i++)
    {
        Cy_AudioTDM_WriteTxData(TDM_STRUCT0_TX, 0x0000FFFFU);
    }
    Cy_AudioTDM_ActivateTx(TDM_STRUCT0_TX);
    Cy_AudioTDM_ActivateRx(TDM_STRUCT0_RX);

    while (rx_count < LOOPBACK_COUNT) { }

    for (;;) { }
}
```

### Expected Outcome

- TX FIFO fires `CY_TDM_INTR_TX_FIFO_TRIGGER` when entries drop below `fifoTriggerLevel`; the ISR refills it.
- RX FIFO fires `CY_TDM_INTR_RX_FIFO_TRIGGER` when sufficient samples accumulate; the ISR drains them.
- After `LOOPBACK_COUNT` RX samples are collected, `lpbk_rxdata[]` contains the loopback audio data.

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| TX FIFO underflow (`CY_TDM_INTR_TX_IF_UNDERFLOW`) | TX FIFO not refilled fast enough | Lower `fifoTriggerLevel` or increase ISR priority; consider DMA |
| RX FIFO overflow (`CY_TDM_INTR_RX_FIFO_OVERFLOW`) | RX not drained quickly enough | Increase `fifoTriggerLevel` or move to DMA |
| No audio output | TX not activated | Call `Cy_AudioTDM_ActivateTx()` after `Cy_AudioTDM_EnableTx()` |
| Clock mismatch / garbled audio | Wrong `clkDiv` for target sample rate | Recalculate: `Fs = SRSS_CLK / (clkDiv × channelSize × channelNum)` |
| Slave not receiving | SCK/FSYNC not driven by master | Check pin routing and ensure TX master is enabled first |
| Build error: `CY_IP_MXTDM` not defined | Targeting wrong device | Confirm target device has MXTDM peripheral |

---

## Related Code Examples

- [PSOC™ Edge MCU: I2S](https://github.com/Infineon/mtb-example-psoc-edge-i2s)
- [PSOC™ Edge MCU: PDM to I2S](https://github.com/Infineon/mtb-example-psoc-edge-pdm-to-i2s)

## Related Application Notes

- Refer to the device TRM, Audio TDM chapter, for clock configuration details.
- I2S specification (IEC 60958 / Sony Philips Digital Interface) for wire-level protocol details.

---

## Configuration Parameters Reference

### `cy_stc_tdm_config_tx_t` / `cy_stc_tdm_config_rx_t`

| Parameter | Type | Description |
|---|---|---|
| `enable` | `bool` | Enable TX/RX path |
| `masterMode` | `cy_en_tdm_device_cfg_t` | `CY_TDM_DEVICE_MASTER` or `CY_TDM_DEVICE_SLAVE` |
| `wordSize` | `cy_en_tdm_ws_t` | PCM word size: 8/10/12/14/16/18/20/24/32 bits |
| `format` | `cy_en_tdm_format_t` | `CY_TDM_LEFT` (left-aligned) or `CY_TDM_RIGHT` (right-aligned) |
| `clkDiv` | `uint8_t` | SCK clock divider from `clkSel` source |
| `clkSel` | `cy_en_tdm_clock_sel_t` | Clock source for SCK generation |
| `sckPolarity` | `cy_en_tdm_clock_polarity_t` | SCK active edge polarity |
| `fsyncPolarity` | `cy_en_tdm_sign_polarity_t` | FSYNC active polarity |
| `fsyncFormat` | `cy_en_tdm_fsync_format_t` | FSYNC pulse vs. channel-period format |
| `channelNum` | `uint8_t` | Number of active TDM channels (1–32) |
| `channelSize` | `uint8_t` | Bit-width of each TDM channel slot |
| `fifoTriggerLevel` | `uint8_t` | FIFO interrupt/trigger threshold (words) |
| `chEn` | `uint32_t` | Bitmask of enabled channels |
| `i2sMode` | `bool` | `true` = I2S 2-channel stereo mode |
| `signExtend` | `cy_en_tdm_sign_extend_t` | RX only — sign or zero extension (TX: N/A) |
| `lateSample` | `bool` | RX only — enable delayed sampling |

### Key API Functions

| Function | Description |
|---|---|
| `Cy_AudioTDM_Init()` | Initialize the TDM structure (TX + RX) |
| `Cy_AudioTDM_DeInit()` | De-initialize the TDM structure |
| `Cy_AudioTDM_EnableTx()` / `Cy_AudioTDM_EnableRx()` | Enable TX or RX path |
| `Cy_AudioTDM_ActivateTx()` / `Cy_AudioTDM_ActivateRx()` | Activate TX/RX clocks and data flow |
| `Cy_AudioTDM_WriteTxData()` | Write one word to the TX FIFO |
| `Cy_AudioTDM_ReadRxData()` | Read one word from the RX FIFO |
| `Cy_AudioTDM_SetTxInterruptMask()` / `Cy_AudioTDM_SetRxInterruptMask()` | Enable interrupt sources |
| `Cy_AudioTDM_GetTxInterruptStatus()` / `Cy_AudioTDM_GetRxInterruptStatus()` | Read interrupt flags |
| `Cy_AudioTDM_ClearTxInterrupt()` / `Cy_AudioTDM_ClearRxInterrupt()` | Clear interrupt flags |

---

## Advanced Usage

### DMA-Based Audio Streaming

Connect a DMA channel to the TX FIFO trigger output and configure a scatter-gather descriptor to write a ping-pong audio buffer. On DMA completion, swap to the next buffer and refill the previous one without CPU involvement. Set `txDmaTrigger = true` in the config to route the FIFO trigger to the DMA system.

### TDM Multi-Channel (>2 channels)

Set `i2sMode = false`, configure `channelNum` up to 32, and set `chEn` bitmask for the channels that carry live audio. Each channel occupies `channelSize` bits in the TDM frame; `wordSize ≤ channelSize` holds the sample data.

### External Clock Source

Set `clkSel` to a MCLK-derived source or external pin. When `extClk = true` in the config, the TDM block expects the SCK to be supplied externally and ignores the internal `clkDiv` divider for SCK generation.

---

## Industry Standards

| Standard | Applicability |
|---|---|
| IEC 60958 / Sony Philips Digital Interface (I²S) | When `i2sMode = true`; defines 2-channel stereo framing |
| TDM (Time-Division Multiplexing) audio bus | When `i2sMode = false`; defines multi-channel framing |

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
