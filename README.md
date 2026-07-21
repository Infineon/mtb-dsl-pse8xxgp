# ModusToolbox™ PSE8xxGP Device Support Library (DSL) - Device support for PSOC™ Edge E8x2, E8x3, E8x5, E8x6 MCUs

# Overview

The ModusToolbox™ Device Support Library (DSL) provides the components required to develop software for the PSOC™ Edge E8x2, E8x3, E8x5, and E8x6 MCUs (6 MB total RAM: 5 MB System SRAM + 1 MB SRAM). It is a required dependency for every application targeting these devices and is added automatically when a project is created for a supported board in ModusToolbox™.

# When to Use

The DSL comprises several components. The sections below describe each component and the conditions under which it should be used.

## Peripheral Driver Library (PDL)

The PDL is the primary interface for programming the device. It provides low-level C APIs to configure, initialize, and operate the on-chip peripherals (for example, GPIO, SCB/UART/SPI/I2C, TCPWM, DMA, SMIF, and Crypto), together with the CMSIS device headers.

Use the PDL when developing device-specific firmware that requires full control of a peripheral and maximum efficiency. It is the default choice for most application code. Include it with `#include "cy_pdl.h"`.

PDL ships a set of Markdown Component Datasheets, one per peripheral feature. Each datasheet describes a feature's purpose, applicable use cases, Device Configurator configuration, and API usage in a plain-text format. Refer to them when configuring or modifying a peripheral.

Please refer to :
- [Component Datasheets](./docs/pdl_features/PDL.md)
- [Library documentation](https://github.com/Infineon/mtb-dsl-pse8xxgp/blob/master/pdl/README.md)

## Hardware Abstraction Layer (HAL)

The HAL provides a generic, device-agnostic API that wraps the lower-level PDL. It addresses the runtime behavior of a peripheral rather than its initialization; the hardware is initialized with the PDL (or the Device Configurator), after which a HAL object is set up over it.

Use the HAL when an application or middleware must be portable across multiple device families and requires a stable, device-independent interface rather than device-specific PDL calls. For code that targets PSOC™ Edge exclusively, the PDL alone is generally the more suitable option. Include it with `#include "mtb_hal.h"`.

Please refer to :
- [Library documentation](https://github.com/Infineon/mtb-dsl-pse8xxgp/blob/master/hal/README.md)

## Device Utilities (device-utils)

Ready-made System Power Management (SysPm) callback implementations for peripherals that need special handling when the MCU enters or exits DeepSleep. Use these when the application relies on low-power modes and requires validated power-transition handling without implementing the callbacks manually. Include with `#include "mtb_syspm_callbacks.h"`.

## Device Configurator Personalities and Device Database

Configuration personalities and the supplemental device database consumed by the ModusToolbox™ Device Configurator. You do not include these in code; the Device Configurator uses them so you can set up clocks, pins, and peripherals graphically and generate the matching initialization code.

## NN Kernel

A neural-network kernel that targets the on-chip hardware accelerator. Use it when performing machine-learning inference on the device. Include with `#include "cy_nn_kernel.h"`.

## GNU make Build System

The make build recipe and scripts used to compile and program applications for these devices. The build system uses it automatically; it is not invoked directly.

# Prerequisites

## Hardware Requirements

Any supported PSOC™ Edge evaluation kit, for example:
* KIT_PSE84_EVAL_EPC2 - PSOC™ Edge E84 Evaluation Kit (EPC2 variant)
* KIT_PSE84_EVAL_EPC4 - PSOC™ Edge E84 Evaluation Kit (EPC4 variant)
* KIT_PSE84_AI - PSOC™ Edge E84 AI Kit
* KIT_PSE84_HMI - PSOC™ Edge E84 HMI Kit

Refer to the kit user guide for power supply, debug probe (KitProg3/MiniProg4), and pin-connection requirements. See [PSOC™ Edge Development Kits](https://documentation.infineon.com/psocedge/docs/hgn1762692110909) for the full list of supported boards.

## Software Requirements

| Tool | Version |
|------|---------|
| ModusToolbox™ | 3.x |
| GCC Compiler | 14.2.1 |
| IAR Compiler | 9.70 |
| ARM Compiler 6 | 6.22 |
| LLVM_ARM Compiler | 19.1.5 |

Development is supported in any IDE supported by ModusToolbox™ (for example, VS Code for ModusToolbox™ or Eclipse for ModusToolbox™), as well as from the command line. The DSL and all of its dependencies (`cmsis`, `device-db`, `core-make`, `async-transfer`, `mtb-srf`) are added automatically when a project is created; they are not added manually.

# Quick Start

This section describes the workflow for developing an application with the DSL, from creating a project through programming the device.

1. In the ModusToolbox™ Project Creator, select a PSOC™ Edge board and create an application from the supported code examples; for example, [mtb-example-psoc-edge-hello-world](https://github.com/Infineon/mtb-example-psoc-edge-hello-world) is a suitable starting point. The DSL and its dependencies are added automatically.
2. Develop the application on the primary (CM33) core project. Configure clocks, pins, and peripherals in the Device Configurator (the BSP provides a working default configuration), then operate them from code.
3. Include the header for the component required by the task (see [When to Use](#when-to-use)). For example, driving a UART uses the PDL, which requires `#include "cy_pdl.h"` rather than an individual peripheral header; refer to the [UART Component Datasheet](./docs/pdl_features/uart.md) for configuration and API usage. `cy_pdl.h` is an umbrella header that includes every available PDL driver for the device.
4. Build and program the application with an IDE for ModusToolbox™ or with `make program`.

Note: The default code example, as provided out of the box, blinks the user LED.

Peripheral configuration is performed in the Device Configurator. For a description of a peripheral's parameters and APIs, use the documentation links provided within the Device Configurator.

# References

* [Device Support Library API Reference Manual](https://infineon.github.io/mtb-dsl-pse8xxgp/html/index.html)
* [PSOC™ Edge documentation](https://documentation.infineon.com/psocedge/docs/huf1750399463231)
* [PSOC™ Edge product overview](https://www.infineon.com/products/microcontroller/32-bit-psoc-arm-cortex/32-bit-psoc-edge-arm)
* [ModusToolbox™ software](https://www.infineon.com/design-resources/development-tools/sdk/modustoolbox-software)
* [Infineon GitHub](https://github.com/infineon)

# Release Notes and Changelog
- [RELEASE.md](https://github.com/Infineon/mtb-dsl-pse8xxgp/blob/master/RELEASE.md)

# License

- [LICENSE](https://github.com/Infineon/mtb-dsl-pse8xxgp/blob/master/LICENSE)
- [EULA](https://github.com/Infineon/mtb-dsl-pse8xxgp/blob/master/EULA)

# Copyright

(c) (2019-2026), Infineon Technologies AG, or an affiliate of Infineon Technologies AG. All rights reserved.