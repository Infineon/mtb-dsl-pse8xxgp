# AXIDMAC - High-Throughput AXI DMA Controller

## Overview
The AXIDMAC driver (`cy_axidmac.h`) configures the `MXSAXIDMAC` AXI-based DMA controller, providing
high-bandwidth memory-to-memory and memory-to/from-peripheral data movement with 1D, 2D, and 3D
descriptor types and support for CM55 DTCM address remapping.

---

## Features
- **Multiple channels** with four priority levels (device-specific channel count)
- **1D, 2D, and 3D memory-copy** descriptor types with independent M/X/Y loop counts (1–65536)
- **Descriptor chaining** for complex multi-stage transfers
- **AXI bus** — higher throughput than DataWire DW blocks for large memory transfers
- **Configurable trigger in/out** and interrupt per descriptor completion
- **DTCM address remapping** for CM55 private memory access by the AXI fabric
- **Debug cache coherency support** — clean DCache before `Cy_AXIDMAC_Channel_Enable()`
- Interrupt causes: Completion, Src/Dst Bus Error, Invalid Descriptor, Null Pointer, Active-Channel Disabled

---

## When to Use
- High-throughput memory copy operations (frame buffer copy, neural network weight loading)
- Moving data between SOCMEM, FLASH, and peripheral FIFOs at full AXI bus speed
- Multi-dimensional data rearrangement (matrix transpose, stride copy) using 2D/3D descriptors
- Streaming from CM55 DTCM to SOCMEM with address-remap support
- Offloading bulk data from peripherals (image sensor, audio) with minimal CPU involvement

---

## Prerequisites

### Hardware Requirements
- `MXSAXIDMAC` IP (`SAXI_DMAC` base address)
- AXI-accessible source and destination memory regions (SOCMEM, FLASH, peripherals)
- For DTCM data: use `cy_DTCMRemapAddr()` to translate CM55-private addresses

### Software Requirements
- `cy_pdl.h` (includes `cy_axidmac.h`)
- Optional: `cy_trigmux.h` for hardware trigger routing

### Configure in the Tool (ModusToolbox Device Configurator)

| Parameter | Description |
|---|---|
| `Channel Number` | 0 … N-1 |
| `Descriptor Type` | 1D / 2D / 3D Memory Copy |
| `M / X / Y Count` | Loop counts (1–65536 each) |
| `Trigger In` | Hardware peripheral trigger or software |
| `Trigger Out` | Output trigger to next peripheral |
| `Interrupt Type` | M-loop / X-loop / Descriptor / Chain |
| `Priority` | 0 (highest) … 3 (lowest) |
| `Retrigger` | Immediate / 4 cycles / 16 cycles / Wait |

---

## Quick Start

**Step 1.** Allocate descriptor and buffers in **AXI-accessible memory** (use `CY_SECTION(".cy_socmem_data")` or equivalent).
**Step 2.** Initialise a descriptor with `Cy_AXIDMAC_Descriptor_Init()`.
**Step 3.** Initialise the channel with `Cy_AXIDMAC_Channel_Init()`.
**Step 4.** Enable the channel with `Cy_AXIDMAC_Channel_Enable()` and the block with `Cy_AXIDMAC_Enable()`.
**Step 5.** Fire a software trigger or rely on a hardware peripheral trigger.

### Sample Code — SOCMEM Buffers (non-DTCM)

```c
#include "cy_pdl.h"

#define DATACNT  (8UL)

/* Place descriptors and buffers in SOCMEM (AXI-accessible) */
CY_SECTION(".cy_socmem_data") cy_stc_axidmac_descriptor_t firstDescriptor;
CY_SECTION(".cy_socmem_data") cy_stc_axidmac_descriptor_t nextDescriptor;
CY_SECTION(".cy_socmem_data") uint32_t src[DATACNT];
CY_SECTION(".cy_socmem_data") uint32_t dst[DATACNT];

CY_SECTION(".cy_socmem_data") cy_stc_axidmac_descriptor_config_t descriptorCfg =
{
    .retrigger      = CY_AXIDMAC_RETRIG_IM,
    .interruptType  = CY_AXIDMAC_DESCR,
    .triggerOutType = CY_AXIDMAC_DESCR,
    .channelState   = CY_AXIDMAC_CHANNEL_ENABLED,
    .triggerInType  = CY_AXIDMAC_DESCR,
    .descriptorType = CY_AXIDMAC_2D_MEMORY_COPY,
    .srcAddress     = src,
    .dstAddress     = dst,
    .mCount         = 1U,
    .srcXincrement  = 1U,
    .dstXincrement  = 1U,
    .xCount         = DATACNT,
    .srcYincrement  = 0U,
    .dstYincrement  = 0U,
    .yCount         = 1UL,
    .nextDescriptor = &nextDescriptor,
};

int main(void)
{
    __enable_irq();

    /* Populate source */
    for (uint32_t i = 0U; i < DATACNT; i++) { src[i] = i; }

    /* 1. Initialise descriptor */
    if (CY_AXIDMAC_SUCCESS != Cy_AXIDMAC_Descriptor_Init(&firstDescriptor, &descriptorCfg))
    {
        /* Handle error */
    }

    /* 2. Initialise channel */
    cy_stc_axidmac_channel_config_t channelCfg =
    {
        .descriptor = &firstDescriptor,
        .enable     = false,
        .bufferable = false,
    };
    if (CY_AXIDMAC_SUCCESS != Cy_AXIDMAC_Channel_Init(SAXI_DMAC, 0UL, &channelCfg))
    {
        /* Handle error */
    }

    /* 3. Set interrupt mask */
    Cy_AXIDMAC_Channel_SetInterruptMask(SAXI_DMAC, 0UL, CY_AXIDMAC_INTR_COMPLETION);

    /* 4. Enable channel and block */
    Cy_AXIDMAC_Channel_SetDescriptor(SAXI_DMAC, 0UL, &firstDescriptor);
    Cy_AXIDMAC_Channel_SetPriority(SAXI_DMAC, 0UL, 3UL);
    Cy_AXIDMAC_Channel_Enable(SAXI_DMAC, 0UL);
    Cy_AXIDMAC_Enable(SAXI_DMAC);

    /* 5. Wait for completion */
    while (!(Cy_AXIDMAC_Channel_GetInterruptStatus(SAXI_DMAC, 0UL) & CY_AXIDMAC_INTR_COMPLETION)) { }
    Cy_AXIDMAC_Channel_ClearInterrupt(SAXI_DMAC, 0UL, CY_AXIDMAC_INTR_COMPLETION);

    for (;;) { }
}
```

### Sample Code — CM55 DTCM Buffers

```c
#include "cy_pdl.h"

#define DATACNT  (8UL)

/* Buffers in DTCM (CM55 private memory — default .data section) */
cy_stc_axidmac_descriptor_t dtcmDescriptor;
uint32_t dtcmSrc[DATACNT];
uint32_t dtcmDst[DATACNT];

int main(void)
{
    __enable_irq();

    cy_stc_axidmac_descriptor_config_t cfg =
    {
        .retrigger      = CY_AXIDMAC_RETRIG_IM,
        .interruptType  = CY_AXIDMAC_DESCR,
        .triggerOutType = CY_AXIDMAC_DESCR,
        .channelState   = CY_AXIDMAC_CHANNEL_ENABLED,
        .triggerInType  = CY_AXIDMAC_DESCR,
        .descriptorType = CY_AXIDMAC_2D_MEMORY_COPY,
        .srcAddress     = dtcmSrc,
        .dstAddress     = dtcmDst,
        .mCount         = 1U,
        .srcXincrement  = 1U,
        .dstXincrement  = 1U,
        .xCount         = DATACNT,
        .srcYincrement  = 0U,
        .dstYincrement  = 0U,
        .yCount         = 1UL,
        .nextDescriptor = NULL,
    };

    /* Remap DTCM addresses for AXI access */
    cfg.srcAddress = (void *)cy_DTCMRemapAddr(dtcmSrc);
    cfg.dstAddress = (void *)cy_DTCMRemapAddr(dtcmDst);

    Cy_AXIDMAC_Descriptor_Init(&dtcmDescriptor, &cfg);

    cy_stc_axidmac_channel_config_t chCfg =
    {
        .descriptor = (cy_stc_axidmac_descriptor_t *)cy_DTCMRemapAddr(&dtcmDescriptor),
        .enable     = false,
        .bufferable = false,
    };
    Cy_AXIDMAC_Channel_Init(SAXI_DMAC, 0UL, &chCfg);
    Cy_AXIDMAC_Channel_Enable(SAXI_DMAC, 0UL);
    Cy_AXIDMAC_Enable(SAXI_DMAC);

    for (;;) { }
}
```

### Expected Outcome
After the transfer completes, the destination buffer contains a copy of the source. The completion interrupt fires at descriptor granularity.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `CY_AXIDMAC_INTR_SRC_BUS_ERROR` | Source not in AXI-accessible memory | Move source/descriptor to SOCMEM or use `cy_DTCMRemapAddr()` for DTCM |
| `CY_AXIDMAC_INTR_DST_BUS_ERROR` | Destination not writable or not AXI-accessible | Verify destination memory region is writable from the AXI fabric |
| `CY_AXIDMAC_INTR_INVALID_DESCR_TYPE` | Unsupported descriptor type | Use only `1D_MEMORY_COPY`, `2D_MEMORY_COPY`, or `3D_MEMORY_COPY` |
| `CY_AXIDMAC_INTR_CURR_PTR_NULL` | Channel enabled with NULL descriptor | Set a valid descriptor before `Cy_AXIDMAC_Channel_Enable()` |
| Transfer silently does nothing | Block not enabled | Call `Cy_AXIDMAC_Enable(SAXI_DMAC)` after enabling the channel |
| DTCM data corrupted | DTCM address not remapped | Use `cy_DTCMRemapAddr()` for all DTCM pointers passed to the AXI DMA |
| Interrupt never fires | Interrupt mask not set | Call `Cy_AXIDMAC_Channel_SetInterruptMask()` with desired interrupt bits |

---

## Related Code Examples

- [PSOC™ Edge MCU: UART Transmit and Receive with DMA](https://github.com/Infineon/mtb-example-psoc-edge-uart-transmit-receive-dma)
- [PSOC™ Edge MCU: SPI with DMA](https://github.com/Infineon/mtb-example-psoc-edge-spi-dma)

## Related Application Notes
- Device TRM: *AXI DMA Controller (AXIDMAC)* chapter

---

## Configuration Parameters Reference

### `cy_stc_axidmac_descriptor_config_t`

| Field | Type | Description |
|---|---|---|
| `descriptorType` | `cy_en_axidmac_descriptor_type_t` | `1D_MEMORY_COPY`, `2D_MEMORY_COPY`, `3D_MEMORY_COPY` |
| `srcAddress` | `void const *` | Source start address (must be AXI-accessible) |
| `dstAddress` | `void *` | Destination start address |
| `mCount` | `uint32_t` | Outer (M) loop count for 3D transfers (1–65536) |
| `srcXincrement` | `int32_t` | X-loop source address step (–32768 … 32767) |
| `dstXincrement` | `int32_t` | X-loop destination address step |
| `xCount` | `uint32_t` | Number of elements per X-loop (1–65536) |
| `srcYincrement` | `int32_t` | Y-loop source address step |
| `dstYincrement` | `int32_t` | Y-loop destination address step |
| `yCount` | `uint32_t` | Number of X-loops per descriptor (1–65536) |
| `interruptType` | `cy_en_axidmac_trigger_type_t` | `M_LOOP`, `X_LOOP`, `DESCR`, `DESCR_CHAIN` |
| `triggerInType` | `cy_en_axidmac_trigger_type_t` | Input trigger granularity |
| `triggerOutType` | `cy_en_axidmac_trigger_type_t` | Output trigger granularity |
| `channelState` | `cy_en_axidmac_channel_state_t` | `ENABLED` or `DISABLED` after descriptor completes |
| `retrigger` | `cy_en_axidmac_retrigger_t` | `RETRIG_IM`, `RETRIG_4CYC`, `RETRIG_16CYC`, `WAIT_FOR_REACT` |
| `nextDescriptor` | `cy_stc_axidmac_descriptor_t *` | Next chained descriptor (NULL to stop) |

### `cy_stc_axidmac_channel_config_t`

| Field | Type | Description |
|---|---|---|
| `descriptor` | `cy_stc_axidmac_descriptor_t *` | Initial descriptor pointer |
| `priority` | `uint32_t` | Channel priority 0 (highest) … 3 |
| `enable` | `bool` | Enable channel immediately on init |
| `bufferable` | `bool` | AXI bufferable write |

---

## Advanced Usage

### 3D Descriptor (Volume/Matrix Copy)
```c
cfg.descriptorType = CY_AXIDMAC_3D_MEMORY_COPY;
cfg.mCount         = NUM_FRAMES;   /* outer loop */
cfg.yCount         = NUM_ROWS;     /* middle loop */
cfg.xCount         = NUM_COLS;     /* inner loop */
/* M/X/Y increments define the stride pattern */
```

### Dynamic Address Redirect
```c
/* Redirect to a new source buffer without re-initialising */
Cy_AXIDMAC_Descriptor_SetSrcAddress(&descriptor, newSrcPtr);
Cy_AXIDMAC_Descriptor_SetDstAddress(&descriptor, newDstPtr);
```

### Chained Descriptor Pipeline
```c
/* Descriptor A → B → A (ping-pong) */
cfgA.nextDescriptor = &descriptorB;
cfgB.nextDescriptor = &descriptorA;
cfgA.channelState   = CY_AXIDMAC_CHANNEL_ENABLED;
cfgB.channelState   = CY_AXIDMAC_CHANNEL_ENABLED;
```

### Monitoring Loop Progress
```c
uint32_t mIdx = Cy_AXIDMAC_Channel_GetCurrentMloopIndex(SAXI_DMAC, 0UL);
uint32_t xIdx = Cy_AXIDMAC_Channel_GetCurrentXloopIndex(SAXI_DMAC, 0UL);
uint32_t yIdx = Cy_AXIDMAC_Channel_GetCurrentYloopIndex(SAXI_DMAC, 0UL);
```

---

## Industry Standards
Not directly applicable. For safety-critical applications refer to the device safety manual for
AXI DMA error-injection and parity-protection guidance (IEC 61508, ISO 26262).

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
