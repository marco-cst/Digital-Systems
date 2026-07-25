#include <esp_now.h>
#include <WiFi.h>

// === REEMPLAZA CON LA MAC DE TU GATEWAY ===
uint8_t gatewayMAC[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11}; 

// === CONFIGURACIÓN WI-FI ===
const char* WIFI_SSID = "---";
const char* WIFI_PASSWORD = "---";

const int PIN_POT = 34;

typedef struct struct_mensaje {
    int id_mota;
    float humedad;
} struct_mensaje;

struct_mensaje misDatos;

void setup() {
    Serial.begin(115200);
    misDatos.id_mota = 2;

    // 1. Conectar a Wi-Fi SOLO para sincronizar el canal del router
    WiFi.mode(WIFI_STA);
    Serial.print("Sincronizando canal Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nCanal sincronizado!");
    
    // 2. Desconectamos del Wi-Fi pero MANTENEMOS el canal
    WiFi.disconnect();
    
    // 3. Iniciar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }

    // 4. Registrar el Gateway como par (peer)
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); // Limpiar la estructura (importante en v3)
    memcpy(peerInfo.peer_addr, gatewayMAC, 6);
    peerInfo.channel = 0; // 0 significa "usar el canal actual en el que estoy" (ya sincronizado)
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Error al agregar par (Peer)");
        return;
    } else {
        Serial.println("Par agregado exitosamente");
    }
}

void loop() {
    int valorPot = analogRead(PIN_POT);
    misDatos.humedad = map(valorPot, 0, 4095, 0, 100);

    esp_err_t result = esp_now_send(gatewayMAC, (uint8_t *) &misDatos, sizeof(misDatos));
    
    if (result == ESP_OK) {
        Serial.printf("Mota %d - Humedad enviada: %.1f%%\n", misDatos.id_mota, misDatos.humedad);
    } else {
        Serial.printf("Error al enviar datos\n");
    }
        
    // Retardo no bloqueante de FreeRTOS (2 segundos = 2000 ms)
    vTaskDelay(pdMS_TO_TICKS(2000)); 
}