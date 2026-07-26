#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define ID_MOTA_ACTUAL 1// <--- Cambiar para cada Mota (1, 2, 3)
#define PIN_POT 34
#define NIVEL_DESCONECTADO 255
#define MAX_CANALES 13

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
uint8_t padreMAC[6];

uint8_t miNivel = NIVEL_DESCONECTADO;
uint8_t canalActual = 1;
uint32_t contadorMsgID = 0;

volatile bool respuestaBeaconRecibida = false;
uint8_t nivelPadreEncontrado = NIVEL_DESCONECTADO;
uint8_t macPadreTemp[6];

volatile bool ackEnvio = false;
volatile bool callbackEnvioRecibido = false;

QueueHandle_t xQueueDatos;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    callbackEnvioRecibido = true;
    ackEnvio = (status == ESP_NOW_SEND_SUCCESS);
}
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (len != sizeof(struct_mensaje)) return;

    struct_mensaje msg;
    memcpy(&msg, incomingData, sizeof(msg));

    // 1. Respuesta a exploración PING
    if (msg.tipo == BEACON_RESP) {
        if (msg.nivel_emisor < nivelPadreEncontrado) {
            nivelPadreEncontrado = msg.nivel_emisor;
            memcpy(macPadreTemp, info->src_addr, 6);
            respuestaBeaconRecibida = true;
        }
        return;
    }

    // 2. Solicitud de otra mota
    if (msg.tipo == BEACON_PING && miNivel < NIVEL_DESCONECTADO) {
        struct_mensaje resp;
        resp.tipo = BEACON_RESP;
        resp.id_mota = ID_MOTA_ACTUAL;
        resp.nivel_emisor = miNivel;
        
        // CAMBIAR info->src_addr POR broadcastMAC:
        esp_now_send(broadcastMAC, (uint8_t *)&resp, sizeof(resp)); 
        return;
    }

    // 3. Retransmisión Malla
    if (msg.tipo == DATO_SENSOR && miNivel < NIVEL_DESCONECTADO) {
        if (msg.nivel_emisor > miNivel && msg.saltos < 5) {
            msg.saltos++;
            msg.nivel_emisor = miNivel;
            esp_now_send(padreMAC, (uint8_t *)&msg, sizeof(msg));
            Serial.printf("[RETRANSMISIÓN] Reenviando dato de Mota %d hacia el Gateway\n", msg.id_mota);
        }
    }
}

bool buscarRedYCanal() {
    Serial.println("\n[BARRIDO] Buscando canal activo del Gateway o Nodo vecino...");
    nivelPadreEncontrado = NIVEL_DESCONECTADO;

    for (uint8_t ch = 1; ch <= MAX_CANALES; ch++) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        respuestaBeaconRecibida = false;

        struct_mensaje ping;
        ping.tipo = BEACON_PING;
        ping.id_mota = ID_MOTA_ACTUAL;
        ping.nivel_emisor = NIVEL_DESCONECTADO;

        esp_now_send(broadcastMAC, (uint8_t *)&ping, sizeof(ping));

        uint32_t tInicio = millis();
        while (!respuestaBeaconRecibida && (millis() - tInicio < 60)) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (respuestaBeaconRecibida) {
            canalActual = ch;
            miNivel = nivelPadreEncontrado + 1;
            memcpy(padreMAC, macPadreTemp, 6);

            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, padreMAC, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            
            if (!esp_now_is_peer_exist(padreMAC)) {
                esp_now_add_peer(&peerInfo);
            }

            Serial.printf("¡Red Encontrada! Canal: %d | Nivel Mesh: %d | Padre MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          canalActual, miNivel,
                          padreMAC[0], padreMAC[1], padreMAC[2], padreMAC[3], padreMAC[4], padreMAC[5]);
            return true;
        }
    }
    Serial.println("[BARRIDO] Sin respuesta en ningún canal. Reintentando...");
    return false;
}

void vTaskSensor(void *pvParameters) {
    float humedadMedida = 0.0;
    while (1) {
        int valorPot = analogRead(PIN_POT);
        humedadMedida = map(valorPot, 0, 4095, 0, 100);

        xQueueSend(xQueueDatos, &humedadMedida, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void vTaskRadio(void *pvParameters) {
    float humedadRecibida = 0.0;
    uint8_t fallosConsecutivos = 0;

    while (1) {
        if (miNivel == NIVEL_DESCONECTADO) {
            if (!buscarRedYCanal()) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
            fallosConsecutivos = 0;
        }

        if (xQueueReceive(xQueueDatos, &humedadRecibida, pdMS_TO_TICKS(1000)) == pdTRUE) {
            struct_mensaje msgData;
            msgData.tipo = DATO_SENSOR;
            msgData.msg_id = ++contadorMsgID;
            msgData.id_mota = ID_MOTA_ACTUAL;
            msgData.humedad = humedadRecibida;
            msgData.nivel_emisor = miNivel;
            msgData.saltos = 0;

            callbackEnvioRecibido = false;
            ackEnvio = false;

            esp_err_t res = esp_now_send(padreMAC, (uint8_t *)&msgData, sizeof(msgData));

            if (res == ESP_OK) {
                uint32_t t0 = millis();
                while (!callbackEnvioRecibido && (millis() - t0 < 50)) {
                    vTaskDelay(pdMS_TO_TICKS(5));
                }

                if (ackEnvio) {
                    Serial.printf("[TX ÉXITO] Mota %d | Humedad: %.1f%% | Nivel: %d | ID: %u\n",
                                  ID_MOTA_ACTUAL, msgData.humedad, miNivel, msgData.msg_id);
                    fallosConsecutivos = 0;
                } else {
                    fallosConsecutivos++;
                    Serial.printf("[TX FALLO] Sin ACK del Padre (%d/3)\n", fallosConsecutivos);
                }
            } else {
                fallosConsecutivos++;
            }

            if (fallosConsecutivos >= 3) {
                Serial.println("[ALERTA] Pérdida de enlace. Reiniciando búsqueda de red...");
                miNivel = NIVEL_DESCONECTADO;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    xQueueDatos = xQueueCreate(10, sizeof(float));

    if (xQueueDatos != NULL) {
        xTaskCreate(vTaskSensor, "TaskSensor", 2048, NULL, 1, NULL);
        xTaskCreate(vTaskRadio,  "TaskRadio",  4096, NULL, 2, NULL);
    }
}

void loop() {
    vTaskDelete(NULL);
}