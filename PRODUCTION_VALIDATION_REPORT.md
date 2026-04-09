# Production Validation Report

## Purpose

Use this report to capture the actual result of a release-candidate validation
run on hardware.

This document is meant to be filled after executing
`PRODUCTION_VALIDATION_CHECKLIST.md`.

---

## Validation Identity

- Date:
- Firmware build:
- Config profile:
- Board / hardware revision:
- ESP-IDF version:
- Operator:
- Environment:

Suggested config profile values:

- `active sdkconfig`
- `sdkconfig.defaults.release`

---

## Validation Scope

Check the scope that was actually executed:

- [ ] Build validation
- [ ] Boot validation
- [ ] Measurement validation
- [ ] OLED / local UI validation
- [ ] Wi-Fi validation
- [ ] MQTT validation
- [ ] Fault-recovery validation
- [ ] 15-minute smoke run
- [ ] 2-hour confidence run
- [ ] 24-hour baseline run

---

## Summary Result

- Overall status:
  - [ ] PASS
  - [ ] PASS WITH LIMITATIONS
  - [ ] FAIL

- Release-candidate recommendation:
  - [ ] Accept as current production baseline
  - [ ] Accept with documented limitations
  - [ ] Reject until fixes are applied

Latest confirmed validation note:

- MQTT topic subscription and telemetry reception were confirmed on broker
  `192.168.0.61:1883`
- Local HTTP configuration portal access and operation were confirmed on target
  hardware for Wi-Fi, MQTT, and calibration forms
- OLED menu navigation, button response, and updated low-flicker panel rendering
  were confirmed on target hardware

---

## Build Result

- Build command used:
- Build passed:
- Warnings observed:
- Binary/partition fit confirmed:
- Notes:

---

## Boot Result

- Boot sequence stable:
- OLED initialization result:
- ADS1015 initialization result:
- Wi-Fi service initialization result:
- MQTT runtime startup behavior:
- Notes:

---

## Measurement Result

- Voltage RMS updates:
- Current RMS updates:
- Power metrics update:
- Stale-value invalidation observed:
- ADS degraded/error state behavior:
- Notes:

---

## OLED / UI Result

- Raw screen:
- Summary screen:
- Screen rotation:
- Heartbeat visibility:
- Status text coherence:
- Notes:
  Latest confirmed point: menu-based navigation with `UP/DOWN/SELECT` works on
  hardware, the `PANEL` screen layout is in active use, and visible flicker was
  reduced after framebuffer + differential flush rendering

---

## Wi-Fi Result

- Provisioning AP visibility:
- Web UI reachable at `http://192.168.4.1`:
- Credential persistence:
- STA connection:
- IP visibility in UI/logs:
- Reconnect backoff behavior:
- Notes:
  Latest confirmed point: local HTTP configuration portal reached from browser and
  operated as expected on target hardware

---

## MQTT Result

- MQTT starts only after Wi-Fi ready:
- Broker connection:
- Telemetry publish:
- Offline behavior:
- Recovery after broker return:
- Notes:
  Current confirmed point: topic subscription and telemetry reception were validated on
  `energy-analyzer/energy-analyzer-001/telemetry`

---

## Fault-Recovery Result

- ADS fault induced:
- Wi-Fi fault induced:
- MQTT/broker fault induced:
- Deadlock observed:
- Silent freeze observed:
- Notes:

---

## Stability Result

- Smoke test duration:
- Confidence test duration:
- Baseline run duration:
- Resets observed:
- Memory-growth suspicion:
- Log readability over time:
- Notes:

---

## Known Limitations Observed

List only the limitations actually observed during validation:

1.
2.
3.

---

## Recommended Follow-Up

1.
2.
3.

---

## Sign-Off

- Validation owner:
- Technical reviewer:
- Baseline accepted:
  - [ ] Yes
  - [ ] No

- Acceptance date:
- Final note:
