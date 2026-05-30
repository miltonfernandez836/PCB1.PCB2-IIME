#pragma once
#include <Arduino.h>
#include <ModbusMaster.h>

// --- CONFIGURACIÓN DE PINES (ESP32) ---
// Usaremos Serial2
#define MB_RX        16
#define MB_TX        17
#define MB_DE_RE     14 

// --- DATOS DEL ESCLAVO (PCB2) ---
#define SLAVE_ID     2
#define ADDR_INPUTS  0  // Donde empiezan los sensores (10001)
#define ADDR_COILS   0  // Donde empiezan las salidas (00001)

// --- ESTRUCTURA DE DATOS ---
// Aquí guardaremos lo que nos diga el PCB2
struct ExtractorData {
    bool zona1;
    bool zona2;
    bool zona3;
    bool zona4;
    bool fallaPE;
    bool fallaTrip;
    
    // Estado del motor (Confirmación)
    bool motorPrendido; 
    bool commsOK = true; ///comunicacion
};

// Variable Global (Para que el WebServer la pueda leer)
extern ExtractorData extractorState;

// --- FUNCIONES ---
void setupModbusMaster();
void loopModbusMaster();

// Funciones para controlar el sistema desde el THC
void cmdMotorExtractor(bool encender); // TRUE = Prender, FALSE = Apagar
void cmdResetAlarmas(); // Manda pulso de reset