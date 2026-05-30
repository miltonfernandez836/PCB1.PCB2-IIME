#include <Arduino.h>
#include <math.h> 
#include <ArduinoJson.h>
#include <Preferences.h> 
#include "web_server.h" 
#include <WebSocketsServer.h>
#include "thc_logic.h"
#include "modbus_master.h"

// --- Objeto de Preferencias ---
Preferences preferences;

/* =========================================================
   CONFIGURACIÓN Y PINES
   ========================================================= */
const int VOLTAGE_PIN   = 36; 
const int THC_UP_PIN    = 25; 
const int THC_DOWN_PIN  = 26; 
const int ARCOK_OUT_PIN = 27; 
const int EMERGENCY_PIN = 32; 

// --- ZONAS DE SEGURIDAD ---
const float V_ZONE_A_MAX = 60.0f;   
const float V_ZONE_C_MIN = 185.0f;  
const float V_ZONE_D_MIN = 280.0f;  
const uint32_t ZONE_C_TIMEOUT = 300; 

const bool USE_ARC_IN   = false;
const int  ARC_IN_PIN   = 34;
const float K_F         = 13.55f; 
const bool OUTPUT_ACTIVE_HIGH = true;

float ARC_SCALE  = 125.0f / 3.3f; 
float ARC_OFFSET = 0.0f;

const float    V_ON     = 22.0f;
const float    V_OFF    = 17.0f;
const uint32_t HOLD_MS  = 150; 

// --- PARÁMETROS CONTROL ---
const uint32_t MIN_PULSE_MS = 600; //60
const uint32_t MIN_GAP_MS   = 400; //40
const float    K_T_MS_PER_V = 500.0f; //50 -----
const uint32_t BURST_MIN_MS = 800; //80 ----
const uint32_t BURST_MAX_MS = 3500; //350 -----
const uint32_t BURST_GAP_MS = 800; //80 -----

/* =========================================================
   VARIABLES GLOBALES (Externas)
   ========================================================= */
extern float targetV;
extern bool  targetSet;
extern float deadband;
extern Mode currentMode;

extern float     vFilt;
extern bool      arcOK;
extern bool      upCmd;
extern bool      downCmd;
extern String    estadoTHCStr;

extern ExtractorData extractorState;
extern WebSocketsServer webSocket;
extern StaticJsonDocument<768> doc;

// --- VARIABLES DE SIMULACIÓN ---
static bool  simModeActive = true; 
static float simVoltage    = 0.0f; 

void setSimMode(bool active) { simModeActive = active; }
void setSimVoltage(float v)   { 
    simVoltage = v; 
    simModeActive = true;
}

// =========================================================
// VARIABLES INTERNAS (MOVIDAS AQUÍ ARRIBA PARA EVITAR ERRORES)
// =========================================================

// 1. Seguridad
static unsigned long safetyTimerStart = 0;
static bool latchedError = false;
static bool hmiEmergencyActive = false;  // Memoria del botón de la pantalla
static bool externalFallaActiva = false; // Memoria de fallas del PCB2

// 2. Extractor (Usando TUS nombres originales)
static unsigned long extractionOffTimer = 0; // Cronómetro para apagado
const unsigned long EXTRACTION_DELAY_MS = 15000; // 15 Segundos de purga
static bool manualFanOverride = false; // ¿El usuario lo prendió manual?

// =========================================================

// --- Función Manual Fan (Definida antes de usarse) ---
void toggleManualFan() {
    manualFanOverride = !manualFanOverride; 
}

// --- GETTERS DE DIAGNÓSTICO (para HMI/CSV) ---
bool thcIsManualFanOn() {
    return manualFanOverride;
}

bool thcIsPurging() {
    // Reproduce la misma lógica que usas en telemetría
    bool timerActive = (millis() - extractionOffTimer < EXTRACTION_DELAY_MS);
    bool isPurging = (!arcOK && !manualFanOverride && timerActive);
    return isPurging;
}


// --- PARTE 2: Tabla de Voltaje ---
struct Setting { float mm, v; };
static Setting table[] = { 
  {1.0f,105.0f},{2.0f,110.0f},{3.0f,114.0f},{4.0f,118.0f},{5.0f,121.0f}
};
const int TABLE_N = sizeof(table)/sizeof(table[0]);

static float getTargetFromTable(float mm) {
  for (int i=0;i<TABLE_N;i++)
    if (fabsf(table[i].mm - mm) < 0.001f) return table[i].v;
  return -1.0f;
}

bool setTargetByThickness(float mm){
  float v = getTargetFromTable(mm);
  if (v < 0.0f) return false;
  targetV = v; targetSet = true; currentMode = MODE_TABLE;
  return true;
}

bool setTargetByVoltage(float v){
  if (v < 50.0f || v > 300.0f) return false; 
  targetV = v; targetSet = true; currentMode = MODE_MANUAL;
  return true;
}

// --- PARTE 3: Lectura ---
const float ALPHA = 0.20f;
const unsigned long PULSE_TIMEOUT_US = 25000UL;

void resetAlarm() {
  latchedError = false;       
  hmiEmergencyActive = false;   // Apagar alarma HMI
  externalFallaActiva = false;  // Apagar alarma PCB2
  safetyTimerStart = 0;       
  if (digitalRead(EMERGENCY_PIN) == LOW) {
      estadoTHCStr = "LISTO (RESETEADO)";
  }
  cmdResetAlarmas(); 
}

static inline float readArcOnce() {
  unsigned long hi = pulseIn(VOLTAGE_PIN, HIGH, PULSE_TIMEOUT_US);
  unsigned long lo = pulseIn(VOLTAGE_PIN, LOW,  PULSE_TIMEOUT_US);
  if (hi == 0 || lo == 0) return 0.0f; 
  float period_us = (float)(hi + lo);
  if (period_us <= 0.0f) return vFilt; 
  float freq = 1000000.0f / period_us;
  return freq / K_F;
}

static void initFilter() {
  float s = 0; int n = 5;
  for (int i=0;i<n;i++) { s += readArcOnce(); delay(10); }
  vFilt = s / n; 
}

static float readArcVoltageFiltered() {
  if (simModeActive) {
      // 1. Pequeña pausa para simular el tiempo de lectura del hardware físico
      delay(5); 

      // 2. ¡EL CAMBIO CLAVE! Aplicamos el filtro EMA a la señal que inyecta Python
      vFilt += 0.01f * (simVoltage - vFilt); 

      if (vFilt > 20.0f) arcOK = true; 
      else arcOK = false;
      
      return vFilt;
  }
  //comportamiento real
  const int N = 3;
  float acc = 0;
  for (int i=0;i<N;i++) acc += readArcOnce();
  float v = acc / N;
  vFilt += ALPHA * (v - vFilt); 
  return vFilt;
}

// --- PARTE 4: Arco ---
static uint32_t edgeTs_arc = 0;
static void resetArcDetector() { arcOK = false; edgeTs_arc = 0; }

static bool updateArcStatus(float v) {
  uint32_t now = millis();
  if (!arcOK) {
    if (v > V_ON) {
      if (edgeTs_arc == 0) edgeTs_arc = now;
      if (now - edgeTs_arc >= HOLD_MS) { arcOK = true; edgeTs_arc = 0; }
    } else {
      edgeTs_arc = 0;
    }
  } else {
    if (v < V_OFF) {
      if (edgeTs_arc == 0) edgeTs_arc = now;
      if (now - edgeTs_arc >= HOLD_MS) { arcOK = false; edgeTs_arc = 0; }
    } else {
      edgeTs_arc = 0;
    }
  }
  if (USE_ARC_IN) {
    if (digitalRead(ARC_IN_PIN) == HIGH) arcOK = true;
  }
  return arcOK; 
}

// --- PARTE 5: Control THC ---
static bool     inBurst = false, inGap = false;
static bool     dirUp   = false;
static uint32_t burstEndTs = 0, gapEndTs = 0;



static void resetTHCControl() {
  upCmd = downCmd = false; 
  inBurst = inGap = false;
  burstEndTs = gapEndTs = 0;
}

static void thcControlStep(float v) {
  uint32_t now = millis();
  
  if (!targetSet || !arcOK) {
    inBurst = inGap = false;
    upCmd = downCmd = false;
    estadoTHCStr = arcOK ? "SIN SETPOINT" : "ARCO APAGADO";
    return;
  }

  float error  = targetV - v;
  float absErr = fabsf(error);

  if (absErr <= deadband) {
    if (inBurst) {
      inBurst = false;
      inGap   = true; 
      gapEndTs = now + BURST_GAP_MS;
    }
    upCmd = downCmd = false;
    estadoTHCStr = "OK (BANDA MUERTA)";
    return;
  }

  if (inBurst) {
    if (now >= burstEndTs) {
      upCmd = downCmd = false;
      inBurst = false;
      inGap   = true;
      gapEndTs = now + BURST_GAP_MS;
    }
    return; 
  }

  if (inGap) {
    if (now >= gapEndTs) inGap = false;
    else return; 
  }

  uint32_t durMs = (uint32_t)(K_T_MS_PER_V * absErr);
  if (durMs < BURST_MIN_MS) durMs = BURST_MIN_MS;
  if (durMs > BURST_MAX_MS) durMs = BURST_MAX_MS;

  if (error > 0) {
    dirUp = true;
    upCmd = true; downCmd = false;
    estadoTHCStr = "SUBIENDO"; 
  } else {
    dirUp = false;
    upCmd = false; downCmd = true;
    estadoTHCStr = "BAJANDO"; 
  }
  inBurst    = true;
  burstEndTs = now + durMs;
}

// --- PARTE 6: Salidas ---
static inline void writePin(int pin, bool on) {
  digitalWrite(pin, OUTPUT_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
}
static void setThcUp(bool on)   { writePin(THC_UP_PIN,    on); }
static void setThcDown(bool on) { writePin(THC_DOWN_PIN,  on); }
static void setArcOk(bool on)   { writePin(ARCOK_OUT_PIN, on); }

// --- SAFETY ---

// --- SAFETY ---
static bool checkSafety(float v) {
  
  // 1. EMERGENCIA GENERAL O DESDE PANTALLA HMI
  if (digitalRead(EMERGENCY_PIN) == HIGH || hmiEmergencyActive) { 
    estadoTHCStr = "¡PARADA POR HMI/EMG!";
    arcOK = false; 
    return false;  
  }

  // 2. FALLO DE COMUNICACIÓN MODBUS (Cable roto o Esclavo apagado)
  if (!extractorState.commsOK) {
    estadoTHCStr = "FALLA COM. PCB2";
    return false; // Bloquea motores inmediatamente
  }

  // 3. EMERGENCIAS FÍSICAS EN EL EXTRACTOR (Botón o Motor)
  if (extractorState.fallaPE) {
      externalFallaActiva = true;
      estadoTHCStr = "EMERGENCIA FÍSICA PCB2";
  } else if (extractorState.fallaTrip) {
      externalFallaActiva = true;
      estadoTHCStr = "TRIP MOTOR EXTRACTOR";
  }
  
  // Si hubo falla física, bloquea y pide reset
  if (externalFallaActiva) {
      if (!extractorState.fallaPE && !extractorState.fallaTrip) {
         estadoTHCStr = "PARADA EXT (REQUIERE RESET)";
      }
      return false;
  }

  // 4. PROTECCIÓN DEL PLASMA (El verdadero Alto Voltaje asegurado)
  if (latchedError) {
    estadoTHCStr = "FALLA: VOLTAJE ALTO";
    return false; 
  }
  
  // 5. EVALUACIÓN DE VOLTAJE EN TIEMPO REAL (Anti-Dive y Arco Abierto)
  if (v >= V_ZONE_D_MIN) {
    estadoTHCStr = "ARCO ABIERTO (OFF)";
    arcOK = false; 
    safetyTimerStart = 0; 
    return false; 
  } 

  if (v >= V_ZONE_C_MIN && v < V_ZONE_D_MIN) {
    if (safetyTimerStart == 0) safetyTimerStart = millis();
    if (millis() - safetyTimerStart > ZONE_C_TIMEOUT) {
      latchedError = true; 
      estadoTHCStr = "FALLA: VOLTAJE ALTO";
      return false; 
    }
  } else {
    safetyTimerStart = 0;
  }
  
  return true; 
}



// --- SETUP ---
void setupTHC() {
  Serial.println("Configurando pines del THC... (thc_logic.cpp)");
  preferences.begin("thc_config", false); 
  deadband = preferences.getFloat("deadband", 2.0f); 
  float lastV = preferences.getFloat("targetV", 0.0f); 
  if (lastV >= 50.0f && lastV <= 300.0f) {
      setTargetByVoltage(lastV);
  }
  Serial.printf("Config cargada: Deadband=%.1fV, Setpoint=%.1fV\n", deadband, targetV);

  pinMode(THC_UP_PIN,    OUTPUT);
  pinMode(THC_DOWN_PIN,  OUTPUT);
  pinMode(ARCOK_OUT_PIN, OUTPUT);
  pinMode(EMERGENCY_PIN, INPUT_PULLUP); 
  if (USE_ARC_IN) pinMode(ARC_IN_PIN, INPUT);
  
  setThcUp(false); setThcDown(false); setArcOk(false);
  pinMode(VOLTAGE_PIN, INPUT); 
  
  initFilter();
  resetArcDetector();
  resetTHCControl();

  // ✅ FIX: al arrancar, NO queremos purga activa
  // Hacemos que el "timer" arranque como ya vencido
  extractionOffTimer = millis() - EXTRACTION_DELAY_MS;

  // ✅ FIX: asegurar que no quede override manual al boot
  manualFanOverride = false;

  // ✅ FIX: mandar OFF al extractor de entrada
  cmdMotorExtractor(false);

  Serial.println("THC listo.");
}

static ExtractorData lastExtState; 
static float lastTargetV = 0.0f;
static bool lastManFan = false; 
static bool lastMotorState = false;
static unsigned long lastSend = 0;

// --- LOOP PRINCIPAL ---
void loopTHC() {
  float v = readArcVoltageFiltered(); 
  bool arc = updateArcStatus(v);      
  bool isSafe = checkSafety(v);

  // --- CONTROL INTELIGENTE DEL EXTRACTOR ---
  
  // 1. Reset timer si hay arco
  if (arcOK) {
      extractionOffTimer = millis(); // <--- NOMBRE CORREGIDO
  }

  // 2. Calcular si el timer sigue activo
  // (Usando TUS nombres de variable)
  bool timerActive = (millis() - extractionOffTimer < EXTRACTION_DELAY_MS);

  // 3. Lógica OR
  bool motorState = arcOK || timerActive || manualFanOverride;

  // 4. Seguridad
 // 4. Seguridad y Paradas de Emergencia (Bloqueo absoluto)
  if (latchedError || 
      digitalRead(EMERGENCY_PIN) == HIGH || 
      hmiEmergencyActive || 
      !extractorState.commsOK || 
      extractorState.fallaPE || 
      extractorState.fallaTrip) {
      
      // Cortamos el motor sin importar timers o modos manuales
      motorState = false;
      manualFanOverride = false;
      
      // Truco extra: "Matamos" el timer de purga enviándolo al pasado
      extractionOffTimer = millis() - EXTRACTION_DELAY_MS;
  }

  cmdMotorExtractor(motorState);

  // --- TOMA DE DECISIONES ---
  if (!isSafe) {
    upCmd = false;
    downCmd = false;
    resetTHCControl(); 
  } 
  else {
    thcControlStep(v); 
  }

  // --- SALIDAS ---
  setArcOk(arcOK); 
  if (!targetSet || !arcOK || !isSafe) { 
    setThcUp(false);
    setThcDown(false);
  } else {
    bool up   = upCmd;
    bool down = downCmd;
    if (up && down) { up = false; down = false; } 
    setThcUp(up);
    setThcDown(down);
  }

  // --- TELEMETRÍA ---
  bool huboCambioExtractor = 
      (extractorState.zona1 != lastExtState.zona1) ||
      (extractorState.zona2 != lastExtState.zona2) ||
      (extractorState.zona3 != lastExtState.zona3) ||
      (extractorState.zona4 != lastExtState.zona4) ||
      (extractorState.motorPrendido != lastExtState.motorPrendido) ||
      (extractorState.fallaPE != lastExtState.fallaPE) ||
      (extractorState.fallaTrip != lastExtState.fallaTrip);
  
  bool huboCambioSetpoint = (fabs(targetV - lastTargetV) > 0.1f);

  bool huboCambioManual   = (manualFanOverride != lastManFan); // <--- DETECTAR CAMBIO
  bool huboCambioMotor    = (motorState != lastMotorState);
  bool tiempoNormal = (millis() - lastSend > 250);
    
  bool cambioUrgente = ((huboCambioExtractor || huboCambioSetpoint || huboCambioManual || huboCambioMotor) && (millis() - lastSend > 15));

  if (tiempoNormal || cambioUrgente) {
    
    lastSend = millis();
    lastExtState = extractorState; 
    lastTargetV = targetV;
    lastManFan = manualFanOverride; // <--- AQUÍ ACTUALIZAMOS LA MEMORIA
    lastMotorState = motorState;


    doc.clear();
    
    doc["voltaje_arco"] = (int)(vFilt * 10) / 10.0;
    doc["voltaje_setpoint"] = targetV;
    doc["deadband"] = deadband;
    doc["estado_thc"] = estadoTHCStr;
    doc["arc_ok"] = arcOK;
    doc["up"] = upCmd;
    doc["down"] = downCmd;
    doc["z1"] = extractorState.zona1;
    doc["z2"] = extractorState.zona2;
    doc["z3"] = extractorState.zona3;
    doc["z4"] = extractorState.zona4;
    doc["motor"] = motorState; 
    doc["man_fan"] = manualFanOverride;

    bool isPurging = (!arcOK && !manualFanOverride && timerActive);
    doc["purging"] = isPurging; 
    doc["err_ext"] = (extractorState.fallaPE || extractorState.fallaTrip || !extractorState.commsOK);
    
    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);
    

  }
}

// --- Función de PARADA TOTAL --
void forceStopFromHMI() {
  Serial.println("!!! PARADA DE EMERGENCIA HMI ACTIVADA !!!");
  hmiEmergencyActive = true; // Activa su propia bandera, no la de voltaje        
  estadoTHCStr = "¡PARADA POR HMI!"; 
  upCmd = false;
  downCmd = false;
  arcOK = false; 
  cmdMotorExtractor(false); 
}