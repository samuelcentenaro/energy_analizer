# Documentation Index - Energy Analyzer

## Purpose

Use this index to find the current production documents first and avoid relying
on older phase reports as if they were the active firmware baseline.

---

## Read First

These are the primary documents for the current firmware state:

1. [PROJECT_CURRENT_STATE.md](PROJECT_CURRENT_STATE.md)
2. [PRODUCTION_VALIDATION_CHECKLIST.md](PRODUCTION_VALIDATION_CHECKLIST.md)
3. [RELEASE_READINESS_BASELINE.md](RELEASE_READINESS_BASELINE.md)
4. [PRODUCTION_VALIDATION_REPORT.md](PRODUCTION_VALIDATION_REPORT.md)
5. [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)
6. [VOLTAGE_HARMONICS_FUTURE_PLAN.md](VOLTAGE_HARMONICS_FUTURE_PLAN.md)

---

## Current Production Docs

### [PROJECT_CURRENT_STATE.md](PROJECT_CURRENT_STATE.md)

What it is:
Current source-of-truth snapshot of the firmware baseline.

Use it for:
- implemented features
- active architecture
- current phase
- remaining production risks

### [PRODUCTION_VALIDATION_CHECKLIST.md](PRODUCTION_VALIDATION_CHECKLIST.md)

What it is:
Bench and release validation checklist for the active firmware.

Use it for:
- build verification
- OLED and ADS checks
- Wi-Fi and MQTT validation
- induced fault and stability tests

### [RELEASE_READINESS_BASELINE.md](RELEASE_READINESS_BASELINE.md)

What it is:
Short release-oriented summary of what is in scope for the current production
baseline and what is intentionally out of scope.

Use it for:
- release candidate alignment
- handoff between sessions
- checking whether the firmware is ready for a production baseline sign-off

### [PRODUCTION_VALIDATION_REPORT.md](PRODUCTION_VALIDATION_REPORT.md)

What it is:
Fillable report for recording real bench validation results for a release
candidate.

Use it for:
- capturing bench results
- recording observed limitations
- supporting release acceptance decisions

### [RELEASE_CANDIDATE_SIGNOFF.md](RELEASE_CANDIDATE_SIGNOFF.md)

What it is:
Short approval record for accepting or rejecting a release candidate.

Use it for:
- baseline acceptance
- decision traceability
- formalizing release-candidate outcome

### [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)

What it is:
Practical flashing, setup, and deployment guidance for the current firmware.

Use it for:
- build and flash commands
- first boot and provisioning flow
- deployment troubleshooting

### [MQTT_TESTING_GUIDE.md](MQTT_TESTING_GUIDE.md)

What it is:
Guide for validating MQTT telemetry behavior.

Use it for:
- broker connection checks
- publish validation
- telemetry troubleshooting

### [VOLTAGE_HARMONICS_FUTURE_PLAN.md](VOLTAGE_HARMONICS_FUTURE_PLAN.md)

What it is:
Future-feature study for voltage harmonics using ADS1015 and Goertzel.

Use it for:
- future planning only
- voltage harmonics scope decisions
- MQTT and Grafana integration planning

---

## Engineering Reference Docs

### [CODING_STANDARDS.md](CODING_STANDARDS.md)

Coding conventions and engineering expectations for the firmware repository.

### [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md)

Original roadmap and long-term phase intent.

Important note:
Treat this as a planning document, not the authoritative statement of what is
already implemented today.

### [PRODUCTION_BACKLOG.md](PRODUCTION_BACKLOG.md)

Operational engineering backlog used to guide implementation sequencing.

### [ARCHITECTURE_REFERENCE.md](ARCHITECTURE_REFERENCE.md)

Supplementary architecture reference for module boundaries and data flow.

### [HARDWARE_DIAGRAM.md](HARDWARE_DIAGRAM.md)

Hardware wiring and board-level reference.

---

## Historical Snapshot Docs

These documents are useful as history, but should not be treated as the current
firmware truth without cross-checking newer files:

- [BUILD_COMPLETION_REPORT.md](BUILD_COMPLETION_REPORT.md)
- [BUILD_COMPLETION_CHECKLIST.md](BUILD_COMPLETION_CHECKLIST.md)
- [PHASE_1_COMPLETION_REPORT.md](PHASE_1_COMPLETION_REPORT.md)
- [ESP_IDF_UPDATE_STATUS.md](ESP_IDF_UPDATE_STATUS.md)

Use them only when you need historical context about earlier migration or phase
work.

---

## Suggested Reading Paths

### For a new firmware session

1. [PROJECT_CURRENT_STATE.md](PROJECT_CURRENT_STATE.md)
2. [PRODUCTION_VALIDATION_CHECKLIST.md](PRODUCTION_VALIDATION_CHECKLIST.md)
3. [PRODUCTION_BACKLOG.md](PRODUCTION_BACKLOG.md)
4. [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)

### For release and production consolidation

1. [RELEASE_READINESS_BASELINE.md](RELEASE_READINESS_BASELINE.md)
2. [PRODUCTION_VALIDATION_CHECKLIST.md](PRODUCTION_VALIDATION_CHECKLIST.md)
3. [PRODUCTION_VALIDATION_REPORT.md](PRODUCTION_VALIDATION_REPORT.md)
4. [PROJECT_CURRENT_STATE.md](PROJECT_CURRENT_STATE.md)

### For future feature planning

1. [PROJECT_CURRENT_STATE.md](PROJECT_CURRENT_STATE.md)
2. [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md)
3. [VOLTAGE_HARMONICS_FUTURE_PLAN.md](VOLTAGE_HARMONICS_FUTURE_PLAN.md)

---

## Current Reality Reminder

- ADS1015 is the official acquisition path.
- Internal ADC code is legacy and must stay isolated from production flow.
- Wi-Fi and MQTT are implemented in the active baseline.
- Harmonics are not implemented in the current firmware.
- OTA and TLS hardening remain outside the current production baseline.

---

**Last Updated:** April 8, 2026
**Documentation Baseline:** Phase 8 production readiness
