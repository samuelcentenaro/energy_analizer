# Production Validation Checklist

## Purpose

Use this checklist to validate the current production baseline of the Energy Analyzer firmware after code changes, release candidates, or bench updates.

This checklist reflects the active baseline:

- ADS1015 acquisition
- RMS and power calculations
- OLED local UI with menu-driven manual navigation
- Wi-Fi provisioning and reconnect
- MQTT telemetry
- local HTTP configuration portal
- fault recovery for sensor/network degradation

---

## 1. Build Validation

- [ ] Build completes successfully in the ESP-IDF environment
- [ ] No new compiler errors appear
- [ ] No unexpected warnings appear
- [ ] Application binary still fits the current partition layout

Suggested command:

```bash
idf.py build
```

---

## 2. Boot Validation

- [ ] Device boots reliably after power cycle
- [ ] Serial monitor shows application startup
- [ ] OLED initializes without blank screen or freeze
- [ ] I2C shared bus starts correctly
- [ ] ADS1015 initialization succeeds or degrades cleanly

Expected high-level sequence:

1. `Energy Analyzer starting`
2. app initialization
3. I2C ready
4. ADS1015 HAL ready
5. measurement service ready
6. Wi-Fi service ready
7. MQTT runtime deferred or started depending on network state

---

## 3. Measurement Validation

- [ ] Voltage RMS updates continuously
- [ ] Current RMS updates continuously
- [ ] Power and power factor update when measurements are valid
- [ ] No stale values remain displayed after sensor degradation timeout
- [ ] ADS fault results in degraded or error state instead of silent freeze

Bench checks:

- [ ] Voltage input variation changes displayed values
- [ ] Current input variation changes displayed values
- [ ] Sensor disconnect or read failure does not hang the firmware

---

## 4. OLED / Local UI Validation

- [ ] Main menu renders
- [ ] `PAINEL` screen renders
- [ ] `SERVICOS` screen renders
- [ ] `METRICAS` screen renders
- [ ] `SENSOR RAW` screen renders
- [ ] `UP`, `DOWN`, and `SELECT` navigate correctly
- [ ] Selected menu item is clearly indicated by the triangular marker
- [ ] Status text is readable and coherent
- [ ] No objectionable OLED flicker is observed in normal use

Current expected local states include:

- `NET:AP`
- `NET:RETRY`
- `NET:IP`
- `NET:ERR`
- `MQTT:OFF`
- `MQTT:WAIT`
- `MQTT:OK`
- `MQTT:ERR`
- `ADS:OK`
- `ADS:DEG`
- `ADS:ERR`
- `RMS:OK`
- `RMS:WAIT`
- `RMS:DEG`

---

## 5. Wi-Fi Validation

- [ ] Provisioning AP appears when credentials are absent
- [ ] Web provisioning/configuration page opens at `http://192.168.4.1`
- [ ] Submitted credentials are stored successfully
- [ ] Device connects in STA mode when credentials are valid
- [ ] IP address appears in UI/logs after connection
- [ ] Wi-Fi reconnect backoff works after forced disconnect
- [ ] Local HTTP portal remains usable after device joins the LAN

Failure-mode checks:

- [ ] Wrong credentials do not block the firmware loop
- [ ] Disconnect/reconnect does not freeze OLED updates
- [ ] Repeated Wi-Fi loss does not flood logs uncontrollably

---

## 6. MQTT Validation

- [ ] MQTT runtime starts only when Wi-Fi is connected
- [ ] MQTT connects to configured broker
- [ ] Telemetry publishes periodically
- [ ] Publish failures do not block acquisition
- [ ] MQTT stop/start behavior remains safe during Wi-Fi loss and return

Current payload expectation:

- voltage RMS
- current RMS
- real power
- power factor
- timestamp

Failure-mode checks:

- [ ] Broker offline does not freeze the firmware
- [ ] Reconnect path recovers after broker return
- [ ] Log output remains readable during prolonged MQTT outage

---

## 7. Fault-Recovery Validation

- [ ] ADS1015 read loss enters degraded behavior
- [ ] Wi-Fi loss transitions to reconnect behavior
- [ ] MQTT loss transitions to offline/error behavior
- [ ] No deadlock appears between telemetry and acquisition
- [ ] No silent freeze appears under repeated faults

Suggested induced-fault tests:

- [ ] remove or disturb ADS1015 path temporarily
- [ ] disable Wi-Fi AP/router temporarily
- [ ] stop MQTT broker temporarily

---

## 8. Stability Validation

- [ ] Firmware remains responsive during extended run
- [ ] OLED keeps updating over time
- [ ] Wi-Fi remains recoverable after repeated drops
- [ ] MQTT remains recoverable after repeated drops
- [ ] No obvious memory-growth symptoms are observed

Suggested durations:

- smoke test: 15 minutes
- confidence test: 2 hours
- production baseline test: 24 hours

---

## 9. Release Readiness

- [ ] Active architecture matches documentation
- [ ] Legacy internal-ADC path remains isolated and unused
- [ ] Current-state documentation matches the actual firmware
- [ ] Deployment instructions still apply to the current baseline
- [ ] Next-step items for OTA/TLS remain out of the current release scope
- [ ] Button and OLED validation notes are reflected in `PRODUCTION_VALIDATION_REPORT.md`

---

## Sign-Off

- [ ] Build validated
- [ ] Bench validated
- [ ] Network validated
- [ ] Telemetry validated
- [ ] Fault recovery validated
- [ ] Production baseline accepted
