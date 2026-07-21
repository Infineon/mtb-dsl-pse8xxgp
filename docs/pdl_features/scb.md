# SCB (Serial Communication Block) - Multi-Protocol Serial Driver for UART, I2C, and SPI

## Overview

The **SCB (Serial Communication Block)** driver provides a unified API for three industry-standard serial protocols — UART, I2C, and SPI — all backed by the same flexible hardware block. It supports both low-level register-direct access and high-level interrupt-driven operation, including EZ (autonomous) modes and DMA transfers, minimizing CPU overhead in communication-intensive applications.

> **Protocol-specific documentation:** For detailed usage, configuration, and examples for each protocol, see the dedicated Component Datasheets:
>
> | Protocol | Component Datasheet | Description |
> |----------|-------------------|-------------|
> | UART | [uart.md](uart.md) | Standard UART, SmartCard (ISO 7816-3), IrDA |
> | I2C | [i2c.md](i2c.md) | I2C master/slave, EZ-I2C autonomous mode |
> | SPI | [spi.md](spi.md) | SPI master/slave, Motorola/TI/MicroWire modes |

## Features

- **Multi-protocol support**: Single hardware block configurable as UART (Standard, SmartCard, IrDA), I2C (master/slave/master-slave), or SPI (master/slave, Motorola/TI/MicroWire)
- **EZ (autonomous) modes**: EZ-I2C and EZ-SPI enable address-triggered autonomous data transfer without CPU intervention
- **DMA-ready**: TX and RX FIFOs with configurable trigger levels for seamless DMA integration
- **Wake-from-DeepSleep**: Address-match (I2C) and activity-detect (UART/SPI) wakeup events keep power consumption low
- **Hardware flow control**: CTS/RTS support for UART; up to four slave-select lines for SPI master
- **IP variant coverage**: Multiple SCB IP variants with a common API surface

## When to Use

- Send debug or application data over UART to a PC terminal or another microcontroller → see [UART CDS](uart.md)
- Read/write sensors and EEPROMs on an I2C bus at 100 kbps, 400 kbps, or 1 Mbps → see [I2C CDS](i2c.md)
- Interface with external flash, DACs, ADCs, or display controllers over SPI up to tens of MHz → see [SPI CDS](spi.md)
- Implement an I2C slave that responds autonomously (EZ-I2C) while the CPU sleeps → see [I2C CDS](i2c.md)
- Build multi-drop SPI bus designs using four independent slave-select lines → see [SPI CDS](spi.md)

## Prerequisites

### Hardware Requirements

- A device with an SCB IP block
- Dedicated SCB pins routed to the I/O mux (HSIOM)
- For UART: external serial terminal or UART peer; for I2C: 4.7 kΩ external pull-up resistors on SDA and SCL; for SPI: matching master/slave hardware

### Software Requirements

- ModusToolbox 3.x or newer
- `cy_pdl.h` (or individual headers: `cy_scb_uart.h`, `cy_scb_i2c.h`, `cy_scb_spi.h`, `cy_scb_ezi2c.h`)

### Configure in the Tool

1. Open **Device Configurator** and select the **Peripherals** tab.
2. Expand **Communication** → enable an **SCB** instance and choose the desired personality: **UART**, **I2C**, **SPI**, **EZI2C**, or **EZISPI**.
3. Configure the parameters in the Parameters pane — see the protocol-specific CDS for detailed parameter tables:
   - [UART parameters](uart.md#configuration-parameters-reference)
   - [I2C parameters](i2c.md#configuration-parameters-reference)
   - [SPI parameters](spi.md#configuration-parameters-reference)
4. Click **File → Save**; the tool generates `cycfg_peripherals.c/.h` with ready-to-use `<alias>_config` and `<alias>_context` structures.
5. Call `init_cycfg_all()` at the start of `main()` to apply clocking and pin routing.

## Quick Start

For protocol-specific quick start guides and sample code, refer to the dedicated CDS:

- **UART**: [Quick Start & Examples](uart.md#quick-start) — 115200-8N1 hello-world with echo
- **I2C**: [Quick Start & Examples](i2c.md#quick-start) — Master write/read, EZ-I2C slave
- **SPI**: [Quick Start & Examples](spi.md#quick-start) — Master loopback, multi-byte transfer

## General Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `Cy_SCB_*_Init` returns error | SCB not enabled in Device Configurator | Enable the peripheral and re-save `.modus` |
| Mixed High/Low-Level API | Using both LL and HL in same instance | Use only one API style per SCB instance |
| EZ mode not responding | Interrupt not registered | EZ modes require SCB interrupt in NVIC |

For protocol-specific troubleshooting, see:
- [UART Troubleshooting](uart.md#troubleshooting)
- [I2C Troubleshooting](i2c.md#troubleshooting)
- [SPI Troubleshooting](spi.md#troubleshooting)

## Advanced Usage

### Wake-from-DeepSleep

SCB supports waking the device from DeepSleep when it detects activity:
- **I2C slave**: address-match wakeup — device wakes when the master addresses it. See [I2C Wake-from-DeepSleep](i2c.md#wake-from-deepsleep).
- **UART/SPI**: activity-detect wakeup — first start bit or SS assertion wakes the device. See [UART Wake](uart.md#wake-from-deepsleep) / [SPI Wake](spi.md#wake-from-deepsleep).

### DMA Integration

Set `rxFifoTriggerLevel` and `txFifoTriggerLevel` in the config structure to the desired FIFO watermarks. Connect the SCB DMA triggers (`TRIG_IN_MUX_*`) to a DMA channel for zero-copy transfers. Refer to the [DMA driver documentation](dma.md) for DMA channel setup.

## Related Code Examples

- [PSOC™ Edge MCU: UART Transmit and Receive with DMA](https://github.com/Infineon/mtb-example-psoc-edge-uart-transmit-receive-dma)
- [PSOC™ Edge MCU: I2C Controller / EZI2C Target](https://github.com/Infineon/mtb-example-psoc-edge-i2c-controller-ezi2c-target)
- [PSOC™ Edge MCU: SPI with DMA](https://github.com/Infineon/mtb-example-psoc-edge-spi-dma)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — SCB chapter


## Industry Standards

- I2C-bus specification: Standard-mode (100 kbps), Fast-mode (400 kbps), Fast-mode Plus (1 Mbps) per NXP UM10204
- SPI: Motorola SPI protocol (modes 0–3), Texas Instruments SPI, National Semiconductor MicroWire
- UART: TIA-232 / RS-232 compatible signaling (with external level shifter)
- IrDA: SIR (Serial Infrared) physical layer
- ISO 7816-3: SmartCard interface mode

## Protocol Component Datasheets

- [UART Component Datasheet](uart.md)
- [I2C Component Datasheet](i2c.md)
- [SPI Component Datasheet](spi.md)

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
