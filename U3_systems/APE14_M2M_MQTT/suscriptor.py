import paho.mqtt.client as mqtt

# Configuración del Broker local
BROKER = "localhost" 
PUERTO = 1883

cliente = mqtt.Client(client_id="SuscriptorPython", protocol=mqtt.MQTTv311)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[Python Suscriptor] Conectado al Broker!")
        # ---- TOPICO ESP32 ----
        cliente.subscribe("esp32/temperatura")
        cliente.subscribe("esp32/led")
        print("[Python Suscriptor] Escuchando tópicos: esp32/temperatura y esp32/led")
    else:
        print(f"[Python Suscriptor] Error al conectar, código rc={rc}")

def on_message(client, userdata, msg):
    mensaje = msg.payload.decode('utf-8')
    if msg.topic == "esp32/temperatura":
        print(f"[Python] Recibido en {msg.topic}: {mensaje} °C")
    elif msg.topic == "esp32/led":
        print(f"[Python] Estado del LED actualizado: {mensaje}")

cliente.on_connect = on_connect
cliente.on_message = on_message

print("[Python Suscriptor] Conectando al broker...")
cliente.connect(BROKER, PUERTO, 60)

try:
    cliente.loop_forever()
except KeyboardInterrupt:
    print("\n[Python Suscriptor] Deteniendo...")
    cliente.disconnect()