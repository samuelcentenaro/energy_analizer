# CODING STANDARDS - Energy Analyzer Firmware

## 1. Padrão de Código

### 1.1 Naming Conventions (MISRA C)
```c
// Tipos
typedef struct {
    uint32_t value;
} my_struct_t;

// Constantes
#define CONFIG_SENSOR_TIMEOUT_MS   1000
#define STATUS_OK                  0x00

// Funções: verb_noun ou component_action
esp_err_t adc_sensor_read(void);
void display_show_voltage(float voltage);

// Variáveis estáticas
static uint32_t g_sensor_count;

// Variáveis locais: snake_case
uint32_t local_value;
```

### 1.2 Estrutura de Arquivos
```
component/
├── include/
│   ├── component_public.h      # API pública
│   └── component_types.h       # Tipos compartilhados
├── private/
│   └── component_private.h     # Definições internas
├── component.c                 # Implementação
├── CMakeLists.txt
└── idf_component.yml
```

### 1.3 Comprimento de Linhas e Indentação
- Máximo 100 caracteres por linha
- Indentação: 4 espaços (NO tabs)
- Quebra de linhas: align logicamente

## 2. Tratamento de Erros

### 2.1 Return Codes
```c
typedef enum {
    ERR_OK = 0x00,
    ERR_INVALID_PARAM = 0x01,
    ERR_TIMEOUT = 0x02,
    ERR_HW_FAILURE = 0x03,
    ERR_NOT_INITIALIZED = 0x04,
    ERR_BUSY = 0x05
} error_code_t;
```

### 2.2 Função com Tratamento de Erro
```c
esp_err_t component_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // Validar estado
    if (g_component_initialized) {
        ESP_LOGW(TAG, "Component already initialized");
        return ESP_OK;
    }
    
    // Executar com verificação
    ret = hw_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware init failed: 0x%02x", ret);
        return ret;
    }
    
    g_component_initialized = true;
    ESP_LOGI(TAG, "Component initialized successfully");
    
    return ESP_OK;
}
```

## 3. Logging

### 3.1 Níveis de Log
- **ERROR** (LOGE): Falhas críticas que impedem funcionamento
- **WARN** (LOGW): Anomalias, comportamento inesperado
- **INFO** (LOGI): Eventos importantes do sistema
- **DEBUG** (LOGD): Informações de diagnóstico
- **VERBOSE** (LOGV): Dados detalhados (apenas desenvolvimento)

### 3.2 Uso Correto
```c
#define TAG "COMPONENT_NAME"

ESP_LOGE(TAG, "Fatal error: sensor not responding (timeout=%d ms)", 
         timeout_ms);
ESP_LOGW(TAG, "Low power state, reducing update frequency");
ESP_LOGI(TAG, "Sensor initialized, sampling rate=%d Hz", sample_rate);
ESP_LOGD(TAG, "Raw ADC value: 0x%04x", raw_value);
```

## 4. Gerenciamento de Estado (FreeRTOS Task-based)

### 4.1 Padrão de Estado
```c
typedef enum {
    STATE_IDLE,
    STATE_INITIALIZING,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_SHUTDOWN
} component_state_t;

typedef struct {
    component_state_t state;
    uint32_t last_update_ms;
    esp_err_t last_error;
} component_handle_t;
```

### 4.2 Task Principal
```c
static void component_task(void *arg)
{
    component_handle_t *handle = (component_handle_t *)arg;
    
    while (1) {
        switch (handle->state) {
            case STATE_IDLE:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case STATE_RUNNING:
                handle->last_error = component_process();
                if (handle->last_error != ESP_OK) {
                    handle->state = STATE_ERROR;
                    ESP_LOGE(TAG, "Process failed");
                }
                break;
                
            case STATE_ERROR:
                // Recuperação ou log
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            default:
                ESP_LOGW(TAG, "Unknown state: %d", handle->state);
                break;
        }
    }
}
```

## 5. Documentação (Doxygen)

### 5.1 Estrutura de Headers Públicos
```c
/**
 * @file component_public.h
 * @brief Descrição breve do módulo
 * 
 * Descrição detalhada da funcionalidade e responsabilidades
 * do módulo.
 */

/**
 * @brief Inicializa o componente
 * 
 * Configura hardware, aloca recursos e prepara o componente
 * para uso. Deve ser chamado apenas uma vez.
 * 
 * @param[in] config Ponteiro para estrutura de configuração
 * 
 * @return ESP_OK se sucesso, código de erro caso contrário
 * 
 * @note Função não é thread-safe no primeiro acesso
 * @warning Modifique config apenas antes de chamar init
 */
esp_err_t component_init(const component_config_t *config);

/**
 * @brief Processa dados do componente
 * 
 * Realiza leitura, computação e armazena resultado em
 * estrutura de dados compartilhada.
 * 
 * @return ESP_OK se sucesso
 */
esp_err_t component_process(void);
```

## 6. Configurações (Arquivos Separados)

### 6.1 Localização
```
components/
├── config/
│   ├── hardware_config.h       # Definições de pinos e HW
│   ├── timing_config.h         # Timeouts e períodos
│   └── feature_config.h        # Flags de funcionalidades
```

### 6.2 Exemplo: hardware_config.h
```c
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// ADC Configuration (ZMPT101B voltage sensor)
#define ADC_UNIT                ADC_UNIT_1
#define ADC_CHANNEL_VOLTAGE      ADC_CHANNEL_0
#define ADC_ATTEN                ADC_ATTEN_DB_11
#define ADC_BITWIDTH             ADC_BITWIDTH_12

// I2C Configuration (OLED Display)
#define I2C_MASTER_NUM           I2C_NUM_0
#define I2C_MASTER_SDA_IO        GPIO_NUM_21
#define I2C_MASTER_SCL_IO        GPIO_NUM_22
#define I2C_MASTER_FREQ_HZ       400000

// GPIO Configuration (Buttons)
#define BUTTON_PIN_UP            GPIO_NUM_35
#define BUTTON_PIN_DOWN          GPIO_NUM_34
#define BUTTON_PIN_SELECT        GPIO_NUM_32

// UART Configuration (Debug)
#define DEBUG_UART_NUM           UART_NUM_0
#define DEBUG_UART_BAUD_RATE     115200

#endif // HARDWARE_CONFIG_H
```

### 6.3 Exemplo: timing_config.h
```c
#ifndef TIMING_CONFIG_H
#define TIMING_CONFIG_H

// ADC Configuration
#define ADC_SAMPLING_PERIOD_MS   100      // 10 Hz
#define ADC_READ_TIMEOUT_MS      50

// Display Configuration
#define DISPLAY_UPDATE_PERIOD_MS 500      // 2 Hz
#define DISPLAY_INIT_TIMEOUT_MS  1000

// MQTT Configuration
#define MQTT_PUBLISH_PERIOD_MS   5000     // 0.2 Hz
#define MQTT_CONNECT_TIMEOUT_MS  30000

// Button Configuration
#define BUTTON_DEBOUNCE_MS       20
#define BUTTON_LONG_PRESS_MS     1000

#endif // TIMING_CONFIG_H
```

## 7. Gerenciamento de Memória

### 7.1 Alocação
- **Preferência:** Alocação estática (compile-time) quando possível
- **Dinâmica:** malloc/free com rastreamento de erros
- **Pattern:** Sempre verificar ponteiro == NULL

```c
uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE);
if (buffer == NULL) {
    ESP_LOGE(TAG, "Memory allocation failed");
    return ESP_ERR_NO_MEM;
}
// ... usar buffer
free(buffer);
buffer = NULL;  // Evitar use-after-free
```

## 8. Unit Testing

### 8.1 Estrutura
```
components/component_name/
├── test/
│   ├── CMakeLists.txt
│   └── test_component.c
```

### 8.2 Framework: Unity (ESP-IDF)
```c
#include "unity.h"
#include "component.h"

TEST_CASE("Component initialization", "[component]")
{
    esp_err_t ret = component_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("Component invalid parameter", "[component]")
{
    esp_err_t ret = component_process_with_param(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}
```

## 9. Boas Práticas Gerais

### 9.1 Validação de Entrada
```c
esp_err_t function(const void *ptr, uint16_t value)
{
    // Validar entrada
    if (ptr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (value > MAX_VALUE) {
        ESP_LOGW(TAG, "Value out of range: %d > %d", value, MAX_VALUE);
        return ESP_ERR_INVALID_ARG;
    }
    
    // ... resto da função
    return ESP_OK;
}
```

### 9.2 Review Checklist
- [ ] Todas as funções públicas documentadas com Doxygen
- [ ] Nenhuma linha com mais de 100 caracteres
- [ ] Tratamento de erro em todas as chamadas críticas
- [ ] Logs apropriados (não verbose, não sparse)
- [ ] Sem variáveis globais mutáveis (usar structs)
- [ ] Sem allocação dinâmica desnecessária
- [ ] Unit tests para lógica crítica
- [ ] Nenhuma warning de compilação

## 10. Estrutura de Release

### 10.1 Versionamento
```
MAJOR.MINOR.PATCH-status
v1.0.0-alpha, v1.0.0-beta, v1.0.0
```

### 10.2 Changelog
Manter CHANGELOG.md atualizado com cada feature/fix
