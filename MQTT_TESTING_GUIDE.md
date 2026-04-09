# MQTT Testing Guide - Energy Analyzer

## Current Baseline

The active local MQTT validation target is:

- Broker host: `192.168.0.61`
- Port: `1883`
- Client ID: `energy-analyzer-001`
- Topic: `energy-analyzer/energy-analyzer-001/telemetry`
- QoS: `0`
- Keep Alive: `60 s`

Current status:

- MQTT topic subscription was validated
- telemetry reception from the ESP32 firmware was validated

Configuration source:

- `sdkconfig`
- `Energy Analyzer -> MQTT Defaults` in `menuconfig` when the ESP-IDF shell is available
- local HTTP configuration portal on the device network for runtime updates

---

## Expected Payload

```json
{
  "voltage_rms": 232.45,
  "current_rms": 0.125,
  "power_real": 29.1,
  "power_factor": 0.987,
  "timestamp": 1712518000
}
```

---

## Topic To Subscribe

```text
energy-analyzer/energy-analyzer-001/telemetry
```

---

## Quick Validation

### Local broker subscription

```bash
mosquitto_sub -h 192.168.0.61 -p 1883 -t "energy-analyzer/energy-analyzer-001/telemetry"
```

### One-message capture

```bash
mosquitto_sub -h 192.168.0.61 -p 1883 -t "energy-analyzer/energy-analyzer-001/telemetry" -C 1
```

### JSON formatting check

```bash
mosquitto_sub -h 192.168.0.61 -p 1883 -t "energy-analyzer/energy-analyzer-001/telemetry" -C 1 | jq .
```

---

## Expected Firmware Behavior

Serial log should show the MQTT runtime progressing through:

1. `DISCONNECTED -> CONNECTING`
2. `CONNECTING -> CONNECTED`
3. periodic telemetry publish while measurements are valid

Current production behavior:

- MQTT starts only after Wi-Fi is ready
- publish does not block measurement processing
- publish is skipped while MQTT is offline
- reconnect remains tied to Wi-Fi and broker availability

---

## Expected Publish Rate

- one message every `5 seconds` while measurements are valid
- around `12 messages per minute`

---

## Troubleshooting

### No connection to broker

- verify Wi-Fi is connected
- verify broker `192.168.0.61` is reachable from the device network
- verify port `1883` is open
- check serial logs for MQTT state transitions

### No messages received

- verify the subscribed topic exactly matches the client ID
- verify the firmware is producing valid measurements
- verify MQTT state is `OK` in the OLED/UI or logs
- verify the broker is not running a different topic namespace

### Messages received but values look wrong

- verify ADS1015 acquisition is valid
- verify RMS calculations are updating in the OLED/UI
- verify bench conditions match expected voltage/current behavior

---

## Next Recommended MQTT Work

1. Keep the current local broker as the stable baseline target.
2. Decide whether broker parameters should also become runtime-configurable.
3. Add TLS/authentication only in the next OTA/security roadmap slice.

---

**Last Updated:** April 8, 2026
**Status:** Local broker publish/subscribe validation confirmed
