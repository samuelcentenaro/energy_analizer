# WiFi Provisioning Guide - Energy Analyzer ESP32

## Quick Reference

| Action | Steps |
|--------|-------|
| **First Setup** | Find `EnergyAnalyzer-Setup` → Connect → Visit `192.168.4.1` → Enter SSID/password → Save |
| **Change WiFi** | Reconnect to `EnergyAnalyzer-Setup` → Visit `192.168.4.1` → Update credentials → Save |
| **Reset Device** | Power cycle ESP32 after saving credentials (auto-connects next boot) |
| **Emergency Reset** | Run `esptool.py -p COM3 erase_flash` (clears all settings) |

---

## Getting Started (First Time)

### Prerequisites
- ✅ ESP32 flashed with latest firmware
- ✅ USB power cable connected
- ✅ WiFi credentials on hand (SSID + password)
- ✅ Smartphone, tablet, or laptop nearby

### Procedure

**1. Boot the Device**
```
Plug in USB power → LED blinks → Serial Monitor shows boot sequence
```

**2. Find Provisioning AP**
```
WiFi Settings → Look for: EnergyAnalyzer-Setup
Status: No WiFi icon needed - it's a direct connection point
```

**3. Connect**
```
Select EnergyAnalyzer-Setup → Connect (no password)
Wait 2-3 seconds for DHCP assignment
```

**4. Open Web Interface**
| Browser | Action |
|---------|--------|
| Chrome/Firefox/Safari | Visit `http://192.168.4.1` |
| Mobile Browser | Search bar or address bar → `http://192.168.4.1` |

**5. Enter Credentials**
```
SSID: [your_network_name]        (1-31 characters, case-sensitive)
Password: [your_password]          (0-63 characters)
```

**6. Save**
```
Click: "Save and Connect"
Wait: 3-5 seconds for connection
Refresh page → should show your IP now (e.g., 192.168.1.200)
```

✅ **Success indicators:**
- Page refreshes with IP address (not 0.0.0.0)
- State shows "Connected"
- You can now disconnect from `EnergyAnalyzer-Setup`

---

## Troubleshooting Scenarios

### "I can't see EnergyAnalyzer-Setup network"

**Why:** Device might not be booting or provisioning AP not started

**Fixes:**
1. Check USB power connection (green LED should flash)
2. Wait 10 seconds after powering on (boot takes ~3 seconds)
3. Disable "Auto-connect to open networks" in phone settings
4. Manually power off ESP32 and try again
5. Check Serial Monitor for errors (look for `[WIFI_SVC]` messages)

---

### "I can connect but 192.168.4.1 won't load"

**Why:** DHCP didn't assign IP or DNS not resolving

**Fixes:**
1. Check phone shows WiFi connected with signal bars
2. Try opening `http://192.168.4.1:80` explicitly
3. Forget network and reconnect:
   - Settings → WiFi → EnergyAnalyzer-Setup → Forget
   - Scan and reconnect
4. Try accessing from a different device (phone → laptop)
5. Restart ESP32 and wait full boot before accessing

---

### "Credentials won't save"

**Why:** Form validation failed or device error

**Common causes:**
- ✗ SSID field left empty (required)
- ✗ Password longer than 63 characters
- ✗ Invalid characters in SSID
- ✗ Device rebooting

**Fixes:**
1. Verify SSID is NOT empty
2. Count password characters (max 63)
3. Try simpler password without special chars
4. Open Serial Monitor and watch for error messages
5. If still failing: Perform full flash erase
   ```bash
   esptool.py -p COM3 erase_flash
   idf.py -p COM3 flash
   ```

---

### "Device connects but disconnects after few seconds"

**Why:** WiFi credentials saved but router rejecting connection

**Fixes (in order):**
1. Reconnect to `EnergyAnalyzer-Setup`
2. Check current saved SSID (web form shows it)
3. Verify SSID and password are correct (typos common!)
4. Clear and re-enter credentials
5. Check router:
   - Is 802.11n enabled? (should be)
   - Try moving ESP32 closer to router
   - Check if router has MAC filtering enabled
   - Restart your WiFi router

---

### "After unplugging, device won't connect on boot"

**Why:** This is normal - provisioning AP may activate if connection fails

**Expected behavior:**
1. Boot → Tries stored credentials (~10 seconds)
2. If fails → Provisioning AP activates
3. If succeeds → Connects silently (no AP visible)

**Check connection status:**
- Serial Monitor: Look for `[WIFI_SVC] Connected to Wi-Fi`
- Or reconnect to `EnergyAnalyzer-Setup` to verify settings

---

## Advanced Usage

### Manual HTTP Request (Curl/Postman)

If web form doesn't work, try direct POST:

```bash
curl -X POST http://192.168.4.1/save \
  -d "ssid=MyNetwork&password=MyPassword"
```

**Response on success:**
```html
<html><body><h1>Saved</h1>
<p>Credentials stored. The device is connecting now.</p>
```

**Response on failure:**
```
500 Internal Server Error: Form too large
500 Internal Server Error: SSID missing
500 Internal Server Error: Failed to save credentials
```

---

### Reading Stored Credentials (Serial Monitor)

Boot the device and watch Serial Monitor output:

```
[WIFI_SVC] Initializing Wi-Fi service...
[WIFI_SVC] Credentials loaded from NVS
[WIFI_SVC] Provisioning AP started: EnergyAnalyzer-Setup
[WIFI_SVC] Wi-Fi service ready
```

If it shows `Credentials loaded`, your settings are saved!

---

### Complete NVS Reset

**When to use:** Emergency reset, all credentials lost, want fresh start

```bash
# Option A: Erase everything
esptool.py -p COM3 erase_flash

# Option B: Erase only NVS partition
idf.py -p COM3 erase-otadata

# Then reflash
idf.py -p COM3 flash
```

**Warning:** This is **permanent** - all saved data deleted

---

## Security Best Practices

### WiFi Credentials
- 🔒 Stored encrypted in NVS flash
- 🔐 Device supports WPA2/PMF
- ⚠️ Credentials transmitted over HTTP (provisioning AP only)
- ✅ Change credentials if roaming/public WiFi

### Provisioning AP
- 🌐 Broadcasts SSID: `EnergyAnalyzer-Setup`
- 🔓 No authentication required (for setup only)
- 🔄 Can be accessed by anyone nearby during provisioning
- 💡 **Recommendation**: Provision in private location, then move device

### WiFi Security Types Supported
| Type | Supported | Notes |
|------|-----------|-------|
| Open (no password) | ✅ Yes | Works but not recommended |
| WEP | ✅ Yes | Legacy - avoid if possible |
| WPA | ✅ Yes | Older standard |
| WPA2 | ✅ Yes | Recommended |
| WPA3 | ✅ Yes | Latest standard (if router supports) |

---

## Network Architecture

### During Provisioning
```
Internet (your WiFi router)
         ↓
    [Your Device]
    WiFi: Connected to router
    HTTP Provisioning Server running on 192.168.4.1
         ↓
   [Phone/Laptop]
   Connected to EnergyAnalyzer-Setup AP
```

### After Provisioning (Normal Operation)
```
Internet (your WiFi router)
         ↓
    [Your Device]
    WiFi: Connected as Station (STA)
    Also broadcasts AP for remote provisioning/status
         ↓
   [Optional: Can still connect to AP for management]
```

---

## FAQ

**Q: Can I access the device remotely (outside my WiFi)?**
A: Not in current firmware (Phase 1-4). Planned for Phase 6 (MQTT).

**Q: What if SSID has spaces or special characters?**
A: Should work! Form URL-encodes automatically. Avoid quotes in field.

**Q: Can I set static IP instead of DHCP?**
A: Not in current version. Device always uses DHCP. Plan for Phase 5.

**Q: Does provisioning AP stay active after connection?**
A: Yes! Device runs in AP+STA mode simultaneously (Soft AP always available).

**Q: How often should I reprovisioning?**
A: Only when changing WiFi networks. Credentials persist across reboots.

**Q: What's the default provisioning AP password?**
A: No password - it's open for setup convenience.

**Q: Can I rename the AP from "EnergyAnalyzer-Setup"?**
A: Not without code changes. Defined in `components/network/wifi_service.c` line 24.

**Q: Is there a timeout for provisioning?**
A: No explicit timeout. AP stays active indefinitely (by design).

---

## Support

**For issues, check:**
1. This guide → Troubleshooting section
2. `DEPLOYMENT_GUIDE.md` → WiFi Configuration section
3. Serial Monitor output during provisioning
4. Device boot logs for error messages

**For firmware bugs:**
- Check `BUILD_COMPLETION_REPORT.md` for known issues
- Review `PROJECT_CURRENT_STATE.md` for implementation status
