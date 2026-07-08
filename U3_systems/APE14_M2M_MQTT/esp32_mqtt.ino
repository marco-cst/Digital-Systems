#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ==================== CONFIGURACIÓN ====================
const char* WIFI_SSID = "----";
const char* WIFI_PASSWORD = "----";
const char* MQTT_BROKER = "192.168.1.X"; // <-- COLOCAR IP local de tu PC
const int MQTT_PORT = 1883;

// Pines
const int PIN_SENSOR = 34;
const int PIN_LED = 2;

// Variables globales y sincronización
volatile bool ledState = false;
SemaphoreHandle_t xLedSemaphore;

WiFiClient espClient;
PubSubClient client(espClient);

// ==================== CALLBACK MQTT ====================
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.printf("[MQTT] Mensaje recibido en tópico %s: %s\n", topic, mensaje.c_str());

  if (String(topic) == "esp32/led") {
    if (mensaje == "ON") {
      ledState = true;
      xSemaphoreGive(xLedSemaphore); 
    } else if (mensaje == "OFF") {
      ledState = false;
      xSemaphoreGive(xLedSemaphore);
    }
  }
}

// ==================== TAREAS FREERTOS ====================

// Tarea 1: Lectura del Sensor LM35
void vTaskSensor(void *pvParameters) {
  for (;;) {
    int adcValue = analogRead(PIN_SENSOR);
    float voltage = (adcValue / 4095.0) * 3.3;
    float temperatura = voltage * 100.0; // Conversión a °C

    // Publicar temperatura
    char tempStr[8];
    dtostrf(temperatura, 1, 2, tempStr);
    client.publish("esp32/temperatura", tempStr);
    
    Serial.printf("[SENSOR] Temperatura publicada: %s°C\n", tempStr);

    vTaskDelay(pdMS_TO_TICKS(2000)); // Retardo no bloqueante de 2 segundos
  }
}

// Tarea 2: Control Físico del LED
void vTaskLED(void *pvParameters) {
  for (;;) {
    // La tarea se bloquea aquí hasta que el semáforo sea liberado por el Callback
    if (xSemaphoreTake(xLedSemaphore, portMAX_DELAY) == pdTRUE) {
      digitalWrite(PIN_LED, ledState);
      Serial.printf("[ACTUADOR] LED actualizado a: %s\n", ledState ? "ON" : "OFF");
    }
  }
}

// Tarea 3: Mantenimiento de Conexión Wi-Fi y MQTT
void vTaskMQTT(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop(); 
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==================== FUNCIONES AUXILIARES ====================
void setupWiFi() {
  Serial.println("[WIFI] Conectando...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("[MQTT] Intentando conectar al broker...");
    String clientId = "ESP32_Client_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("[MQTT] Conectado y suscrito.");
      client.subscribe("esp32/led"); // Suscripción crítica
    } else {
      Serial.print("[MQTT] Falló, rc=");
      Serial.print(client.state());
      Serial.println(" Intentando de nuevo en 2 segundos");
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// ==================== SETUP PRINCIPAL ====================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_LED, OUTPUT);

  // Crear semáforo binario
  xLedSemaphore = xSemaphoreCreateBinary();

  // Conectar a Red
  setupWiFi();

  // Configurar MQTT
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqtt_callback);

  // Crear Tareas en el Núcleo 1 (App CPU) - El Núcleo 0 es para la pila Wi-Fi (Pro CPU)
  xTaskCreatePinnedToCore(vTaskSensor, "TareaSensor", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(vTaskLED,   "TareaLED",   2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(vTaskMQTT,  "TareaMQTT",  4096, NULL, 1, NULL, 1);
  
  Serial.println("[SISTEMA] Tareas FreeRTOS iniciadas en Núcleo 1.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}