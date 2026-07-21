# UART (Universal Asynchronous Receiver-Transmitter) - SCB-Based Serial Driver

## Overview

The **UART driver** configures an SCB hardware block as an asynchronous serial port supporting Standard UART, SmartCard (ISO 7816-3), and IrDA (Serial Infrared) modes. It provides configurable baud rates, data widths (4–16 bits), parity, stop bits, hardware flow control (CTS/RTS), and both interrupt-driven and DMA-assisted transfers.

## Features

- **Three UART modes**: Standard, SmartCard (ISO 7816-3), IrDA (SIR physical layer)
- **Configurable baud rate**: determined by `clk_scb / oversample` (oversample 8–16 typical)
- **Data width**: 4 to 16 bits per frame
- **Parity**: None, Even, or Odd
- **Stop bits**: 1, 1.5, 2, or more (half-bit granularity)
- **Hardware flow control**: CTS/RTS for backpressure-based communication
- **TX/RX FIFOs** with configurable trigger levels for interrupt and DMA integration
- **MSb-first or LSb-first** bit ordering
- **Activity-detect Wake-from-DeepSleep**: device wakes on first start-bit detection
- **Break detection and generation** for LIN-bus and similar protocols
- **DMA integration**: FIFO watermark triggers for zero-copy data movement

## When to Use

- Send debug or application data over UART to a PC terminal or another microcontroller
- Implement command-line interfaces (CLI) over serial
- Interface with GPS modules, Bluetooth modules, or other UART peripherals
- SmartCard reader applications (ISO 7816-3)
- IrDA short-range wireless communication
- Low-power designs with UART activity wake from DeepSleep

## Prerequisites

### Hardware Requirements

- A device with an SCB IP block
- Dedicated SCB pins routed to the I/O mux (HSIOM) for TX and RX (and optionally CTS/RTS)
- External serial terminal, UART peer, or USB-to-UART bridge

### Software Requirements

- ModusToolbox 3.x or newer
- `cy_pdl.h` (or individual header: `cy_scb_uart.h`)

### Configure in the Tool

1. Open **Device Configurator** and select the **Peripherals** tab.
2. Expand **Communication** → enable an **SCB** instance and choose the **UART** personality.
3. Configure the parameters in the Parameters pane (see table below).
4. Click **File → Save**; the tool generates `cycfg_peripherals.c/.h` with ready-to-use `<alias>_config` and `<alias>_context` structures.
5. Call `init_cycfg_all()` at the start of `main()` to apply clocking and pin routing.

**UART personality parameters (uart-\*.cypersonality)**

| Parameter | Value for 115200-8N1 | Notes |
|-----------|----------------------|-------|
| UART Mode | Standard | Standard UART |
| Baud Rate | 115200 | Oversampling × clk_scb = baud |
| Data Width | 8 | 4–16 bits supported |
| Parity | None | Even/Odd available |
| Stop Bits | 1 | 0.5–4 in half-bit steps |
| CTS/RTS | Disabled | Enable for hardware flow control |

## Quick Start

**Step 1:** Enable an SCB in UART personality (Device Configurator), 115200-8N1.
**Step 2:** Connect TX/RX pins; save the `.modus` file.
**Step 3:** Add the sample code below to `main.c`.
**Step 4:** Build, program, and open a serial terminal at 115200 baud.

**Expected Outcome:** `"Hello, UART!\r\n"` appears in the terminal; typed characters are echoed back.

### Sample Code

#### Bare Metal UART Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

/* Context allocated by the application */
cy_stc_scb_uart_context_t uartContext;

/* Interrupt handler - must be registered in the vector table */
void UART_Interrupt(void)
{
    Cy_SCB_UART_Interrupt(SCB5, &uartContext);
}

int main(void)
{
    /* Initialize the device and peripherals from Device Configurator */
    init_cycfg_all();
    __enable_irq();

    /* The generated config structure is named after the personality Alias */
    Cy_SCB_UART_Init(SCB5, &scb_5_uart_config, &uartContext);
    Cy_SCB_UART_Enable(SCB5);

    /* Transmit a string */
    Cy_SCB_UART_PutString(SCB5, "Hello, UART!\r\n");

    for (;;)
    {
        /* Echo received characters */
        if (Cy_SCB_UART_GetNumInRxFifo(SCB5) > 0U)
        {
            uint32_t ch = Cy_SCB_UART_Get(SCB5);
            Cy_SCB_UART_Put(SCB5, ch);
        }
    }
}
```

#### UART with Ring Buffer (Interrupt-Driven)

```c
#include "cy_pdl.h"
#include "cybsp.h"

#define RX_BUF_SIZE (128U)

cy_stc_scb_uart_context_t uartContext;
uint8_t rxRingBuffer[RX_BUF_SIZE];

void UART_Interrupt(void)
{
    Cy_SCB_UART_Interrupt(SCB5, &uartContext);
}

int main(void)
{
    init_cycfg_all();
    __enable_irq();

    Cy_SCB_UART_Init(SCB5, &scb_5_uart_config, &uartContext);

    /* Start ring buffer for background reception */
    Cy_SCB_UART_StartRingBuffer(SCB5, rxRingBuffer, RX_BUF_SIZE, &uartContext);

    Cy_SCB_UART_Enable(SCB5);

    Cy_SCB_UART_PutString(SCB5, "UART Ring Buffer Ready\r\n");

    for (;;)
    {
        /* Check if data available in ring buffer */
        uint32_t numAvailable = Cy_SCB_UART_GetNumInRingBuffer(SCB5, &uartContext);
        if (numAvailable > 0U)
        {
            uint8_t data;
            Cy_SCB_UART_Receive(SCB5, &data, 1U, &uartContext);
            Cy_SCB_UART_Put(SCB5, data);  /* Echo back */
        }
    }
}
```

### Expected Outcome

- **UART TX**: `"Hello, UART!\r\n"` visible in serial terminal.
- **UART Echo**: Each keystroke typed in the terminal is echoed back.
- **Ring Buffer**: Background data reception without polling; characters available when firmware checks.

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No UART output | Wrong baud rate or oversample | Verify `clk_scb` ÷ oversample = baud rate |
| Garbled characters | Baud rate mismatch | Ensure both sides use identical baud, data bits, parity, stop bits |
| RX drops characters | No interrupt/ring buffer | Enable interrupt-driven RX or use ring buffer |
| CTS blocks TX | CTS line not driven | Connect CTS to peer's RTS or disable flow control |
| `Cy_SCB_UART_Init` returns error | SCB not enabled in Device Configurator | Enable the peripheral and re-save `.modus` |
| Break detected unexpectedly | Noise on RX line | Add filtering or check cable connections |

## Configuration Parameters Reference

**UART (`cy_stc_scb_uart_config_t`)**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `uartMode` | `cy_en_scb_uart_mode_t` | `CY_SCB_UART_STANDARD` | Standard / SmartCard / IrDA |
| `oversample` | `uint32_t` | 12 | Clock cycles per UART bit (8–16 typical) |
| `dataWidth` | `uint32_t` | 8 | Frame size in bits (4–16) |
| `parity` | `cy_en_scb_uart_parity_t` | `NONE` | None / Even / Odd |
| `stopBits` | `cy_en_scb_uart_stop_bits_t` | `1` | 1, 1.5, 2, or 3 stop bits |
| `enableMsbFirst` | `bool` | false | LSb-first (false) or MSb-first (true) |
| `enableCts` | `bool` | false | Enable CTS hardware flow control |
| `rtsRxFifoLevel` | `uint32_t` | 0 | RX FIFO level that de-asserts RTS |
| `rxFifoTriggerLevel` | `uint32_t` | 0 | RX FIFO level that triggers interrupt/DMA |
| `txFifoTriggerLevel` | `uint32_t` | 0 | TX FIFO level that triggers interrupt/DMA |

## Related Code Examples

- [PSOC™ Edge MCU: UART Transmit and Receive with DMA](https://github.com/Infineon/mtb-example-psoc-edge-uart-transmit-receive-dma)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — SCB UART chapter


## Advanced Usage and Examples

### Wake-from-DeepSleep

UART supports activity-detect wakeup: the device wakes from DeepSleep on the first start-bit detection on the RX line.

Register `Cy_SCB_UART_DeepSleepCallback` with `Cy_SysPm_RegisterCallback()` before entering DeepSleep.

### DMA Integration

Set `rxFifoTriggerLevel` and `txFifoTriggerLevel` in the UART config structure to the desired FIFO watermarks. Connect the SCB DMA triggers (`TRIG_IN_MUX_*`) to a DMA channel for zero-copy transfers. Refer to the [DMA driver documentation](dma.md) for DMA channel setup.

## Industry Standards and Compliance

- UART: TIA-232 / RS-232 compatible signaling (with external level shifter)
- IrDA: SIR (Serial Infrared) physical layer
- ISO 7816-3: SmartCard interface mode

## See Also

- [SCB Overview](scb.md) — parent component datasheet for the Serial Communication Block
- [I2C Driver](i2c.md) — SCB configured as I2C master/slave
- [SPI Driver](spi.md) — SCB configured as SPI master/slave

## Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
