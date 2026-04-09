# OTA (Over-The-Air) Firmware Update Implementation Plan

**Status:** ✅ Planned Addition (Phase 7.5)  
**Dependency:** Phase 4 (WiFi/MQTT infrastructure must be complete)  
**Priority:** Medium (Post-release feature)  
**Estimated Effort:** 1 week

---

## Overview

OTA update capability allows remote firmware updates via WiFi without requiring physical access to the device. This is critical for:
- Bug fixes in deployed units
- Feature additions without site visits
- Security patches
- Rollback capabilities

---

## Architecture Integration

### Network Layer Components
```
components/network/
├── include/
│   └── ota_service.h          ← New OTA service interface
├── ota_service.c              ← OTA implementation
└── CMakeLists.txt             ← Updated with esp_https_ota
```

### OTA Service Interface
```c
// ota_service.h
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VALIDATING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED
} ota_state_t;

typedef struct {
    const char *firmware_url;           // Update server URL
    const char *cert_pem;               // Server certificate (optional, for HTTPS)
    ota_state_t state;
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    int error_code;
} ota_context_t;

// Check for updates (periodic task)
esp_err_t ota_check_for_updates(ota_context_t *ctx);

// Perform update
esp_err_t ota_start_update(const char *firmware_url);

// Get current OTA status
ota_state_t ota_get_state(void);

// Cancel ongoing update
esp_err_t ota_cancel_update(void);
```

---

## Implementation Phases

### Phase 1: Server Configuration (Local Testing)
1. **Simple HTTP server** for firmware hosting
   - Serve .bin files from `build/` directory
   - Implement version checking endpoint
   - Return version and URL for latest firmware

2. **Configuration storage** (NVS)
   - Server URL: `nvs:ota/server_url`
   - Update check interval: `nvs:ota/check_interval_s` (default: 86400 = 24h)
   - Last successful version: `nvs:ota/last_version`

3. **Version management**
   - Store current version in firmware
   - Compare against server version
   - Only update if server version > local version

### Phase 2: Core OTA Task
1. **Periodic update check** (runs in network task)
   ```
   Every 24 hours (configurable):
   → Check server for new version
   → If available, download firmware
   → Validate signature/hash
   → Install and reboot
   ```

2. **ESP-IDF built-in support** (esp_https_ota)
   - Leverages existing OTA partition scheme
   - Automatic rollback on corruption
   - Built-in validation

3. **Progress callback**
   - Report download progress to app layer
   - Update display with "Updating: 45%..."
   - Allow cancellation

### Phase 3: Safety & Validation
1. **Signature verification**
   - Sign firmware with private key
   - Validate signature during download
   - Prevent unauthorized updates

2. **Hash validation**
   - SHA-256 hash of firmware
   - Compare downloaded vs expected
   - Abort if mismatch

3. **Rollback mechanism**
   - Keep previous firmware in backup partition
   - If new firmware crashes, auto-revert
   - Log rollback event

### Phase 4: User Control
1. **Manual update trigger**
   - Menu option: "Check Update" → downloads immediately
   - Shows version info before updating
   - Requires confirmation

2. **Status display**
   - Current firmware version on main menu
   - Update status on separate menu
   - Last update timestamp

3. **Update logs** (NVS or SPIFFS)
   - Success/failure timestamps
   - Downloaded versions
   - Rollback events

---

## Partition Table Update

Current partition table needs OTA support:

```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x100000,      ← Current firmware
ota_0,    app,  ota_0,   0x110000, 0x100000,     ← OTA slot 1
ota_1,    app,  ota_1,   0x210000, 0x100000,     ← OTA slot 2 (backup)
```

**Command:**
```bash
cd e:\dev\embarcados\energy_analizer\analisador
python idf.py partition-table
```

---

## Firmware Hosting Setup

### Simple Python HTTP Server (for testing)
```python
# firmware_server.py
from http.server import HTTPServer, SimpleHTTPRequestHandler
import json
import os

class FirmwareHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/api/version':
            response = {
                'version': '1.0.1',
                'url': 'http://192.168.1.100:8000/analisador.bin',
                'size': os.path.getsize('analisador.bin'),
                'hash': get_sha256('analisador.bin')
            }
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            super().do_GET()

if __name__ == '__main__':
    server = HTTPServer(('0.0.0.0', 8000), FirmwareHandler)
    print("Firmware server running on port 8000")
    server.serve_forever()
```

### Production Setup (AWS, Azure, etc.)
- Upload firmware to cloud storage
- Implement version API endpoint
- Use HTTPS for security
- Implement rate limiting

---

## Software Components

### 1. OTA Service (`components/network/ota_service.c`)
- Handles firmware download
- Validates during download
- Manages partitions
- Handles rollback

### 2. OTA Task (`components/rtos/ota_task.c`)
- Periodic version checking
- Coordinates with WiFi task
- Updates app layer on progress

### 3. UI Integration (`components/services/ui_service.c`)
- Display update status
- Menu for manual updates
- Show version info

### 4. Configuration (`components/config/ota_config.h`)
```c
#define OTA_CHECK_INTERVAL_S        (86400)  // 24 hours
#define OTA_DOWNLOAD_TIMEOUT_S      (300)    // 5 minutes
#define OTA_SERVER_URL              "http://192.168.1.100:8000"
#define OTA_ENABLE_HTTPS            1
#define OTA_ENABLE_SIGNATURE_CHECK  1
```

---

## Testing Strategy

### Unit Tests
- Version comparison logic
- Hash validation
- URL parsing

### Integration Tests
1. **Happy path:** Download and install valid firmware
2. **Corruption:** Simulate corrupted download → rollback
3. **Timeout:** Network timeout during download → retry
4. **Version check:** Current == Server (no update)
5. **Version check:** Current > Server (downgrade attempted → denied)

### Hardware Tests
1. Download firmware over WiFi
2. Reboot during download → auto-recovery
3. Power loss during verify → rollback
4. Multiple update cycles → stability

---

## API Reference

### Header: `components/network/include/ota_service.h`

```c
/**
 * @brief Initialize OTA service
 * @return ESP_OK on success
 */
esp_err_t ota_service_init(void);

/**
 * @brief Check for firmware updates
 * @param[out] has_update Set to true if update available
 * @return ESP_OK if check successful, error code otherwise
 */
esp_err_t ota_check_for_updates(bool *has_update);

/**
 * @brief Start firmware update
 * Blocks until complete or error
 * @param[in] firmware_url URL to firmware .bin file
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t ota_perform_update(const char *firmware_url);

/**
 * @brief Get current OTA state
 * @return ota_state_t current state
 */
ota_state_t ota_get_state(void);

/**
 * @brief Get downloaded bytes and total
 * @param[out] current Current bytes downloaded
 * @param[out] total Total bytes to download
 * @return ESP_OK on success
 */
esp_err_t ota_get_progress(uint32_t *current, uint32_t *total);

/**
 * @brief Cancel ongoing update
 * @return ESP_OK on success
 */
esp_err_t ota_cancel_update(void);
```

---

## Timeline

| Phase | Task | Duration | Depends On |
|-------|------|----------|-----------|
| 7.5.1 | Partition table update & testing | 2 days | Phase 6 |
| 7.5.2 | OTA service implementation | 3 days | Phase 6 |
| 7.5.3 | Firmware server setup | 2 days | 7.5.1 |
| 7.5.4 | UI integration (menu + display) | 2 days | 7.5.2 |
| 7.5.5 | Safety testing (rollback, corruption) | 3 days | 7.5.4 |
| 7.5.6 | Documentation & release prep | 2 days | 7.5.5 |

**Total: ~2 weeks after Phase 7 acceptance**

---

## Security Considerations

⚠️ **Critical:**
- [ ] Sign firmware with RSA-2048 key
- [ ] Verify signature before installation
- [ ] Use HTTPS (TLS 1.2+) for downloads
- [ ] Certificate pinning for known servers
- [ ] Rate limiting on update requests
- [ ] Audit log of all updates (NVS)

---

## Success Criteria

✅ Successfully download firmware via WiFi  
✅ Validate firmware integrity  
✅ Rollback if new firmware crashes  
✅ Manual update trigger via menu  
✅ Display shows current version  
✅ 24-hour automated update check (configurable)  
✅ Support for HTTPS with certificate validation  
✅ Zero data loss during update process  
✅ Update complete in <5 minutes (LAN) or <15 minutes (remote)  

---

## References

- [ESP-IDF OTA Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [Partition Table Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/guide/partition-tables.html)
- [esp_https_ota Component](https://github.com/espressif/esp-idf/tree/master/components/esp_https_ota)
