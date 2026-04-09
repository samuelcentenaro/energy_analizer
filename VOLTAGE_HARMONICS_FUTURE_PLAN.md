# Voltage Harmonics Future Plan

## Purpose

Document a future feature for voltage-only harmonic measurement in the Energy Analyzer firmware.

This plan keeps the current external ADS1015 acquisition path and limits scope to:

- voltage harmonics only
- 3rd, 7th, 9th, and 11th order
- no implementation in the current production baseline

---

## Scope Definition

### Included

- voltage harmonic analysis only
- harmonic orders:
  - 3rd
  - 7th
  - 9th
  - 11th
- future MQTT publication for dashboard consumption
- future compact OLED summary screen

### Excluded

- current harmonics
- full FFT spectrum display
- flicker
- sag/swell
- IEC-grade power quality certification

---

## Current Firmware Reality

The current firmware does **not** implement harmonic analysis.

Today the production path is:

- `components/board_hal/ads1015_hal.c`
- `components/services/measurement_service.c`
- `components/app/energy_analyzer_app.c`

Current calculations are limited to:

- RMS voltage
- RMS current
- real power
- apparent power
- reactive power
- power factor

The existing OLED UI does not expose harmonic information.

---

## Harmonic Targets

Assuming nominal mains frequency near 60 Hz, the target frequencies are:

- 3rd harmonic: 180 Hz
- 7th harmonic: 420 Hz
- 9th harmonic: 540 Hz
- 11th harmonic: 660 Hz

These are the only harmonic orders planned for the future feature.

---

## Constraint: Keep ADS1015

This plan intentionally keeps the ADS1015 as the acquisition device.

That decision has important consequences:

- the feature must remain modest in scope
- the algorithm should target only a small set of frequencies
- the acquisition path must be more disciplined than the current RMS path

Because the ADS1015 is slower than a dedicated high-speed ADC, the implementation should avoid a broad-spectrum FFT design and instead use a selective-frequency approach.

---

## Recommended Algorithm

Use **Goertzel** instead of FFT.

Why Goertzel fits this feature:

- only a few harmonic bins are required
- lower RAM cost than FFT
- lower CPU cost than FFT for this scope
- easier to validate against known frequencies
- better fit for an ADS1015-based future implementation

Future analysis outputs should include:

- estimated fundamental frequency or assumed nominal frequency
- H3 amplitude or percentage
- H7 amplitude or percentage
- H9 amplitude or percentage
- H11 amplitude or percentage
- voltage THD based on the included orders

---

## Required Architectural Changes

This feature should **not** be implemented inside the current `measurement_service.c`.

Create a dedicated analysis component, for example:

- `components/analysis/include/voltage_harmonics_service.h`
- `components/analysis/voltage_harmonics_service.c`

Recommended future data flow:

1. ADS1015 voltage samples are captured into a fixed analysis window.
2. A dedicated analysis service runs Goertzel for the selected harmonic orders.
3. A result structure is produced.
4. The app layer decides what to show locally and what to publish remotely.

This keeps harmonic processing isolated from the stable RMS production pipeline.

---

## Acquisition Strategy With ADS1015

To keep ADS1015 and still make this feasible, the future implementation should:

1. prioritize voltage-only acquisition during harmonic windows
2. use the highest practical ADS1015 data rate supported by the design
3. use a fixed sampling interval with minimal jitter
4. avoid mixing the harmonic window with unrelated blocking work
5. store one analysis window in a dedicated buffer before running Goertzel

Important note:

The current firmware alternates responsibilities around a cooperative main loop and single-shot reads. That is acceptable for RMS-oriented behavior, but future harmonic analysis will need a more controlled sampling window than the current approach.

---

## Future Result Model

Suggested future result structure:

```c
typedef struct {
    float voltage_rms;
    float thd_voltage_percent;
    float harmonic_3_percent;
    float harmonic_7_percent;
    float harmonic_9_percent;
    float harmonic_11_percent;
    float fundamental_hz;
    uint32_t timestamp_ms;
    bool valid;
} voltage_harmonics_result_t;
```

This structure should be separate from the current `measurement_result_t`.

---

## Future OLED Presentation

Do not attempt a full harmonic spectrum on the SSD1306 display.

Recommended future OLED screen:

- line 1: `PQ VOLT`
- line 2: `THD:2.1%`
- line 3: `H3 :1.2%`
- line 4: `H7 :0.4%`
- line 5: `H9 :0.3%`
- line 6: `H11:0.2%`
- line 7: `NET:OK`
- line 8: `UI:<heartbeat>`

This should be a summary screen only, added as another rotation target in the app layer.

---

## Future MQTT Design

Recommended topic:

- `energy-analyzer/<device_id>/power-quality/voltage`

Recommended JSON payload:

```json
{
  "ts": 1712518000,
  "voltage_rms": 232.4,
  "thd_voltage": 2.1,
  "h3": 1.2,
  "h7": 0.4,
  "h9": 0.3,
  "h11": 0.2,
  "fundamental_hz": 60.0,
  "valid": true
}
```

This is the preferred payload shape for future dashboard integration because it:

- keeps all harmonic values in one publish event
- is easy to parse in Node-RED or Telegraf
- maps cleanly to time-series storage
- avoids topic explosion

---

## Grafana Integration Example

Recommended external flow:

1. ESP32 publishes harmonic summary JSON to MQTT.
2. Telegraf or Node-RED subscribes to the topic.
3. The data is written to InfluxDB.
4. Grafana reads the time-series data and renders:
   - THD over time
   - H3 over time
   - H7 over time
   - H9 over time
   - H11 over time

Suggested Grafana panels:

- voltage THD trend
- harmonic order comparison
- alarm panel for high THD
- latest harmonic summary table

---

## Risks and Limitations

### Technical Risks

- ADS1015 sample rate leaves limited headroom for spectral work
- cooperative loop timing may add jitter if not redesigned for the feature
- harmonic accuracy may vary with mains frequency drift and sensor conditioning

### Product Risks

- values may be useful for monitoring/trending without being suitable for compliance-grade claims
- UI space is limited, so only summary information should be shown locally

---

## Recommendation

Treat this as a future monitoring feature, not a current production requirement.

When implemented, prefer:

- ADS1015 retained
- voltage-only mode
- Goertzel
- compact OLED summary
- JSON summary over MQTT

Do not merge this into the current RMS/power path until a dedicated acquisition window and validation strategy are defined.

---

## Suggested Future Implementation Order

1. define the harmonic result structure
2. create `components/analysis/voltage_harmonics_service.*`
3. add fixed-window voltage sampling support using ADS1015
4. implement Goertzel for H3, H7, H9, H11
5. derive THD from the selected harmonic set
6. expose a compact OLED summary screen
7. publish harmonic summary via MQTT
8. validate against known test waveform sources
