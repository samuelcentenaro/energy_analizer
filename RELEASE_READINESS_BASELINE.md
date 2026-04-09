# Release Readiness Baseline

## Purpose

This document defines the current production baseline that is realistic to sign
off after bench validation.

It is not an OTA/TLS release plan. It is the firmware baseline that should be
used to stabilize the product before the next roadmap slice.

---

## Baseline Identity

- Project: Energy Analyzer firmware for ESP32
- Date: April 8, 2026
- Current phase: Phase 8 - Production Readiness
- Baseline type: operational production candidate

Primary references:

- `PROJECT_CURRENT_STATE.md`
- `PRODUCTION_VALIDATION_CHECKLIST.md`
- `PRODUCTION_VALIDATION_REPORT.md`
- `RELEASE_CANDIDATE_SIGNOFF.md`
- `.claude/AI_PROJECT_CONTEXT.md`
- `RELEASE_CONFIG_REVIEW.md`

---

## In Scope For This Baseline

- boot through the unified `main -> app -> HAL/services/network` path
- ADS1015 as the official acquisition path
- voltage/current measurement pipeline with RMS and power metrics
- OLED local UI with menu-driven navigation, operational states, and degraded states
- Wi-Fi provisioning AP and credential persistence
- STA reconnect behavior with backoff
- MQTT telemetry runtime tied to real Wi-Fi readiness
- runtime degradation handling for sensor and network faults

---

## Explicitly Out Of Scope

- voltage harmonic measurement in firmware
- current harmonic measurement
- flicker and sag/swell analytics
- OTA update flow
- TLS-hardening for production telemetry
- release packaging tied to git tags or CI automation

These items may already exist in plans or docs, but they are not part of the
current production baseline.

---

## Release Candidate Criteria

The baseline is ready to be treated as a release candidate when all of the
following are true:

1. Build passes in the target ESP-IDF environment.
2. Bench validation passes using `PRODUCTION_VALIDATION_CHECKLIST.md`.
3. Wi-Fi provisioning and reconnection work without breaking measurement flow.
4. MQTT telemetry publishes and recovers cleanly after broker or network loss.
5. OLED menu, panel, and status screens remain legible and stable without objectionable flicker.
6. Button-driven navigation works reliably on target hardware.
7. Current-state documentation matches observed runtime behavior.

---

## Current Acceptance Risks

The following items still require care before any production sign-off:

1. Extended-duration stability still needs bench execution.
2. OTA and TLS are not available yet for remote fleet-grade deployment.
3. Some older documents remain historical snapshots and must not override the
   current-state references.
4. Long-duration OLED visual stability still deserves confirmation in extended
   confidence and baseline runs, even though the current anti-flicker strategy
   already passed functional hardware validation.

---

## Recommended Validation Sequence

1. `idf.py build`
2. flash and boot validation
3. ADS1015, OLED, and button/menu validation
4. Wi-Fi provisioning and reconnect test
5. MQTT connect/publish/recover test
6. local HTTP portal validation
7. induced-fault test
8. 2-hour confidence run
9. 24-hour baseline run

---

## Baseline Ready Now

The current firmware baseline is already validated for the following user-facing
behaviors on target hardware:

- MQTT publish/subscribe against broker `192.168.0.61:1883`
- local HTTP configuration portal for Wi-Fi, MQTT, and calibration
- OLED main menu with manual navigation through `UP`, `DOWN`, and `SELECT`
- `PANEL` screen with measured electrical quantities in a two-column layout
- reduced OLED flicker after moving to framebuffer + differential page flush rendering

The remaining acceptance work is therefore focused on release-confidence
duration, not on basic feature bring-up.

Recommended release-candidate config input:

- `sdkconfig.defaults.release`

---

## Release Notes Guidance

When the project advances to a named release tag, the release notes should
summarize:

- supported hardware path
- validated telemetry behavior
- local UI status coverage
- known limitations
- features intentionally deferred

Use `CHANGELOG.md` as the source for those notes.

---

## Operational Rule For Future Sessions

After completing any production-facing work:

1. update `PROJECT_CURRENT_STATE.md` if the effective baseline changed
2. update `.claude/AI_PROJECT_CONTEXT.md` for future agents
3. update `CHANGELOG.md` when the change affects release understanding

This rule keeps future sessions aligned with the actual firmware baseline.
