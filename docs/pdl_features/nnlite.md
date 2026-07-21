# NNLite - Neural Network Lite Hardware Accelerator

## Overview
The NNLite driver (MXNNLITE) provides a PDL API for configuring and running the on-chip neural network lite accelerator. It enables low-latency, hardware-accelerated inference of convolutional and fully-connected layers directly from MCU firmware without an external NPU.

## Features
- Hardware-accelerated convolution, pooling, and fully-connected layer execution
- Interrupt-driven and polling completion modes
- Multiple IP versions supported
- Memory-streamer error detection for activation, weight, bias, and output streams
- Saturation and accumulation overflow interrupt reporting
- Datawire trigger support for pipelined operation

## When to Use
- Deploying a compact ML model (e.g., keyword detection, gesture recognition) on an Arm Cortex-M55 subsystem
- Offloading inference from the CPU to reduce active-cycle power consumption
- Integrating with a tool-generated network descriptor produced by a quantisation-aware training flow
- Running layer-by-layer inference where DMA feeds activation and weight buffers

## Prerequisites

### HW Requirements
- Device with MXNNLITE IP
- Sufficient SRAM for weight, activation, and output buffers

### SW Requirements
- ModusToolbox PDL (`cy_pdl.h`)
- System clock initialised; MXNNLITE peripheral clock enabled via `Cy_SysClk_PeriGroupSlaveInit()`

### Configure in the Tool
- Enable the MXNNLITE peripheral clock in the Device Configurator
- Map weight and activation memory regions to non-cached, non-bufferable SRAM if needed

## Quick Start

### Steps
1. Call `init_cycfg_all()` to apply Device Configurator settings.
2. Enable the MXNNLITE peripheral clock with `Cy_SysClk_PeriGroupSlaveInit()`.
3. Initialise the accelerator with `Cy_NNLite_Init()`.
4. Load network descriptors (layer configs, weights, biases) into SRAM.
5. Start inference with `Cy_NNLite_Start()`.
6. Wait for completion via interrupt (`Cy_NNLite_InterruptHandler()`) or polling `Cy_NNLite_GetOperationStatus()`.
7. Read output activations from the configured output buffer.

### Sample Code

```c
#include "cy_pdl.h"

/* Supplied by the ML toolchain */
extern const uint8_t  network_weights[];
extern const uint32_t network_weights_size;
extern cy_stc_nnlite_layer_config_t layer_config[];
extern const uint32_t layer_count;

static cy_stc_nnlite_context_t nnlite_ctx;
static uint8_t output_buffer[256];

void NNLite_InferenceExample(void)
{
    cy_en_nnlite_status_t status;

    /* 1. Enable peripheral clock */
    Cy_SysClk_PeriGroupSlaveInit(CY_MMIO_MXNNLITE_PERI_NR,
                                  CY_MMIO_MXNNLITE_GROUP_NR,
                                  CY_MMIO_MXNNLITE_SLAVE_NR,
                                  CY_MMIO_MXNNLITE_CLK_HF_NR);

    /* 2. Initialise the NNLite block */
    status = Cy_NNLite_Init(NNLITE0, &nnlite_ctx);
    CY_ASSERT(status == CY_NNLITE_SUCCESS);

    /* 3. Enable interrupts and set mask */
    Cy_NNLite_SetInterruptMask(NNLITE0, NNLITE_INTR_ENABLE_MASK);
    __enable_irq();

    /* 4. Start a single layer inference */
    status = Cy_NNLite_Start(NNLITE0, &layer_config[0], &nnlite_ctx);
    CY_ASSERT(status == CY_NNLITE_SUCCESS);

    /* 5. Poll for completion */
    uint32_t opStatus = 0u;
    do {
        Cy_NNLite_GetOperationStatus(NNLITE0, &opStatus);
    } while (opStatus == 0u);

    /* 6. De-initialise when done */
    Cy_NNLite_DeInit(NNLITE0, &nnlite_ctx);

    /* 7. Disable peripheral clock */
    Cy_SysClk_PeriGroupSlaveDeinit(CY_MMIO_MXNNLITE_PERI_NR,
                                    CY_MMIO_MXNNLITE_GROUP_NR,
                                    CY_MMIO_MXNNLITE_SLAVE_NR);
}
```

### Expected Outcome
After `Cy_NNLite_Start()` returns, the hardware processes the configured layer. The operation-status flag sets to non-zero on completion, and the output activations are available in the configured output SRAM region.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| `Cy_NNLite_Init()` returns error | Peripheral clock not enabled | Call `Cy_SysClk_PeriGroupSlaveInit()` before `Init` |
| `NNLITE_INTR_ERRORS_MASK` fires | AXI bus fault reading weight/activation SRAM | Verify buffer addresses and sizes against SRAM map |
| Saturation interrupt fires | Accumulator overflow | Increase weight quantisation precision or rescale inputs |
| Output buffer contains garbage | Cache coherency issue | Flush/invalidate D-cache over output buffer before reading |
| Hangs in polling loop | Layer descriptor misconfigured | Check layer dimensions match weight tensor shape |

## Related Code Examples

- [PSOC™ Edge MCU: ML DeepCraft Deploy Vision](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-vision)
- [PSOC™ Edge MCU: ML Face ID](https://github.com/Infineon/mtb-example-psoc-edge-ml-face-id)
- [PSOC™ Edge MCU: ML DeepCraft Deploy Ready Model](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-ready-model)

## Related Application Notes
- Refer to the device Technical Reference Manual (TRM), MXNNLITE chapter

## Configuration Parameters Reference

| Parameter | Type | Description |
|---|---|---|
| `NNLITE_INTR_ENABLE_MASK` | Macro | Interrupt enable mask covering DONE + all streamer errors |
| `NNLITE_NO_SCALING` | Macro | Output scaling factor that disables scaling (IEEE 754 = 1.0) |
| `NNLITE_BYTE_CLIPING` | Macro | 8-bit output clipping mask |
| `NNLITE_TANH_FRAC_BITS` | Macro | Fractional bits from tanh interpolation (24) |
| `NNLITE_SIGM_FRAC_BITS` | Macro | Fractional bits from sigmoid interpolation (25) |

## Advanced Usage
- **v2 saturation flags**: Read per-stage saturation via `NNLITE_SATURATION_MASK` fields to fine-tune quantisation.- **Datawire trigger**: Call `Cy_NNLite_DatawireTriggerEnable()` to start inference from a hardware trigger source, enabling zero-CPU-cycle pipelining with a DMA completion event.
- **Multiple layers**: Iterate over `layer_config[]` array, calling `Cy_NNLite_Start()` for each layer; reuse the same context structure.
- **Power gating**: Disable the peripheral clock between inference bursts to minimise leakage.

## Industry Standards
- Quantisation format compatible with TensorFlow Lite Micro 8-bit symmetric quantisation

---
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
