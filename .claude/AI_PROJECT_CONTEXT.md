# AI Project Context - Energy Analyzer

## Purpose

This file gives future AI agents a compact, reliable snapshot of the current production baseline for this repository.

Use it before proposing architectural changes, restoring legacy paths, or planning new features.

---

## Current Status

- Project: Energy Analyzer firmware for ESP32
- Date context: production baseline after Phases 6 and 7
- Current phase: Phase 8 - Production Readiness in progress
- Firmware status: operational baseline with ADS1015, OLED, Wi-Fi, MQTT, and runtime fault recovery

Primary status reference:

- `PROJECT_CURRENT_STATE.md`
- `PRODUCTION_VALIDATION_CHECKLIST.md`
- `RELEASE_READINESS_BASELINE.md`
- `RELEASE_CONFIG_REVIEW.md`
- `PRODUCTION_VALIDATION_REPORT.md`
- `RELEASE_CANDIDATE_SIGNOFF.md`

---

## Official Production Path

The active production firmware path is:

1. `main/main.c`
2. `components/app/energy_analyzer_app.c`
3. `components/board_hal/i2c_hal.c`
4. `components/board_hal/ads1015_hal.c`
5. `components/services/measurement_service.c`
6. `components/network/wifi_service.c`
7. `components/network/mqtt_client.c`

Important rule:

- Treat `components/board_hal/ads1015_hal.c` as the official acquisition path.
- Do not reintroduce the deprecated internal ADC path into the production flow.

---

## Legacy Code Policy

Legacy internal-ADC files remain in the repository only for history/reference:

- `components/adc_sensor/`
- `components/drivers/adc_driver.c`
- `components/drivers/include/adc_driver.h`

How to treat them:

- do not wire them back into the app
- do not present them as the recommended path
- only touch them if the task is explicitly about cleanup, deprecation, or archival maintenance

---

## Implemented Production Features

- ADS1015-based voltage/current acquisition
- RMS and power calculations
- OLED operational and degraded-state UI with a main menu and four destinations: `PAINEL`, `SERVICOS`, `METRICAS`, and `SENSOR RAW`
- the `PANEL` screen now uses a clean two-column layout with centered headings and values for `TENSAO`, `POTENCIA`, `CORRENTE`, and `F.P`
- `SELECT` opens the main menu from any screen, `UP/DOWN` move inside the menu, and `SELECT` confirms the destination
- the main OLED menu now uses a pixel-drawn triangular marker to indicate the currently selected item
- the OLED header is now stable text only; the old `UI:0/1/2/3` heartbeat marker was removed to avoid confusing menu navigation
- automatic screen rotation has been removed; screen changes are now user-driven only
- OLED redraws are now change-driven instead of periodic, reducing visible flicker from repeated clear-and-redraw cycles
- OLED rendering now uses a RAM framebuffer with differential page flushes to the SSD1306 instead of immediate draw calls, which is the primary anti-flicker strategy in the current firmware
- board-level button GPIOs are now explicitly configured as inputs with pull-up enabled during initialization
- `components/board_hal` now explicitly depends on `esp_driver_gpio` for ESP-IDF 6.0 GPIO input configuration
- button runtime debugging now logs raw levels and GPIO mapping to help distinguish hardware, pin selection, and debounce issues
- button polling is now explicitly executed in the main application loop; previously the button logic existed but was not being called
- button-driven menu navigation has already been validated on target hardware
- the current anti-flicker OLED strategy has also been validated by the user on hardware
- Wi-Fi provisioning AP and STA connection
- credential persistence in NVS
- local HTTP configuration portal for Wi-Fi, MQTT, and calibration
- the local HTTP portal is served in lightweight chunks to reduce stack/heap pressure
- the local HTTP portal now compiles cleanly in the active baseline after chunked-page formatting fixes
- the local HTTP portal has been runtime-validated on target hardware and operated as expected
- MQTT telemetry runtime
- Wi-Fi reconnect backoff
- MQTT runtime safety around disconnect/reconnect
- stale-measurement invalidation for sensor degradation
- project-level `menuconfig` defaults for MQTT host/IP, port, client ID, and keep-alive
- local MQTT validation already confirmed with broker `192.168.0.61:1883`
- `feature_config.h` now reflects the real baseline and keeps future analytics disabled
- OTA code exists in the repository as scaffolding (`ota_service.h`, `ota_service.c`, `ota_config.h`), but it is not in the active build, not integrated into the app, and the current release profile still uses a single-app partition layout
- the first OTA preparation delivery now exists as `partitions_ota_2mb.csv` plus `sdkconfig.defaults.ota_prep`; these are prep artifacts only and do not change the active baseline
- a structural fit check already confirmed that the current `analisador.bin` fits the proposed OTA slot size, but the real OTA-prep build has not yet been executed from a working ESP-IDF shell
- the OTA-prep build has since succeeded, confirming that the current firmware fits in the 960K OTA slots, but only with about 2% free space, so binary growth is now a hard constraint for OTA work
- a first binary-size reduction pass has already removed dead serial-command code and other unused UI helpers from `energy_analyzer_app.c`
- the active `sdkconfig` no longer uses compiler debug optimization; it has been aligned to a smaller release-oriented optimization profile to preserve OTA slot headroom

---

## Not Implemented Yet

These items may be mentioned in roadmap/docs/types, but are not part of the active baseline:

- harmonic analysis in firmware
- flicker measurement
- sag/swell event analysis
- OTA updates
- TLS-hardened production telemetry path
- authenticated local web administration

Do not assume they exist just because a type, feature flag, or roadmap section mentions them.

---

## Future Feature Decision Already Made

Voltage harmonic analysis was discussed and intentionally deferred.

Current decision:

- future feature only
- voltage harmonics only
- keep ADS1015
- scope limited to 3rd, 7th, 9th, and 11th order
- recommended algorithm: Goertzel
- not to be implemented in the current production baseline

Reference:

- `VOLTAGE_HARMONICS_FUTURE_PLAN.md`
- `OTA_STATUS_REVIEW.md`
- `OTA_PREPARATION_DELIVERY.md`

---

## Documentation Reality Check

Some older documents were written when the project was earlier in the roadmap. Prefer these files first when you need current truth:

1. `PROJECT_CURRENT_STATE.md`
2. `PRODUCTION_VALIDATION_CHECKLIST.md`
3. current source files in `components/app`, `components/board_hal`, `components/services`, and `components/network`

Treat older phase/build reports as historical snapshots, not authoritative current-state documents.

Current release-config note:

- active `sdkconfig` is still the validated working profile
- `sdkconfig.defaults.release` is the recommended release-candidate override set
- do not assume the release defaults are already bench-validated until explicitly confirmed
- current local `sdkconfig` MQTT default host is `192.168.0.61` on port `1883`
- MQTT publish/subscribe flow has already been validated against that broker
- the local HTTP config portal currently has no authentication and should be treated as trusted-network only
- the local HTTP config portal is integrated, compiling, and already validated from a browser on target hardware
- the OLED menu/buttons/panel workflow has been exercised on hardware after GPIO remap and UI rework
- release-readiness and sign-off documents were updated to treat UI/button/portal/MQTT validation as already achieved baseline evidence
- record bench execution results in `PRODUCTION_VALIDATION_REPORT.md`
- use `RELEASE_CANDIDATE_SIGNOFF.md` only after validation evidence exists

---

## Guidance For Future Agents

When making suggestions:

- preserve the ADS1015-centered architecture
- prefer production stability over speculative refactors
- keep new features isolated from the stable RMS path when possible
- update current-state documentation when architecture-level changes are made
- update release/context documents after production-facing work is completed
- avoid proposing roadmap items as if they were already implemented

When answering questions:

- distinguish clearly between implemented, planned, and deprecated
- verify against code before trusting older docs

When adding features:

- integrate through the real app flow
- keep OLED, Wi-Fi, and MQTT behavior intact unless the task explicitly changes them
- preserve the current menu-based OLED navigation unless the task is explicitly a UI redesign

---

## Required Context Update Rule

After any substantial production-facing change, update:

1. `PROJECT_CURRENT_STATE.md`
2. `.claude/AI_PROJECT_CONTEXT.md`
3. `CHANGELOG.md` when release understanding changed

This repository now treats context maintenance as part of the production work,
not as optional follow-up.
