# Release Config Review

## Purpose

This note records the current configuration review for the production baseline
and defines the recommended release-oriented SDKCONFIG overrides.

---

## Current Config Snapshot

Observed in the active `sdkconfig`:

- application optimization is still set to debug
- default runtime log level is INFO
- bootloader log level is INFO
- partition layout is `single_app`
- task watchdog is enabled
- core dump is disabled
- Bluetooth is disabled

This is close to a usable production baseline, but it still carries a
development-oriented optimization profile.

---

## Main Finding

The most important release gap is:

- `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y`

For a production-candidate build, the application should not stay on debug
optimization by default.

---

## Recommended Release Overrides

The repository now includes:

- `sdkconfig.defaults.release`
- `sdkconfig.defaults.ota_prep`

This file keeps the current architecture assumptions and only changes the items
that matter most for a release-oriented build:

- explicit app version `1.0.0`
- performance optimization instead of debug optimization
- bootloader logs reduced from INFO to WARN
- INFO-level runtime logs preserved
- single-app partition layout preserved
- watchdogs preserved
- core dumps still disabled

---

## Why Single-App Is Still Correct

The current production baseline does not include OTA as an accepted release
feature. Because of that, the existing `single_app` partition layout remains the
correct baseline for now.

Do not switch to an OTA partition layout until the OTA roadmap slice is
actually being implemented and validated.

That said, the repository now also includes an OTA-preparation profile and a
2MB OTA-capable partition file for the next roadmap slice:

- `sdkconfig.defaults.ota_prep`
- `partitions_ota_2mb.csv`

---

## Recommended Adoption Path

Use the release defaults in a dedicated release-candidate configuration pass,
instead of silently changing the already validated working `sdkconfig`.

Suggested command flow:

```bash
idf.py fullclean
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults.release" reconfigure
idf.py build
```

After that:

1. flash the release-candidate build
2. run `PRODUCTION_VALIDATION_CHECKLIST.md`
3. update `PROJECT_CURRENT_STATE.md` if the release config becomes the active default

---

## Caution

This file defines the recommended release candidate profile.

It does not prove runtime behavior by itself. The release profile still needs
bench validation after build and flash.

---

**Last Updated:** April 8, 2026
**Status:** Recommended release-oriented config baseline created
