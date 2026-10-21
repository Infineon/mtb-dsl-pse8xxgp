# ModusToolbox™ PSE8xxGP Device Support Library 1.8.0
## What's Included?

Refer to the [README.md](./README.md) for a complete description of the ModusToolbox™ PSE8xxGP Device Support Library.

## Change log
### v1.8.0
#### Driver updates
* Added Cy_SysClk_PllIsConfigured() to distinguish configured PLLs from unconfigured PLLs, including when a configured PLL is disabled.
* Fixed DPLL-LP frequency-reporting rounding.
* Corrected reclaimed RRAM address calculations for EPC2 devices and RRAM programming-lock handling.
* Added GPIO port-level HSIOM security APIs and corrected SMIF GPIO/HSIOM address calculations.
* Fixed counter initialization for asymmetric CC0/CC1 PWM alignment to use the count-up initial value.
* Prevented unsigned underflow in AXI DMA M/X/Y loop-count setters and the DMA Y-loop count setter.
* Prevented unsigned underflow in RTC day-of-week calculations and added explicit no-op instructions to weak RTC alarm and century interrupt callbacks.
* Prevented unsigned underflow in SPI receive-FIFO trigger-level calculations for zero-length transfers.
* Added Cortex-M33 data-cache maintenance support for Crypto operations and corrected CPU/crypto-engine address translation for Curve25519 and Ed25519.
* Made Crypto-server algorithm includes conditional on enabled Crypto features and guarded vector-unit state against unsupported hardware configurations.
* Updated octal-memory SFDP initialization and added Cy_SMIF_GetRxCaptureMode().
* Hardened I3C target CCC payload handling.
* Added bounded bit shifts in CAN FD receive-buffer interrupt handling.
* Cleaned up IPC interrupt-source conversion, made the message cache-clean size explicit, and removed a redundant semaphore-status assignment.
* Improved NN driver type handling in FFT configuration, range checks, and interrupt-error mapping.
* Added Ethernet instance bounds checking and hardened bus-master access-control parameter validation.
* Fixed VGLite GPU interrupt polling and acknowledgment, corrected the graphics driver identifier, and improved graphics register-name compatibility.
* Centralized PDL result-module IDs and corrected the AXI DMA driver identifier.
* Added SRF header guards, explicit feature-macro checks, and default peripheral-clock protection definitions for RTC, SysClk, SysPm, and SMIF secure-service builds.
* Added the CY_SECTION_NON_CACHE section attribute for non-cacheable data.
* Updated MISRA compliance, type handling, and compiler/static-analysis diagnostics across drivers, including AutoAnalog, Crypto, DMA, GPIO, IPC, NN, RTC, SMIF, SysClk, and SysPm.
#### Personality updates
* Added LPPASS ADC scan-group conversion time/rate calculations, sequencer conversion-time and result-format displays, and sequencer indices.
* Added generated ADC, channel, FIFO-slice, and scan-group entry-address definitions for application use.
* Corrected ADC temperature-sensor validation for Low Power mode and the default negative-input selection for single-ended channels.
* Corrected DAC high-speed refresh-rate reporting and added automatic output-buffer power selection based on the refresh rate.
* Enabled oversample factors of 2-16 for high-speed SPI SCB blocks.
* Added UART configuration up to 16 Mbps in High Power mode and corrected integer/fractional clock-divider calculations.
* Fixed fractional PLL solving, external-clock frequency parameter handling, and alternate SysTick source-clock display.
* Corrected SMIF GPIO security initialization, slew-rate/drive-strength configuration, and interrupt-vector handling; fixed per-port pull-up resistance detection.
* Corrected TCPWM counter, PWM, quadrature-decoder, and shift-register peripheral-clock divider assignment.
* Improved MPC metadata lookup, added validation against unsupported Non-Secure Callable PPC regions, and improved security-domain links.
* Updated secure initialization for SMIF XIP and Secure Enclave runtime services, and added the CYCFG_MPU_NS_ENABLED definition for configured Cortex-M33 non-secure MPU regions.
* Fixed memory relocation generation to omit memories with no allocated regions, avoiding zero-sized entries rejected by edgeprotecttools.
#### HAL updates
* Fixed NVM region reporting on EPC2 devices when protected RRAM is reclaimed for main RRAM, including fully reclaimed protected regions.
#### Make updates
* Fixed MVE_SELECT auto-selection not taking effect for VFP_SELECT=softfloat on Cortex-M55.
* Added Arm Toolchain for Embedded 22.1.0 support.
* Included other build-recipe maintenance and minor bug fixes.
#### Miscellaneous updates
* Updated driver API documentation and configuration help, including PWM, clock sources, RRAM, and SRF macro/data-structure documentation.
### v1.7.0
* Increased ECO enable timeout from 3 ms to 10 ms.
### v1.6.0
* Added SCB EZI2C hardware mode for I2C driver.
* Added SCB SPI EZ mode support.
* Added SCB SPI External Clock support.
* Added new API in Graphics to support Tearing Effect interrupt based frame transfers.
* Added new API Cy_Mpc_GetViolationResponse in PPC driver.
* Added additional call for RX Error handling in Ethernet driver.
* Added Peripheral Driver Component Datasheets (CDS) documentation entry for PDL drivers.
* Bug Fixes and documentation updates.
### v1.5.0
#### Driver updates
* Added support for per channel channel quantization for Fully Connected layer
* sysclk driver updates
* New API added in GPIO Driver
* SDHC Driver bit field parsing bug fix
* Added HAL API to get max SDIO Clock Frequency
#### Personality updates
* Memory and protection personality updates
* Pin Personality updates to support SMIF Data lines
* ECO Personality fix computing gtrim value
#### Miscellaneous updates
* Documentation Updates
* Bug Fixes
### v1.4.0
* Protection personality bug fix
* Fix recipe-make version
### v1.3.0
* Fixed issue with Micron SFDP compatibility
* Documentation updates
* Personality updates
* Bug Fixes
### v1.2.0
* PDL ethif driver update with the new API Cy_ETHIF_GetPrivateData
* Graphics updates
* HAL update on mtb_hal_i2c_setup signature
* Fixed SMIF slave index calculation for LLVM compiler
* Improved SRF error message handling
* Added system Power Transition APIs with enhanced documentation
* Improved and cleaned up recipe-make scripts
* Documentation updates
* Personality updates
### v1.1.1
* Bug fix on secure aware implementation of Cy_SysPm_CpuEnterDeepSleep
### v1.1.0
* Documentation updates
* Personality updates
### v1.0.0
* Initial release

## Known Issues

* Warnings when compiling on IAR in the third-party VGLite graphic library. There is no known functional or runtime impact.

## Supported Software and Tools

This version of the ModusToolbox™ PSE8xxGP Device Support Library 1.8.0 was validated for the compatibility with the following Software and Tools:

| Software and Tools                                                            | Version      |
| :---                                                                          | :----        |
| [Infineon Core Library](https://github.com/Infineon/core-lib)                 | 1.5.0        |
| CMSIS                                                                         | 6.1.0        |
| GCC Compiler                                                                  | 14.2.1       |
| IAR Compiler                                                                  | 9.70         |
| ARM Compiler 6                                                                | 6.22         |
| LLVM_ARM Compiler                                                             | 19.1.5       |
| FreeRTOS                                                                      | 10.6.202     |

## More information

For more information, refer to the following documents:

* [Infineon Technologies AG](https://www.infineon.com)

---
© 2026, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.
