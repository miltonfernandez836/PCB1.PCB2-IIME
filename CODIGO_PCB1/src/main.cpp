#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "thc_logic.h"
#include "web_server.h"
#include "modbus_master.h"

// --- VARIABLES GLOBALES ---
float targetV = 0.0f;
bool  targetSet = false;
float deadband = 2.0f;
Mode currentMode = MODE_MANUAL;

// Variables de estado
float vFilt = 0.0f;
bool  arcOK = false;
bool  upCmd = false;
bool  downCmd = false;
String estadoTHCStr = "INICIO";

// --- ¡ESTA ES LA LÍNEA QUE FALTABA! ---
// Creamos el objeto JSON aquí para que thc_logic pueda usarlo
StaticJsonDocument<768> doc; 

// --- MULTITAREA (FreeRTOS) ---
TaskHandle_t TaskModbusHandle;

// Esta función correrá en el NÚCLEO 0 (independiente)
void TaskModbusCode(void * pvParameters) {
  Serial.print("Modbus corriendo en Core: ");
  Serial.println(xPortGetCoreID());

  // Bucle infinito exclusivo para Modbus
  for(;;) {
    loopModbusMaster();
    // Pequeña pausa para no calentar el núcleo (10ms)
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== INICIANDO SISTEMA THC (ETHERNET + DUAL CORE) ===");

  // 1. Configurar THC
  setupTHC();

  // 2. Configurar Modbus (Hardware)
  setupModbusMaster();

  // 3. Configurar Ethernet y WebServer
  setupWebServer();

  // 4. LANZAR LA TAREA PARALELA (MODBUS EN CORE 0)
  xTaskCreatePinnedToCore(
      TaskModbusCode,   /* Función de la tarea */
      "TaskModbus",     /* Nombre */
      4096,             /* RAM asignada */
      NULL,             /* Parámetros */
      1,                /* Prioridad baja */
      &TaskModbusHandle,/* Handle */
      0);               /* NÚCLEO 0 (Importante) */

  Serial.println("Sistema Operativo: Multitarea Activada");
}

void loop() {
  // --- NÚCLEO 1 (POR DEFECTO) ---
  
  // 1. Atender Ethernet (HMI)
  runWebServer(); 
  
  // === LÍNEA AGREGADA: INICIA EL CRONÓMETRO ===
  unsigned long inicio = micros();

  // 2. Lógica del THC
  loopTHC();

// === LÍNEA AGREGADA: DETIENE EL CRONÓMETRO ===
  unsigned long fin = micros(); 

  // === BLOQUE AGREGADO: IMPRESIÓN IMPERCEPTIBLE AL RENDIMIENTO ===
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 1000) {
      Serial.print("Tiempo de ciclo THC (microsegundos): ");
      Serial.println(fin - inicio);
      ultimoPrint = millis();
  }

}