# DEVELOPMENT ROADMAP - Energy Analyzer

## Phase 1: Foundation & Hardware Setup (Week 1-2)

### 1.1 ESP-IDF Configuration
- [ ] Install ESP-IDF (v5.1 or latest)
- [ ] Create build configuration scripts
- [ ] Test basic compilation and flashing
- [ ] Set up hardware COM port connection
- [ ] Verify serial monitor output

### 1.2 ADC & Sensor Integration
- [ ] Configure ADS1015 ADC for ZMPT101B voltage sensor
- [ ] Implement voltage sampling routine
- [ ] Configure ADS1015 ADC for SCT013-030 current sensor
- [ ] Implement current sampling routine
- [ ] Create calibration procedure
- [ ] Write first unit tests

**Deliverable:** ADS1015 ADC module reads and logs sensor values every 100ms

**Related Files:**
- `components/adc_sensor/`
- `components/config/hardware_config.h`
- `components/config/timing_config.h`

---

## Phase 2: Display & User Interface (Week 2-3)

### 2.1 OLED Display Driver
- [ ] Initialize I2C bus for SSD1306 display
- [ ] Implement basic text rendering
- [ ] Create display abstraction layer
- [ ] Test screen brightness and contrast
- [ ] Add frame buffer management

### 2.2 Button Interface
- [ ] Configure GPIO for button inputs
- [ ] Implement debounce logic
- [ ] Create button event system
- [ ] Add long-press detection
- [ ] Integrate with menu navigation

### 2.3 Menu System
- [ ] Design menu structure and hierarchy
- [ ] Implement menu state machine
- [ ] Create display update callbacks
- [ ] Add menu item cycling logic
- [ ] Test navigation flow

**Deliverable:** Working UI showing real-time voltage/current on OLED with button control

**Related Files:**
- `components/display/`
- `components/ui/`

---

## Phase 3: Signal Analysis (Week 3-4)

### 3.1 RMS Calculation
- [ ] Implement RMS voltage calculation
- [ ] Implement RMS current calculation
- [ ] Create sliding window buffer
- [ ] Optimize for real-time performance
- [ ] Add unit tests

### 3.2 Harmonic Analysis
- [ ] Implement FFT-based harmonic detection
- [ ] Calculate THD (Total Harmonic Distortion)
- [ ] Extract fundamental and harmonic amplitudes
- [ ] Create harmonic display format
- [ ] Optimize computational load

### 3.3 Event Detection (Sag/Swell)
- [ ] Implement SAG detection algorithm
- [ ] Implement SWELL detection algorithm
- [ ] Create event logging system
- [ ] Add threshold configuration
- [ ] Test with simulated waveforms

### 3.4 Flicker Measurement
- [ ] Implement flicker detection algorithm
- [ ] Calculate Pst (short-term) flicker
- [ ] Calculate Plt (long-term) flicker
- [ ] Add visualization on display
- [ ] Calibrate sensitivity

### 3.5 Power Factor Calculation
- [ ] Calculate real power (W)
- [ ] Calculate reactive power (VAR)
- [ ] Calculate apparent power (VA)
- [ ] Calculate power factor (cos φ)
- [ ] Format for display output

**Deliverable:** Complete quality metrics calculation - all analysis results available in memory

**Related Files:**
- New component: `components/analysis/`
- Header: `components/config/common_types.h` (quality_metrics_t)

---

## Phase 4: Connectivity (Week 4-5)

### 4.1 WiFi Integration
- [ ] Configure WiFi credentials (NVS storage)
- [ ] Implement WiFi connection routine
- [ ] Create WiFi state machine
- [ ] Add reconnection logic with exponential backoff
- [ ] Display WiFi status on OLED

### 4.2 MQTT Client
- [ ] Configure MQTT broker connection
- [ ] Implement publish routine
- [ ] Create topic hierarchy
- [ ] Add error handling and reconnect
- [ ] Test publishing to broker

### 4.3 Data Serialization
- [ ] Define JSON payload format
- [ ] Implement data-to-JSON conversion
- [ ] Create timestamp management
- [ ] Test message integrity

**Deliverable:** Data successfully published to MQTT broker every 5 seconds

**Related Files:**
- `components/mqtt/`
- WiFi configuration in `components/config/`

---

## Phase 5: Power Management & Optimization (Week 5-6)

### 5.1 Power Monitoring
- [ ] Measure battery voltage (if applicable)
- [ ] Implement low-power mode detection
- [ ] Add warning indicators

### 5.2 Task Optimization
- [ ] Measure CPU load per task
- [ ] Optimize sampling rates
- [ ] Implement sleep modes
- [ ] Reduce WiFi duty cycle

### 5.3 Memory Management
- [ ] Profile memory usage
- [ ] Optimize buffer sizes
- [ ] Test for memory leaks
- [ ] Implement memory statistics

**Deliverable:** System running efficiently with optimized power consumption

---

## Phase 6: Testing & Validation (Week 6-7)

### 6.1 Unit Tests
- [ ] Write tests for RMS calculation
- [ ] Write tests for harmonic analysis
- [ ] Write tests for SAG/SWELL detection
- [ ] Write tests for data serialization
- [ ] Test error handling paths

### 6.2 Integration Tests
- [ ] Test full data flow: Sensor → Analysis → MQTT
- [ ] Test menu navigation and display updates
- [ ] Test button responsiveness
- [ ] Test WiFi reconnection scenarios

### 6.3 Hardware Calibration & External Testing
- [ ] Calibrate voltage sensor against known values
- [ ] Calibrate current sensor against known values
- [ ] Test with real AC power supply
- [ ] Record calibration constants
- [ ] **External MQTT Testing:** Test telemetry publishing to external broker (test.mosquitto.org)
- [ ] Validate JSON payload format and data integrity
- [ ] Test MQTT reconnection scenarios with external broker

### 6.4 Load Testing
- [ ] Run extended duration test (24+ hours)
- [ ] Monitor for memory leaks
- [ ] Check for task starvation
- [ ] Verify data consistency

**Deliverable:** Full test suite passing, hardware calibrated, external MQTT connectivity validated

---

## Phase 7: Documentation & Release (Week 7-8)

### 7.1 Code Documentation
- [ ] Complete Doxygen comments on all public functions
- [ ] Generate Doxygen HTML
- [ ] Create internal documentation diagrams
- [ ] Document calibration procedure

### 7.2 User Documentation
- [ ] Create user manual
- [ ] Document installation procedure
- [ ] Create troubleshooting guide
- [ ] Document menu structure

### 7.3 Release Preparation
- [ ] Set version number (v1.0.0)
- [ ] Create CHANGELOG.md
- [ ] Tag git repository
- [ ] Build final firmware binary

### 7.4 Deployment
- [ ] Create deployment scripts
- [ ] Document flashing procedure
- [ ] Create quick-start guide
- [ ] Package release artifacts

**Deliverable:** v1.0.0 release ready for production

---

## Phase 7.5: Over-The-Air (OTA) Firmware Updates (Week 8-9)

### 7.5.1 OTA Infrastructure
- [ ] Update partition table for OTA slots (ota_0, ota_1)
- [ ] Configure OTA support in sdkconfig
- [ ] Set up firmware hosting server
- [ ] Implement version checking API endpoint

### 7.5.2 OTA Service Implementation
- [ ] Create OTA service component in `components/network/`
- [ ] Implement firmware download with progress callback
- [ ] Add hash validation (SHA-256) during download
- [ ] Implement digital signature verification (RSA-2048)
- [ ] Add automatic rollback on corruption

### 7.5.3 Integration & UI
- [ ] Create OTA periodic check task (every 24 hours)
- [ ] Add "Check for Update" menu option
- [ ] Display current firmware version
- [ ] Show update progress on OLED display
- [ ] Implement update logs in NVS

### 7.5.4 Security & Testing
- [ ] Configure HTTPS with certificate validation
- [ ] Implement certificate pinning for known servers
- [ ] Create unit tests for signature validation
- [ ] Test rollback scenario (corrupted firmware)
- [ ] Test update with network interruption
- [ ] Verify zero data loss during update

**Deliverable:** Remote firmware update capability with automatic rollback and security validation

**Related Files:**
- `components/network/ota_service.h` (OTA public API)
- `components/config/ota_config.h` (OTA configuration)
- `OTA_IMPLEMENTATION_PLAN.md` (detailed implementation reference)
- Updated partition table in `sdkconfig`

---

## Implementation Checklist

### Code Quality
- [ ] No compiler warnings
- [ ] All MISRA C rules followed
- [ ] Consistent naming conventions
- [ ] All functions documented with Doxygen
- [ ] Code review completed

### Testing Coverage
- [ ] Unit test coverage > 80%
- [ ] Integration tests passing
- [ ] Hardware tests passing
- [ ] 24-hour stability test passing

### Documentation
- [ ] CODING_STANDARDS.md
- [ ] API documentation (Doxygen)
- [ ] User manual
- [ ] Troubleshooting guide
- [ ] Calibration certificate

### Release Artifacts
- [ ] Firmware binary (.bin)
- [ ] Bootloader binary
- [ ] Partition table
- [ ] Flashing instructions
- [ ] Version tag in git

---

## Risk Assessment & Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| ADC noise on analog signals | High | Medium | Use hardware filtering, software averaging |
| WiFi connectivity unstable | Medium | High | Implement state machine, exponential backoff |
| Harmonic analysis inaccuracy | Medium | Medium | Calibrate against known signals, use FFT library |
| Memory constraints | Low | High | Profile early, optimize buffer sizes |
| Real-time deadline miss | Low | High | Priority-based task scheduling, RTOS |

---

## Success Criteria

✅ **Functional Requirements:**
- Measures voltage and current with <2% error
- Detects harmonics up to 20th order
- Detects SAG/SWELL with 2-cycle latency
- Calculates flicker according to IEC 61000-4-15
- Publishes data to MQTT every 5 seconds
- WiFi auto-reconnect on dropout
- Display updates every 500ms

✅ **Non-Functional Requirements:**
- Uptime > 99% over 24-hour test
- Response time < 100ms for button presses
- Memory usage < 80% of available RAM
- CPU load < 70% average
- Zero compiler warnings
- Full MISRA C compliance

✅ **Documentation:**
- Complete API documentation
- User guide with screenshots
- Calibration procedure documented
- Code review sign-off
