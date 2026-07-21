#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

const char* ssid = "---";
const char* password = "---";
WiFiUDP udp;
Coap coap(udp);

void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("\n[COAP Server] Mensaje recibido en /sensores");
  Serial.print("Tamano del payload recibido: ");
  Serial.print(packet.payloadlen);
  Serial.println(" bytes");
  
  String payload = "";
  for (int i = 0; i < packet.payloadlen; i++) {
    payload += (char)packet.payload[i];
  }
  Serial.print("Payload (Si es binario se veran simbolos raros): ");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  Serial.println("Servidor Conectado. IP: " + WiFi.localIP().toString());
  coap.server(callback_response, "sensores");
  coap.start();
}

void loop() { coap.loop(); }