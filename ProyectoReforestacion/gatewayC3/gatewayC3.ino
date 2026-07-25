#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>

// === CONFIGURACIÓN WI-FI ===
const char* WIFI_SSID = "---";
const char* WIFI_PASSWORD = "---";

// === CONFIGURACIÓN MQTT CON SEGURIDAD ===
const char* MQTT_BROKER = "10.20.136.xx"; // IP de tu Ubuntu
const int MQTT_PORT = 1883;
// Usuario y contraseña del broker MQTT
const char* MQTT_USER = "---";     
const char* MQTT_PASSWORD = "---.";

WiFiClient espClient;
PubSubClient client(espClient);

// Variables para almacenar humedades de las 3 motas
float humedad_mota1 = 0.0;
float humedad_mota2 = 0.0; // Por ahora no llegarán, inicializan en 0
float humedad_mota3 = 0.0; // Por ahora no llegarán, inicializan en 0
bool datoRecibido = false;

// Estructura (Debe coincidir con la Mota)
typedef struct struct_mensaje {
    int id_mota;
    float humedad;
} struct_mensaje;

void OnDataRecv(const esp_now_recv_info_t *recv_info,
                const uint8_t *incomingData,
                int len) {

    struct_mensaje datosRecibidos;
    memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));

    Serial.printf("ESP-NOW Recibido -> Mota %d: %.1f%%\n",
                  datosRecibidos.id_mota,
                  datosRecibidos.humedad);

    // Si quieres conocer la MAC del emisor:
    const uint8_t *mac = recv_info->src_addr;

    Serial.printf("MAC origen: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);

    if (datosRecibidos.id_mota == 1) humedad_mota1 = datosRecibidos.humedad;
    if (datosRecibidos.id_mota == 2) humedad_mota2 = datosRecibidos.humedad;
    if (datosRecibidos.id_mota == 3) humedad_mota3 = datosRecibidos.humedad;

    datoRecibido = true;
}


void setup() {
    Serial.begin(115200);
    
    WiFi.mode(WIFI_STA);
    Serial.print("MAC DEL GATEWAY: "); Serial.println(WiFi.macAddress()); // <--- IMPRIMIR MAC
    WiFi.disconnect();
    delay(100);
    // ... resto del setup

    // Iniciar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error iniciando ESP-NOW en Gateway");
        return;
    }
    
    // Registrar callback de recepción
    esp_now_register_recv_cb(OnDataRecv);

    // Conectar a Wi-Fi
    Serial.print("Conectando a Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Conectado!");

    // Configurar MQTT con Usuario y Contraseña
    client.setServer(MQTT_BROKER, MQTT_PORT);
    reconnectMQTT();
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();

        if (datoRecibido) {
        datoRecibido = false; 
        
        int motasActivas = 0;
        float sumaHumedad = 0.0;
        
        if(humedad_mota1 > 0) { sumaHumedad += humedad_mota1; motasActivas++; }
        if(humedad_mota2 > 0) { sumaHumedad += humedad_mota2; motasActivas++; }
        if(humedad_mota3 > 0) { sumaHumedad += humedad_mota3; motasActivas++; }
        
        float promedio = 0.0;
        String alertaStr = "OK: Humedad adecuada";
        
        if(motasActivas > 0) { 
            promedio = sumaHumedad / motasActivas;
            if (promedio < 30.0) {
                alertaStr = "ALERTA: Humedad baja, riesgo para plantación";
            }
        } else {
            alertaStr = "Sistema desconectado";
        }
        
        // === CONSTRUIR JSON ===
        String jsonPayload = "{";
        jsonPayload += "\"m1\":" + String(humedad_mota1, 1) + ",";
        jsonPayload += "\"m2\":" + String(humedad_mota2, 1) + ",";
        jsonPayload += "\"m3\":" + String(humedad_mota3, 1) + ",";
        jsonPayload += "\"avg\":" + String(promedio, 1) + ",";
        jsonPayload += "\"alert\":\"" + alertaStr + "\"";
        jsonPayload += "}";
        
        // Publicar el JSON en un solo tópico unificado
        client.publish("reforestacion/suelo/datos", jsonPayload.c_str());
        Serial.printf("MQTT Publicado JSON: %s\n", jsonPayload.c_str());
    }

}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Conectando a MQTT...");
        String clientId = "GatewayESP32-";
        clientId += String(random(0xffff), HEX);
        
        // Conexión con AUTENTICACIÓN
        if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("Conectado!");
        } else {
            Serial.print("Falló, rc=");
            Serial.print(client.state());
            Serial.println(" Intentando en 3s");
            delay(3000);
        }
    }
}