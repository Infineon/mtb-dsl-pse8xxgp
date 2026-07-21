# Ethos-U - Arm Ethos-U NPU Driver for microNPU Inference

## Overview
The `ethos_u` driver integrates the Arm Ethos-U NPU open-source driver into the PDL framework, enabling firmware to run Arm Ethos-U accelerated ML inference. It handles NPU initialisation, command-stream dispatch, interrupt routing, and power management.

## Features
- Supports Arm Ethos-U NPU variants (command-stream based, Vela-compiled models)
- Command-stream-based inference — model compiled offline by Vela compiler
- Interrupt-driven job completion with semaphore-based synchronisation
- Fast-memory (ITCM/DTCM) mapping for latency-critical activations
- Per-device security/privilege configuration at initialisation
- PMU (Performance Monitoring Unit) support via `pmu_ethosu.h`

## When to Use
- Running Vela-compiled TensorFlow Lite Micro models with full NPU acceleration
- Offloading vision, audio, or sensor ML workloads from the CPU cores
- Benchmarking ML performance with on-chip PMU counters
- Building multi-model pipelines with the on-chip NPU

## Prerequisites

### HW Requirements
- Device with an Ethos-U NPU; use the NPU base address defined in the device header
- Sufficient SRAM/DTCM for model command stream, weight data, and activation buffers

### SW Requirements
- ModusToolbox PDL (`cy_pdl.h`)
- Arm Ethos-U driver sources: `asset/drivers/ethos_u/third_party/` (Apache-2.0)
- Vela-compiled model (`.tflite` → command stream + weight binary)
- RTOS semaphore or bare-metal spin-wait for inference synchronisation

### Configure in the Tool
- Enable the NPU power island in the Device Configurator or call the appropriate `Cy_SysEnableU()` API
- Route the NPU interrupt to the appropriate CPU core

## Quick Start

### Steps
1. Call `init_cycfg_all()`.
2. Enable the NPU using the device-specific `Cy_SysEnableU()` function.
3. Install the NPU ISR and call `Cy_SysInt_Init()` + `NVIC_EnableIRQ()`.
4. Initialise the driver with `ethosu_init()`, providing the NPU base address.
5. Enable global interrupts: `__enable_irq()`.
6. Submit an inference job with `ethosu_invoke()`, passing base addresses for each tensor arena.
7. Wait for job completion (interrupt fires → `ethosu_irq_handler()` → semaphore post).
8. Read inference outputs from the output tensor arena.

### Sample Code

```c
#include "cy_pdl.h"
#include "ethosu_driver.h"

/* Provided by Vela compiler output */
extern const uint8_t  network_model_data[];
extern const size_t   network_model_size;
extern uint8_t        tensor_arena[];
extern const size_t   tensor_arena_size;

static struct ethosu_driver ethosu_drv;

void npu_irq_handler(void)
{
    ethosu_irq_handler(&ethosu_drv);
}

int main(void)
{
    init_cycfg_all();

    /* Enable NPU power island (device-specific) and obtain base address and IRQ */
    Cy_SysEnableNPU(true);   /* replace with device-specific API */
    const void *npu_base = (void *)NPU_BASE;        /* defined in device header */
    IRQn_Type   npu_irqn = npu_interrupt_IRQn;      /* defined in device header */

    /* Install IRQ handler */
    cy_stc_sysint_t irq_cfg = { .intrSrc = npu_irqn, .intrPriority = 2u };
    Cy_SysInt_Init(&irq_cfg, npu_irq_handler);
    NVIC_EnableIRQ(npu_irqn);

    /* Initialise the Ethos-U driver */
    if (ethosu_init(&ethosu_drv, npu_base,
                    NULL,  /* fast mem base (NULL if unused) */
                    0,     /* fast mem size */
                    0,     /* security disable */
                    1) != 0) {
        /* Handle error */
        while (1);
    }

    __enable_irq();

    /* Submit inference */
    const uint64_t *base_addrs[]    = { (uint64_t *)tensor_arena };
    const size_t    base_addr_sizes[] = { tensor_arena_size };

    int result = ethosu_invoke(&ethosu_drv,
                               network_model_data,
                               network_model_size,
                               base_addrs,
                               base_addr_sizes,
                               1 /* num_base_addr */);

    if (result != 0) {
        /* Inference failed */
        while (1);
    }

    /* Output is now in tensor_arena at the offset defined by the model */
    return 0;
}
```

### Expected Outcome
`ethosu_invoke()` blocks (or yields if RTOS is present) until the NPU signals completion via interrupt. On return, the output tensors within `tensor_arena` hold the inference results.

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| `ethosu_init()` returns non-zero | NPU not powered / base address wrong | Confirm NPU power enable API called; verify NPU base address |
| IRQ never fires | ISR not registered or NVIC not enabled | Check `Cy_SysInt_Init()` and `NVIC_EnableIRQ()` |
| Inference result is incorrect | Vela compile options mismatch | Recompile model with correct `--ethos-u-config` for your NPU variant |
| Bus fault during inference | Tensor arena not accessible to NPU | Place tensor arena in NPU-accessible SRAM; check MPC/PPC |
| `ethosu_invoke()` hangs | Semaphore not released | Ensure IRQ priority allows ISR to preempt calling task |

## Related Code Examples

- [PSOC™ Edge MCU: ML DeepCraft Deploy Vision](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-vision)
- [PSOC™ Edge MCU: ML Face ID](https://github.com/Infineon/mtb-example-psoc-edge-ml-face-id)

## Related Application Notes
- [Arm Ethos-U Driver](https://git.mlplatform.org/ml/ethos-u/ethos-u-core-driver.git)
- Refer to the device TRM for NPU chapter details
- [Vela Compiler documentation](https://pypi.org/project/ethos-u-vela/)

## Configuration Parameters Reference

| Parameter | Description |
|---|---|
| `ETHOSU_DRIVER_VERSION_MAJOR/MINOR/PATCH` | Driver version (0.16.0) |
| `ETHOSU_SEMAPHORE_WAIT_FOREVER` | Timeout value for blocking inference wait |
| `ETHOSU_SEMAPHORE_WAIT_INFERENCE` | Default timeout (= WAIT_FOREVER unless overridden) |
| `ethosu_driver.fast_memory` | DTCM base address for fast activation scratch |
| `ethosu_driver.fast_memory_size` | Size of fast memory region |

## Advanced Usage
- **PMU counters**: Use `pmu_ethosu.h` APIs to measure cycle counts, MAC utilisation, and memory bandwidth per inference.
- **Fast memory**: Pass DTCM base and size to `ethosu_init()` to store intermediate activations in tightly-coupled memory for lower latency.
- **Multiple NPU instances**: Allocate separate `ethosu_driver` structs per NPU instance; the driver maintains a global driver list.
- **RTOS integration**: Replace the default semaphore with an RTOS semaphore by implementing `ethosu_semaphore_create/take/give` stubs.
- **Security**: Set `security_enable = 1` and configure TrustZone SAU/MPC to restrict NPU access to Secure world.

## Industry Standards
- Arm Ethos-U NPU architecture specification (Arm IHI 0109)
- TensorFlow Lite Micro quantisation spec (8-bit symmetric / asymmetric)

---
© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
