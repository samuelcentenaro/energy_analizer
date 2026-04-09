#!/usr/bin/env python3
"""
MQTT Testing Script for Energy Analyzer
Testa conectividade e visualiza mensagens MQTT
"""

import json
import time
import sys
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("❌ Instalar paho-mqtt: pip install paho-mqtt")
    sys.exit(1)

# Configuração
BROKER = "test.mosquitto.org"  # ou "127.0.0.1" para local
PORT = 1883
TOPIC = "energy-analyzer/energy-analyzer-001/telemetry"
CLIENT_ID = "mqtt-tester"

# Estatísticas
messages_received = 0
start_time = time.time()

def on_connect(client, userdata, flags, rc):
    """Callback quando conecta ao broker"""
    if rc == 0:
        print(f"✅ Conectado ao broker {BROKER}:{PORT}")
        client.subscribe(TOPIC)
        print(f"📡 Assinado tópico: {TOPIC}")
    else:
        print(f"❌ Falha na conexão: {rc}")

def on_message(client, userdata, msg):
    """Callback quando recebe mensagem"""
    global messages_received

    try:
        # Parse JSON
        payload = json.loads(msg.payload.decode())
        messages_received += 1

        # Calcular frequência
        elapsed = time.time() - start_time
        freq = messages_received / elapsed if elapsed > 0 else 0

        # Exibir dados formatados
        print(f"\n📨 Mensagem #{messages_received} (freq: {freq:.2f} msg/s)")
        print(f"   🕒 Timestamp: {datetime.fromtimestamp(payload['timestamp']/1000)}")
        print(f"   ⚡ Tensão RMS: {payload['voltage_rms']:.2f} V")
        print(f"   🔌 Corrente RMS: {payload['current_rms']:.3f} A")
        print(f"   💡 Potência Real: {payload['power_real']:.1f} W")
        print(f"   📊 Fator Potência: {payload['power_factor']:.3f}")

        # Verificar valores razoáveis
        if payload['voltage_rms'] < 180 or payload['voltage_rms'] > 260:
            print("   ⚠️  Tensão fora do range típico (180-260V)")
        if payload['current_rms'] < 0 or payload['current_rms'] > 10:
            print("   ⚠️  Corrente fora do range típico (0-10A)")
        if payload['power_factor'] < 0 or payload['power_factor'] > 1:
            print("   ⚠️  Fator potência inválido (0-1)")

    except json.JSONDecodeError:
        print(f"❌ JSON inválido: {msg.payload.decode()}")
    except KeyError as e:
        print(f"❌ Campo faltando: {e}")
    except Exception as e:
        print(f"❌ Erro: {e}")

def main():
    """Função principal"""
    print("🚀 Energy Analyzer MQTT Tester")
    print(f"📡 Broker: {BROKER}:{PORT}")
    print(f"📨 Tópico: {TOPIC}")
    print("⏹️  Ctrl+C para parar\n")

    # Criar cliente
    client = mqtt.Client(CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        # Conectar
        client.connect(BROKER, PORT, 60)

        # Loop principal
        client.loop_forever()

    except KeyboardInterrupt:
        print(f"\n⏹️  Interrompido pelo usuário")
        print(f"📊 Total mensagens: {messages_received}")

    except Exception as e:
        print(f"❌ Erro: {e}")

    finally:
        client.disconnect()

if __name__ == "__main__":
    main()