# GPIO Driver (PDL) - Flexible Pin Control for Embedded C Applications

# Overview

The **GPIO driver** provides a comprehensive API to configure and access all device Input/Output pins — GPIO, SIO, HSIO, and AUXIO variants — enabling digital input/output, interrupt generation, HSIOM signal routing, and drive-mode selection. Use it to control LEDs, read buttons, route peripheral signals to physical pins, and respond to external events via pin interrupts, all with minimal CPU overhead.

# Features

- **Full pin and port initialization** via `cy_stc_gpio_pin_config_t` (single pin) or `cy_stc_gpio_prt_config_t` (whole port)
- **8 configurable drive modes** including Strong, Open-Drain, Pull-Up/Down, High-Z, and Analog per pin
- **HSIOM (High-Speed IO Matrix) routing** to connect peripheral signals (SCB, TCPWM, SAR, etc.) to physical pins
- **Pin interrupts** with rising, falling, or both-edge detection, with hardware filtering option
- **SIO / HSIO special pin support** for regulated output voltage (Voh), differential input, and adjustable trip points
- **Slew rate and drive strength control** with extended `cfgSlew` and `cfgDriveSel` registers on supported variants

# When to Use

- Toggle or read a GPIO for LEDs, buttons, relays, or external logic
- Route an SCB/UART/SPI/I2C signal from the peripheral to a physical MCU pin via HSIOM
- Generate a CPU interrupt on a rising/falling edge from an external signal (e.g., sensor data-ready)
- Interface with 1.8 V and 3.3 V logic levels using SIO regulated output or adjustable trip points
- Configure entire ports at startup for efficiency (port-level batch init)
- Use open-drain output for I2C bus without an external pull-up transistor

# Prerequisites

## Hardware Requirements

- A GPIO-capable pin with appropriate drive mode configured
- For SIO features: pin must reside on an SIO-capable port (check device TRM)
- External pull-up/pull-down resistors if Open-Drain drive mode is used without internal resistors

## Software Requirements

- ModusToolbox with PDL included, or standalone PDL — include via `#include "cy_pdl.h"`

## Configure in the Tool

1. Open the **Device Configurator** and navigate to the **Pins** tab
2. Click a pin in the pin map to enable it and assign the **Pin** personality
3. Configure the parameters below, then save the `.modus` file — generated structures appear in the **Code Preview** tab and are written to `GeneratedSource/`
4. Reference the generated port/pin macros (`LED_RED_PORT`, `LED_RED_PIN`) in application code — **do not** hand-write config structures

| Parameter | Description | Value for LED Blink | Value Explanation | Parameter Description |
|-----------|-------------|---------------------|-------------------|----------------------|
| Drive Mode | Pin electrical behavior | `CY_GPIO_DM_STRONG` | Actively drives high and low; input buffer on | One of 8 drive modes: Strong, OD-Low, OD-High, Pull-Up, Pull-Down, Pull-Up/Down, High-Z, Analog |
| Initial Drive State | Output value at reset | `1` (High) | LED off at startup (active-low LED) | Written to OUT register after device reset |
| Interrupt Trigger Type | Edge detection | `CY_GPIO_INTR_DISABLE` | Not used for output pin | None / Rising / Falling / Both |
| Slew Rate | Output edge speed | `CY_GPIO_SLEW_FAST` | Fast transitions for digital signal integrity | Fast or Slow |
| Drive Strength | Output current | `CY_GPIO_DRIVE_FULL` | Maximum drive for LED current | Full / 1/2 / 1/4 / 1/8 |
| Threshold | Input logic levels | `CY_GPIO_VTRIP_CMOS` | Default CMOS threshold | CMOS or TTL |

# Quick Start

This quick start demonstrates blinking an LED on a GPIO pin configured as a strong-drive output.

**Step 1:** Enable the target pin in Device Configurator with Drive Mode = `Strong Drive. Input buffer on` and alias it `LED_RED`.

**Step 2:** Save and let Device Configurator generate `cycfg_pins.h/c` in `GeneratedSource/`.

**Step 3:** Call `cybsp_init()` in `main()` — this initialises all Device Configurator configured pins automatically.

**Step 4:** Add the following toggle loop (see [Sample Code](#sample-code) for the complete example).

**Expected Outcome:** The LED toggles at 1 Hz (500 ms on, 500 ms off).

## Sample Code

**Configuration:** Uses the pin configuration generated in [Prerequisites](#prerequisites). Do not manually create pin configuration structures; always reference generated structures.

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

int main(void)
{
    /* Initialize the device and board peripherals (generated pin configs applied here) */
    cy_rslt_t result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0); /* cybsp_init failed */
    }

    __enable_irq();

    for (;;)
    {
        /* Toggle the LED GPIO pin */
        Cy_GPIO_Inv(LED_RED_PORT, LED_RED_PIN);

        /* Delay 500 ms */
        Cy_SysLib_Delay(500U);
    }
}
```

### GPIO Interrupt Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

/* Interrupt configuration — button on P0.3 triggers CM4/CM33 */
static const cy_stc_sysint_t btn_irq_cfg = {
    .intrSrc      = ioss_interrupts_gpio_0_IRQn,
    .intrPriority = 3U
};

static void gpio_isr(void)
{
    /* Clear the latched interrupt */
    Cy_GPIO_ClearInterrupt(BUTTON_PORT, BUTTON_PIN);

    /* Toggle LED in response */
    Cy_GPIO_Inv(LED_RED_PORT, LED_RED_PIN);
}

int main(void)
{
    cybsp_init();
    __enable_irq();

    /* Hook ISR and enable NVIC line */
    Cy_SysInt_Init(&btn_irq_cfg, gpio_isr);
    NVIC_EnableIRQ(btn_irq_cfg.intrSrc);

    for (;;) { /* idle */ }
}
```

### Manual Pin Initialization (without Device Configurator)

```c
#include "cy_pdl.h"

#define MY_LED_PORT  GPIO_PRT0
#define MY_LED_PIN   3U

int main(void)
{
    __enable_irq();

    /* Initialize P0.3 as strong output, initial state HIGH */
    cy_stc_gpio_pin_config_t led_cfg = {
        .outVal    = 1UL,               /* Initial output = HIGH */
        .driveMode = CY_GPIO_DM_STRONG, /* Strong drive */
        .hsiom     = P0_3_GPIO,         /* Software/GPIO control */
        .intEdge   = CY_GPIO_INTR_DISABLE,
        .intMask   = 0UL,
        .vtrip     = CY_GPIO_VTRIP_CMOS,
        .slewRate  = CY_GPIO_SLEW_FAST,
        .driveSel  = CY_GPIO_DRIVE_FULL,
        .vregEn    = 0UL,
        .ibufMode  = 0UL,
        .vtripSel  = 0UL,
        .vrefSel   = 0UL,
        .vohSel    = 0UL,
    };

    if (CY_GPIO_SUCCESS != Cy_GPIO_Pin_Init(MY_LED_PORT, MY_LED_PIN, &led_cfg))
    {
        while (1); /* Handle error */
    }

    for (;;)
    {
        Cy_GPIO_Inv(MY_LED_PORT, MY_LED_PIN);
        Cy_SysLib_Delay(500U);
    }
}
```

## Expected Outcome

- **Visual:** The LED connected to the configured pin blinks at 1 Hz.
- **Logic Analyzer:** GPIO signal toggles every 500 ms ± software timing overhead.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Pin stays low/high, never toggles | Wrong `hsiom` — pin routed to peripheral, not GPIO | Set `hsiom` to `Pxx_y_GPIO` (software controlled) |
| Interrupt fires repeatedly | Interrupt source not cleared inside ISR | Call `Cy_GPIO_ClearInterrupt(port, pin)` at the start of the ISR |
| Pin output has no effect | Input buffer off in analog drive mode (`CY_GPIO_DM_ANALOG`) | Change drive mode to `CY_GPIO_DM_STRONG` or `CY_GPIO_DM_STRONG_IN_OFF` |
| `Cy_GPIO_Port_Init()` returns `CY_GPIO_BAD_PARAM` | Invalid config field values | Verify all bitmask fields fit within port width; unused fields must be zero |
| SIO pin output level wrong | `vregEn`/`vohSel` not configured | Set `vregEn=1` and select appropriate `vohSel` for desired regulated voltage |
| Pin reads wrong logic level | Input buffer disabled | Ensure drive mode has input buffer enabled (no `_IN_OFF` suffix) |

# Related Code Examples

- [PSOC™ Edge MCU: GPIO Interrupt](https://github.com/Infineon/mtb-example-psoc-edge-gpio-interrupt)
- [PSOC™ Edge MCU: Hello World (GPIO LED blink)](https://github.com/Infineon/mtb-example-psoc-edge-hello-world)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — GPIO chapter


# Configuration Parameters Reference

Full Device Configurator GPIO (Pin personality) parameter table:

| Parameter | Description | Typical Value | Notes |
|-----------|-------------|---------------|-------|
| Drive Mode | Electrical output/input behavior | `CY_GPIO_DM_STRONG` | 8 choices; affects power consumption |
| Initial Drive State | OUT register value at startup | `1` / `0` | Hardware reset drives all pins High-Z |
| Threshold (vtrip) | Input logic-level detection | `CY_GPIO_VTRIP_CMOS` | CMOS or TTL |
| Interrupt Trigger Type | Edge for pin interrupt | `CY_GPIO_INTR_DISABLE` | None / Rising / Falling / Both |
| Slew Rate | Output edge rate | `CY_GPIO_SLEW_FAST` | Fast = better signal quality; Slow = lower EMI |
| Drive Strength | Output current fraction | `CY_GPIO_DRIVE_FULL` | Full / 1/2 / 1/4 / 1/8 extended selection on supported variants |
| Secure Attribute (nonSec) | TrustZone access control | `1` (Non-secure) | Supported on variants with TrustZone |
| Pull-up resistor (pullUpRes) | Additional integrated pull-up | `CY_GPIO_PULLUP_RES_DISABLE` | Multiple resistance options on supported variants |
| SIO: Input Buffer Differential | Differential vs. single-ended | `false` | SIO ports only |
| SIO: Reference Voltage | Vref for trip-point | `CY_SIO_VREF_1_2V` | SIO ports: pin / 1.2 V / AMUXBUS A/B |
| SIO: Regulated Output (Voh) | Output voltage regulation | `CY_SIO_VOH_1_00` | Multiples of Vref: 1×–4.16× |

# Advanced Usage and Examples

- **Port-level batch initialization** with `Cy_GPIO_Port_Init()` is more efficient at startup than initializing each pin individually; use it in production code where all pins in a port are known at compile time.
- **HSIOM remapping at runtime**: call `Cy_GPIO_SetHSIOM()` to dynamically reconnect a pin between GPIO software control and a peripheral (e.g., switch from SPI to GPIO during power-save entry).
- **AMux bus switches**: use `Cy_GPIO_SetAmuxSplit()` / `Cy_GPIO_CloseAmuxSplit()` to connect analog signals across the port-level AMux splitter cells.
- **Thread safety**: functions that perform read-modify-write on port registers (e.g., `Cy_GPIO_Write()`, `Cy_GPIO_Set()`) are **not** thread-safe. Protect shared port accesses with critical sections (`Cy_SysLib_EnterCriticalSection()`) in multi-core or RTOS environments.
- **Deep-Sleep wakeup**: a pin interrupt can wake the CPU from Deep Sleep if the interrupt source is connected to a Deep-Sleep capable NVIC channel.

# Industry Standards and Compliance

No specific external protocol standard applies to general-purpose GPIO. For I2C open-drain usage, the pin configuration must comply with the I2C electrical specification (NXP UM10204).

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
