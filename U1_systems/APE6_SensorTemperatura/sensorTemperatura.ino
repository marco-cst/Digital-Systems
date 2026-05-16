/*********************
 * SISTEMA INTELIGENTE DE MONITOREO - PARTE 2 || APE6
 * Sensor: DHT11 (Simulado con DHT22 por limitaciones de Wokwi)
 *********************/

#include <DHT.h>
#include <LiquidCrystal.h>

#define DHTPIN 2
// ---------------------------------------------------------
// NOTA TECNICA:
// Wokwi y Tinkercad no incluyen el modelo visual DHT11 en 
// su libreria. Para validar la lógica del sistema, se utiliza
// el componente visual DHT22

// IMPLEMENTACIÓN FÍSICA: 
// Al pasar al prototipo real en fisico, 
// cambiar esta linea a: #define DHTTYPE DHT11
#define DHTTYPE DHT22   
// ---------------------------------------------------------

DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

const int PinAzul = 3;
const int PinVerde = 4;
const int PinRojo = 5;

unsigned long tiempoAnterior = 0;
const unsigned long intervalo = 2000;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  dht.begin();

  pinMode(PinAzul, OUTPUT);
  pinMode(PinVerde, OUTPUT);
  pinMode(PinRojo, OUTPUT);
}

void loop() {

  if (millis() - tiempoAnterior >= intervalo) {
    tiempoAnterior = millis();
    // Leer temperatura
    float Temperatura = dht.readTemperature();

    // Verificar lectura
    if (isnan(Temperatura)) {
      lcd.setCursor(0, 0);
      lcd.print("Error sensor  ");

      lcd.setCursor(0, 1);
      lcd.print("Revise cables ");

      Serial.println("Error leyendo DHT22");
      return;
    }
    // Mostrar temperatura en LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(Temperatura, 1);
    lcd.print(" C   ");

    // Apagar LEDs
    digitalWrite(PinAzul, LOW);
    digitalWrite(PinVerde, LOW);
    digitalWrite(PinRojo, LOW);

    // Evaluar temperatura
    lcd.setCursor(0, 1);

    if (Temperatura < 20.0) {

      lcd.print("Estado: FRIO ");
      digitalWrite(PinAzul, HIGH);

    } else if (Temperatura > 30.0) {
      lcd.print("Estado: CALOR");
      digitalWrite(PinRojo, HIGH);
    } else {
      lcd.print("Estado:NORMAL");
      digitalWrite(PinVerde, HIGH);
    }
  // mostrar temperatura en consola
    Serial.print("Temperatura: ");
    Serial.print(Temperatura);
    Serial.println(" C");
  }
}