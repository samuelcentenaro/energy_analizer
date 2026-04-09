# Changelog

All notable firmware changes for the Energy Analyzer project should be recorded
in this file.

The project follows a production-oriented changelog style focused on firmware
baseline changes, validation milestones, and architecture-impacting decisions.

## [Unreleased]

### Added

- Production validation checklist for the active ADS1015-based firmware path
- Release readiness baseline document for Phase 8 consolidation
- Release config review and release-oriented `sdkconfig.defaults.release`
- Production validation report and release-candidate sign-off templates
- AI project context snapshot for future coding agents
- Future-feature study for voltage harmonics using ADS1015 and Goertzel

### Changed

- Confirmed ADS1015 as the official production acquisition path
- Clarified legacy internal-ADC code as deprecated and out of the active flow
- Integrated MQTT runtime into the active application baseline
- Added project `menuconfig` defaults for MQTT broker/IP, port, client ID, and keep-alive
- Confirmed MQTT publish/subscribe validation on local broker `192.168.0.61:1883`
- Aligned feature flags and production backlog with the real implemented baseline
- Added a local HTTP configuration portal for Wi-Fi, MQTT, and calibration updates
- Refactored the local HTTP portal to serve page content in chunks instead of one large buffer
- Stabilized the local HTTP portal build by splitting oversized HTML formatting into smaller chunk-safe responses
- Confirmed runtime browser validation of the local HTTP configuration portal on target hardware
- Expanded the OLED UI from a three-screen flow to a four-screen flow with a new dashboard-style `PANEL` screen
- Added a main OLED navigation menu with button-based selection for `PAINEL`, `SERVICOS`, `METRICAS`, and `SENSOR RAW`
- Replaced the textual menu cursor with a pixel-drawn triangular selector in the main OLED menu
- Simplified the `PANEL` OLED screen to show only measured electrical quantities with inline `V`, `I`, `W`, and `%` units
- Reorganized the `PANEL` OLED screen into a cleaner two-column layout with centered headings and values
- Removed automatic OLED screen cycling and made screen selection fully manual through the menu/buttons
- Reduced OLED flicker by switching from periodic full redraws to change-driven rendering
- Reworked OLED drawing to use a framebuffer plus differential page flushes to the SSD1306, reducing visible flash during updates
- Removed the animated `UI:0/1/2/3` header marker from the OLED to keep menu navigation visually stable
- Explicitly configured button GPIOs as pull-up inputs in the board HAL and added boot-time warning when a button starts active
- Added explicit `esp_driver_gpio` component dependency for button GPIO initialization under ESP-IDF 6.0
- Added raw button trace logs and GPIO mapping logs to simplify hardware/button debugging
- Fixed a regression where button handling logic existed but was not called from the main application loop
- Confirmed hardware validation of OLED menu navigation, button response, and the updated low-flicker rendering strategy
- Consolidated release-readiness, sign-off, and validation checklist documents around the current tested baseline
- Added an OTA status review documenting that OTA is currently scaffolded only and blocked by build integration plus single-app partition layout
- Added the first OTA preparation delivery with a 2MB OTA partition layout and a separate OTA-prep SDKCONFIG profile
- Recorded OTA pre-validation showing that the current firmware binary fits the proposed OTA slot size, pending a real ESP-IDF OTA-prep build
- Confirmed that the OTA-prep build passes in ESP-IDF and that the firmware fits the OTA slots, though with only about 2% free space remaining
- Started a no-feature-change binary-size reduction pass to protect OTA headroom
- Switched the active `sdkconfig` away from compiler debug optimization and reduced bootloader log verbosity for leaner images
- Removed dead serial-command parsing code and unused UI helpers from `energy_analyzer_app.c`
- Refined the OLED UI into a clearer multi-screen layout with consistent header/footer rendering
- Reduced OLED navigation friction by pausing auto-rotation after manual button input
- Added explicit OLED feedback for rotation mode while the user is in manual-hold window
- Improved Wi-Fi and MQTT runtime behavior under disconnect and reconnect
- Improved OLED degraded-state vocabulary for network and sensor faults
- Normalized production documentation around the current baseline

### Fixed

- MQTT managed-component integration for the current ESP-IDF environment
- Local MQTT wrapper/header naming conflict with the ESP-MQTT component
- MQTT restart path in the application after prior undefined config usage
- Runtime stale-measurement handling to avoid presenting old values as valid
- HTTP portal compile failures caused by oversized `snprintf` blocks and calibration form formatting regressions

### Deferred

- Voltage harmonics kept as a future feature only
- OTA and TLS hardening remain outside the current production release scope

---

Guidance:

- Add new sections for release tags such as `v1.0.0-beta` or `v1.0.0`.
- Record only user-visible, architecture-impacting, or validation-impacting
  changes.
