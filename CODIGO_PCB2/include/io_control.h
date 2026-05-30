#pragma once
#include <Arduino.h>

//ESP32 PINES
// --- DEFINICIÓN DE PINES (Según tu diagrama ESP32) ---
/*
// ENTRADAS (Sensores y Seguridad)
#define PIN_SENS_Z1   36 // VP - Inductivo Zona 1
#define PIN_SENS_Z2   39 // VN - Inductivo Zona 2
#define PIN_SENS_Z3   34 // Inductivo Zona 3
#define PIN_SENS_Z4   35 // Inductivo Zona 4
#define PIN_PE_REMOTO 32 // Botón Emergencia (0=OK, 1=FALLA)
#define PIN_TRIP      33 // Guardamotor (1=OK, 0=FALLA)

// SALIDAS (Actuadores)
// Nota: Tu hardware activa con 0 (LOW)
#define PIN_VALV_1    25 
#define PIN_VALV_2    26
#define PIN_VALV_3    27
#define PIN_VALV_4    14
#define PIN_MOTOR     13
*/


//PINES ARDUINO NANO
// ENTRADAS (Sensores y Seguridad)
#define PIN_SENS_Z1   5  // D5
#define PIN_SENS_Z2   6  // D6
#define PIN_SENS_Z3   7  // D7
#define PIN_SENS_Z4   8  // D8
#define PIN_PE_REMOTO 9  // D9 (Botón Emergencia)
#define PIN_TRIP      10 // D10 (Guardamotor)

// SALIDAS (Actuadores)
// Nota: Tu hardware activa con 0 (LOW)
#define PIN_VALV_1    11 // D11
#define PIN_VALV_2    12 // D12
#define PIN_VALV_3    13 // D13 (Cuidado con el LED integrado)
#define PIN_VALV_4    A0 // Pin A0 usado como Digital
#define PIN_MOTOR     A1 // Pin A1 usado como Digital

// --- ESTRUCTURA DE ESTADO GLOBAL ---
// Usaremos esto para compartir datos entre el Hardware y Modbus
struct PCB2State {
    // Entradas (Lo que leemos)
    bool sensZ1;
    bool sensZ2;
    bool sensZ3;
    bool sensZ4;
    bool peRemoto; // true = Falla
    bool trip;     // true = Falla
    bool enFalla;  // Resumen: ¿Hay alguna falla activa?

    // Salidas (Lo que controlamos)
    bool valv1;
    bool valv2;
    bool valv3;
    bool valv4;
    bool motor;
    
    // Comandos desde Maestro
    bool cmdMotor; // El Maestro pide prender motor
    bool cmdReset; // El Maestro pide resetear fallas
};

// Variable global externa (vivirá en main.cpp)
extern PCB2State systemState;

// --- FUNCIONES ---
void setupIO();  // Configurar pines
void updateIO(); // Leer sensores y decidir estados