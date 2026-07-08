import paho.mqtt.client as mqtt

# Configuración del Broker local
BROKER = "localhost" 
PUERTO = 1883

cliente = mqtt.Client(client_id="PublicadorPython", protocol=mqtt.MQTTv311)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[Python Publicador] Conectado al Broker exitosamente!")
    else:
        print(f"[Python Publicador] Error al conectar, código rc={rc}")

cliente.on_connect = on_connect
print("[Python Publicador] Conectando al broker...")
cliente.connect(BROKER, PUERTO, 60)
cliente.loop_start()

try:
    while True:
        comando = input('Escribe "ON" para encender, "OFF" para apagar (o "salir" para terminar): ').strip().upper()
        
        if comando == "SALIR":
            break
        
        if comando in ["ON", "OFF"]:
            # ---- TOPICO ESP32 ----
            topico = "esp32/led"
            cliente.publish(topico, comando)
            print(f"[Python] Comando enviado: {comando} al tópico {topico}")
        else:
            print("[Python] Comando no reconocido. Solo se acepta ON, OFF o salir.")

except KeyboardInterrupt:
    pass
finally:
    print("\n[Python Publicador] Deteniendo...")
    cliente.loop_stop()
    cliente.disconnect()