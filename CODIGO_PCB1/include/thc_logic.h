#pragma once

// ¡NUEVO! Definición del 'enum' aquí
// Ahora todos los archivos que incluyan "thc_logic.h" 
// sabrán qué es "Mode", "MODE_TABLE", etc.
enum Mode { MODE_NONE, MODE_TABLE, MODE_MANUAL };


// Funciones del THC que llamaremos desde otros archivos
void setupTHC();
void loopTHC();

// Funciones de control que el WebServer necesita llamar
bool setTargetByThickness(float mm);
bool setTargetByVoltage(float v);

void resetAlarm();
//
void setSimMode(bool active);
void setSimVoltage(float v);
void forceStopFromHMI();
void toggleManualFan();

// --- DIAGNÓSTICO PARA HMI/LOG ---
bool thcIsManualFanOn();
bool thcIsPurging();


