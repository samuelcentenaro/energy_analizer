# 🔌 Hardware Connections & Block Diagram

**Energy Analyzer - ESP32 Based Power Quality Monitor**

---

## 📋 Table of Contents

1. [Block Diagram](#block-diagram)
2. [Pin Allocation](#pin-allocation)
3. [Signal Flow](#signal-flow)
4. [Sensor Connections](#sensor-connections)
5. [Display Interface (I2C)](#display-interface-i2c)
6. [Button Interface](#button-interface)
7. [WiFi & MQTT](#wifi--mqtt)
8. [Power Budget](#power-budget)
9. [Testing & Calibration Setup](#testing--calibration-setup)

---

## Block Diagram

```
                            ┌──────────────────┐
                            │   AC MAINS 220V  │
                            │   60 Hz, ~30A    │
                            └────────┬─────────┘
                                     │
                          ┌──────────┴──────────┐
                          │                     │
                    ┌─────▼─────┐         ┌────▼────┐
                    │  Voltage  │         │ Current │
                    │  Sensor   │         │ Sensor  │
                    │ ZMPT101B  │         │ SCT013  │
                    └─────┬─────┘         └────┬────┘
                          │ 0-3.3V DC          │ 0-3.3V DC
                          │                    │
                    ┌─────▼────────────────────▼────┐
                    │     ADS1015 ADC               │
                    │  (12-bit, I2C, 3300 SPS)      │
                    │  Channels: A0=A1 (Voltage)    │
                    │           A2=A3 (Current)     │
                    └──────┬──────────────────┬──────┘
                           │                  │
                           │ I2C Data         │
                           ▼                  ▼
                    ┌──────────────────────────────┐
                    │   ESP32 I2C Master           │
                    │  GPIO21 (SDA), GPIO22 (SCL)  │
                    │  Address: 0x48 (default)     │
                    └──────┬──────────────────┬────┘
                    │  ├─ RMS Calculation          │
                    │  ├─ FFT Analysis             │
                    │  ├─ Harmonic Extraction      │
                    │  ├─ Quality Metrics          │
                    │  └─ State Detection (SAG/...) │
                    └──────┬──────────────────┬────┘
                           │                  │
                    ┌──────▼──┐        ┌─────▼──────┐
                    │  OLED   │        │ WiFi/MQTT  │
                    │ Display │        │ Publishing │
                    │  I2C    │        │  5s rate   │
                    │ 200ms   │        │            │
                    └────┬────┘        └─────┬──────┘
                         │                   │
                    ┌────▼───┐          ┌────▼───────┐
                    │ 3 Btns  │          │   Server   │
                    │ GPIO    │          │   Metrics  │
                    │ Input   │          │  Analysis  │
                    └─────────┘          └────────────┘
```

---

## Pin Allocation

### **Quick Reference Table**

| Pin | GPIO# | Type | Purpose | Notes |
|-----|-------|------|---------|-------|
| **ADC Inputs (ADS1015)** | | | | |
| GPIO21 | 21 | I2C SDA | ADS1015 Data | Shared with OLED, 4.7kΩ pull-up |
| GPIO22 | 22 | I2C SCL | ADS1015 Clock | Shared with OLED, 4.7kΩ pull-up |
| **Sensor Analog** | | | | |
| A0-A1 | - | Analog IN | Voltage sensor | ZMPT101B output (differential) |
| A2-A3 | - | Analog IN | Current sensor | SCT013 output (differential) |
| **Button Inputs** | | | | |
| GPIO35 | 35 | Digital IN | Button UP | Active low, pull-up |
| GPIO34 | 34 | Digital IN | Button DOWN | Active low, pull-up |
| GPIO32 | 32 | Digital IN | Button SELECT | Active low, pull-up |
| **Status Output** | | | | |
| GPIO2 | 2 | Digital OUT | Status LED | Built-in blue LED |
| **Debug UART** | | | | |
| GPIO1 | 1 | UART OUT | TX (debug) | idf.py monitor |
| GPIO3 | 3 | UART IN | RX (debug) | Future param upload |
| **Power** | | | | |
| VCC | - | Power | +3.3V | Regulated |
| GND | - | Ground | 0V Reference | Common to all |

---

## Sensor Connections

### **ZMPT101B - AC Voltage Sensor**

```
AC Mains (220V, 60Hz)
│
└──[Potential Transformer/Divider]──► 0-3.3V DC signal
                                       │
                                       └──► ADS1015 A0-A1 (Differential)
```

**Specifications:**
- Input: 0-250V AC RMS
- Output: 0-3.3V DC (proportional)
- Accuracy: ~2% (after calibration)
- Requires voltage divider or built-in scaling

**ADS1015 Configuration:**
- Channel: Differential A0-A1
- PGA Gain: 2 (for 0-2.048V range)
- Data Rate: 1600 SPS
- Resolution: 12-bit (0-2047 counts)

---

### **SCT013-030 - AC Current Sensor**

```
AC Circuit (0-30A)
│
└──[Current Transformer]──► 0-1V AC signal (with burden resistor)
                            │
                            └──► ADS1015 A2-A3 (Differential)
```

**Specifications:**
- Input: 0-30A AC RMS
- Output: 0-1V AC (with 18Ω burden + capacitor)
- Accuracy: ~1.5% (after calibration)
- Requires burden resistor and bias network

**ADS1015 Configuration:**
- Channel: Differential A2-A3
- PGA Gain: 1 (for 0-4.096V range)
- Data Rate: 1600 SPS
- Resolution: 12-bit (0-2047 counts)

---

## ADC Interface (ADS1015)

### **ADS1015 12-bit ADC Connection**

```
ESP32                          ADS1015 ADC
──────                         ────────────
GPIO21 (SDA) ──[4.7kΩ↑]─────► SDA
GPIO22 (SCL) ──[4.7kΩ↑]─────► SCL
GND ──────────────────────────► GND
3.3V ──────────────────────────► VCC

ZMPT101B ─────────────────────► A0 (+)
GND ──────────────────────────► A1 (-)
SCT013 ───────────────────────► A2 (+)
GND ──────────────────────────► A3 (-)

Address: 0x48 (default, A0-A1-A2 to GND)
Clock: 400 kHz I2C
Data Rate: 1600 SPS per channel
Resolution: 12-bit
```

**Channel Configuration:**
- CH0 (A0-A1): Voltage sensor (differential)
- CH1 (A2-A3): Current sensor (differential)
- PGA: Configurable gain (1, 2, 4, 8, 16)
- Mode: Continuous conversion
- Alert: Not used (optional comparator)

---

## Display Interface (I2C)

### **SSD1306 OLED Display Connection**

```
ESP32                          SSD1306 OLED
──────                         ────────────
GPIO21 (SDA) ──[4.7kΩ↑]─────► SDA
GPIO22 (SCL) ──[4.7kΩ↑]─────► SCL
GND ──────────────────────────► GND
3.3V ──────────────────────────► VCC

Address: 0x3C (default)
Clock: 400 kHz
Data: 128×64 pixels
```

**Timing:**
- Update rate: 200 ms (5 Hz)
- Transaction time: ~25 ms per full screen
- Refresh: Continuous (display buffer + I2C DMA)

---

## Button Interface

### **Button Input Configuration**

```
Button UP              Button DOWN           Button SELECT
(GPIO35)              (GPIO34)              (GPIO32)
  │                     │                     │
  │ Active Low          │ Active Low          │ Active Low
  │ (Press → GND)       │ (Press → GND)       │ (Press → GND)
  │                     │                     │
  ├─[Pull-up 10kΩ]      ├─[Pull-up 10kΩ]      ├─[Pull-up 10kΩ]
  │ or Internal         │ or Internal         │ or Internal
  │                     │                     │
  ├─►ESP32 GPIO Input   ├─►ESP32 GPIO Input   ├─►ESP32 GPIO Input
  │                     │                     │
  └─ GND                └─ GND                └─ GND
```

**Debouncing Strategy:**
```
Hardware:    20 ms capacitive filter
Software:    20 ms debounce timer
Total:       40 ms debounce time

Detection:
- Edge trigger (falling = press)
- GPIO ISR with task notification
- Response time: < 100 ms guaranteed
```

**Long Press Detection:**
```
If button held for > 1000 ms:
  → Force restart / menu reset
  → Calibration unlock (optional)
```

---

## Status LED

### **GPIO2 Status Indicator**

```
ESP32 GPIO2 ──[220Ω resistor]──► LED +
                                    │
                               Blue LED
                                    │
                                   GND
```

**Status Patterns:**
| Pattern | Meaning |
|---------|---------|
| LED ON (solid) | WiFi connected, measuring OK |
| LED BLINK 1Hz | WiFi connecting / buffering data |
| LED BLINK 5Hz | Error state / SAG/SWELL active |
| LED OFF | No power / Critical error |

---

## WiFi & MQTT

### **Connectivity Stack**

```
┌─────────────────────────────────────┐
│  MQTT Message Publishing            │
│  (JSON payload, ~200 bytes)          │
│  Every 5 seconds                     │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│  WiFi TCP/IP Stack                  │
│  Mode: Station (STA)                 │
│  Freq: 2.4 GHz                      │
│  Auto-reconnect enabled              │
└────────────┬────────────────────────┘
             │
    ┌────────▼────────┐
    │  MQTT Broker    │
    │  mosquitto.org  │
    │  Port: 1883     │
    │  QoS: 0         │
    │  Keep-Alive: 60s│
    └─────────────────┘
```

### **MQTT Topic Structure**

```
energy/voltage/rms        → 220.45 (V)
energy/current/rms        → 12.30 (A)
energy/frequency          → 60.10 (Hz)
energy/power/real         → 2706.0 (W)
energy/power/reactive     → 450.0 (VAR)
energy/power/apparent     → 2743.0 (VA)
energy/power/factor       → 0.987
energy/thd                → 2.3 (%)
energy/harmonics/odd/#    → [1,3,5,7,9,11,13,15,17,19]%
energy/events/sag         → {active: 0, depth: 0.95}
energy/events/swell       → {active: 0, depth: 1.12}
energy/events/flicker     → {pst: 1.2, plt: 0.8}
energy/status             → "OK" / "SAG" / "SWELL" / "ERROR"
```

---

## Power Budget

### **Current Consumption**

| Component | Current | Voltage | Notes |
|-----------|---------|---------|-------|
| ESP32 (idle) | 10 mA | 3.3V | Sleep mode available |
| ESP32 (WiFi) | 150 mA | 3.3V | Normal operation |
| ESP32 (TX peak) | 200 mA | 3.3V | WiFi transmission peak |
| ZMPT101B | 5 mA | 3.3V | Always on |
| SCT013-030 | 2 mA | 3.3V | Always on |
| SSD1306 OLED | 15 mA | 3.3V | 50% brightness typical |
| Buttons pullups | <1 mA | 3.3V | Negligible |
| Status LED | 5 mA | 3.3V | With 220Ω resistor |
| **Total (WiFi)** | **±190 mA** | 3.3V | Average with WiFi idle |
| **Total (TX Peak)** | **±230 mA** | 3.3V | WiFi transmission |

### **Power Supply Recommendation**

```
Minimum: 500 mA @ 5V → 3.3V regulator
Recommended: 1000 mA @ 5V → Margins for future expansion
Source: USB or Wall adapter (5V@1A minimum)
```

---

## Testing & Calibration Setup

### **Calibration Equipment Needed**

```
┌──────────────────────────────────────────────────────────┐
│ VOLTAGE CALIBRATION                                      │
├──────────────────────────────────────────────────────────┤
│ Instrument: True RMS Multimeter                          │
│            (±0.5% accuracy or better)                    │
│                                                           │
│ Procedure:                                               │
│  1. Set multimeter to AC voltage mode                   │
│  2. Measure 3 different voltage levels:                 │
│     - Low:    ~100V AC                                  │
│     - Medium: ~150V AC                                  │
│     - High:   ~220V AC (nominal)                        │
│  3. LCD displays multimeter reference                   │
│  4. Press "Calibrate" in OLED menu                      │
│  5. System reads ADC simultaneously                     │
│  6. Calculates offset + scale factors                  │
│  7. Stores in NVS flash                                │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│ CURRENT CALIBRATION                                      │
├──────────────────────────────────────────────────────────┤
│ Instrument: AC Current Clamp Meter                       │
│            (±1% accuracy or better)                      │
│                                                           │
│ Procedure:                                               │
│  1. Set clamp meter to AC current mode (A)              │
│  2. Insert ESP32 measured circuit in clamp              │
│  3. Load 3 different current values:                     │
│     - Low:    ~5A AC                                    │
│     - Medium: ~15A AC                                   │
│     - High:   ~30A AC                                   │
│  4. LCD displays clamp reference value                  │
│  5. Press "Calibrate" in OLED menu                      │
│  6. System reads ADC simultaneously                     │
│  7. Calculates offset + scale factors                  │
│  8. Stores in NVS flash                                │
└──────────────────────────────────────────────────────────┘
```

### **Expected Accuracy After Calibration**

- Voltage: ±2% (of reading)
- Current: ±1.5% (of reading)
- Frequency: ±0.1 Hz
- THD: ±0.5% (of fundamental)

---

## Schematic Reference

### **Simplified Schematic (ASCII)**

```
                          ESP32 Development Board
                          ─────────────────────
                ┌────────────────────────────────┐
                │   USB              3.3V  GND   │
                │   CH340            ↑     ↓    │
                │   ╱╲               │     │    │
                └──┼┼────────────────┼──┬──┴────┘
                   ╲╱                │  │
                    │                │  │
          Regulated 5V              │  │
          ← → ┌────────┐            │  │
              │ 3.3V   │            │  │
              │ Reg    │───────────┬┴──┘
              └────┬───┘           │
                   │   ┌───────────┴───┐
              ┌────┴───►SDA (GPIO21)    │
              │   ┌────►SCL (GPIO22)    │
    ┌─────┐   │   │    GND ────────────┤
    │ ADC │───┘   │                    │
  ┌─┤GPIO35├┐    │   ┌──────────────┐  │
  │ │(Volt)│──┐  │   │  SSD1306     │  │
  │ └──────┘ │  └───►OLED 128x64    │  │
  │          │      │  @ 0x3C       │  │
  │   ┌─────►GPIO34 │              │  │
  │   │ ┌───┤(Curr)├┘             │  │
  │   │ │ └──────┘               │  │
  │   │ │   ┌─────►GPIO32 (BTN3) ────┘
  │   │ └──►GPIO35 (BTN1) ──┐
  │   └────►GPIO34 (BTN2) ──┤
  │                          │
  └──────────────────────────┘

Legend:
ADC = Analog Inputs (Voltage + Current sensors)
BTN = Digital Inputs (3 buttons)
LED = Digital Output (Status LED on GPIO2)
UART = Serial Debug (TX/RX on GPIO1/3)
```

---

## Development Notes

### **Critical Connections to Double-Check**

- ✅ **I2C Pull-ups:** 4.7kΩ to 3.3V (not optional!)
- ✅ **Voltage Divider:** Proper resistor values for 0-3.3V conversion
- ✅ **Current Burden:** 18Ω resistor + capacitor (don't skip!)
- ✅ **Common GND:** All sensors and display share same GND
- ✅ **Button Pullups:** Internal or external (no floating inputs)
- ✅ **3.3V Supply:** Must be stable (bypass capacitor near ESP32)

### **Common Mistakes to Avoid**

❌ Connecting sensor output > 3.3V directly to GPIO  
❌ Missing pull-up resistors on I2C  
❌ Not using burden resistor on current sensor  
❌ Floating button inputs (unreliable)  
❌ Mixing 5V and 3.3V logic levels  
❌ No decoupling capacitors on power pins  

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2026-04-06 | 1.0 | Initial hardware diagram for Energy Analyzer |
| - | - | - |

---

**Document:** Hardware Connections & Block Diagram  
**Project:** Energy Analyzer ESP32  
**Status:** APPROVED FOR DEVELOPMENT  
**Last Updated:** April 6, 2026
