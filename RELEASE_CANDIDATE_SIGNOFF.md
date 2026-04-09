# Release Candidate Sign-Off

## Purpose

This is the short decision record for accepting or rejecting the current
production-candidate firmware baseline.

Use it only after:

1. `sdkconfig.defaults.release` build succeeds
2. `PRODUCTION_VALIDATION_CHECKLIST.md` is executed
3. `PRODUCTION_VALIDATION_REPORT.md` is filled

---

## Candidate Identity

- Candidate version:
- Candidate date:
- Build profile:
- Board / hardware revision:
- ESP-IDF version:

---

## Decision

- [ ] Accepted as production baseline
- [ ] Accepted with limitations
- [ ] Rejected

---

## Evidence

- Validation report:
- Checklist completed by:
- Build verified by:
- Bench duration achieved:

---

## Accepted Scope

If accepted, this candidate covers:

- ADS1015 acquisition
- RMS and power calculations
- OLED local UI with main menu, manual button navigation, and low-flicker panel rendering
- Wi-Fi provisioning and reconnect
- MQTT telemetry runtime
- local HTTP configuration portal for Wi-Fi, MQTT, and calibration
- fault recovery for sensor/network degradation

---

## Explicitly Deferred

The following remain outside this sign-off unless separately approved:

- OTA
- TLS hardening
- voltage harmonics
- local web authentication

---

## Limitations / Conditions

1.
2.
3.

---

## Approval

- Technical owner:
- Reviewer:
- Approval date:
- Final note:
