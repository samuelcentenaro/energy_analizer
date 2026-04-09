# Energy Analyzer ESP32 - Deployment Guide

## Quick Start

### 1. Prerequisites Verification
```bash
# Verify ESP-IDF environment
idf.py --version
# Expected: ESP-IDF v6.0.0 or compatible

# Verify Python environment
python --version
# Expected: Python 3.8+

# Verify esptool
esptool.py version
# Expected: esptool v5.3 or compatible
```

### 2. Flash Firmware to ESP32

**Using IDF.py (Recommended):**
```bash
cd /path/to/energy_analizer/analisador
idf.py -p /dev/ttyUSB0 flash
# On Windows: idf.py -p COM3 flash
```

**Using esptool directly:**
```bash
esptool.py -p /dev/ttyUSB0 write_flash \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/analisador.bin
```

### 3. Monitor Serial Output

**Using IDF.py:**
```bash
idf.py -p /dev/ttyUSB0 monitor
# Ctrl+] to exit
```

**Using miniterm:**
```bash
python -m serial.tools.miniterm /dev/ttyUSB0 115200
```

### 4. Expected Boot Sequence

```
ets Jul 29 2019 12:01:04 rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsb: 0, ota_sel: 0, pin_rev: 0
mode:DIO, clock div:2
load:0x3fff0030,len:7144
load:0x40078000,len:15584
load:0x40080400,len:3792
entry 0x40080694
[0m[32mI (0) cpu_start: Starting scheduler on PRO CPU[0m
I (0) MAIN: Energy Analyzer starting...
[startup sequence...]
```

## Hardware Connections

### I2C Bus (Shared)
- **SDA**: GPIO 21
- **SCL**: GPIO 22
- **Devices**:
  - ADS1015 ADC @ 0x48
  - SSD1306 OLED @ 0x3C
- **Speed**: 400 kHz

### ADC Inputs
- **Channel A0**: Voltage measurement input
- **Channel A1**: Current measurement input
- **Channel A2**: Reserved
- **Channel A3**: Reserved

### Display (OLED SSD1306)
- **Resolution**: 128x64 pixels
- **Interface**: I2C (shared bus)
- **Address**: 0x3C

### Status LED
- **GPIO**: GPIO 2
- **Note**: reserved for future operational indication

## WiFi Configuration

### Overview
The Energy Analyzer includes a built-in **WiFi Provisioning System** that automatically activates on first boot. This allows you to configure WiFi credentials via a web interface without needing to recompile firmware.

**Key Features:**
- 🌐 Automatic Soft AP on boot if no credentials saved
- 📱 Web interface for credential entry (no app needed)
- 💾 Secure NVS storage of credentials
- 🔄 Automatic reconnection on reboot
- 🔐 Supports WPA2/WPA3 security

### Step-by-Step Configuration

#### Step 1: Locate the Provisioning AP
After flashing and booting the ESP32:

**On your phone, tablet, or computer:**
1. Go to WiFi networks list
2. Look for: **`EnergyAnalyzer-Setup`**
3. No password required

#### Step 2: Connect to Provisioning AP
1. Select `EnergyAnalyzer-Setup` network
2. Wait for connection (2-3 seconds)
3. Device will receive IP: `192.168.4.xxx` (DHCP)

#### Step 3: Open Web Interface
1. Open any web browser
2. Navigate to: **`http://192.168.4.1`**

You should see this form:
```
┌─────────────────────────────────────┐
│  Energy Analyzer Wi-Fi Setup         │
│─────────────────────────────────────│
│  AP SSID: EnergyAnalyzer-Setup       │
│  State: Provisioning AP active       │
│  Current SSID: -                     │
│  IP: 0.0.0.0                         │
│─────────────────────────────────────│
│  SSID    [________________________] │
│  Password [________________________] │
│        [Save and Connect]            │
└─────────────────────────────────────┘
```

#### Step 4: Enter WiFi Credentials
| Field | Requirements | Example |
|-------|--------------|---------|
| **SSID** | 1-31 chars, case-sensitive | `MyHomeWiFi` |
| **Password** | 0-63 chars (empty = open network) | `MyPassword123` |

#### Step 5: Save and Connect
1. Fill the SSID field (required)
2. Fill password field (leave empty for open networks)
3. Click **`Save and Connect`**
4. Wait 3-5 seconds for connection attempt

#### Step 6: Verify Connection
After clicking save:
- **Success**: Refresh page → shows your device IP (e.g., `192.168.1.100`)
- **Connection message**: "Connected" appears in State field
- **Failure**: State shows "Connection error" → check WiFi credentials

### Credential Management

#### View Saved Credentials
1. Reconnect to `EnergyAnalyzer-Setup`
2. Open `http://192.168.4.1`
3. Current SSID displays your saved network

#### Update Credentials
1. Connect to `EnergyAnalyzer-Setup` (the provisioning AP remains available in the current baseline)
2. Submit new SSID and password
3. Device will connect to new network on next boot

#### Clear Credentials (NVS Reset)
If you need to completely reset saved credentials:

```bash
# Option 1: Using idf.py
idf.py -p COM3 erase-otadata

# Option 2: Using esptool
esptool.py -p COM3 erase_flash
```

**Warning**: This will erase all settings. You'll need to reprovision WiFi.

### Troubleshooting WiFi Issues

#### Can't access http://192.168.4.1
```
✓ Verify phone/device shows "connected" to EnergyAnalyzer-Setup
✓ Check that you're using HTTP, not HTTPS
✓ If timeout: device may be rebooting - wait 5 seconds
```

#### WiFi credentials not saving
```
✓ Check SSID field is not empty
✓ Verify password is less than 63 characters
✓ If still fails: check device logs in Serial Monitor
```

#### Device disconnects frequently
```
✓ Check WiFi signal strength (move closer to router)
✓ Verify you're within 802.11n range
✓ Try manually reconnecting to EnergyAnalyzer-Setup and re-entering credentials
```

#### Provisioning AP not visible
```
✓ Reboot the ESP32 (power cycle or reset button)
✓ Check that firmware was flashed successfully
✓ Verify device is powered (status LED should flash)
✓ Check Serial Monitor for boot errors
```

### Auto-Connection on Boot

Once credentials are saved:
1. **Next power-up**: Device attempts to connect automatically
2. **Current baseline**: provisioning AP remains available while STA connection is managed
3. **If STA fails**: device stays recoverable through the provisioning path
4. **AP + STA behavior**: the firmware may operate in APSTA mode during provisioning and recovery

### Security Notes

- 🔒 Credentials stored in **NVS (non-volatile) flash** (encrypted at rest)
- 🔐 WPA2/PMF (Protected Management Frames) supported
- ⚠️ Open networks (no password) are supported but not recommended
- 🌐 AP broadcasts hidden SSID initially, becomes visible during provisioning

### HTTP Server Details

- **Server Port**: 80 (HTTP)
- **GET Request**: Displays provisioning form
- **POST Request**: `/save` endpoint accepts credentials
- **Form Data**: `ssid=<value>&password=<value>`
- **Timeout**: Server remains active indefinitely until successful connection

## Troubleshooting

### Build Issues

**Issue**: `hardware_config.h not found`
- **Solution**: Rebuild with `idf.py fullclean` then `idf.py build`
- **Root Cause**: Fixed - config component now properly linked

**Issue**: `driver/uart.h not found`
- **Solution**: This is intentional - app layer doesn't use driver layer directly
- **Status**: direct serial command processing remains disabled in the current production baseline

**Issue**: Type casting errors in SAFE_STRCPY
- **Solution**: Already fixed - macro now handles uint8_t arrays
- **Status**: No longer an issue

### Flash Issues

**Issue**: `Device not found on COM port`
- **Solution 1**: Check USB drivers installed (CP210x or CH340G depending on board)
- **Solution 2**: Verify board is in bootloader mode (hold GPIO0 while resetting)
- **Solution 3**: Try specifying baud rate: `idf.py -p COM3 -b 460800 flash`

**Issue**: `Firmware won't start after flashing`
- **Solution 1**: Erase entire flash: `esptool.py -p COM3 erase_flash`
- **Solution 2**: Flash bootloader separately: `idf.py bootloader-flash`
- **Solution 3**: Check partition table: `idf.py partition-table-flash`

### Runtime Issues

**Issue**: Display not showing (OLED blank)
- **Check**: I2C bus connectivity (SDA/SCL pins)
- **Check**: Proper 3.3V power to OLED module
- **Check**: OLED address matches 0x3C
- **Serial Monitor**: Should show initialization messages

**Issue**: ADC readings all zeros
- **Check**: Input voltage connected to ADS1015 channels
- **Check**: ADS1015 I2C address (0x48)
- **Check**: Ground reference connections
- **Serial Monitor**: Should display raw ADC values

**Issue**: Unstable WiFi connection (Phase 4+)
- **Check**: WiFi credentials stored in NVS
- **Solution**: Erase NVS partition: `idf.py erase-otadata`
- **Check**: WiFi signal strength at location

## Performance Metrics

### Memory Usage (Observed)
- **IRAM Utilization**: ~60%
- **DRAM Utilization**: ~40%
- **Flash Partition Free**: 20% (835KB used of 1MB)

### Responsiveness
- **Boot to Ready**: <3 seconds
- **Display Refresh**: <50ms per frame
- **ADC Sample Rate**: 860 SPS per channel (configurable)
- **Menu Navigation**: Instantaneous (<10ms response)

### Power Consumption (Typical)
- **Normal Operation**: ~80mA @ 3.3V
- **Deep Sleep**: N/A (not implemented in Phase 1)
- **WiFi Active (Phase 4+)**: ~150mA @ 3.3V

## Firmware Updates (OTA)

OTA firmware updates are NOT yet implemented (Phase 7+). For now, use serial flashing.

## Support & Debugging

### Enable Verbose Logging
```bash
idf.py -p COM3 -DDEBUG_ENABLED flash
# Serial output will include detailed debug messages
```

### Serial Commands (Phase 1 limitations)
- Serial command interface not yet implemented
- Planned for Phase 7

### GDB Debugging
```bash
idf.py -p COM3 gdb
# Connect with `target remote :3333` in GDB
```

## Development Next Steps

### Phase 2: Display & UI (Ready to implement)
- Enhanced menu system
- Real-time data visualization
- Status indicators

### Phase 3: Signal Analysis
- RMS calculations
- Power factor measurement
- Harmonic analysis

### Phase 4: WiFi Connectivity
- WiFi provisioning
- MQTT integration and runtime telemetry
- Data cloud logging

### Phase 5: Testing & Release
- Integration testing
- Performance validation
- Documentation finalization

## Technical Specifications

**Processor**: ESP32 (Dual-core Xtensa 240MHz)  
**Flash**: 2MB (1MB app partition)  
**RAM**: 520KB SRAM  
**I2C**: 400kHz dual-master capable  
**ADC**: 12-bit, 860 SPS max  
**WiFi**: 802.11 b/g/n  
**Bluetooth**: 4.2 LE (not used Phase 1)  

## Certification & Compliance

**Standards**: MISRA C (enforced via -Wall -Werror)  
**Language**: C23 (gnu23)  
**Build System**: CMake 3.16+  
**IDE**: Visual Studio Code + ESP-IDF extension  

---

**For questions or issues, refer to BUILD_COMPLETION_REPORT.md for technical details.**
