#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>

const char* ssid = "--";
const char* password = "--";
IPAddress serverIp(192, 168, 1, 100); // Cambia esto a la IP del servidor CoAP
WiFiUDP udp;
Coap coap(udp);

struct SensorData { int id; float temperatura; float humedad; bool estado; };

void TaskCoapClient(void *pvParameters) {
  SensorData data = {1, 25.5, 60.2, true};
  bool sendJson = true;

  while (1) {
    StaticJsonDocument<200> doc;
    doc["id"] = data.id;
    doc["temperatura"] = data.temperatura;
    doc["humedad"] = data.humedad;
    doc["estado"] = data.estado;

    if (sendJson) {
      String jsonStr;
      uint32_t start_time = micros();
      serializeJson(doc, jsonStr);
      uint32_t end_time = micros();
      
      Serial.println("\n--- ENVIANDO JSON ---");
      Serial.print("Tamano: "); Serial.print(jsonStr.length()); Serial.println(" bytes");
      Serial.print("Tiempo serializacion: "); Serial.print(end_time - start_time); Serial.println(" us");
      
      coap.send(serverIp, 5683, "sensores", COAP_CON, COAP_POST, nullptr, 0, (const uint8_t*)jsonStr.c_str(), jsonStr.length(), COAP_TEXT_PLAIN);
      sendJson = false;
    } else {
      uint8_t buffer[200];
      uint32_t start_time = micros();
      size_t msgpack_len = serializeMsgPack(doc, buffer, sizeof(buffer));
      uint32_t end_time = micros();
      
      Serial.println("\n--- ENVIANDO MESSAGEPACK ---");
      Serial.print("Tamano: "); Serial.print(msgpack_len); Serial.println(" bytes");
      Serial.print("Tiempo serializacion: "); Serial.print(end_time - start_time); Serial.println(" us");
      
      coap.send(serverIp, 5683, "sensores", COAP_CON, COAP_POST, nullptr, 0, buffer, msgpack_len, 0);
      sendJson = true;
    }
    vTaskDelay(pdMS_TO_TICKS(5000)); // Delay RTOS no bloqueante
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  Serial.println("Cliente Conectado. IP: " + WiFi.localIP().toString());
  coap.start();
  xTaskCreatePinnedToCore(TaskCoapClient, "CoapClientTask", 10000, NULL, 1, NULL, 0);
}

void loop() { coap.loop(); }