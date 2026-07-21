# PDM-PCM v2 - High-Quality Digital Microphone Capture

## Overview

The PDM-PCM v2 driver (`cy_pdm_pcm_v2`) converts 1-bit Pulse-Density Modulated (PDM) audio streams
from digital microphones into multi-bit Pulse-Code Modulated (PCM) data. It targets the **MXPDM IP**
and provides a configurable CIC → FIR0 → FIR1 → DC-block signal-processing
pipeline with up to 8 independent receiver channels and 64-entry per-channel FIFOs.

---

## Features

- Supports **up to 8 PDM receivers** simultaneously
- **Stereo and mono** dual-mode PDM-to-PCM conversion
- Three-stage filter chain: **CIC filter → FIR0 filter → FIR1 filter → DC-block filter**
- Programmable **PCM word size**: 8, 10, 12, 14, 16, 18, 20, 24, or 32 bits
- **64-entry RX FIFO** per channel with interrupt and DMA trigger support
- **Half-rate sampling** mode to reduce system power consumption
- Programmable FIR filter coefficients, decimation rates, and DC-blocking coefficient
- Built-in **test mode** with programmable PDM pattern generator

---

## When to Use

| Scenario | Details |
|---|---|
| Voice/keyword detection | Capturing 16 kHz or 8 kHz mono/stereo PCM from a DMIC |
| High-fidelity recording | 48 kHz stereo capture from dual DMIC microphones |
| Low-power listening | Half-rate mode to minimize dynamic power while polling for audio events |
| DMA-based audio pipeline | Zero-CPU audio capture via DMA trigger from RX FIFO threshold |

---

## Prerequisites

### Hardware Requirements

- Device with MXPDM peripheral
- Digital MEMS microphone with PDM output (clock frequency 384 kHz – 6.144 MHz)
- PDM_CLK and PDM_DATA pins routed to the microphone

### Software Requirements

- ModusToolbox™ 3.x or later
- PDL version 1.x with `cy_pdm_pcm_v2.h`
- Include `cy_pdl.h` to access all PDL declarations

### Configure in the Tool

1. Open **Device Configurator** in ModusToolbox.
2. Enable the **PDM-PCM** peripheral and select the desired channel(s).
3. Assign **PDM_CLK** and **PDM_DATA** pins for each active receiver.
4. Configure the clock divider to achieve the target PDM interface clock.
5. If using DMA, configure a **DMA channel** with a trigger connected to the PDM RX FIFO.

---

## Quick Start

### Step-by-Step

1. Fill `cy_stc_pdm_pcm_config_v2_t` with clock divider, clock source, and optional FIR coefficients.
2. Call `Cy_PDM_PCM_Init()` to configure the PDM block.
3. Fill `cy_stc_pdm_pcm_channel_config_t` for each channel (word size, decimation rates, FIFO level).
4. Call `Cy_PDM_PCM_Channel_Init()` for each channel.
5. Enable the desired interrupt mask with `Cy_PDM_PCM_SetInterruptMask()`.
6. Enable the channel with `Cy_PDM_PCM_Channel_Enable()`.
7. In the ISR, read samples with `Cy_PDM_PCM_ReadFifo()` and clear with `Cy_PDM_PCM_Channel_ClearInterrupt()`.

### Sample Code

```c
#include "cy_pdl.h"

/* Recording buffer: 24000 samples at 16-bit, mono */
#define BUFFER_SIZE     (24000U)
#define FIFO_TRIG_LEVEL (10U)
#define PDM_CHANNEL_IDX (0U)

volatile uint32_t fifo_count = 0U;
int16_t recorded_data[BUFFER_SIZE];

/* PDM block configuration */
cy_stc_pdm_pcm_config_v2_t pdm_config =
{
    .clkDiv  = 11U,                       /* (clkDiv+1) divides interface clock  */
    .clksel  = CY_PDM_PCM_SEL_SRSS_CLOCK,
    .halverate = CY_PDM_PCM_RATE_FULL,
    .route   = 0U,
    .fir0_coeff_user_value = 0U,          /* use default FIR0 coefficients       */
    .fir1_coeff_user_value = 0U,          /* use default FIR1 coefficients       */
};

/* Per-channel configuration */
cy_stc_pdm_pcm_channel_config_t ch_config =
{
    .sampledelay     = 1U,
    .wordSize        = CY_PDM_PCM_WSIZE_16_BIT,
    .signExtension   = true,
    .rxFifoTriggerLevel = FIFO_TRIG_LEVEL,
    .fir0_enable     = false,
    .cic_decim_code  = CY_PDM_PCM_CH_CIC_DECIM_32,
    .fir0_decim_code = CY_PDM_PCM_CH_FIR0_DECIM_1,
    .fir0_scale      = 0U,
    .fir1_decim_code = CY_PDM_PCM_CH_FIR1_DECIM_2,
    .fir1_scale      = 15U,
    .dc_block_disable = false,
    .dc_block_code   = CY_PDM_PCM_CH_DCBLOCK_COEF_1,
};

void PDM_ISR_Handler(void)
{
    uint32_t intr = Cy_PDM_PCM_Channel_GetInterruptStatus(PDM0_CH0);
    if (intr & CY_PDM_PCM_INTR_RX_TRIGGER)
    {
        for (uint32_t i = 0U; i < FIFO_TRIG_LEVEL && fifo_count < BUFFER_SIZE; i++)
        {
            recorded_data[fifo_count++] = (int16_t)Cy_PDM_PCM_ReadFifo(PDM0_CH0);
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(PDM0_CH0, CY_PDM_PCM_INTR_RX_TRIGGER);
    }
}

int main(void)
{
    /* Initialize PDM block */
    cy_rslt_t result = Cy_PDM_PCM_Init(PDM0, &pdm_config);
    if (result != CY_PDM_PCM_SUCCESS) { /* handle error */ }

    /* Initialize channel 0 */
    result = Cy_PDM_PCM_Channel_Init(PDM0_CH0, &ch_config);
    if (result != CY_PDM_PCM_SUCCESS) { /* handle error */ }

    /* Register and enable ISR */
    cy_stc_sysint_t irq_cfg = { .intrSrc = pdm_0_CHANNEL_0_IRQ, .intrPriority = 5U };
    Cy_SysInt_Init(&irq_cfg, PDM_ISR_Handler);
    NVIC_EnableIRQ(pdm_0_CHANNEL_0_IRQ);

    /* Enable FIFO trigger interrupt and start channel */
    Cy_PDM_PCM_SetInterruptMask(PDM0_CH0, CY_PDM_PCM_INTR_RX_TRIGGER);
    Cy_PDM_PCM_Channel_Enable(PDM0_CH0);

    /* Wait until the buffer is filled */
    while (fifo_count < BUFFER_SIZE) { }

    Cy_PDM_PCM_Channel_Disable(PDM0_CH0);

    for (;;) { }
}
```

### Expected Outcome

- `recorded_data[]` is filled with 16-bit signed PCM samples at the configured sampling rate (e.g., 16 kHz with CIC×32, FIR1×2 decimation).
- The `CY_PDM_PCM_INTR_RX_TRIGGER` interrupt fires each time the FIFO reaches the trigger level.
- No CPU polling is required; when combined with a DMA channel, zero CPU involvement is needed.

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| No interrupt fires | Channel not enabled or interrupt mask not set | Call `Cy_PDM_PCM_Channel_Enable()` and `Cy_PDM_PCM_SetInterruptMask()` |
| Audio is distorted / clipping | FIR scale too high | Reduce `fir0_scale` / `fir1_scale` (try 6–8 for real DMIC) |
| Audio has DC offset | DC block disabled | Set `dc_block_disable = false` |
| FIFO overflow (`CY_PDM_PCM_INTR_RX_OVERFLOW`) | ISR too slow or trigger level too high | Lower `rxFifoTriggerLevel`; move to DMA |
| FIR overflow (`CY_PDM_PCM_INTR_RX_FIR_OVERFLOW`) | CIC producing samples faster than FIR can process | Verify decimation combination; refer to memo VBIH-247 |
| Clock jitter in recording | Non-integer PERI divider | Ensure `CLK_IF_SRSS_FREQ = (CLOCK_DIV+1) × M × Fs` divides evenly |
| Driver not compiling | Wrong device / missing IP | Confirm target has `CY_IP_MXPDM` defined |

---

## Related Code Examples

- [PSOC™ Edge MCU: PDM-PCM](https://github.com/Infineon/mtb-example-psoc-edge-pdm-pcm)
- [PSOC™ Edge MCU: PDM to I2S](https://github.com/Infineon/mtb-example-psoc-edge-pdm-to-i2s)

## Related Application Notes

- Refer to the device TRM, PDM-PCM chapter, for hardware clock tree details.
- Memo VBIH-247: Filter design coefficients and valid decimation combinations.

---

## Configuration Parameters Reference

### `cy_stc_pdm_pcm_config_v2_t` (Block-Level)

| Parameter | Type | Description |
|---|---|---|
| `clkDiv` | `uint8_t` | PDM interface clock divider; actual divisor = `clkDiv + 1` |
| `clksel` | `cy_en_pdm_pcm_clock_sel_t` | Clock source: SRSS, pdm_data[0], pdm_data[1], or OFF |
| `halverate` | `cy_en_pdm_pcm_halve_rate_sel_t` | Full or half-rate sampling |
| `route` | `uint8_t` | Input signal routing per receiver (1-bit per channel) |
| `fir0_coeff_user_value` | `uint8_t` | Enable user-defined FIR0 coefficients (0 = default) |
| `fir1_coeff_user_value` | `uint8_t` | Enable user-defined FIR1 coefficients (0 = default) |
| `fir0_coeff[8]` | `cy_stc_pdm_pcm_fir_coeff_t` | Symmetric 30-tap FIR0 coefficients (14-bit signed, range −8192..8191) |
| `fir1_coeff[14]` | `cy_stc_pdm_pcm_fir_coeff_t` | Symmetric 55-tap FIR1 coefficients (14-bit signed); default has built-in droop correction |

### `cy_stc_pdm_pcm_channel_config_t` (Per-Channel)

| Parameter | Type | Description |
|---|---|---|
| `sampledelay` | `uint8_t` | PDM sample capture delay; internally `sampledelay + 1` |
| `wordSize` | `cy_en_pdm_pcm_word_size_t` | PCM output width: 8/10/12/14/16/18/20/24/32 bits |
| `signExtension` | `bool` | `true` = sign-extend; `false` = zero-extend |
| `rxFifoTriggerLevel` | `uint8_t` | FIFO interrupt trigger level (0–63 words) |
| `fir0_enable` | `bool` | Enable user-defined FIR0 coefficients for this channel |
| `cic_decim_code` | `cy_en_pdm_pcm_ch_cic_decimcode_t` | CIC decimation factor |
| `fir0_decim_code` | `cy_en_pdm_pcm_ch_fir0_decimcode_t` | FIR0 decimation factor |
| `fir0_scale` | `uint8_t` | FIR0 output scaling (0–31) |
| `fir1_decim_code` | `cy_en_pdm_pcm_ch_fir1_decimcode_t` | FIR1 decimation factor |
| `fir1_scale` | `uint8_t` | FIR1 output scaling (0–31) |
| `dc_block_disable` | `bool` | `true` = disable DC blocker (debug/test only) |
| `dc_block_code` | `cy_en_pdm_pcm_ch_dcblock_coef_t` | DC-blocking coefficient |

### Key API Functions

| Function | Description |
|---|---|
| `Cy_PDM_PCM_Init()` | Initialize PDM block with block-level config |
| `Cy_PDM_PCM_Channel_Init()` | Initialize a single PDM channel |
| `Cy_PDM_PCM_Channel_Enable()` / `Cy_PDM_PCM_Channel_Disable()` | Start/stop a channel |
| `Cy_PDM_PCM_ReadFifo()` | Read one word from the channel RX FIFO |
| `Cy_PDM_PCM_SetInterruptMask()` | Enable selected interrupt sources |
| `Cy_PDM_PCM_Channel_GetInterruptStatus()` | Read pending interrupt flags |
| `Cy_PDM_PCM_Channel_ClearInterrupt()` | Clear pending interrupt flags |
| `Cy_PDM_PCM_DeInit()` | De-initialize the PDM block |

---

## Advanced Usage

### DMA-Based Capture (Zero-CPU Audio)

Configure a DMA channel to trigger from `CY_PDM_PCM_INTR_RX_TRIGGER` and transfer `rxFifoTriggerLevel` words per trigger burst to a circular ping-pong buffer. When the DMA completion interrupt fires, swap the active buffer for processing. This allows the CPU to remain in WFI/sleep while audio is captured.

### Dual-Microphone Stereo Setup

Set channels 0 (left) and 1 (right) to identical `cy_stc_pdm_pcm_channel_config_t` values. Use `route` field in `cy_stc_pdm_pcm_config_v2_t` to assign `pdm_data[0]` and `pdm_data[1]` as clock and data for each channel. Interleave left/right samples in your ISR or via dual DMA descriptors into a single stereo PCM buffer.

### Custom FIR Filter Design

For 8 kHz or 16 kHz target rates, populate `fir0_coeff[8]` with 30-tap symmetric coefficients (14-bit range) before calling `Cy_PDM_PCM_Init()`. Set `fir0_coeff_user_value = 1` and `fir0_enable = true` in the matching channel config. Refer to memo VBIH-247 for coefficient tables and valid decimation combinations.

### Half-Rate Mode

Set `halverate = CY_PDM_PCM_RATE_HALVE` in `cy_stc_pdm_pcm_config_v2_t` to halve the PDM interface clock and proportionally reduce power. This halves the effective PCM sampling rate; update clock dividers and decimation accordingly.

---

## Industry Standards

Not applicable — PDM-PCM is a proprietary digital microphone interface standard used by MEMS DMIC vendors; no formal IEEE/IEC standard governs it.

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
