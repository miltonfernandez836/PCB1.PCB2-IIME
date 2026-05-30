#include <Arduino.h>
#include "io_control.h"
#include "modbus_slave.h"

// --- VARIABLE GLOBAL DE ESTADO ---
// Aquí vive la instancia real de la estructura que definimos en io_control.h
PCB2State systemState;

void setup() {
    // 1. Inicializar Serial Monitor (para debug)
    //Serial.begin(115200);
    //delay(100);
    //Serial.println("\n[MAIN] Iniciando PCB2 - ESP32")

    // 2. Inicializar Hardware (Pines)
    setupIO();

    // 3. Inicializar Comunicaciones (RS485)
    setupModbus();
    
}

void loop() {
    // 1. Tarea de Hardware (Rápida)
    // Lee sensores, controla válvulas locales y verifica seguridad
    updateIO();

    // 2. Tarea de Comunicación
    // Atiende al Maestro y sincroniza la estructura systemState
    updateModbus();

    // (Opcional) Pequeño delay para estabilidad si fuera necesario,
    // pero al ser Modbus RTU es mejor dejarlo correr libre.
}