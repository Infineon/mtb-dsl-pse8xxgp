# MIPI DSI - High-Speed Serial Display Interface

## Overview

The MIPI DSI driver (`cy_mipidsi`) configures the **MIPIDSI IP block** within the MXS22GFXSS subsystem
as a MIPI DSI host controller with an integrated D-PHY. It supports DPI-2 (video mode) and DBI-2 Type B
(command mode) displays at up to 1.5 Gbps per lane, enabling high-speed display connectivity.

---

## Features

- **DPI-2 and DBI-2 Type B** display interface support (video mode and command mode)
- Up to **2 data lanes** at maximum **1.5 Gbps per lane**
- Integrated **D-PHY** with on-chip PLL for high-speed mode clock generation
- Full **DCS (Display Command Set)** command interface: generic and proprietary read/write
- Built-in **video pattern generator** for display testing without GPU
- **Ultra-Low-Power (ULP)** mode with disabled PLL for minimal quiescent current
- **Tearing Effect (TE)** interrupt support for synchronized DBI display refresh
- **Shutdown and Color Mode control** for DPI displays
- Selectable video mode sub-types: burst, non-burst sync pulses, non-burst sync events

---

## When to Use

| Scenario | Details |
|---|---|
| Small / wearable OLED display | DBI command-mode MIPI DSI OLED panel (TE-synchronized refresh) |
| High-resolution TFT panel | DPI video-mode 1024×600 display at 60 Hz with 2 data lanes |
| Panel initialization | Sending proprietary DCS initialization sequences during boot |
| Low-power display blanking | Enter ULP mode when display is off; exit before next frame |

> **Note**: The MIPI DSI driver is typically initialized as part of `Cy_GFXSS_Init()`. Use the standalone
> MIPIDSI API for sending DCS/proprietary commands after GFXSS init, or when direct PHY control is needed.

---

## Prerequisites

### Hardware Requirements

- Device with MXS22GFXSS peripheral (includes MIPIDSI host + D-PHY)
- MIPI DSI display panel: DPI or DBI interface, 1 or 2 data lanes
- Panel reset GPIO connected to the device
- Sufficient decoupling on DSI power rails (D-PHY requires clean supply)

### Software Requirements

- ModusToolbox™ 3.x or later
- PDL version 1.x with `cy_mipidsi.h` (included via `cy_graphics.h` or directly)
- Add `COMPONENTS+=GFXSS` in your application's `.mk` file
- Include `cy_pdl.h` or individual headers to access all PDL declarations

### Configure in the Tool

1. Open **Device Configurator** in ModusToolbox.
2. Enable the **GFXSS / MIPIDSI** block.
3. Select the DSI mode (video/command), number of lanes, and pixel format.
4. Enter the display timing parameters (pixel clock, H/V active, sync widths, porches).
5. Assign the display reset GPIO and any TE interrupt pin (for DBI command mode).

---

## Quick Start

### Step-by-Step

1. Fill `cy_stc_mipidsi_display_params_t` with pixel clock (kHz) and H/V timing parameters.
2. Fill `cy_stc_mipidsi_config_t` with lane count, per-lane speed (Mbps), DSI mode, pixel format, and a pointer to display params.
3. (Optional) Call `Cy_MIPIDSI_Init()` directly if not using `Cy_GFXSS_Init()`.
4. Call `Cy_MIPIDSI_Enable()` to power up the D-PHY and start the DSI clock.
5. Assert and then de-assert the display reset GPIO with appropriate delays.
6. Send the panel DCS initialization sequence using `Cy_MIPIDSI_WritePacket()`.
7. Send `MIPI_DCS_SET_DISPLAY_ON` (0x29) to enable the display.
8. For DBI displays: enable TE interrupt with `Cy_MIPIDSI_SetInterruptMask(MIPIDSI_DBI_TE_INTERRUPT_MASK)`.

### Sample Code

```c
#include "cy_pdl.h"
#include "cy_mipidsi.h"
#include "cy_gpio.h"

/* Display timing: 720x1280 @ 60 Hz, 2-lane DSI */
cy_stc_mipidsi_display_params_t dsi_display_params =
{
    .pixel_clock  = 67000U,   /* kHz */
    .hdisplay     = 720U,
    .hsync_width  = 4U,
    .hfp          = 20U,
    .hbp          = 20U,
    .vdisplay     = 1280U,
    .vsync_width  = 4U,
    .vfp          = 8U,
    .vbp          = 8U,
    .polarity_flags = DISPLAY_PARAMS_FLAG_NHSYNC | DISPLAY_PARAMS_FLAG_NVSYNC,
};

/* MIPI DSI host config: 2 lanes, 500 Mbps/lane, video burst mode, RGB565 */
cy_stc_mipidsi_config_t dsi_config =
{
    .virtual_ch     = 0U,
    .num_of_lanes   = 2U,
    .per_lane_mbps  = 500U,
    .dpi_fmt        = CY_MIPIDSI_FMT_16BIT_16BPP,
    .max_phy_clk    = 1000U,   /* max D-PHY clock in MHz */
    .dsi_mode       = DSI_VIDEO_MODE,
    .mode_flags     = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST,
    .display_params = &dsi_display_params,
};

/* Panel DCS initialization commands (vendor-specific) */
static const uint8_t panel_init[][2] =
{
    { 0xB2, 0x10 },   /* 2 data lanes */
    /* ... additional vendor commands ... */
};

cy_stc_mipidsi_context_t dsi_context;

int main(void)
{
    GFXSS_MIPIDSI_Type *dsi_base = (GFXSS_MIPIDSI_Type *)GFXSS_GFXSS_MIPIDSI;

    /* Initialize and enable MIPI DSI host + D-PHY */
    cy_en_mipidsi_status_t status = Cy_MIPIDSI_Init(dsi_base, &dsi_config, &dsi_context);
    if (status != CY_MIPIDSI_SUCCESS) { /* handle error */ }

    Cy_MIPIDSI_Enable(dsi_base);

    /* Hardware reset display panel */
    Cy_GPIO_Clr(DISP_RST_PORT, DISP_RST_PIN);
    Cy_SysLib_Delay(5U);
    Cy_GPIO_Set(DISP_RST_PORT, DISP_RST_PIN);
    Cy_SysLib_Delay(10U);

    /* Send DCS initialization sequence */
    for (uint32_t i = 0U; i < (sizeof(panel_init) / sizeof(panel_init[0])); i++)
    {
        status = Cy_MIPIDSI_WritePacket(dsi_base, panel_init[i], 2U);
        if (status != CY_MIPIDSI_SUCCESS) { /* handle error */ }
    }
    Cy_SysLib_Delay(120U);  /* panel stabilization time */

    /* Enable display */
    uint8_t cmd_on[] = { 0x29, 0x00 };  /* MIPI_DCS_SET_DISPLAY_ON */
    Cy_MIPIDSI_WritePacket(dsi_base, cmd_on, sizeof(cmd_on));

    /* Display is now active; framebuffer output is driven by the DC via GFXSS */
    for (;;) { }
}
```

### Expected Outcome

- After the initialization sequence the display panel becomes active and begins accepting pixel data.
- In video mode, the DC streams pixel data to the DSI host continuously at the configured pixel clock.
- In command mode (DBI), use `Cy_MIPIDSI_WritePacket()` to trigger each frame write; the TE interrupt synchronizes refresh.

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| `Cy_MIPIDSI_Init()` returns `CY_MIPIDSI_BAD_PARAM` | Zero lanes or unsupported pixel format | Verify `num_of_lanes` (1 or 2) and `dpi_fmt` value |
| `Cy_MIPIDSI_WritePacket()` returns `CY_MIPIDSI_TIMEOUT` | Panel not responding or reset not released | Check reset GPIO polarity and timing; add longer delay after de-assert |
| Display flickers or shows noise | Incorrect D-PHY PLL settings (`per_lane_mbps`) | Recalculate: `per_lane_mbps ≥ pixel_clock × bpp / num_of_lanes` |
| Horizontal tears in DBI mode | Missing TE synchronization | Enable `MIPIDSI_DBI_TE_INTERRUPT_MASK` and trigger frame write only in TE ISR |
| `MIPIDSI_DPI_HALT_INTERRUPT_MASK` fires | DPI command transmission error | Reduce pixel clock or check display timing parameters |
| Display works but colors wrong | Incorrect `dpi_fmt` (bpp mismatch) | Match `dpi_fmt` to the panel's color depth; use `CY_MIPIDSI_FMT_RGB888` for 24-bit |
| Power consumption too high at idle | PLL active during blanking | Call `Cy_MIPIDSI_EnterSleep()` when display is off |
| Driver not compiling | Missing `CY_IP_MXS22GFXSS` | Target must have MXS22GFXSS peripheral; add `COMPONENTS+=GFXSS` in `.mk` |

---

## Related Code Examples

- [PSOC™ Edge MCU: GFX DSI ULPM Data Lane](https://github.com/Infineon/mtb-example-psoc-edge-gfx-dsi-ulpm-data-lane)
- [PSOC™ Edge MCU: GFX Single/Double Buffering](https://github.com/Infineon/mtb-example-psoc-edge-gfx-single-double-buffering)
- [PSOC™ Edge MCU: GFX LVGL Smartwatch](https://github.com/Infineon/mtb-example-psoc-edge-gfx-lvgl-smartwatch)

## Related Application Notes

- Refer to the device TRM, GFXSS / MIPIDSI chapter.
- MIPI DSI Specification v1.3 — Packet types, DCS command set, D-PHY electrical parameters.
- MIPI D-PHY Specification — Lane protocol, LP/HS mode transitions.

---

## Configuration Parameters Reference

### `cy_stc_mipidsi_config_t`

| Parameter | Type | Description |
|---|---|---|
| `virtual_ch` | `uint32_t` | DSI virtual channel ID (typically 0) |
| `num_of_lanes` | `uint32_t` | Number of active data lanes (1 or 2) |
| `per_lane_mbps` | `uint32_t` | Per-lane data rate in Mbps (≤ 1500 Mbps) |
| `dpi_fmt` | `cy_en_mipidsi_pixel_format_t` | Pixel color format (e.g., `CY_MIPIDSI_FMT_RGB888`, `CY_MIPIDSI_FMT_16BIT_16BPP`) |
| `max_phy_clk` | `uint32_t` | Maximum D-PHY clock in MHz (used for PLL programming) |
| `dsi_mode` | `cy_en_mipidsi_mode_t` | `DSI_VIDEO_MODE` or `DSI_COMMAND_MODE` |
| `mode_flags` | `uint32_t` | Additional mode flags (e.g., `MIPI_DSI_MODE_VIDEO_BURST`, `MIPI_DSI_MODE_LPM`) |
| `display_params` | `cy_stc_mipidsi_display_params_t *` | Display timing: resolution, sync widths, porches, pixel clock |

### `cy_stc_mipidsi_display_params_t`

| Parameter | Type | Description |
|---|---|---|
| `pixel_clock` | `uint32_t` | Pixel clock frequency in kHz |
| `hdisplay` | `uint16_t` | Horizontal active pixels |
| `hsync_width` | `uint16_t` | Horizontal sync pulse width (pixels) |
| `hfp` | `uint16_t` | Horizontal front porch (pixels) |
| `hbp` | `uint16_t` | Horizontal back porch (pixels) |
| `vdisplay` | `uint16_t` | Vertical active lines |
| `vsync_width` | `uint16_t` | Vertical sync pulse width (lines) |
| `vfp` | `uint16_t` | Vertical front porch (lines) |
| `vbp` | `uint16_t` | Vertical back porch (lines) |
| `polarity_flags` | `uint32_t` | HSYNC/VSYNC polarity (`DISPLAY_PARAMS_FLAG_PHSYNC/NHSYNC/PVSYNC/NVSYNC`) |

### Interrupt Mask Flags

| Macro | Description |
|---|---|
| `MIPIDSI_CORE_INTERRUPT_MASK` | DSI core interrupt (generic error) |
| `MIPIDSI_DPI_HALT_INTERRUPT_MASK` | Command transmission error in DPI mode |
| `MIPIDSI_DBI_TE_INTERRUPT_MASK` | Tearing Effect signal from DBI display |

### Key API Functions

| Function | Description |
|---|---|
| `Cy_MIPIDSI_Init()` | Initialize MIPI DSI host and D-PHY |
| `Cy_MIPIDSI_Enable()` | Enable D-PHY PLL and DSI clocks |
| `Cy_MIPIDSI_Disable()` | Disable DSI (stop clocks) |
| `Cy_MIPIDSI_DeInit()` | De-initialize and release resources |
| `Cy_MIPIDSI_WritePacket()` | Send DCS command packet to display |
| `Cy_MIPIDSI_GenericWritePacket()` | Send generic (non-DCS) command packet |
| `Cy_MIPIDSI_ReadPacket()` | Read response packet from display |
| `Cy_MIPIDSI_EnterSleep()` | Send DCS `ENTER_SLEEP_MODE` (0x10) |
| `Cy_MIPIDSI_ExitSleep()` | Send DCS `EXIT_SLEEP_MODE` (0x11) |
| `Cy_MIPIDSI_SetInterruptMask()` | Enable selected MIPI DSI interrupt sources |
| `Cy_MIPIDSI_GetInterruptStatusMasked()` | Read pending masked interrupt flags |

---

## Advanced Usage

### Ultra-Low-Power (ULP) Display Blanking

When the display is not actively refreshing (e.g., system in sleep), call `Cy_MIPIDSI_Disable()` to stop
the D-PHY PLL. Before the next frame write, call `Cy_MIPIDSI_Enable()` to restart the PLL. The re-lock
time depends on the PLL settings; allow adequate settling time before transmitting.

### DBI Command Mode with Tearing Effect

In command mode, each frame must be triggered by software. Enable `MIPIDSI_DBI_TE_INTERRUPT_MASK`. In the
TE ISR, issue the `MIPI_DCS_WRITE_MEMORY_START` (0x2C) DCS command followed by the pixel data. Use
`Cy_MIPIDSI_WritePacket()` for the header and then trigger the DC to stream pixel data via GFXSS.

### Reading Display Status Registers

Use `Cy_MIPIDSI_ReadPacket()` with the appropriate `cy_en_mipidsi_packet_type_t` (e.g.,
`MIPI_DSI_DCS_READ`) and a DCS command such as `MIPI_DCS_GET_DISPLAY_STATUS` (0x09) to read panel status
registers for diagnostic purposes. Set the maximum return packet size first with
`MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE` (0x37) before issuing a read.

---

## Industry Standards

| Standard | Applicability |
|---|---|
| **MIPI DSI Specification v1.3** | Packet layer protocol, virtual channels, command/video mode framing |
| **MIPI DCS (Display Command Set) v1.3** | Standard display commands (0x00–0xFF range defined in spec) |
| **MIPI D-PHY Specification v1.2** | Physical layer: LP mode, HS mode, lane protocol, electrical levels |

---

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
