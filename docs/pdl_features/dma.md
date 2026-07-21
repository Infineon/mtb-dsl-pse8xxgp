# DMA - CPU-Free Data Movement with Descriptor Chaining

## Overview
The DMA driver (`cy_dma.h`) configures the `MXAHBDMAC` / DataWire (DW) hardware block to transfer data
between memory and peripherals without CPU intervention, using a flexible descriptor-based model that
supports 1D, 2D, and CRC transfers with chained descriptor sequences.

---

## Features
- **Multiple DW blocks** with multiple channels per block (device-specific count)
- **Four priority levels** per channel (0 = highest)
- **1D and 2D transfers**: configurable source/destination address with independent X/Y increments
- **Descriptor chaining**: link descriptors to build complex multi-buffer pipelines
- **Transfer widths**: Byte (8-bit), Half-Word (16-bit), Word (32-bit)
- **CRC calculation** on transferred data (CPUSS v2 only)
- **Interrupt on completion** (or per-element, per-loop, per-descriptor)
- **Software trigger** via `Cy_TrigMux_SwTrigger()` or hardware peripheral trigger via TrigMux

---

## When to Use
- Moving ADC sample buffers to SRAM without blocking the CPU
- Feeding UART/SPI/I2C TX FIFOs from a memory buffer
- Filling display frame buffers from image data
- Ping-pong buffering with chained descriptors for continuous streaming
- Computing CRC over a data block in hardware (CPUSS v2)

---

## Prerequisites

### Hardware Requirements
- `MXAHBDMAC` (DataWire) IP block (DW0, DW1 depending on device)
- Peripheral trigger connected via TrigMux, or use software trigger

### Software Requirements
- `cy_pdl.h` (includes `cy_dma.h`)
- Optional: `cy_trigmux.h` for hardware trigger routing

### Configure in the Tool (ModusToolbox Device Configurator)

| Parameter | Description |
|---|---|
| `DMA Block` | Select DW0 or DW1 |
| `Channel Number` | 0 … N-1 |
| `Descriptor Type` | Single / 1D / 2D / CRC |
| `Data Size` | Byte / Half-Word / Word |
| `Trigger In` | Source trigger (peripheral output or software) |
| `Trigger Out` | Destination trigger (to another peripheral) |
| `Interrupt Type` | None / 1-element / X-loop / Descriptor / Chain |
| `Priority` | 0 (highest) … 3 (lowest) |

---

## Quick Start

**Step 1.** Declare and initialise a descriptor with `Cy_DMA_Descriptor_Init()`.
**Step 2.** Initialise the channel with `Cy_DMA_Channel_Init()` and assign the descriptor.
**Step 3.** Enable the channel with `Cy_DMA_Channel_Enable()`.
**Step 4.** Enable the DMA block with `Cy_DMA_Enable()`.
**Step 5.** Fire a software trigger (or rely on hardware trigger).

### Sample Code

```c
#include "cy_pdl.h"
#include <string.h>

#define DATACNT  (8UL)

/* 32-byte-aligned buffers required when D-cache is present */
CY_ALIGN(32) uint32_t srcData[DATACNT];
CY_ALIGN(32) uint32_t dstData[DATACNT];

CY_ALIGN(32) cy_stc_dma_descriptor_t dmaDescriptor;

cy_stc_dma_descriptor_config_t descriptorCfg =
{
    .retrigger       = CY_DMA_RETRIG_IM,
    .interruptType   = CY_DMA_DESCR,
    .triggerOutType  = CY_DMA_DESCR,
    .channelState    = CY_DMA_CHANNEL_ENABLED,
    .triggerInType   = CY_DMA_DESCR,
    .dataSize        = CY_DMA_WORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType  = CY_DMA_1D_TRANSFER,
    .srcAddress      = srcData,
    .dstAddress      = dstData,
    .srcXincrement   = 1,
    .dstXincrement   = 1,
    .xCount          = DATACNT,
    .srcYincrement   = 0,
    .dstYincrement   = 0,
    .yCount          = 1UL,
    .nextDescriptor  = NULL,
};

int main(void)
{
    __enable_irq();

    /* Populate source data */
    for (uint32_t i = 0U; i < DATACNT; i++) { srcData[i] = i; }

    /* 1. Initialise descriptor */
    if (CY_DMA_SUCCESS != Cy_DMA_Descriptor_Init(&dmaDescriptor, &descriptorCfg))
    {
        /* Handle error */
    }

    /* 2. Initialise channel */
    cy_stc_dma_channel_config_t channelCfg =
    {
        .descriptor  = &dmaDescriptor,
        .preemptable = false,
        .priority    = 3UL,
        .enable      = false,
        .bufferable  = false,
    };
    if (CY_DMA_SUCCESS != Cy_DMA_Channel_Init(DW0, 0UL, &channelCfg))
    {
        /* Handle error */
    }

    /* 3. Enable interrupt mask */
    Cy_DMA_Channel_SetInterruptMask(DW0, 0UL, CY_DMA_INTR_MASK);

    /* 4. Enable channel and block */
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
    SCB_CleanDCache_by_Addr((uint32_t*)srcData, sizeof(srcData));
    SCB_CleanDCache_by_Addr((uint32_t*)&dmaDescriptor, sizeof(dmaDescriptor));
#endif
    Cy_DMA_Channel_Enable(DW0, 0UL);
    Cy_DMA_Enable(DW0);

    /* 5. Software trigger */
    Cy_TrigMux_SwTrigger(PERI_0_TRIG_OUT_MUX_0_PDMA0_TR_IN0, CY_TRIGGER_TWO_CYCLES);

    /* Wait for completion */
    while (!Cy_DMA_Channel_GetInterruptStatus(DW0, 0UL)) { }
    Cy_DMA_Channel_ClearInterrupt(DW0, 0UL);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
    SCB_InvalidateDCache_by_Addr((uint32_t*)dstData, sizeof(dstData));
#endif

    /* Verify */
    if (0 != memcmp(srcData, dstData, sizeof(srcData)))
    {
        /* Transfer error */
    }

    for (;;) { }
}
```

### Expected Outcome
After the transfer completes, `dstData` contains a copy of `srcData`. The channel interrupt fires once (descriptor-level) and is cleared by the application.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `CY_DMA_INTR_CAUSE_SRC_BUS_ERROR` interrupt | DCache not cleaned before `Cy_DMA_Channel_Enable()` | Call `SCB_CleanDCache_by_Addr()` on the descriptor and source buffer immediately before enable |
| `CY_DMA_INTR_CAUSE_DST_BUS_ERROR` | Destination address not writeable or not mapped | Verify destination buffer is in writable, DMA-accessible memory |
| `CY_DMA_INTR_CAUSE_CURR_PTR_NULL` | Channel enabled with NULL descriptor pointer | Set descriptor via `Cy_DMA_Channel_SetDescriptor()` before enabling |
| DMA never fires | Hardware trigger not connected | Use `Cy_TrigMux_Connect()` to route source peripheral trigger to DW channel |
| Corrupt destination data | DCache not invalidated after transfer | Call `SCB_InvalidateDCache_by_Addr()` on destination buffer before CPU read |
| Transfer stops mid-chain | `channelState` set to `CY_DMA_CHANNEL_DISABLED` in intermediate descriptor | Set `channelState = CY_DMA_CHANNEL_ENABLED` in all non-terminal descriptors |

---

## Related Code Examples

- [PSOC™ Edge MCU: UART Transmit and Receive with DMA](https://github.com/Infineon/mtb-example-psoc-edge-uart-transmit-receive-dma)
- [PSOC™ Edge MCU: SPI with DMA](https://github.com/Infineon/mtb-example-psoc-edge-spi-dma)

## Related Application Notes
- Device TRM: *DataWire (DW) DMA* chapter

---

## Configuration Parameters Reference

### `cy_stc_dma_descriptor_config_t`

| Field | Type | Description |
|---|---|---|
| `descriptorType` | `cy_en_dma_descriptor_type_t` | `SINGLE_TRANSFER`, `1D_TRANSFER`, `2D_TRANSFER`, `CRC_TRANSFER` |
| `srcAddress` | `void *` | Source start address |
| `dstAddress` | `void *` | Destination start address |
| `srcXincrement` | `int32_t` | Address step between X-loop source elements (–2048 … 2047) |
| `dstXincrement` | `int32_t` | Address step between X-loop destination elements |
| `xCount` | `uint32_t` | Number of elements per X-loop (1–256) |
| `srcYincrement` | `int32_t` | Address step between Y-loop rows (source) |
| `dstYincrement` | `int32_t` | Address step between Y-loop rows (destination) |
| `yCount` | `uint32_t` | Number of X-loops per descriptor (1–256) |
| `dataSize` | `cy_en_dma_data_size_t` | `BYTE`, `HALFWORD`, `WORD` |
| `interruptType` | `cy_en_dma_trigger_type_t` | Trigger level for interrupt: `1ELEMENT`, `X_LOOP`, `DESCR`, `DESCR_CHAIN` |
| `triggerInType` | `cy_en_dma_trigger_type_t` | Input trigger granularity |
| `triggerOutType` | `cy_en_dma_trigger_type_t` | Output trigger granularity |
| `channelState` | `cy_en_dma_channel_state_t` | `ENABLED` or `DISABLED` after descriptor completes |
| `retrigger` | `cy_en_dma_retrigger_t` | `RETRIG_IM`, `RETRIG_4CYC`, `RETRIG_16CYC`, `WAIT_FOR_REACT` |
| `nextDescriptor` | `cy_stc_dma_descriptor_t *` | Pointer to next descriptor in chain (NULL to stop) |

### `cy_stc_dma_channel_config_t`

| Field | Type | Description |
|---|---|---|
| `descriptor` | `cy_stc_dma_descriptor_t *` | First descriptor to execute |
| `preemptable` | `bool` | Allow higher-priority channels to preempt |
| `priority` | `uint32_t` | Channel priority 0 (highest) … 3 |
| `enable` | `bool` | Enable channel immediately on init |
| `bufferable` | `bool` | Bufferable (posted) AHB write |

---

## Advanced Usage

### Descriptor Chaining (Ping-Pong)
```c
/* Two descriptors alternating between two buffers */
descriptorCfgA.nextDescriptor = &descriptorB;
descriptorCfgB.nextDescriptor = &descriptorA;
descriptorCfgA.channelState   = CY_DMA_CHANNEL_ENABLED;
descriptorCfgB.channelState   = CY_DMA_CHANNEL_ENABLED;
```

### CRC Transfer (CPUSS v2 only)
```c
cy_stc_dma_crc_config_t crcCfg =
{
    .polynomial      = 0x04C11DB7UL, /* CRC-32 */
    .lfsrInitVal     = 0xFFFFFFFFUL,
    .dataReverse     = false,
    .dataXor         = 0x00000000UL,
    .reminderReverse = false,
    .reminderXor     = 0x00000000UL,
};
Cy_DMA_Crc_Init(DW0, &crcCfg);
/* Use CY_DMA_CRC_TRANSFER descriptor type */
```

### Dynamic Address Update
```c
/* Redirect descriptor source without re-initialising */
Cy_DMA_Descriptor_SetSrcAddress(&dmaDescriptor, newSrcPtr);
/* Clean DCache if present before re-triggering */
```

---

## Industry Standards
The DMA driver is a general-purpose transport layer. When used in safety-critical systems, refer to the
device's safety manual for DMA error-handling requirements (IEC 61508, ISO 26262 considerations).

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
