// --- CONFIGURACIÓN DE PINES ---
const int trigPin = 2;
const int echoPin = 3;
const int tmpPin = A0;
const int ventiladorPin = 9; 
const int buzzerPin = 8;

// =========================================================================
// COMUNICACIÓN ENTRE TAREAS (QUEUES) 
// =========================================================================

// COLA 1: Datos brutos (Sensor -> Procesamiento)
struct RawDataQueue {
  float distancia_bruta;
  float temperatura_bruta;
  bool disponible; // Simula el bloqueo de cola (true = hay datos)
};
RawDataQueue Queue_DatosBrutos = {0.0, 0.0, false};

// COLA 2: Eventos procesados (Procesamiento -> Salida)
struct ProcessedEventQueue {
  bool evento_colision;
  bool evento_sobrecalentamiento;
  float distancia_reporte;
  float temperatura_reporte;
  bool disponible; // Simula el bloqueo de cola
};
ProcessedEventQueue Queue_EventosProcesados = {false, false, 0.0, 0.0, false};

// =========================================================================
// SINCRONIZACIÓN DEL SISTEMA 
// =========================================================================

// Simulación de vTaskDelay para la Tarea de Adquisición
unsigned long lastTime_Adquisicion = 0;
const long periodo_Adquisicion = 60; // Prioridad Alta: se ejecuta cada 60ms

// Control de tiempo para la Tarea de Actuación (Evitar saturar el Serial)
unsigned long lastTime_SerialPrint = 0;
const long periodo_SerialPrint = 500; 

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ventiladorPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
  digitalWrite(ventiladorPin, LOW);
  digitalWrite(buzzerPin, LOW);
  
  Serial.begin(9600);
}

void loop() {
  unsigned long tiempoActual = millis();

  // =========================================================================
  // TAREA 1: ADQUISICIÓN DE DATOS (Prioridad Alta) - Punto 2 y 3
  // =========================================================================
  // Se usa vTaskDelay simulado (millis) para controlar su ejecución
  if (tiempoActual - lastTime_Adquisicion >= periodo_Adquisicion) {
    lastTime_Adquisicion = tiempoActual;
    
    // 1. Lectura de sensores múltiples
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duracion = pulseIn(echoPin, HIGH, 30000); 
    float distCm = duracion * 0.034 / 2;

    int lecturaADC = analogRead(tmpPin);
    float voltaje = lecturaADC * (5.0 / 1023.0);
    float tempC = (voltaje - 0.5) * 100.0; 

    // 2. Simula xQueueSend(Queue_DatosBrutos, ...)
    Queue_DatosBrutos.distancia_bruta = distCm;
    Queue_DatosBrutos.temperatura_bruta = tempC;
    Queue_DatosBrutos.disponible = true; // Desbloquea la tarea de procesamiento
  }

  // =========================================================================
  // TAREA 2: PROCESAMIENTO (Prioridad Media) 
  // =========================================================================
  // SINCRONIZACIÓN: "Bloqueo por espera de cola". 
  // Esta tarea NO se ejecuta (se salta) hasta que Queue_DatosBrutos.disponible sea true.
  if (Queue_DatosBrutos.disponible) {
    
    // 1. Filtrado y decisiones lógicas (Detección de eventos)
    bool hay_colision = (Queue_DatosBrutos.distancia_bruta > 0 && Queue_DatosBrutos.distancia_bruta < 20);
    bool hay_calor = (Queue_DatosBrutos.temperatura_bruta > 40.0);

    // 2. Simula xQueueSend(Queue_EventosProcesados, ...)
    Queue_EventosProcesados.evento_colision = hay_colision;
    Queue_EventosProcesados.evento_sobrecalentamiento = hay_calor;
    Queue_EventosProcesados.distancia_reporte = Queue_DatosBrutos.distancia_bruta;
    Queue_EventosProcesados.temperatura_reporte = Queue_DatosBrutos.temperatura_bruta;
    Queue_EventosProcesados.disponible = true; // Desbloquea la tarea de actuación

    // 3. Limpiar cola de entrada (Simula que el dato fue consumido)
    Queue_DatosBrutos.disponible = false; 
  }

  // =========================================================================
  // TAREA 3: ACTUACIÓN / COMUNICACIÓN (Prioridad Baja) 
  // =========================================================================
  // SINCRONIZACIÓN: "Bloqueo por espera de cola".
  // Solo actúa si hay un evento procesado disponible.
  if (Queue_EventosProcesados.disponible) {
    
    // 1. Motores y alarmas (Respuesta a eventos)
    // Se ejecuta inmediatamente al llegar el evento de la cola, sin importar el Serial
    digitalWrite(buzzerPin, Queue_EventosProcesados.evento_colision ? HIGH : LOW);
    digitalWrite(ventiladorPin, Queue_EventosProcesados.evento_sobrecalentamiento ? HIGH : LOW);

    // 2. Comunicación (Transmisión de datos al Serial Monitor)
    // Throttling: Limitamos la impresión a cada 500ms para no afectar la latencia del sistema
    if (tiempoActual - lastTime_SerialPrint >= periodo_SerialPrint) {
      lastTime_SerialPrint = tiempoActual;
      
      Serial.println("-----------------------------------------");
      Serial.print("[PROCESADO] Distancia: ");
      Serial.print(Queue_EventosProcesados.distancia_reporte);
      Serial.println(" cm");
      Serial.print("[PROCESADO] Temperatura: ");
      Serial.print(Queue_EventosProcesados.temperatura_reporte);
      Serial.println(" C");
      Serial.print("[ACCION] Buzzer: ");
      Serial.println(Queue_EventosProcesados.evento_colision ? "ACTIVADO (Colision)" : "Apagado");
      Serial.print("[ACCION] Ventilador: ");
      Serial.println(Queue_EventosProcesados.evento_sobrecalentamiento ? "ACTIVADO (Frenos/Temp)" : "Apagado");
      Serial.println("-----------------------------------------");
    }

    // 3. Limpiar cola de eventos (Simula que el dato fue consumido)
    Queue_EventosProcesados.disponible = false;
  }
  
  // El loop() se ejecuta a máxima velocidad, pero las tareas están "bloqueadas" 
  // por sus respectivas esperas de cola y tiempos, garantizando un comportamiento estable.
}
