/*
  APE 2: Juego de Luces Interactivo
  Marco Castillo

  INSTRUCCIONES DE USO:

  1. Compilar
  2. Pulsar el interruptor para evidenciar el caso 1.
  3. Apagar y encender interruptor para evidenciar siguiente caso.

  NOTA: EXISTEN 2 FORMAS DE EJECUCION, COMENTAR/DESCOMENTAR  
       LINEA 42: para ejecucion secuencial (caso tras caso)
       LINEA 45: para ejecucion aleatoria.
  
  "EN ESTE CASO ESTAMOS USANDO EJECUCION ALEATORIA DE PATRON."
*/

// Definición de Pines
const int LED_PINS[] = {2, 3, 4, 5, 6, 7}; 
const int BOTON_PIN = 8;                   
int patronActual = 0;                      
bool ultimoEstadoBoton = LOW;              

void setup() {
  // Configurar pines de LEDs como salida usando un bucle for
  for (int i = 0; i < 6; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }

  //pinMode(BOTON_PIN, INPUT);
  pinMode(BOTON_PIN, INPUT_PULLUP);
  
  randomSeed(analogRead(0));
}

void leerBoton() {
  bool estadoBoton = digitalRead(BOTON_PIN);
  
  //if (estadoBoton == HIGH && ultimoEstadoBoton == LOW) {
  if (estadoBoton == LOW && ultimoEstadoBoton == HIGH){
    // PATRON SECUENCIAL:
    //patronActual = (patronActual + 1) % 5; 
    
    // PATRON ALEATORIO:
    patronActual = random(0, 5); 
    
    // Apagar leds antes de pasar al siguiente patron.
    for (int i = 0; i < 6; i++) digitalWrite(LED_PINS[i], LOW);
    
    delay(200);
  }
  ultimoEstadoBoton = estadoBoton;
}

// CASO 0: Secuencia - Se encienden uno por uno y se quedan encendidos
void patronSecuencia() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(100);
  }
  delay(200);
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  delay(200);
}

// CASO 1: Persecución - Solo un LED encendido a la vez moviéndose
void patronPersecucion() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(80);
    digitalWrite(LED_PINS[i], LOW);
  }
}

// CASO 2: Parpadeo - Todos los LEDs parpadean simultáneamente
void patronParpadeo() {
  int contador = 0;
  while (contador < 3) {
    for (int i = 0; i < 6; i++) digitalWrite(LED_PINS[i], HIGH);
    delay(200);
    for (int i = 0; i < 6; i++) digitalWrite(LED_PINS[i], LOW);
    delay(200);
    contador++;
  }
}

// CASO 3: Aleatorio - Los LEDs se encienden al azar
void patronAleatorio() {
  int ledAzar = random(0, 6);
  digitalWrite(LED_PINS[ledAzar], HIGH);
  delay(100);
  digitalWrite(LED_PINS[ledAzar], LOW);
}

// CASO 4: Onda - Efecto de ida y vuelta
void patronOnda() {
  // De izquierda a derecha
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(70);
    digitalWrite(LED_PINS[i], LOW);
  }
  // De derecha a izquierda
  for (int i = 4; i > 0; i--) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(70);
    digitalWrite(LED_PINS[i], LOW);
  }
}

void loop() {
  leerBoton();
  switch (patronActual) {
    case 0: patronSecuencia();   break;
    case 1: patronPersecucion(); break;
    case 2: patronParpadeo();    break;
    case 3: patronAleatorio();   break;
    case 4: patronOnda();        break;
    default: patronSecuencia();  break;
  }
}