# OTA Status Review

**Date:** April 8, 2026  
**Scope:** review the real OTA status in code, compare it against the roadmap, and define a safe next-step plan.

---

## Summary

OTA is **not ready** in the current firmware baseline.

There is a partial OTA codebase in the repository, but it is not yet part of
the active production flow and should currently be treated as **architectural
scaffolding**, not as an implemented feature.

---

## What Already Exists

The repository already contains:

- public OTA API in `components/network/include/ota_service.h`
- OTA configuration constants in `components/config/ota_config.h`
- an OTA implementation file in `components/network/ota_service.c`
- a planning document in `OTA_IMPLEMENTATION_PLAN.md`
- roadmap coverage in `DEVELOPMENT_ROADMAP.md`

This means OTA was already designed at API/config level and partially started in
code.

---

## What Blocks OTA Today

### 1. OTA is not in the active build

`components/network/CMakeLists.txt` currently builds:

- `wifi_service.c`
- `mqtt_client.c`

It does **not** build `ota_service.c`.

So even though OTA source code exists, it is not linked into the running
firmware baseline.

### 2. OTA is not integrated into the application flow

The active application path is:

- `main/main.c`
- `components/app/energy_analyzer_app.c`
- `components/board_hal/*`
- `components/services/measurement_service.c`
- `components/network/wifi_service.c`
- `components/network/mqtt_client.c`

There is currently no OTA startup, no OTA task control, no menu entry, and no
runtime orchestration in `energy_analyzer_app.c`.

### 3. The current partition layout is single-app

The release profile still explicitly uses:

- `CONFIG_PARTITION_TABLE_SINGLE_APP=y`
- `CONFIG_PARTITION_TABLE_FILENAME="partitions_singleapp.csv"`

That is correct for the current baseline, but it also means OTA cannot become
real without a partition-layout change.

### 4. The implementation is still stubbed

In `components/network/ota_service.c`:

- `ota_check_for_updates()` is marked as stub
- `ota_perform_update()` is marked as stub

So the most important runtime parts are not complete yet.

### 5. Security goals are defined but not implemented end-to-end

`components/config/ota_config.h` already declares ambitions such as:

- HTTPS
- hash validation
- signature verification
- certificate pinning

But these are configuration-level intentions only. They do not yet correspond
to a completed, validated OTA pipeline.

---

## Comparison With The Roadmap

The roadmap expects OTA in Phase 7.5 with these milestones:

1. OTA partition support
2. OTA service implementation
3. integration with UI/runtime
4. security validation and rollback testing

Current reality versus roadmap:

- partition support: **not done**
- OTA service file created: **partially done**
- runtime integration: **not done**
- security validation: **not done**
- rollback testing: **not done**

So the roadmap intent exists, but real execution is still at the very beginning
of Phase 7.5.

---

## Recommended Implementation Strategy

The safest OTA path for this project is:

### Step 1. Prepare the architecture only

- keep current production baseline untouched
- document OTA as the next roadmap slice
- avoid mixing OTA bring-up with ongoing baseline stabilization

### Step 2. Switch partition layout first

- create an OTA-capable partition table with `ota_0` and `ota_1`
- verify binary fit against the new layout
- keep a rollback-capable scheme from the start

This is the real technical gate. Without it, OTA is not actionable.

### Step 3. Bring OTA service into the build

- add `ota_service.c` to `components/network/CMakeLists.txt`
- add the required ESP-IDF dependencies (`esp_https_ota`, `app_update`, `esp_partition`, and any TLS dependencies actually used)
- keep the OTA service isolated from MQTT and measurement flow

### Step 4. Finish the service before UI exposure

- implement update check
- implement actual firmware download
- implement image validation and install
- implement reboot/boot confirmation logic
- implement failure mapping and logs

Only after the service is real should it appear in the user-facing UI.

### Step 5. Add manual OTA entry before automatic OTA

For the first OTA-capable version, prefer:

- manual trigger only
- local status/progress view
- no automatic periodic update yet

This reduces risk while validating the pipeline.

### Step 6. Add security and rollback validation

Before calling OTA production-ready:

- HTTPS certificate validation must pass
- rollback scenario must be tested
- interrupted download must be tested
- invalid image must be rejected safely

---

## Recommended Next Deliverable

The next OTA deliverable should **not** be “OTA complete”.

It should be:

**OTA Preparation Milestone**

Including:

- OTA-capable partition plan
- build integration plan
- dependency list
- decision on manual-first OTA flow
- acceptance checklist for the first real OTA implementation

---

## Decision

At this moment:

- OTA is **planned**
- OTA is **partially scaffolded**
- OTA is **not implemented**
- OTA is **not validated**
- OTA should be treated as the next major roadmap slice after the current baseline

