# IPC - Inter-Processor Communication Driver for Safe Multi-Core Data Exchange

## Overview

The **IPC (Inter-Processor Communication)** driver provides a hardware-backed, race-condition-free mechanism for transferring data and synchronizing operations between CPU cores on multi-core Infineon devices. It builds on a set of lockable hardware channels and uses interrupt-driven notify/release events, making it the foundation for the system pipe, semaphore, flash write synchronization, and Bluetooth subsystem communication layers.

## Features

- **Hardware-enforced mutual exclusion**: atomic lock-acquire and lock-release on individual IPC channels prevents simultaneous access from multiple cores
- **Pipe layer**: full-duplex message channels between CPU pairs; a single pipe supports multiple software clients with independent callbacks
- **Semaphore layer**: shared flag array (up to 128 flags) for cross-core resource and event synchronization
- **Notify/Release interrupts**: non-blocking event delivery across CPU boundaries with configurable interrupt masks
- **Flash operation synchronization**: System IPC channels are used internally to coordinate flash writes across CM0+ and CM4/CM33
- **Scalable channel map**: 4–16 channels per IP instance; some devices have two IPC instances for independent subsystems

## When to Use

- Pass commands or data structures from a CM0+ security core to a CM4/CM33 application core
- Use semaphores to guard shared SRAM, peripherals, or non-reentrant library functions accessed from multiple CPUs
- Implement a mailbox protocol where one core posts a message and the other core processes it asynchronously
- Synchronize flash erase/write operations so only one CPU accesses flash at a time
- Build Bluetooth subsystem (BTSS) communication via the BTIPC layer on devices that include a BT subsystem

## Prerequisites

### Hardware Requirements

- Any multi-core Infineon device; no external hardware required; IPC is entirely on-chip

### Software Requirements

- `#include "cy_pdl.h"` — or selectively include `cy_ipc_drv.h`, `cy_ipc_pipe.h`, `cy_ipc_sema.h`
- For both cores: the same IPC channel and interrupt numbers must be agreed upon at application design time
- System IPC channels (0–7) are reserved by the PDL system layer; application channels start at `CY_IPC_CHAN_USER`

### Configure in the Tool

IPC does not require a Device Configurator personality. Channel assignments and interrupt routing are configured in firmware. The system pipe is pre-configured in `cycfg_system.h` for standard dual-core designs.

## Quick Start

**Step 1:** Choose an application IPC channel index ≥ `CY_IPC_CHAN_USER` and a matching interrupt index.
**Step 2:** On the receiving core, configure and enable the IPC interrupt.
**Step 3:** On the sending core, acquire the channel, write data, and send a notify.
**Step 4:** On the receiving core, read the data in the interrupt handler and release the channel.

**Expected Outcome:** The receiving core's interrupt fires, data is read correctly, and the channel is released.

## Sample Code

### Bare Metal IPC Channel Send/Receive (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

/* Use application-safe channel and interrupt indices */
#define APP_IPC_CHANNEL    (8U)                          /* First user-available channel */
#define APP_IPC_INTR       (8U)                          /* Matching interrupt structure */
#define NOTIFY_MASK        CY_IPC_CH_MASK(APP_IPC_CHANNEL)
#define RELEASE_MASK       CY_IPC_CH_MASK(APP_IPC_CHANNEL)

/* Shared message buffer - must be accessible from both cores */
uint32_t ipcMessage[2] = {0x12345678UL, 0xDEADBEEFUL};
uint32_t ipcReadBuf[2];

volatile bool msgReceived = false;

/* IPC interrupt handler (runs on receiving core) */
void IPC_App_Interrupt(void)
{
    IPC_INTR_STRUCT_Type *intrPtr = Cy_IPC_Drv_GetIntrBaseAddr(APP_IPC_INTR);
    uint32_t masked = Cy_IPC_Drv_GetInterruptStatusMasked(intrPtr);
    uint32_t notifyMask = Cy_IPC_Drv_ExtractAcquireMask(masked);

    if (notifyMask & NOTIFY_MASK)
    {
        Cy_IPC_Drv_ClearInterrupt(intrPtr, CY_IPC_NO_NOTIFICATION, notifyMask);

        IPC_STRUCT_Type *chPtr = Cy_IPC_Drv_GetIpcBaseAddress(APP_IPC_CHANNEL);
        if (Cy_IPC_Drv_IsLockAcquired(chPtr))
        {
            Cy_IPC_Drv_ReadDDataValue(chPtr, ipcReadBuf);
            msgReceived = true;

            /* Release the channel back to the sender */
            Cy_IPC_Drv_LockRelease(chPtr, RELEASE_MASK);
        }
    }
}

int main(void)
{
    init_cycfg_all();
    __enable_irq();

    /* --- Receiving core setup --- */
    const cy_stc_sysint_t ipcIntrCfg = {
        .intrSrc      = (IRQn_Type)CY_IPC_INTR_MUX(APP_IPC_INTR),
        .intrPriority = 2U,
    };
    Cy_SysInt_Init(&ipcIntrCfg, IPC_App_Interrupt);
    NVIC_EnableIRQ((IRQn_Type)CY_IPC_INTR_MUX(APP_IPC_INTR));

    /* Set interrupt mask: notify on channel acquire */
    Cy_IPC_Drv_SetInterruptMask(
        Cy_IPC_Drv_GetIntrBaseAddr(APP_IPC_INTR),
        RELEASE_MASK,       /* release mask */
        NOTIFY_MASK         /* notify mask  */
    );

    /* --- Sending core logic --- */
    IPC_STRUCT_Type *chPtr = Cy_IPC_Drv_GetIpcBaseAddress(APP_IPC_CHANNEL);

    /* Acquire the channel (blocking spin until available) */
    while (CY_IPC_DRV_SUCCESS != Cy_IPC_Drv_LockAcquire(chPtr)) {}

    /* Write 64-bit data value */
    Cy_IPC_Drv_WriteDDataValue(chPtr, ipcMessage);

    /* Notify receiving core */
    Cy_IPC_Drv_AcquireNotify(chPtr, NOTIFY_MASK);

    /* Wait for receive acknowledgment */
    while (!msgReceived) {}

    /* Verify data */
    CY_ASSERT(ipcReadBuf[0] == ipcMessage[0]);
    CY_ASSERT(ipcReadBuf[1] == ipcMessage[1]);

    for (;;) {}
}
```

### Semaphore Usage (fragment)

```c
#include "cy_pdl.h"

/* Semaphore pool stored in shared SRAM, initialized once by CM0+ */
uint32_t semaphorePool[CY_IPC_SEMA_COUNT / CY_IPC_SEMA_PER_WORD];

#define MY_SEMA_NUM   (8U)   /* Application semaphore index */

void ipc_sema_init_example(void)
{
    /* Initialize semaphore layer - call once on the primary core */
    Cy_IPC_Sema_Init(CY_IPC_CHAN_SEMA, CY_IPC_SEMA_COUNT, semaphorePool);
}

void shared_resource_access(void)
{
    /* Acquire semaphore (spin-wait) */
    while (CY_IPC_SEMA_SUCCESS != Cy_IPC_Sema_Set(MY_SEMA_NUM, false)) {}

    /* --- critical section: access shared resource --- */

    /* Release semaphore */
    Cy_IPC_Sema_Clear(MY_SEMA_NUM, false);
}
```

### Pipe Usage (fragment)

```c
#include "cy_pdl.h"
#include "cy_ipc_pipe.h"

/* Client callback invoked when the other core sends data to this client ID */
void myPipeCallback(uint32_t *msgData)
{
    /* Process message; first word is client ID */
    uint32_t payload = msgData[1];
    (void)payload;
    /* Pipe automatically calls release callback on sender side after return */
}

void myReleaseCallback(void)
{
    /* Pipe is free for the next message */
}

void ipc_pipe_send_example(void)
{
    /* Message: first word = client ID, remaining = payload */
    static uint32_t pipeMsg[2] = {CY_IPC_EP_CYPIPE_CLIENT_ID, 0xCAFEBABEUL};

    Cy_IPC_Pipe_SendMessage(
        CY_IPC_EP_CYPIPE_ADDR_VALID,  /* destination endpoint (other core) */
        CY_IPC_EP_CYPIPE_ADDR_VALID,  /* source endpoint (this core) */
        (void *)pipeMsg,
        myReleaseCallback
    );
}
```

## Expected Outcome

- IPC channel message: receiving core interrupt fires with correct `ipcReadBuf` values; channel returns to released state.
- Semaphore: `Cy_IPC_Sema_Set()` returns `CY_IPC_SEMA_SUCCESS`; other core's attempt blocks until `Cy_IPC_Sema_Clear()`.
- Pipe: `myPipeCallback` on the receiving core is called with the correct message data.

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Channel lock never acquired | Another core holds the lock | Check release logic on the other core; avoid holding locks in error paths |
| Interrupt never fires | Interrupt mask not set | Call `Cy_IPC_Drv_SetInterruptMask()` with the correct notify/release masks |
| Wrong channel/interrupt index | Index mismatch between sender and receiver | Ensure both cores use the same channel and interrupt numbers |
| Semaphore init error | `CY_IPC_CHAN_SEMA` locked by system | Initialize semaphore layer only once, from CM0+, before CM4/CM33 starts |
| Data corruption | Non-atomic multi-word writes | Use `WriteDDataValue` for two 32-bit words; use the pipe layer for larger payloads |
| Dual IPC instances | Wrong base address | On devices with multiple IPC instances, use the channel indices and base address macros defined in the device header |
| Flash write hangs | IPC system channel not released | Do not hold IPC system channels (0–7) in application code |

## Related Code Examples

- [PSOC™ Edge MCU: IPC Semaphore](https://github.com/Infineon/mtb-example-psoc-edge-ipc-sema)
- [PSOC™ Edge MCU: IPC Pipes](https://github.com/Infineon/mtb-example-psoc-edge-ipc-pipes)

## Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — IPC chapter


## Configuration Parameters Reference

### IPC Channel API (`cy_ipc_drv.h`)

| Function | Description |
|----------|-------------|
| `Cy_IPC_Drv_GetIpcBaseAddress(ch)` | Get pointer to IPC channel hardware register struct |
| `Cy_IPC_Drv_GetIntrBaseAddr(intr)` | Get pointer to IPC interrupt hardware register struct |
| `Cy_IPC_Drv_LockAcquire(ch)` | Atomically acquire (lock) an IPC channel |
| `Cy_IPC_Drv_LockRelease(ch, mask)` | Release channel and optionally notify one or more interrupt structures |
| `Cy_IPC_Drv_IsLockAcquired(ch)` | Returns true if channel is currently locked |
| `Cy_IPC_Drv_WriteDataValue(ch, val)` | Write a 32-bit data word to the channel |
| `Cy_IPC_Drv_WriteDDataValue(ch, ptr)` | Write two 32-bit words to the channel |
| `Cy_IPC_Drv_ReadDataValue(ch)` | Read the 32-bit data word from the channel |
| `Cy_IPC_Drv_ReadDDataValue(ch, ptr)` | Read two 32-bit words from the channel |
| `Cy_IPC_Drv_AcquireNotify(ch, mask)` | Send notify interrupt to selected interrupt structures |
| `Cy_IPC_Drv_SetInterruptMask(intr, rel, notify)` | Configure which channels trigger this interrupt structure |
| `Cy_IPC_Drv_GetInterruptStatusMasked(intr)` | Read the masked interrupt status word |
| `Cy_IPC_Drv_ClearInterrupt(intr, rel, notify)` | Clear selected interrupt flags |
| `Cy_IPC_Drv_ExtractAcquireMask(status)` | Extract the notify (acquire) mask from the status word |
| `Cy_IPC_Drv_ExtractReleaseMask(status)` | Extract the release mask from the status word |

### Semaphore API (`cy_ipc_sema.h`)

| Function | Description |
|----------|-------------|
| `Cy_IPC_Sema_Init(ch, count, pool)` | Initialize the semaphore layer; call once on the primary core |
| `Cy_IPC_Sema_Set(num, wait)` | Set (acquire) a semaphore; `wait=true` for blocking |
| `Cy_IPC_Sema_Clear(num, wait)` | Clear (release) a semaphore |
| `Cy_IPC_Sema_Status(num)` | Query current state of a semaphore |

### Pipe API (`cy_ipc_pipe.h`)

| Function | Description |
|----------|-------------|
| `Cy_IPC_Pipe_Init(config)` | Initialize the pipe layer; configure endpoints |
| `Cy_IPC_Pipe_RegisterCallback(ep, cb, clientId)` | Register a client message callback |
| `Cy_IPC_Pipe_SendMessage(to, from, msg, relCb)` | Send a message to another core's endpoint |
| `Cy_IPC_Pipe_ExecCallback(ep)` | Called from ISR to dispatch to the correct client callback |

## Advanced Usage

### Multi-Client Pipe Design

A single pipe can serve multiple software clients. Each client registers a unique callback via `Cy_IPC_Pipe_RegisterCallback()` with a distinct `clientId` (0–N). The first 32-bit word of every message must contain the client ID so the pipe layer can dispatch to the correct callback.

### Dual-IPC Instance

Some devices have two IPC instances (IPC0 and IPC1). IPC0 channels are for the primary CPU subsystem; IPC1 channels are for a secondary CPU subsystem. Use `CY_IPC_CHAN_USER` and the channel base address macros defined in the device header to target the correct instance.

### Bluetooth Subsystem IPC (BTIPC)

Some devices provide dedicated IPC channels for HCI and HPC messages between the MCU and the BT subsystem. Use the `cy_ipc_bt.h` API layer. Short messages (≤7 bytes) use the IPC registers directly; larger payloads are copied to shared SRAM automatically.

### Flash Write Coordination

The PDL flash driver (`cy_flash`) uses IPC system channels internally to ensure only one CPU writes or erases flash at a time. Application code must never hold IPC system channels (0–7) for extended periods, as this can cause the flash driver to block indefinitely.

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
