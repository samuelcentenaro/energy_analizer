# PRODUCTION BACKLOG - Energy Analyzer

## Purpose

This document is the operational backlog for the active firmware baseline.

Use it to decide what to build next on top of the current production path,
without re-opening already closed architectural decisions.

Guiding principles:

- every step must preserve a compilable firmware
- local UI, measurement, and telemetry must evolve together
- legacy paths must not be reintroduced into the active app flow
- validation evidence should be recorded as the baseline matures

---

## Current Status

### Baseline already confirmed in code

- ADS1015 is the official acquisition path
- OLED local UI is integrated into the app flow
- RMS and power metrics are active
- Wi-Fi provisioning AP and credential persistence are active
- MQTT telemetry is active
- MQTT publish/subscribe was validated against local broker `192.168.0.61:1883`
- fault-recovery behavior exists for sensor and network degradation
- local HTTP configuration portal now exposes Wi-Fi, MQTT, and calibration forms

### Still open

- button behavior exists in firmware but is not hardware-validated
- the configuration portal currently has no authentication and should be treated as trusted-network only
- end-to-end stability validation is still pending
- OTA and TLS are still outside the current production baseline
- some legacy code remains in the repository for history/reference

### Active Phase

`Phase 8 - Production Readiness`

Immediate focus:

- keep the integrated firmware stable
- align docs and flags with the real firmware state
- complete production-facing validation evidence
- prepare the next roadmap slice without enabling OTA/TLS yet

---

## Official Production Path

1. `main/main.c`
2. `components/app/energy_analyzer_app.c`
3. `components/board_hal/i2c_hal.c`
4. `components/board_hal/ads1015_hal.c`
5. `components/services/measurement_service.c`
6. `components/network/wifi_service.c`
7. `components/network/mqtt_client.c`

Rules:

- `components/board_hal/ads1015_hal.c` is the official acquisition path
- internal ADC code remains legacy only
- local UI and telemetry must reflect the same measurement source

---

## Immediate Backlog

### 1. End-to-End Consolidation

- [ ] Confirm full integrated path remains stable:
  `boot -> OLED -> ADS1015 -> RMS -> Wi-Fi -> MQTT`
- [ ] Record confirmed validation outcomes in `PRODUCTION_VALIDATION_REPORT.md`
- [ ] Keep `PROJECT_CURRENT_STATE.md` and `.claude/AI_PROJECT_CONTEXT.md` synchronized

### 2. MQTT Configuration Decision

- [x] Add runtime configuration path for MQTT parameters
- [ ] Validate portal-driven MQTT changes under active runtime conditions
- [ ] Preserve the existing `menuconfig` defaults as the safe fallback

### 3. Release Candidate Preparation

- [ ] Validate the release-oriented config path (`sdkconfig.defaults.release`)
- [ ] Use `PRODUCTION_VALIDATION_CHECKLIST.md` as the release baseline checklist
- [ ] Use `RELEASE_CANDIDATE_SIGNOFF.md` when the candidate is ready for acceptance

### 4. Next Roadmap Slice Preparation

- [ ] Decide authentication strategy for the local configuration portal in the next version
- [ ] Prepare OTA/TLS scope without enabling it in the active baseline
- [ ] Define which OTA items are architectural preparation vs. active implementation
- [ ] Keep partition layout unchanged until OTA is explicitly started

---

## Phase Status By Reality

### Phase 0 - Baseline Consolidation

- [x] ADS1015 confirmed as official acquisition path
- [x] initialization flow unified
- [x] legacy internal ADC isolated from active build path
- [ ] remove or archive remaining conflicting legacy references where useful

### Phase 1 - Hardware Diagnostics

- [x] I2C startup path present
- [x] OLED initialization present
- [x] ADS1015 status visible in UI/logs
- [ ] final repeatable hardware-validation image still depends on bench confirmation

### Phase 2 - Raw Acquisition

- [x] raw ADS1015 values read in firmware
- [x] raw values exposed to OLED and logs
- [x] read failures surfaced cleanly
- [ ] long-duration repeated-read stability still needs bench evidence

### Phase 3 - Measurement Pipeline

- [x] RMS calculation implemented
- [x] power metrics implemented
- [x] field calibration structure prepared
- [ ] calibration against external reference still pending

### Phase 4 - Local UI

- [x] local operator UI active
- [x] `RAW`, `RMS`, and `STATE` screens active
- [x] degraded-state vocabulary active
- [ ] hardware validation of button navigation still pending

### Phase 5 - Wi-Fi

- [x] provisioning AP active
- [x] local web credential entry active
- [x] credential persistence active
- [x] reconnect logic active
- [x] shared local HTTP configuration portal active

### Phase 6 - MQTT

- [x] MQTT client integrated
- [x] queue between measurement and publish path active
- [x] telemetry payload publishing active
- [x] local broker validation confirmed
- [x] runtime web configuration path added for broker, port, client ID, and keep-alive
- [ ] external broker validation is still optional/future evidence

### Phase 7 - Fault Recovery

- [x] Wi-Fi reconnect backoff active
- [x] MQTT reconnect parameters active
- [x] stale measurement invalidation active
- [x] degraded local UI states active
- [ ] extended stability run still pending

### Phase 8 - Production Readiness

- [x] baseline docs aligned with active firmware
- [x] release-readiness artifacts created
- [x] MQTT defaults exposed through project `menuconfig`
- [ ] release-candidate evidence still needs to be fully recorded
- [ ] final sign-off still pending

---

## Explicitly Out Of Scope For Current Baseline

- voltage harmonic analysis in firmware
- flicker measurement
- sag/swell detection
- OTA activation
- TLS-hardened telemetry path
- production fleet-management features

These may exist in roadmap or types, but they are not part of the active
production baseline.

---

## Validation Artifacts To Use

- `PROJECT_CURRENT_STATE.md`
- `PRODUCTION_VALIDATION_CHECKLIST.md`
- `PRODUCTION_VALIDATION_REPORT.md`
- `RELEASE_READINESS_BASELINE.md`
- `RELEASE_CANDIDATE_SIGNOFF.md`

---

## Notes

- This backlog should reflect the real active firmware, not the earliest roadmap wording.
- Update this file when the baseline changes materially.
- Do not mark future features as active just because placeholder types or files exist.
