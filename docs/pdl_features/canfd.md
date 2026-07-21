# CAN FD - ISO 11898-1 Flexible Data-Rate CAN Bus Driver for Industrial and Automotive Applications

## Overview

The **CAN FD (Controller Area Network Flexible Data-Rate)** driver provides an API for the MXTTCANFD hardware block, supporting both classic CAN 2.0 (11-bit and 29-bit identifiers) and CAN FD (ISO 11898-1:2015) frames with up to 64-byte payloads and data-phase bit rates up to 8 Mbps. Hardware message filtering, dual FIFO receive buffers, and an interrupt-based callback model minimize CPU overhead while meeting the real-time requirements of automotive and industrial networks.

## Features

- **CAN FD and Classic CAN**: Single driver handles Classic CAN 2.0B and ISO CAN FD in the same configuration; selectable per-frame
- **Up to 8 Mbps data-phase bit rate**: Independently configurable arbitration-phase and data-phase bit timing
- **64-byte payload**: Full ISO CAN FD frame size; 8-byte payload in classic CAN mode
- **Hardware message filtering**: Standard (11-bit) and extended (29-bit) ID filters with range, mask, and exact-match modes; global filter for non-matching frames
- **Dual RX FIFOs**: Two independent receive FIFOs (FIFO 0 and FIFO 1) plus up to 64 dedicated receive buffers in Message RAM
- **Interrupt-driven operation**: Separate interrupt lines for RX, TX complete, and error events; `Cy_CANFD_IrqHandler()` dispatches all sources to registered callbacks
- **Loopback/test mode**: Internal loopback for self-test without external bus hardware

## When to Use

- Implement a CAN FD node in an automotive body-control, powertrain, or ADAS application
- Build an industrial fieldbus node that must coexist with legacy CAN 2.0 devices on the same bus
- Develop a gateway that bridges CAN FD to another protocol (e.g., Ethernet, UART)
- Run self-test with loopback mode to verify transceiver hardware before deploying to a live bus
- Log or filter CAN traffic using hardware ID filters to reduce CPU interrupt load

## Prerequisites

### Hardware Requirements

- A device with MXTTCANFD IP
- External CAN transceiver (e.g., TJA1043, TCAN1042) connected to the CANFD TX and RX pins
- 120 Ω bus termination resistors at each physical end of the CAN bus
- A stable peripheral clock; 8 MHz or 48 MHz are typical (see bit timing table in test app)

### Software Requirements

- ModusToolbox 3.x with PDL containing `cy_canfd.h`
- `#include "cy_pdl.h"` or `#include "cy_canfd.h"`

### Read Documentation First

- Device TRM — CAN FD chapter for Message RAM layout and bit timing constraints
- [TCAN1042 Datasheet](https://www.ti.com/product/TCAN1042) (or equivalent) for transceiver wiring

### Configure in the Tool

1. Open **Device Configurator** → **Peripherals** tab.
2. Expand **Communication** → enable a **CAN FD channel** (default alias: `canfd_0_chan_0`).
3. Set the **Clock Signal** divider so the CAN FD block receives the desired source frequency.
4. Configure the **Arbitration Bit Timing** and **Data Bit Timing** fields.
5. Add **Standard ID Filters** and **Extended ID Filters** as required.
6. Configure **RX FIFO 0** and **RX FIFO 1** sizes.
7. Register callback function names in the Callback Functions section.
8. Save the `.modus` file; the tool generates `canfd_0_chan_0_config` in `cycfg_peripherals.c/.h`.
9. Call `init_cycfg_all()` before any CAN FD API calls.

| Parameter | Example Value | Notes |
|-----------|--------------|-------|
| Clock divider (8 MHz source) | ÷1 → 8 MHz | `prescaler+1` must yield desired tq |
| Arbitration prescaler | 5 (÷5 → 100 kbps nominal) | tq = (prescaler+1) × T_clk |
| Arb. TimeSegment1 | 10 | Phase Seg1 + Prop Seg (hardware adds +1) |
| Arb. TimeSegment2 | 5 | Phase Seg2 (hardware adds +1) |
| Arb. SyncJumpWidth | 4 | Max re-sync adjustment |
| Data prescaler | 2 (÷2 → 500 kbps data) | Must be ≤ arb. bit rate or equal |
| Data TimeSegment1 | 5 | Phase Seg1 data phase |
| Data TimeSegment2 | 2 | Phase Seg2 data phase |
| RX FIFO 0 size | 8 elements | Each element: 72 bytes for 64-byte payload |
| Standard ID filter count | 2 | Up to 128 filters |
| Extended ID filter count | 1 | Up to 64 filters |

## Quick Start

**Step 1:** Enable a CAN FD channel in the Device Configurator with arbitration rate 100 kbps, data rate 500 kbps.
**Step 2:** Connect TX pin to transceiver TXD, RX pin to transceiver RXD; connect transceiver to the CAN bus with termination.
**Step 3:** Register the `Cy_CANFD_IrqHandler` in the NVIC for `canfd_0_interrupts0_0_IRQn`.
**Step 4:** Add the sample code below to `main.c`.

**Expected Outcome:** CAN FD initialization succeeds; received frames matching the configured filter arrive in the RX callback; transmitted frames appear on the bus.

## Sample Code

### Bare Metal CAN FD Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

/* Shared context - unique per CAN FD channel; must be global */
cy_stc_canfd_context_t canfdContext;

/* CAN FD channel index */
#define CAN_CHANNEL    (0U)

/*******************************************************************************
* RX callback: called by Cy_CANFD_IrqHandler for every received frame
*******************************************************************************/
void CAN_RxCallback(bool rxFifoMsg, uint8_t bufOrFifoNum,
                    cy_stc_canfd_rx_buffer_t *rxMsg)
{
    (void)rxFifoMsg;
    (void)bufOrFifoNum;

    if (rxMsg->r0_f->rtr == 0U)   /* Data frame only */
    {
        uint32_t rxId  = rxMsg->r0_f->id;
        uint8_t  dlc   = (uint8_t)rxMsg->r1_f->dlc;
        (void)rxId;
        (void)dlc;
        /* Application data is in rxMsg->data_area_f[] */
    }
}

/*******************************************************************************
* TX callback: called when a TX buffer transmission completes
*******************************************************************************/
void CAN_TxCallback(uint8_t txBufNum)
{
    (void)txBufNum;
}

/*******************************************************************************
* Error callback: called on protocol error events
*******************************************************************************/
void CAN_ErrorCallback(uint32_t errorMask)
{
    (void)errorMask;
}

/*******************************************************************************
* ISR: must be registered for canfd_0_interrupts0_0_IRQn (or consolidated IRQ)
*******************************************************************************/
void CAN_IrqHandler(void)
{
    Cy_CANFD_IrqHandler(CANFD_HW, CAN_CHANNEL, &canfdContext);
}

int main(void)
{
    /* Initialize device (clocks, pins) from Device Configurator */
    init_cycfg_all();
    __enable_irq();

    /* Register ISR */
    const cy_stc_sysint_t canfdIsrCfg = {
        .intrSrc      = canfd_0_interrupts0_0_IRQn,
        .intrPriority = 3U,
    };
    Cy_SysInt_Init(&canfdIsrCfg, CAN_IrqHandler);
    NVIC_EnableIRQ(canfd_0_interrupts0_0_IRQn);

    /* Initialize CAN FD using configuration generated by Device Configurator.
     * The variable name is <alias>_config, e.g. canfd_0_chan_0_config.
     * Callbacks are registered through the config structure. */
    cy_en_canfd_status_t status = Cy_CANFD_Init(CANFD_HW, CAN_CHANNEL,
                                                 &canfd_0_chan_0_config,
                                                 &canfdContext);
    if (CY_CANFD_SUCCESS != status)
    {
        /* Initialization failed - check clocks and pin routing */
        for (;;) {}
    }

    /* Prepare a CAN FD transmit buffer */
    static uint8_t txData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    cy_stc_canfd_t0_t t0 = {
        .id  = 0x200UL,                     /* 11-bit standard ID */
        .rtr = CY_CANFD_RTR_DATA_FRAME,
        .xtd = CY_CANFD_XTD_STANDARD_ID,
        .esi = CY_CANFD_ESI_ERROR_ACTIVE,
    };
    cy_stc_canfd_t1_t t1 = {
        .dlc = 8U,                          /* 8 data bytes */
        .brs = true,                        /* Bit Rate Switch: use data-phase rate */
        .fdf = CY_CANFD_FDF_CAN_FD_FRAME,   /* CAN FD frame */
        .efc = false,
        .mm  = 0U,
    };
    cy_stc_canfd_tx_buffer_t txBuffer = {
        .t0_f        = &t0,
        .t1_f        = &t1,
        .data_area_f = (uint32_t *)txData,
    };

    /* Transmit using TX buffer index 0 */
    Cy_CANFD_UpdateAndTransmitMsgBuffer(CANFD_HW, CAN_CHANNEL,
                                        &txBuffer, 0U, &canfdContext);

    for (;;)
    {
        /* Wait for interrupts; application logic here */
        __WFI();
    }
}
```

## Expected Outcome

- `Cy_CANFD_Init()` returns `CY_CANFD_SUCCESS`.
- Transmitted frame with ID 0x200 appears on the CAN bus (verify with a CAN analyzer or oscilloscope).
- Incoming frames matching the configured standard/extended ID filters arrive in `CAN_RxCallback()` with correct ID and data.
- In loopback test mode: transmitted frame immediately appears in the RX callback without external bus hardware.

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `Cy_CANFD_Init()` returns error | Clock not running or wrong divider | Verify `init_cycfg_all()` called; check peripheral clock frequency |
| No frames transmitted | TX pin not routed to transceiver TXD | Check HSIOM pin assignment in Device Configurator |
| Bus-Off state | Too many errors — likely bit timing mismatch | Match arbitration bit timing with all nodes on the bus |
| RX callback never called | ID filter not matching | Verify filter SFID1/SFID2 range includes the sender's ID |
| RX callback never called | RX interrupt not enabled | Only RX interrupts are enabled by default; `Cy_CANFD_SetInterruptMask()` is not needed for RX |
| Data phase errors (BRS) | Data bit rate too high for cable length | Reduce data bit rate or use a shorter, properly terminated bus |
| Loopback not working | Echo not disabled in RX callback | In loopback mode, comment out the re-transmit in the RX callback to avoid infinite loop |

## Related Code Examples

- [PSOC™ Edge MCU: CAN FD](https://github.com/Infineon/mtb-example-psoc-edge-canfd)

## Related Application Notes

- [AN228821 – Getting Started with CAN FD on Traveo II](https://www.infineon.com/an228821)
- [AN235056 – CAN FD Bit Timing Calculation](https://www.infineon.com/an235056)

## Configuration Parameters Reference

### Bit Timing (`cy_stc_canfd_bitrate_t`)

| Field | Description | Note |
|-------|-------------|------|
| `prescaler` | Clock prescaler minus 1 | tq = (prescaler+1) ÷ f_clk |
| `timeSegment1` | Phase Seg1 + Prop Seg, minus 1 | Hardware adds +1 to programmed value |
| `timeSegment2` | Phase Seg2, minus 1 | Hardware adds +1; data-phase rate ≥ arb-phase rate |
| `syncJumpWidth` | Re-sync jump width, minus 1 | Must be ≤ min(timeSegment1, timeSegment2) |

**Bit time formula**: `T_bit = (timeSegment1 + timeSegment2 + 3) × tq`

**Common configurations** (source clock = 8 MHz):

| Target Rate | prescaler | TS1 | TS2 | SJW |
|-------------|-----------|-----|-----|-----|
| 100 kbps (arb) | 4 | 10 | 5 | 3 |
| 500 kbps (arb) | 1 | 5 | 2 | 1 |
| 1 Mbps (arb) | 0 | 3 | 4 | 0 |
| 2 Mbps (data, 48 MHz src) | 1 | 5 | 6 | 0 |

### Standard ID Filter (`cy_stc_id_filter_t`)

| Field | Description |
|-------|-------------|
| `sfid1` | Filter ID 1 (exact match / range lower bound) |
| `sfid2` | Filter ID 2 (mask / range upper / buffer index for `STORE_RX_BUFFER`) |
| `sfec` | Filter element config: DISABLE, RX_FIFO_0, RX_FIFO_1, STORE_RX_BUFFER, etc. |
| `sft` | Filter type: RANGE, DUAL_ID, CLASSIC_FILTER |

### Extended ID Filter (`cy_stc_extid_filter_t` / `cy_stc_canfd_f0_t` + `cy_stc_canfd_f1_t`)

| Field | Description |
|-------|-------------|
| `efid1` | Extended filter ID 1 |
| `efid2` | Extended filter ID 2 / buffer index |
| `efec` | Filter element config (same options as standard) |
| `eft` | Filter type: RANGE, DUAL_ID, CLASSIC_FILTER, RANGE_XIDAM_MASK |

### Main Configuration (`cy_stc_canfd_config_t`)

| Field | Type | Description |
|-------|------|-------------|
| `txCallback` | function pointer | Called when TX buffer transmission completes |
| `rxCallback` | function pointer | Called for each received frame (dedicated buffer or FIFO) |
| `errorCallback` | function pointer | Called for error interrupt events |
| `canFDMode` | `bool` | true = CAN FD mode; false = Classic CAN only |
| `bitrate` | `cy_stc_canfd_bitrate_t` | Arbitration-phase bit timing |
| `fastBitrate` | `cy_stc_canfd_bitrate_t` | Data-phase bit timing (CAN FD only) |
| `tdcConfig` | `cy_stc_canfd_transceiver_delay_compensation_t` | Transceiver delay compensation |
| `sidFilterConfig` | pointer to filter array | Standard ID filter table |
| `sidFiltersCount` | `uint8_t` | Number of standard ID filters (0–128) |
| `extIdFilterConfig` | pointer to filter array | Extended ID filter table |
| `extIdFiltersCount` | `uint8_t` | Number of extended ID filters (0–64) |
| `rxFifo0Config` | `cy_stc_canfd_fifo_config_t` | RX FIFO 0 configuration |
| `rxFifo1Config` | `cy_stc_canfd_fifo_config_t` | RX FIFO 1 configuration |
| `noOfRxBuffers` | `uint8_t` | Dedicated RX buffer count in Message RAM |
| `noOfTxBuffers` | `uint8_t` | Dedicated TX buffer count in Message RAM |

### Key API Functions

| Function | Description |
|----------|-------------|
| `Cy_CANFD_Init(base, ch, config, ctx)` | Initialize the CAN FD channel |
| `Cy_CANFD_DeInit(base, ch, ctx)` | De-initialize and reset the channel |
| `Cy_CANFD_UpdateAndTransmitMsgBuffer(base, ch, txBuf, idx, ctx)` | Load TX buffer and trigger transmission |
| `Cy_CANFD_GetTxBufferStatus(base, ch, ctx)` | Query transmission status of TX buffers |
| `Cy_CANFD_IrqHandler(base, ch, ctx)` | Process all pending CAN FD interrupt sources |
| `Cy_CANFD_SetInterruptMask(base, ch, intrMask)` | Enable additional interrupt sources |
| `Cy_CANFD_SetInterruptLine(base, ch, intrMask, line)` | Route interrupt sources to interrupt line 0 or 1 |
| `Cy_CANFD_SidFilterSetup(base, ch, filter, idx, ctx)` | Reconfigure a standard ID filter at runtime |
| `Cy_CANFD_XidFilterSetup(base, ch, filter, idx, ctx)` | Reconfigure an extended ID filter at runtime |

## Advanced Usage

### Dual Interrupt Lines

The MXTTCANFD block has two interrupt lines per channel (`canfd_0_interrupts0_0_IRQn` and `canfd_0_interrupts1_0_IRQn`) plus a consolidated interrupt (`canfd_0_interrupt0_IRQn`). Use `Cy_CANFD_SetInterruptLine()` to route TX-complete interrupts to line 0 and RX/error interrupts to line 1 for independent priority management.

### Runtime Filter Updates

Filters can be reconfigured at runtime without re-initializing the entire block:

```c
/* Change standard filter 0 to accept ID 0x100 into FIFO 0 */
cy_stc_id_filter_t newFilter = {
    .sfid1 = 0x100UL,
    .sfid2 = 0x100UL,
    .sfec  = CY_CANFD_SFEC_STORE_FIFO_0,
    .sft   = CY_CANFD_SFT_CLASSIC_FILTER,
};
Cy_CANFD_SidFilterSetup(CANFD_HW, CAN_CHANNEL, &newFilter, 0U, &canfdContext);
```

### Transceiver Delay Compensation (TDC)

For data-phase rates above 2 Mbps, enable TDC in `cy_stc_canfd_transceiver_delay_compensation_t` to compensate for the propagation delay of the external transceiver. Set `tdcEnabled = true` and calibrate `tdcOffset` to the transceiver's loop delay.

### Loopback Self-Test

Set test mode to `CY_CANFD_TEST_MODE_INTERNAL_LOOPBACK` in `cy_stc_canfd_test_mode_t` during `Cy_CANFD_Init()` to route TX back to RX internally without a transceiver. Remember to disable the echo logic in the RX callback to prevent an infinite loop.

## Industry Standards

- **ISO 11898-1:2015** — CAN FD (Flexible Data-Rate) Data Link Layer and Physical Signaling
- **CAN FD 1.0** — Robert Bosch GmbH CAN with Flexible Data-Rate Specification 1.0
- **ISO 11898-2** — CAN High-Speed Physical Layer (transceiver compatibility)

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
