# OTA Preparation Delivery

**Date:** April 8, 2026  
**Purpose:** first OTA delivery focused on architecture and partition readiness, without enabling OTA in the active production baseline.

---

## Delivered Artifacts

This delivery adds:

- `partitions_ota_2mb.csv`
- `sdkconfig.defaults.ota_prep`
- `OTA_STATUS_REVIEW.md`

These files are preparation artifacts only. They do **not** activate OTA in the
current firmware baseline.

---

## What This Delivery Solves

### 1. OTA-capable partition layout for the real hardware profile

The current project is configured for:

- flash size: `2MB`
- current active layout: `single_app`

This delivery introduces an OTA-ready custom partition layout for 2MB flash:

- `nvs` = `0x6000`
- `otadata` = `0x2000`
- `phy_init` = `0x1000`
- `ota_0` = `0xF0000`
- `ota_1` = `0xF0000`

This is the key architectural step required before real OTA can exist.

### 2. Safe configuration path

The new profile `sdkconfig.defaults.ota_prep` lets the team test OTA partition
preparation without overwriting the validated production baseline.

### 3. Rollback-ready direction

The preparation profile enables rollback-related settings so the next OTA phase
can be validated with a safer boot strategy.

---

## What This Delivery Does Not Do

It does **not**:

- activate OTA in the running firmware
- compile `ota_service.c` into the active build
- add OTA menu entries
- implement version checks
- perform firmware download
- validate TLS/certificate flow
- validate rollback on real hardware

So OTA remains **not implemented** in the current production candidate.

---

## Expected Next Implementation Slice

The next OTA slice should do this in order:

1. Validate the OTA partition profile builds cleanly.
2. Confirm the binary still fits inside `ota_0` / `ota_1`.
3. Add `ota_service.c` to `components/network/CMakeLists.txt`.
4. Add the ESP-IDF dependencies required by the real OTA implementation.
5. Integrate manual OTA trigger and status reporting.
6. Validate rollback and interrupted-download behavior.

---

## Suggested Build Flow

When the team is ready to test the OTA-preparation profile:

```bash
idf.py fullclean
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults.ota_prep" reconfigure
idf.py build
```

Only after that passes should the team move into real OTA service integration.

---

## Pre-Validation Result

A structural pre-check was completed against the current build artifacts:

- current binary: `build/analisador.bin`
- observed size: `965120` bytes
- OTA slot size in `partitions_ota_2mb.csv`: `0xF0000` = `983040` bytes
- current fit result: **fits**
- current free margin per OTA slot: `17920` bytes

Important note:

- this is a useful fit check, but it is **not** a substitute for an actual
  `idf.py` OTA-prep build
- the current shell used for this review did not have `idf.py` available, so the
  real build step still needs to be run from the ESP-IDF environment

---

## OTA-Prep Build Result

The OTA-preparation build was later executed in a valid ESP-IDF environment and
completed successfully.

Observed result:

- partition table generated successfully
- OTA layout recognized as:
  - `otadata` = `8K`
  - `ota_0` = `960K`
  - `ota_1` = `960K`
- application binary size = `0xEBA00`
- smallest app partition = `0xF0000`
- free margin = `0x4600` = `17920` bytes
- free space percentage = `2%`

Important conclusion:

- the OTA-prep build **passes**
- the current firmware **fits**
- but the margin is now **too tight for comfort**

This means the project can move to OTA integration work, but size control must
be treated as an active constraint from this point forward.

---

## Decision

This delivery is considered successful if:

- the repository now has a realistic OTA-capable partition layout for 2MB flash
- OTA can be prepared without modifying the active release baseline
- future OTA work has a clear starting point
