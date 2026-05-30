//--------------------------------------------------------------
//ESP32
//--------------------------------------------------------------
/*
#include "modbus_slave.h"

// Instancia de ModbusRTU
ModbusRTU mb;

void setupModbus() {
    Serial.println("Configurando Modbus Esclavo ID: 2...");
    
    // 1. Iniciar Serial2 (Hardware Serial)
    Serial2.begin(RS485_BAUD, SERIAL_8N1, PIN_RX2, PIN_TX2);
    
    // 2. Iniciar Modbus en Serial2 con el pin de control DE/RE
    mb.begin(&Serial2, PIN_DERE);
    
    // 3. Configurar como Esclavo
    mb.slave(SLAVE_ID);
    
    // 4. Definir las "Cajas" de memoria (Add Coils y Ists)
    // Esto crea el espacio para que el Maestro lea/escriba
    mb.addCoil(COIL_MOTOR,  false); // 00001
    mb.addCoil(COIL_RESET,  false); // 00002
    
    mb.addIsts(ISTS_ZONA1,  false); // 10001
    mb.addIsts(ISTS_ZONA2,  false); // 10002
    mb.addIsts(ISTS_ZONA3,  false); // 10003
    mb.addIsts(ISTS_ZONA4,  false); // 10004
    mb.addIsts(ISTS_ALM_PE, false); // 10005
    mb.addIsts(ISTS_ALM_TRIP, false); // 10006
}

void updateModbus() {
    // 1. Tarea principal de Modbus (Procesar mensajes)
    mb.task();
    
    // 2. ACTUALIZAR MAPA MODBUS (Hardware -> Modbus)
    // Escribimos el estado de tus sensores en los registros "Ists"
    mb.Ists(ISTS_ZONA1, systemState.sensZ1);
    mb.Ists(ISTS_ZONA2, systemState.sensZ2);
    mb.Ists(ISTS_ZONA3, systemState.sensZ3);
    mb.Ists(ISTS_ZONA4, systemState.sensZ4);
    
    mb.Ists(ISTS_ALM_PE,   systemState.peRemoto);
    mb.Ists(ISTS_ALM_TRIP, systemState.trip);

    // 3. LEER COMANDOS DEL MAESTRO (Modbus -> Hardware)
    // Leemos el Coil 0 para saber si el Maestro quiere prender el motor
    systemState.cmdMotor = mb.Coil(COIL_MOTOR);
    
    // Leemos el Coil 1 para ver si piden Reset
    systemState.cmdReset = mb.Coil(COIL_RESET);
    
    // Auto-limpieza del Reset (para que no quede pegado en true)
    if (systemState.cmdReset) {
        mb.Coil(COIL_RESET, false);
    }
    
    // Muestra de actividad en Serial (Opcional, para depurar)
    // Solo si el motor cambia de estado
    static bool lastMotor = false;
    if (systemState.cmdMotor != lastMotor) {
        Serial.printf("Comando Modbus Motor: %s\n", systemState.cmdMotor ? "ON" : "OFF");
        lastMotor = systemState.cmdMotor;
    }
}
*/



//--------------------------------------------------------------
// ARDUINO NANO (MODO HARDWARE SERIAL)
//--------------------------------------------------------------
#include "modbus_slave.h"

// ELIMINADO: SoftwareSerial softSerial(...)
// Usaremos el puerto Serial nativo (Serial) que es mucho más robusto.

// Instancia de ModbusRTU
ModbusRTU mb;

void setupModbus() {
    // 1. Iniciar Hardware Serial (Pines 0 y 1)
    // Importante: Al subir código, desconectar cables de pines 0 y 1
    Serial.begin(RS485_BAUD); 
    
    // 2. Iniciar Modbus pasando el Serial NATIVO (&Serial)
    mb.begin(&Serial, PIN_DERE);
    
    // 3. Configurar como Esclavo ID 2
    mb.slave(SLAVE_ID);
    
    // 4. Definir las "Cajas" de memoria
    mb.addCoil(COIL_MOTOR,  false); 
    mb.addCoil(COIL_RESET,  false); 
    
    mb.addIsts(ISTS_ZONA1,  false); 
    mb.addIsts(ISTS_ZONA2,  false); 
    mb.addIsts(ISTS_ZONA3,  false); 
    mb.addIsts(ISTS_ZONA4,  false); 
    mb.addIsts(ISTS_ALM_PE, false); 
    mb.addIsts(ISTS_ALM_TRIP, false); 
}

void updateModbus() {
  
    mb.task();
       
    mb.Ists(ISTS_ZONA1, systemState.sensZ1);
    mb.Ists(ISTS_ZONA2, systemState.sensZ2);
    mb.Ists(ISTS_ZONA3, systemState.sensZ3);
    mb.Ists(ISTS_ZONA4, systemState.sensZ4);
    mb.Ists(ISTS_ALM_PE,   systemState.peRemoto);
    mb.Ists(ISTS_ALM_TRIP, systemState.trip);

    systemState.cmdMotor = mb.Coil(COIL_MOTOR);
    systemState.cmdReset = mb.Coil(COIL_RESET);
    
    if (systemState.cmdReset) {
        mb.Coil(COIL_RESET, false);
    }
}




