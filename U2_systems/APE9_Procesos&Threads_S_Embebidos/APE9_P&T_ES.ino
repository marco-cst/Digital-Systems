// ==========================================
// SISTEMA IoT: MULTITAREA COOPERATIVA
// Procesos y Threads en Sistemas Embebidos
// =========================================
// --- DEFINICIÓN DE PINES ---
const int PIN_SENSOR = A0;
const int PIN_PULSADOR = 2;
const int PIN_LED_VERDE = 8;
const int PIN_LED_ROJO = 7;
const int PIN_BUZZER = 6;

// --- VARIABLES DE TIEMPO (TIMERS PARA CADA THREAD) ---
unsigned long prevMillisHeartbeat = 0;
unsigned long prevMillisTelemetria = 0;
unsigned long prevMillisAlarma = 0;

// --- INTERVALOS DE TIEMPO ---
const unsigned long INTERVALO_HEARTBEAT = 500;   // 0.5 segundos
const unsigned long INTERVALO_TELEMETRIA = 2000; // 2 segundos
const unsigned long INTERVALO_ALARMA = 300;      // 300 ms

// --- VARIABLES DE ESTADO DEL SISTEMA ---
float temperaturaC = 0.0;
bool alarmaActivada = false;
bool alarmaSilenciada = false;
bool estadoParpadeoAlarma = LOW; // Para alternar el LED y Buzzer

// --- CONFIGURACIÓN INICIAL ---
void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_PULSADOR, INPUT); // Configurado con pull-down externo de 10k
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Asegurar estados iniciales apagados
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);
  noTone(PIN_BUZZER);

  //Serial.println("=======================================");
  //Serial.println(" SISTEMA IoT INICIADO (Multitarea) ");
  //Serial.println("=======================================");
}

// --- BUCLE PRINCIPAL (PLANIFICADOR / SCHEDULER) ---
void loop() {
  unsigned long currentMillis = millis(); // Tiempo actual

  // Ejecución de threads lógicos
  taskHeartbeat(currentMillis);    // Thread 1
  taskTelemetry(currentMillis);    // Thread 2
  taskAlarmaTermica(currentMillis);// Thread 3
  taskPulsador();                  // Thread 4 (Evento asíncrono)
}

// ==========================================
// IMPLEMENTACIÓN DE THREADS LÓGICOS
// ==========================================

// THREAD 1: Indicador de vida (Heartbeat)
void taskHeartbeat(unsigned long currentMillis) {
  if (currentMillis - prevMillisHeartbeat >= INTERVALO_HEARTBEAT) {
    prevMillisHeartbeat = currentMillis;
    // Toggle (invertir) el estado del LED verde
    digitalWrite(PIN_LED_VERDE, !digitalRead(PIN_LED_VERDE));
  }
}

// THREAD 2: Monitoreo y Envío de Datos (Telemetría)
void taskTelemetry(unsigned long currentMillis) {
  if (currentMillis - prevMillisTelemetria >= INTERVALO_TELEMETRIA) {
    prevMillisTelemetria = currentMillis;
    
    // 1. Leer sensor TMP36
    int lecturaADC = analogRead(PIN_SENSOR);
    float voltaje = (lecturaADC * 5.0) / 1024.0;
    temperaturaC = (voltaje - 0.5) * 100.0; // Fórmula TMP36

    // 2. Evaluar umbral de temperatura
    if (temperaturaC > 30.0) {
      alarmaActivada = true;
      // Si recién sube de 30, reiniciamos el silenciamiento manual
      if (!alarmaSilenciada) {
        // Se permite sonar
      }
    } else {
      alarmaActivada = false;
      alarmaSilenciada = false; // Reiniciar flag al enfriarse
      // Forzar apagado de alarma por seguridad
      digitalWrite(PIN_LED_ROJO, LOW);
      noTone(PIN_BUZZER);
      estadoParpadeoAlarma = LOW;
    }

    // 3. Enviar datos por puerto serial
    Serial.print("Temperatura: ");
    Serial.print(temperaturaC);
    Serial.println(" grados C");
  }
}

// THREAD 3: Sistema de Alarma Térmica
void taskAlarmaTermica(unsigned long currentMillis) {
  if (alarmaActivada && !alarmaSilenciada) {
    if (currentMillis - prevMillisAlarma >= INTERVALO_ALARMA) {
      prevMillisAlarma = currentMillis;
      
      estadoParpadeoAlarma = !estadoParpadeoAlarma; // Alternar estado
      digitalWrite(PIN_LED_ROJO, estadoParpadeoAlarma);
      
      // El buzzer suena solo cuando el LED está en HIGH
      if (estadoParpadeoAlarma) {
        tone(PIN_BUZZER, 1000); // Tono de 1000 Hz
      } else {
        noTone(PIN_BUZZER);
      }
    }
  } else if (!alarmaActivada) {
    // Si no hay alarma, asegurar que todo quede apagado
    if (estadoParpadeoAlarma == HIGH) {
      digitalWrite(PIN_LED_ROJO, LOW);
      noTone(PIN_BUZZER);
      estadoParpadeoAlarma = LOW;
    }
  }
}

// THREAD 4: Silenciamiento Manual de Alarma (Acknowledge)
void taskPulsador() {
  // El pulsador está en configuración pull-down. HIGH = presionado
  if (digitalRead(PIN_PULSADOR) == HIGH) {
    
    // Si hay alarma y no ha sido silenciada aún
    if (alarmaActivada && !alarmaSilenciada) {
      alarmaSilenciada = true; // Activar flag de silencio
      
      // Apagar inmediatamente indicadores
      digitalWrite(PIN_LED_ROJO, LOW);
      noTone(PIN_BUZZER);
      estadoParpadeoAlarma = LOW;
      
      // Registrar evento en serial
      Serial.println(">>> ALERTA: Alarma silenciada manualmente por operador <<<");
    }
    
    // Anti-rebote simple por software (esperar a que se suelte)
    while(digitalRead(PIN_PULSADOR) == HIGH); 
  }
}