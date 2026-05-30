//--------------------------------------------------------------
//ESP32
//--------------------------------------------------------------
/*
#pragma once
#include <Arduino.h>
#include <ModbusRTU.h> // Nueva librería
#include "io_control.h"

// --- CONFIGURACIÓN RS485 
#define SLAVE_ID 2
#define RS485_BAUD 9600

//Pines Serial 2  (Según tu Mapa Completo)

#define PIN_RX2  16
#define PIN_TX2  17
#define PIN_DERE 4  // <--- CORREGIDO: Usamos GPIO 4 para DE/RE (No el 14)

// --- DIRECCIONES MODBUS (Offsets) ---
// Coils (0xxxx) - Salidas
#define COIL_MOTOR   0  // Dirección 00001
#define COIL_RESET   1  // Dirección 00002

// Discrete Inputs (1xxxx) - Entradas
#define ISTS_ZONA1    0  // Dirección 10001
#define ISTS_ZONA2    1  // Dirección 10002
#define ISTS_ZONA3    2  // Dirección 10003
#define ISTS_ZONA4    3  // Dirección 10004
#define ISTS_ALM_PE   4  // Dirección 10005
#define ISTS_ALM_TRIP 5  // Dirección 10006

// --- FUNCIONES ---
void setupModbus();
void updateModbus();
*/


//--------------------------------------------------------------
// ARDUINO NANO (MODO HARDWARE SERIAL)
//--------------------------------------------------------------

#pragma once
#include <Arduino.h>
#include <ModbusRTU.h>
// #include <SoftwareSerial.h>  <-- ELIMINADO: Usaremos Hardware Serial (Pines 0 y 1)
#include "io_control.h"

// --- CONFIGURACIÓN RS485 (NANO) ---
#define SLAVE_ID 2
#define RS485_BAUD 115200  // La velocidad blindada

// Pines
// RX y TX son los pines 0 y 1 del Nano (No se definen, son nativos)
#define PIN_DERE     4   // Conectar a DE/RE del MAX485

// --- DIRECCIONES MODBUS (Igual que antes) ---
#define COIL_MOTOR   0 
#define COIL_RESET   1 

#define ISTS_ZONA1    0 
#define ISTS_ZONA2    1 
#define ISTS_ZONA3    2 
#define ISTS_ZONA4    3 
#define ISTS_ALM_PE   4 
#define ISTS_ALM_TRIP 5 

// --- FUNCIONES ---
void setupModbus();
void updateModbus();


