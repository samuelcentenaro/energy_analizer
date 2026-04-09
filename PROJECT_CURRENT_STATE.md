# Project Current State - Energy Analyzer

**Last Updated:** April 8, 2026
**Current Phase:** Phase 8 - Production Readiness in progress
**Firmware Status:** Operational production baseline

---

## Executive Summary

The firmware now has an integrated end-to-end path:

- ADS1015 acquisition
- RMS and power calculation
- OLED local UI
- Wi-Fi provisioning and reconnect
- MQTT telemetry
- runtime fault recovery for sensor and network degradation

The current focus is no longer feature bring-up. It is production consolidation:
removing ambiguity from legacy paths, keeping the public APIs aligned with the
active architecture, preserving a stable baseline for future OTA/TLS work, and
preparing release-oriented validation artifacts for the current firmware.

---

## Effective Status By Phase

### Phase 0 - Baseline Consolidation
- ADS1015 confirmed as the official acquisition path
- initialization flow unified through `main -> app -> board_hal/services/network`
- legacy internal-ADC code isolated from the active build
- compile-time feature flags now match the current production baseline
- remaining work: keep legacy references clearly marked as deprecated

### Phase 1 to Phase 4
- hardware diagnostics, raw acquisition, measurement pipeline, and local UI are active
- OLED now uses a main menu with four destinations: `PAINEL`, `SERVICOS`, `METRICAS`, and `SENSOR RAW`
- `PAINEL` is the primary visual dashboard for the measured electrical quantities only, now arranged in two centered columns for `TENSAO`, `POTENCIA`, `CORRENTE`, and `F.P`
- `SELECT` now opens the main menu from any operational screen, and menu navigation is handled with `UP/DOWN`
- the selected menu item is now indicated by a graphical triangular marker on the OLED
- OLED headers were stabilized as fixed text labels to avoid the old heartbeat marker being misread as menu cycling
- automatic screen cycling was removed; the user is now fully responsible for choosing the active screen
- OLED updates are now rendered only when display state changes, reducing visible flicker
- SSD1306 rendering now uses a buffered frame approach with page-level diff flushes, avoiding the old visible clear-and-redraw behavior
- button GPIOs are now explicitly initialized as inputs with pull-up in the board HAL before polling begins
- button polling is now wired into the main application loop so menu navigation can actually react to physical button events
- OLED shows operational and degraded states with a consistent header/footer pattern
- button-driven navigation was validated on hardware after GPIO remap and polling fixes
- the panel anti-flicker strategy was validated on hardware after adopting framebuffer + differential flush rendering

### Phase 5 - Wi-Fi
- AP provisioning works
- credentials persist in NVS
- reconnect logic is present
- Wi-Fi state is surfaced in the UI and logs
- local configuration portal now serves Wi-Fi, MQTT, and calibration forms over HTTP on the device network
- the portal page is now served in lightweight HTTP chunks instead of one large generated buffer
- the portal integration is compiling cleanly again after HTTP page-formatting fixes
- portal runtime validation completed on target hardware and operated as expected from the browser

### Phase 6 - MQTT
- MQTT client component integrated
- telemetry payload publish path active
- publish path is decoupled from measurement by queue
- MQTT start is tied to real Wi-Fi readiness
- MQTT broker host, port, client ID, and keep-alive now have project-level `menuconfig` defaults
- local broker validation completed: topic subscription and telemetry reception were confirmed on `192.168.0.61:1883`

### Phase 7 - Fault Recovery
- Wi-Fi reconnect backoff added
- MQTT timeout/reconnect parameters configured
- measurement invalidation added for stale ADS1015 data
- OLED degraded states improved
- runtime logs normalized around state transitions

### Phase 8 - Production Readiness
- in progress
- legacy public surface is being cleaned up
- documentation is being aligned with the real firmware state
- release baseline documents now exist for validation and handoff
- release-oriented SDKCONFIG overrides now exist without changing the validated active config
- validation report and sign-off templates now exist for release-candidate use
- OTA review completed: repository contains OTA scaffolding, but OTA is not in the active build, not integrated into the app, and still blocked by the current single-app partition layout
- first OTA preparation delivery completed: a 2MB OTA partition layout and a separate OTA-prep sdkconfig profile now exist, but OTA remains inactive in the baseline
- OTA pre-validation confirmed that the current binary size fits the proposed 2MB OTA slot layout, though the actual `idf.py` OTA-prep build still needs to be executed in a valid ESP-IDF shell
- OTA-prep build has now passed in the ESP-IDF environment; the firmware fits the 960K OTA slots, but with only about 2% free space remaining
- a binary-size reduction pass is now underway to protect OTA headroom without changing firmware behavior
- the active build config has been moved away from compiler debug optimization, and dead serial/UI code has been removed from the app layer to reduce image size

---

## Active Production Path

```text
main/main.c
  -> energy_analyzer_app_init()
  -> energy_analyzer_app_start()
  -> energy_analyzer_app_run()

components/board_hal/i2c_hal.c
  -> shared I2C bus

components/board_hal/ads1015_hal.c
  -> official acquisition path

components/services/measurement_service.c
  -> RMS and power metrics

components/network/wifi_service.c
  -> provisioning, station mode, reconnect

components/network/mqtt_client.c
  -> telemetry runtime
```

Official source of acquisition truth:

- `components/board_hal/ads1015_hal.c`

Legacy internal-ADC references retained only for repository history:

- `components/adc_sensor/`
- `components/drivers/adc_driver.c`

---

## Current Risks

1. The local configuration portal does not use authentication yet and must stay on trusted networks only.
2. OTA/TLS are not part of the current production baseline yet.
3. Extended-duration validation still needs bench execution outside the codebase.
4. The OLED UI has been validated functionally, but long-duration visual stability should still be confirmed during extended bench runs.

---

## Recommended Next Work

1. Consolidate end-to-end validation with measurement + OLED + Wi-Fi + MQTT running together.
2. Decide whether the unauthenticated portal should evolve into authenticated maintenance access in the next version.
3. Consolidate release-candidate validation evidence for the current baseline.
4. Use `RELEASE_READINESS_BASELINE.md` and `PRODUCTION_VALIDATION_CHECKLIST.md`
   as the acceptance pair for the current baseline.
5. Record confirmed validation outcomes in `PRODUCTION_VALIDATION_REPORT.md`.
6. Validate `sdkconfig.defaults.release` as a release-candidate profile on bench.
7. Use `RELEASE_CANDIDATE_SIGNOFF.md` when the candidate is ready for acceptance.
8. Prepare the next roadmap slice for OTA, TLS, and remote management.

Reference for the current OTA diagnosis:

- `OTA_STATUS_REVIEW.md`
- `OTA_PREPARATION_DELIVERY.md`

---

## Verification Checklist

- Build passes
- Board boots
- OLED renders
- new `PANEL` dashboard screen renders and remains legible on hardware
- manual menu navigation via `UP`, `DOWN`, and `SELECT` works on hardware
- OLED flicker behavior is improved and acceptable after framebuffer + differential flush changes
- ADS1015 reads successfully
- measurement path updates continuously
- Wi-Fi provisioning still works
- MQTT telemetry still connects and publishes
- degraded states remain visible and understandable
- release and AI-agent context documents remain synchronized with the code
