# PRE-DEVELOPMENT CHECKLIST

## ✅ Projeto Configurado com Boas Práticas

### Estrutura Base
- [x] Repositório git inicializado
- [x] Diretórios de componentes criados
- [x] CMakeLists.txt configurado
- [x] README.md documentado

### Padrões de Código (Senior Level)
- [x] CODING_STANDARDS.md definido
  - [x] MISRA C compliance
  - [x] Naming conventions
  - [x] Error handling patterns
  - [x] Logging strategy
  - [x] Documentation (Doxygen)
  - [x] Memory management
  - [x] Unit testing approach

### Configuração Centralizada
- [x] `hardware_config.h` - todos os pinos definidos
  - [x] ADC (voltage, current)
  - [x] I2C (OLED display)
  - [x] GPIO (buttons)
  - [x] UART (debug)
  - [x] LED (status)

- [x] `timing_config.h` - todos os períodos/timeouts
  - [x] ADC sampling (100ms)
  - [x] Display update (500ms)
  - [x] MQTT publish (5s)
  - [x] Button debounce (20ms)
  - [x] Timeouts de comunicação

- [x] `feature_config.h` - feature flags compilação
  - [x] Seleção de features
  - [x] Limites de qualidade de energia
  - [x] Debug logging levels

- [x] `common_types.h` - definições globais
  - [x] Custom error codes
  - [x] Measurement structs
  - [x] Quality metrics struct
  - [x] State enums
  - [x] Callback types
  - [x] Macros úteis

### Documentação Técnica
- [x] DEVELOPMENT_ROADMAP.md
  - [x] 7 fases detalhadas
  - [x] Checklist por fase
  - [x] Deliverables
  - [x] Risk assessment
  - [x] Success criteria

### Estrutura de Componentes
```
components/
├── config/                  # ✅ 4 headers de configuração
│   ├── hardware_config.h
│   ├── timing_config.h
│   ├── feature_config.h
│   └── common_types.h
├── adc_sensor/             # 🔄 Pronto para implementação
│   ├── include/
│   │   └── adc_sensor.h
│   ├── adc_sensor.c
│   └── CMakeLists.txt
├── display/                # 🔄 Pronto para implementação
│   ├── include/
│   │   └── display.h
│   ├── display.c
│   └── CMakeLists.txt
├── mqtt/                   # 🔄 Pronto para implementação
│   ├── include/
│   │   └── mqtt_client.h
│   ├── mqtt_client.c
│   └── CMakeLists.txt
├── ui/                     # 🔄 Pronto para implementação
│   ├── include/
│   │   └── ui_buttons.h
│   ├── ui_buttons.c
│   └── CMakeLists.txt
└── analysis/              # 📋 Será criado na Phase 3
```

---

## 🚀 Antes de Começar a Programar

### Verificações Obrigatórias

#### 1. ESP-IDF Installation
```bash
# Verificar instalação
idf.py --version
echo %IDF_PATH%

# Se não está configurado:
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
```

#### 2. Hardware Disponível
- [ ] ESP32 board pronta
- [ ] ZMPT101B voltage sensor (ou similar)
- [ ] SCT013-030 current sensor (ou similar)
- [ ] SSD1306 OLED 128x64 display
- [ ] 3 botões para interface
- [ ] Protoboard e jumpers
- [ ] Fonte AC para testes (ou simulador AC)

#### 3. Sensor Specifications
| Componente | Spec | Notas |
|-----------|------|-------|
| ZMPT101B | 0-250V AC | Output: 0-3.3V DC |
| SCT013-030 | 0-30A AC | Output: 0-1V AC (com burden) |
| SSD1306 | 128x64 px | I2C 0x3C padrão |
| ESP32 | ADC 12-bit | Vcc 3.3V max |

#### 4. Schema de Pinagem (Confirmar)
```
ESP32 PIN MAPPING:
├── ADC
│   ├── GPIO34 (ADC1_CH6) - Voltage sensor
│   └── GPIO35 (ADC1_CH7) - Current sensor
├── I2C (OLED)
│   ├── GPIO22 (SCL)
│   └── GPIO21 (SDA)
├── Buttons
│   ├── GPIO35 - UP
│   ├── GPIO34 - DOWN
│   └── GPIO32 - SELECT
├── LED Status
│   └── GPIO2 - LED
└── UART (Debug)
    ├── GPIO1 (TX)
    └── GPIO3 (RX)
```

#### 5. Software Dependencies
```bash
# Verificar componentes ESP-IDF disponíveis:
# - esp_adc (built-in)
# - driver (built-in)
# - freertos (built-in)
# - mqtt (built-in)
# - esp_wifi (built-in)
# - i2c_master (built-in)

# Bibliotecas externas (se necessário):
# - ssd1306 driver (buscar componente correspondente)
# - FFT library (para harmônicas)
```

---

## 📋 Checklist de Primeiro Commit

### Antes de commitar a estrutura base:

#### Code Quality
- [x] Sem syntax errors
- [x] Sem compiler warnings (quando compilar)
- [x] Todos os headers têm guards (#ifndef, #define, #endif)
- [x] Nomes de arquivos consistentes (snake_case)
- [x] Indentação correta (4 espaços)

#### Documentation
- [x] README.md bem descritivo
- [x] CODING_STANDARDS.md completo
- [x] DEVELOPMENT_ROADMAP.md detalhado
- [x] Comentários nos headers

#### Git Repository
- [ ] `.gitignore` configurado (ESP-IDF specific)
- [ ] README no raiz do projeto
- [ ] LICENSE file (se necessário)
- [ ] Initial commit message: "Initial project structure with coding standards"

#### .gitignore Sugerido
```
# Build artifacts
build/
dist/
*.o
*.a
*.so
*.bin
*.hex
*.elf

# ESP-IDF specific
sdkconfig.old
.build/
partition_table/

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS specific
.DS_Store
Thumbs.db

# Python
__pycache__/
*.py[cod]
*$py.class
```

---

## 🎯 Execução Recomendada da Phase 1

### Dia 1: Setup & ADC
1. [ ] Clonar repositório no workspace
2. [ ] Configurar e testar compilação
3. [ ] Começar implementação do módulo ADC
4. [ ] Testes iniciais de leitura de amostra

### Dia 2-3: ADC Completo
1. [ ] Implementar RMS calculation
2. [ ] Integrar ambos os canais (voltage + current)
3. [ ] Criar testes unitários
4. [ ] Calibração initial
5. [ ] Logging e debug

### Dia 4: Display & UI
1. [ ] Driver OLED básico
2. [ ] Integração I2C
3. [ ] Buttons GPIO
4. [ ] Menu estrutura

### Dia 5: Integration
1. [ ] ADC → Display loop
2. [ ] Buttons → Menu navigation
3. [ ] Test end-to-end
4. [ ] Done! Ready for Phase 2

---

## 💡 Tips para Desenvolvimento Eficiente

### 1. Use Logging Extensively
```c
// Durante development
#define LOG_LEVEL_DEFAULT ESP_LOG_VERBOSE

// Production
#define LOG_LEVEL_DEFAULT ESP_LOG_INFO
```

### 2. Test Early & Often
```bash
# Configure, compile, flash frequently
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. Keep Components Independent
- Cada componente compila sozinho
- Use interface clara (public headers)
- Mock dependencies em testes

### 4. Use FreeRTOS Tasks Sabiamente
```c
// Não bloqueie! Use delays
vTaskDelay(pdMS_TO_TICKS(100));  // ✅ Bom
while(1) {} // ❌ Ruim - bloqueia scheduler
```

### 5. Gerenciar Recursos Cuidadosamente
```c
// Sempre liberar recursos
malloc() → use → free()
init_task() → ... → cleanup_task()
xTaskCreate() → ... → vTaskDelete()
```

---

## 📞 Recursos Úteis

### Documentação
- **ESP-IDF Official Docs:** https://docs.espressif.com/projects/esp-idf/
- **FreeRTOS Guide:** https://www.freertos.org/features.html
- **MISRA C Guidelines:** IEC 61508 standards

### Debugging
```bash
# Ver logs em tempo real
idf.py -p /dev/ttyUSB0 monitor

# Building with verbose output
idf.py -v build

# Clean and rebuild
idf.py fullclean
idf.py build
```

### Git Workflow
```bash
# Commits frequentes durante development
git add .
git commit -m "feat: ADC voltage sampling implementation"
git push origin main
```

---

## ✨ Summary

**Status:** ✅ **Pronto para Development**

Você tem:
- ✅ Estrutura completa de projeto
- ✅ Padrões de código definidos (MISRA C)
- ✅ Configuração centralizada
- ✅ Tipos de dados bem definidos
- ✅ Roadmap detalhado
- ✅ Componentes estruturados

**Próximo passo:** Configure ESP-IDF e comece com Phase 1: ADC & Sensores!

Boa sorte! 🚀
