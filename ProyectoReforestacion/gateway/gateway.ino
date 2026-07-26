#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>

const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";
const char* MQTT_SERVER   = "";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "reforestacion/suelo/humedad";
const char* MQTT_USER     = "";
const char* MQTT_PASSWORD = "";

#define NIVEL_GATEWAY 0
#define UMBRAL_ALERTA_HUMEDAD 30.0

enum TipoMensaje { BEACON_PING, BEACON_RESP, DATO_SENSOR };

typedef struct struct_mensaje {
    uint8_t tipo;
    uint32_t msg_id;
    int id_mota;
    float humedad;
    uint8_t nivel_emisor;
    uint8_t saltos;
} struct_mensaje;

uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

WiFiClient espClient;
PubSubClient mqttClient(espClient);

QueueHandle_t xQueueMQTT;

// Almacenamiento local de humedades
float humedad_m1 = 0.0;
float humedad_m2 = 0.0;
float humedad_m3 = 0.0;

struct RegistroMota {
    int id_mota;
    uint32_t ultimo_msg_id;
};
RegistroMota tablaMotas[15];
int cantidadMotas = 0;

bool esDuplicado(int id_mota, uint32_t msg_id) {
    for (int i = 0; i < cantidadMotas; i++) {
        if (tablaMotas[i].id_mota == id_mota) {
            if (tablaMotas[i].ultimo_msg_id >= msg_id) return true;
            tablaMotas[i].ultimo_msg_id = msg_id;
            return false;
        }
    }
    if (cantidadMotas < 15) {
        tablaMotas[cantidadMotas] = {id_mota, msg_id};
        cantidadMotas++;
    }
    return false;
}

// Interrupción Recepción ESP-NOW (Adaptada para ESP32 v3.x)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (len != sizeof(struct_mensaje)) return;

    struct_mensaje msgRecibido;
    memcpy(&msgRecibido, incomingData, sizeof(msgRecibido));

    // 1. Responder al barrido de las motas
    if (msgRecibido.tipo == BEACON_PING) {
        struct_mensaje respuesta;
        respuesta.tipo = BEACON_RESP;
        respuesta.id_mota = 0;
        respuesta.nivel_emisor = NIVEL_GATEWAY;
        
        // CORRECCIÓN CLAVE: Responder usando broadcastMAC que ya está registrado como Peer
        esp_now_send(broadcastMAC, (uint8_t *)&respuesta, sizeof(respuesta));
        return;
    }

    // 2. Procesar datos entrantes de los sensores
    if (msgRecibido.tipo == DATO_SENSOR) {
        if (!esDuplicado(msgRecibido.id_mota, msgRecibido.msg_id)) {
            xQueueSendFromISR(xQueueMQTT, &msgRecibido, NULL);
        }
    }
}

void vTaskMQTT(void *pvParameters) {
    struct_mensaje msg;

    while (1) {
        if (!mqttClient.connected()) {
            Serial.print("[MQTT] Conectando al Broker...");
            String clientId = "ESP32Gateway-" + String(random(0xffff), HEX);
            
            if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
                Serial.println(" ¡Conectado!");
            } else {
                Serial.printf(" Falló (%d). Reintentando en 3s...\n", mqttClient.state());
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }

        mqttClient.loop();

        if (xQueueReceive(xQueueMQTT, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (msg.id_mota == 1) humedad_m1 = msg.humedad;
            else if (msg.id_mota == 2) humedad_m2 = msg.humedad;
            else if (msg.id_mota == 3) humedad_m3 = msg.humedad;

            // Calcular promedio SOLO de las motas activas (mayores a 0%)
            int motasActivas = 0;
            float sumaHumedad = 0.0;
            
            if (humedad_m1 > 0.0) { sumaHumedad += humedad_m1; motasActivas++; }
            if (humedad_m2 > 0.0) { sumaHumedad += humedad_m2; motasActivas++; }
            if (humedad_m3 > 0.0) { sumaHumedad += humedad_m3; motasActivas++; }

            float promedio = 0.0;
            if (motasActivas > 0) {
                promedio = sumaHumedad / motasActivas;
            }

            // Lógica de Alertas (Umbral 30% para promedio, 20% para zona crítica individual)
            String textoAlerta = "Estado Normal";
            bool alertaPromedio = (promedio < 30.0 && motasActivas > 0); // Promedio bajo 30%
            bool alertaZonaCritica = (humedad_m1 > 0 && humedad_m1 < 20) || 
                                     (humedad_m2 > 0 && humedad_m2 < 20) || 
                                     (humedad_m3 > 0 && humedad_m3 < 20); // Una zona específica muriendo

            if (alertaPromedio || alertaZonaCritica) {
                textoAlerta = "ALERTA: Humedad Baja en Suelo";
            }

            char jsonPayload[256];
            snprintf(jsonPayload, sizeof(jsonPayload),
                     "{\"m1\": %.1f, \"m2\": %.1f, \"m3\": %.1f, \"avg\": %.1f, \"alert\": \"%s\"}",
                     humedad_m1, humedad_m2, humedad_m3, promedio, textoAlerta.c_str());

            mqttClient.publish(MQTT_TOPIC, jsonPayload);
            Serial.printf("[MQTT PUBLICADO] %s\n", jsonPayload);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("[Wi-Fi] Conectando");
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.printf("\n[Wi-Fi] Conectado! Canal actual: %d | IP: %s\n", WiFi.channel(), WiFi.localIP().toString().c_str());

    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW en Gateway");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    // Registrar broadcast MAC
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    xQueueMQTT = xQueueCreate(15, sizeof(struct_mensaje));

    if (xQueueMQTT != NULL) {
        xTaskCreatePinnedToCore(vTaskMQTT, "TaskMQTT", 4096, NULL, 2, NULL, 1);
    }
}

void loop() {
    vTaskDelete(NULL);
}