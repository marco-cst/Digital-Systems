//Practica 001 - Uso de Wokwi y Aruino IDE en Microcontroladores AVR 
// Castillo Marco
// Sistemas Digitales 
#define LED_PIN 13

// Variables
int a = 5;
byte b = 3;
long c = 10;
float d = 2.5;
bool e = true;

byte estadoLed = 0;
int contador = 0;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);

  Serial.println("Operaciones Bitwise:");

  Serial.print("AND (5 & 3): ");
  Serial.println(a & b, BIN);

  Serial.print("OR (5 | 3): ");
  Serial.println(a | b, BIN);

  Serial.print("XOR (5 ^ 3): ");
  Serial.println(a ^ b, BIN);

  Serial.print("NOT (~5): ");
  Serial.println(~a, BIN);

  Serial.print("SHIFT LEFT (1 << 2): ");
  Serial.println(1 << 2, BIN);

  Serial.print("SHIFT RIGHT (4 >> 1): ");
  Serial.println(4 >> 1, BIN);

  estadoLed = estadoLed | (1 << 0);
}

void loop() {

  // XOR (parpadeo)
  estadoLed = estadoLed ^ 0b00000001;

  if ((estadoLed & 1) == 1) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  delay(500);

  // C bits
  contador = (contador + 1) % 8;

  Serial.print("Shift: ");
  Serial.println(1 << contador, BIN);

  delay(500);
}