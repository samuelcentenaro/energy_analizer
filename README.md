# Energy Analizer

Ferramenta de monitoramento e analise de consumo energetico para sistemas embarcados (ESP32).
O codigo esta organizado para compilar com ESP-IDF, usar OTA, MQTT e Wi-Fi para enviar dados a um broker.

## Visao geral do projeto

| Item | Descricao |
|------|-----------|
| **Arquitetura** | Firmware embarcado (C/C++) + scripts de teste Python. |
| **Componentes de hardware** | ESP32-S2, sensor de corrente (ACS712 ou SCT-013), divisor de tensao, modulo Wi-Fi integrado, opcional display OLED. |
| **Conexoes principais** | VCC -> 3.3 V do ESP32; GND -> GND comum; saida do sensor de corrente -> ADC (GPIO 34); divisor de tensao -> ADC (GPIO 35); I2C para display -> SDA GPIO 21, SCL GPIO 22 |
| **Comunicacao** | MQTT. OTA preparado via `sdkconfig.defaults.ota_prep`. |
| **Build** | CMake + ESP-IDF. Use `idf.py build flash monitor` ou o script `verify_build.py`. |
| **Testes** | `test_mqtt.py` valida conexao e publicacao de mensagens. |

## Estrutura de pastas

```text
.
|- components/                # Codigo reutilizavel (MQTT, OTA, drivers)
|- main/                      # Entry point e logica da aplicacao
|- managed_components/        # Componentes gerenciados pelo ESP-IDF
|- build/                     # Artefatos gerados (elf, bin, map)
|- CMakeLists.txt             # Configuracao de build
|- sdkconfig*                 # Configuracoes do ESP-IDF
'- README.md                  # Este arquivo
```

## Como construir e rodar

```bash
idf.py build
idf.py -p PORT flash monitor
```

## OTA

A estrutura de particao OTA ja foi preparada, mas a OTA ainda nao esta ativa no firmware atual.

## Documentacao util

- `ARCHITECTURE_REFERENCE.md`
- `HARDWARE_DIAGRAM.md`
- `HW_CONNECTIONS.txt`
- `BUILD_COMPLETION_CHECKLIST.md`
- `DEVELOPMENT_ROADMAP.md`
- `PROJECT_CURRENT_STATE.md`

## Contribuindo

1. Crie uma branch descritiva.
2. Siga `CODING_STANDARDS.md`.
3. Atualize a documentacao relacionada.
4. Abra um Pull Request.
