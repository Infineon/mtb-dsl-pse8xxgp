# RTC - Real-Time Clock with Dual Alarms, DST Support, and Hibernate Wakeup

Provides a complete calendar-and-time API for the hardware Real-Time Clock located in the Backup domain, with two independent alarms, automatic leap-year correction, Daylight Saving Time (DST) management, and callbacks for seamless Deep Sleep and Hibernate transitions.

# Overview

The **RTC driver** configures and manages the hardware Real-Time Clock in the SRSS Backup domain, which continues running from a dedicated low-power clock source (WCO, ILO, or PILO) even when the main supply is removed. Applications use it to track wall-clock time and date, schedule timed events via the two on-chip alarm registers, and wake the device from Deep Sleep or Hibernate using the alarm interrupt — without any external RTC IC.

# Features

- Full calendar tracking: seconds, minutes, hours (12/24 h), day-of-week, date, month, year with automatic leap-year correction
- Two independent alarm channels (Alarm1, Alarm2) with per-field enable/disable for flexible periodic or one-shot scheduling
- Built-in Daylight Saving Time (DST) management using Alarm2; supports both fixed-date and relative-date (e.g., second Sunday in March) rules
- BCD-encoded hardware registers for direct display-ready output
- Clock sources: 32.768 kHz WCO (recommended), ILO, PILO, or external 50/60 Hz sine-wave via `Cy_RTC_SelectFrequencyPrescaler()`
- SysPm callbacks for Deep Sleep (`Cy_RTC_DeepSleepCallback`) and Hibernate (`Cy_RTC_HibernateCallback`)
- Secure Aware API: RTC operations marked SA automatically use the Secure Request Framework on ARM TrustZone devices

# When to Use

- Timestamp data log entries with wall-clock time in an embedded data-logger application
- Schedule a daily or weekly task (e.g., transmit a report, run a self-test) using the alarm interrupt
- Wake the MCU from Hibernate once per hour/day to perform maintenance, then return to Hibernate
- Replace an external battery-backed RTC IC with the on-chip solution using the WCO and Backup domain supply
- Display local time including DST transitions without custom calendar logic

# Prerequisites

## Hardware Requirements

- **WCO (recommended):** 32.768 kHz crystal + load capacitors on XTAL32/EXTAL32 pins; supplies the Backup domain clock
- **Alternative:** ILO (lower accuracy, ±30%) or PILO (±2% with calibration); note ILO/PILO require Vddd, WCO can run from Vback alone
- Backup domain supply must be maintained through power mode transitions for the clock to keep running

## Software Requirements

- ModusToolbox 3.x with PSOC PDL (`cy_pdl.h`)
- SysClk driver: call `Cy_SysClk_ClkBakSetSource()` to route WCO or ILO to the Backup domain before calling `Cy_RTC_Init()`
- SysPm driver if Hibernate wakeup is required

## Configure in the Tool

1. Open the Device Configurator → **System** → **Backup Domain**.
2. Select the **Clock Source** (WCO recommended).
3. Enable and configure the **WCO** under the Clocks tab if using the crystal oscillator.
4. No explicit RTC personality is required; the RTC is initialized via API after the clock source is set.

| Parameter | Description | Value for Daily Alarm | Value Explanation | Parameter Description |
|---|---|---|---|---|
| Clock Source | Backup domain clock | WCO | Highest accuracy; survives Hibernate | `Cy_SysClk_ClkBakSetSource(CY_SYSCLK_BAK_IN_WCO)` |
| Hour Format | 12 or 24-hour | `CY_RTC_24_HOURS` | Simpler for embedded logging | Selects BCD register interpretation |
| Initial Time | Starting calendar value | Application-specific | Set to build time or network time | `cy_stc_rtc_config_t` fields |
| Alarm1 Match | Fields to match for alarm | Seconds=0, Minutes=0, others disabled | Fires once per hour | Enable only the fields that must match |

# Quick Start

This quick start sets the RTC to a known time and configures Alarm1 to fire every hour.

**Step 1:** Ensure the WCO or ILO is enabled and routed to the Backup domain with `Cy_SysClk_ClkBakSetSource()`.

**Step 2:** Fill a `cy_stc_rtc_config_t` structure with the initial time and call `Cy_RTC_Init()`.

**Step 3:** Configure Alarm1 with seconds=0, minutes=0 enabled, all other fields disabled; call `Cy_RTC_SetAlarmDateAndTime()`.

**Step 4:** Add the following code to your `main.c`:

**Expected Outcome:** The RTC starts counting from the configured initial time. `Cy_RTC_Alarm1Interrupt()` fires every hour (at HH:00:00), setting `alarm_fired`. The main loop reads the current time and can log or act on the event.

## Sample Code

### Bare Metal Example (main.c)

```c
#include "cy_pdl.h"
#include "cybsp.h"

static volatile bool alarm_fired = false;

/* Override the weak Alarm1 handler */
void Cy_RTC_Alarm1Interrupt(void)
{
    alarm_fired = true;
}

/* RTC interrupt service routine — dispatches to Cy_RTC_Interrupt() */
void RTC_ISR(void)
{
    Cy_RTC_Interrupt(NULL, false); /* NULL = no DST, false = DST not enabled */
}

int main(void)
{
    cy_rslt_t result;

    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    __enable_irq();

    /* Route WCO to the Backup domain */
    Cy_SysClk_ClkBakSetSource(CY_SYSCLK_BAK_IN_WCO);

    /* Set initial date/time: Monday 2025-01-06, 08:30:00 (24-hour) */
    cy_stc_rtc_config_t rtc_init = {
        .sec          = 0U,
        .min          = 30U,
        .hour         = 8U,
        .amPm         = CY_RTC_AM,          /* ignored in 24-h mode */
        .hrFormat     = CY_RTC_24_HOURS,
        .dayOfWeek    = CY_RTC_MONDAY,
        .date         = 6U,
        .month        = CY_RTC_JANUARY,
        .year         = 25U,                /* 2000 + 25 = 2025 */
    };
    result = Cy_RTC_Init(&rtc_init);
    CY_ASSERT(result == CY_RTC_SUCCESS);

    /* Configure Alarm1: fire every hour at :00:00 */
    cy_stc_rtc_alarm_t alarm1 = {
        .sec          = 0U,
        .secEn        = CY_RTC_ALARM_ENABLE,
        .min          = 0U,
        .minEn        = CY_RTC_ALARM_ENABLE,
        .hourEn       = CY_RTC_ALARM_DISABLE,  /* do not match the hour */
        .dayOfWeekEn  = CY_RTC_ALARM_DISABLE,
        .dateEn       = CY_RTC_ALARM_DISABLE,
        .monthEn      = CY_RTC_ALARM_DISABLE,
        .almEn        = CY_RTC_ALARM_ENABLE,
    };
    result = Cy_RTC_SetAlarmDateAndTime(&alarm1, CY_RTC_ALARM_1);
    CY_ASSERT(result == CY_RTC_SUCCESS);

    /* Configure and enable the RTC NVIC interrupt */
    static const cy_stc_sysint_t rtc_int_cfg = {
        .intrSrc      = srss_interrupt_backup_IRQn,
        .intrPriority = 7U,
    };
    Cy_SysInt_Init(&rtc_int_cfg, RTC_ISR);
    NVIC_EnableIRQ(srss_interrupt_backup_IRQn);

    /* Enable the Alarm1 interrupt mask */
    Cy_RTC_SetInterruptMask(CY_RTC_INTR_ALARM1);

    for (;;)
    {
        if (alarm_fired)
        {
            alarm_fired = false;

            cy_stc_rtc_config_t now;
            Cy_RTC_GetDateAndTime(&now);
            /* Use now.hour, now.min, now.sec, now.date, now.month, now.year */
        }

        /* Optionally enter Deep Sleep between alarms */
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }
}
```

## Expected Outcome

- The RTC counts continuously after `Cy_RTC_Init()`.
- Every hour, at exactly :00:00, the Alarm1 interrupt fires and `alarm_fired` is set.
- The main loop reads the current date/time via `Cy_RTC_GetDateAndTime()`.
- Between alarms the CPU remains in Deep Sleep; only the RTC hardware is active.

# Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| RTC not counting | Backup domain clock not enabled | Call `Cy_SysClk_ClkBakSetSource()` before `Cy_RTC_Init()` |
| Time resets after power cycle | WCO / Backup domain supply lost | Connect Vback supply or use the ILO; check board power design |
| Alarm never fires | Alarm not enabled in structure | Set `almEn = CY_RTC_ALARM_ENABLE` in `cy_stc_rtc_alarm_t` |
| Alarm fires immediately after configuration | Match conditions already satisfied | Set match values to a future time before calling `Cy_RTC_SetAlarmDateAndTime()` |
| DST adjustment never happens | Alarm2 interrupt not routed | If DST is enabled, Alarm2 is reserved; do not configure Alarm2 for user events |
| Hibernate wakeup not working | Hibernate callback not registered | Register `Cy_RTC_HibernateCallback` with `Cy_SysPm_RegisterCallback()` |
| Inaccurate timekeeping | ILO used instead of WCO | Switch to WCO (32.768 kHz crystal) for ±20 ppm accuracy |

# Related Code Examples

- [PSOC™ Edge MCU: RTC Basics](https://github.com/Infineon/mtb-example-psoc-edge-rtc-basics)
- [PSOC™ Edge MCU: RTC Periodic Wakeup](https://github.com/Infineon/mtb-example-psoc-edge-rtc-periodic-wakeup)

# Related Application Notes

- Refer to the device Technical Reference Manual (TRM) — RTC (Real-Time Clock) chapter


# Configuration Parameters Reference

| Parameter | API / Structure Field | Values / Range | Description |
|---|---|---|---|
| Hour Format | `cy_stc_rtc_config_t.hrFormat` | `CY_RTC_12_HOURS`, `CY_RTC_24_HOURS` | Selects 12 or 24-hour time format for all read/write operations |
| Initial Seconds | `cy_stc_rtc_config_t.sec` | 0–59 (BCD) | Starting seconds value |
| Initial Minutes | `cy_stc_rtc_config_t.min` | 0–59 (BCD) | Starting minutes value |
| Initial Hour | `cy_stc_rtc_config_t.hour` | 1–12 or 0–23 depending on format | Starting hour value |
| Day of Week | `cy_stc_rtc_config_t.dayOfWeek` | `CY_RTC_SUNDAY`–`CY_RTC_SATURDAY` | Starting day of week |
| Year | `cy_stc_rtc_config_t.year` | 0–99 (offset from 2000) | Two-digit year; 25 = 2025 |
| Alarm Match Enable | `cy_stc_rtc_alarm_t.*En` fields | `CY_RTC_ALARM_ENABLE`, `CY_RTC_ALARM_DISABLE` | Per-field match control; disable a field to treat it as "don't care" |
| Alarm Number | `Cy_RTC_SetAlarmDateAndTime()` alarm arg | `CY_RTC_ALARM_1`, `CY_RTC_ALARM_2` | Alarm2 is reserved when DST is enabled |
| Interrupt Source | `Cy_RTC_SetInterruptMask()` | `CY_RTC_INTR_ALARM1`, `CY_RTC_INTR_ALARM2`, `CY_RTC_INTR_CENTURY` | Selects which RTC events generate an interrupt |
| External Clock Prescaler | `Cy_RTC_SelectFrequencyPrescaler()` | `CY_RTC_FREQ_50_HZ`, `CY_RTC_FREQ_60_HZ` | Selects prescaler for 50 Hz or 60 Hz external clock source |

# Advanced Usage

- **Hibernate wakeup:** Register `Cy_RTC_HibernateCallback` with `Cy_SysPm_RegisterCallback()` to enable RTC alarm wakeup from Hibernate. The Backup domain continues running from Vback.
- **DST configuration:** Fill `cy_stc_rtc_dst_t` with start and stop rules (fixed or relative date) and call `Cy_RTC_EnableDstTime()`. This automatically configures Alarm2; do not use Alarm2 independently when DST is active.
- **Relative alarm rules:** Use `CY_RTC_DST_RELATIVE` and specify week number + day-of-week (e.g., second Sunday) for region-specific DST rules that vary year to year.
- **Secure Aware RTC:** On TrustZone-enabled devices, calls to secure RTC functions from non-secure code are automatically proxied through the Secure Request Framework. Disable with `DEFINE+=CY_PDL_ENABLE_SECURE_AWARE_RTC=0` if the overhead is unacceptable.
- **Century interrupt:** Enable `CY_RTC_INTR_CENTURY` to receive a one-shot interrupt at the year-2100 rollover for century-tick correction.
- **External 50/60 Hz clock:** If no 32 kHz crystal is populated, route the AC power-line frequency to the RTC clock input and call `Cy_RTC_SelectFrequencyPrescaler(CY_RTC_FREQ_50_HZ)` to use the mains frequency as the timekeeping reference.

---

# Copyright

© Copyright 2018-2026 Infineon Technologies AG and its affiliates. All rights reserved.
