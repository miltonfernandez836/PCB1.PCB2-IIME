#include "modbus_master.h"

ModbusMaster node;
ExtractorData extractorState;

// Variables internas
bool targetMotorState = false; 
bool lastSentMotorState = true; // Para saber si ya enviamos la orden
bool resetRequested = false;   
unsigned long lastPoll = 0;
int errorCount = 0;

// --- CONTROL RS485 ---
void preTransmission() {
  digitalWrite(MB_DE_RE, HIGH); 
  delayMicroseconds(500);       
}

void postTransmission() {
  Serial2.flush(); 
  delayMicroseconds(500);
  digitalWrite(MB_DE_RE, LOW);  
  delay(2); 
  while(Serial2.available()) Serial2.read(); // Limpiar buffer
}

// --- SETUP ---
void setupModbusMaster() {
    Serial.println("Configurando Modbus Maestro (Optimizado)...");
    // Mantén 115200 si ya te funcionó
    Serial2.begin(115200, SERIAL_8N1, MB_RX, MB_TX);
    
    pinMode(MB_DE_RE, OUTPUT);
    digitalWrite(MB_DE_RE, LOW);
    
    node.begin(SLAVE_ID, Serial2);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
}

// --- MANDOS ---
void cmdMotorExtractor(bool encender) {
    targetMotorState = encender;
    // No enviamos aquí, el loop lo detectará
}

void cmdResetAlarmas() {
    resetRequested = true;
}


// --- BUCLE OPTIMIZADO Y TOLERANTE A RUIDO ---
void loopModbusMaster() {
    // Si hay error, espera 100ms. Si no, ¡corre cada 5ms!
    int intervalo = (errorCount > 0) ? 100 : 5;
    
    static bool commsErrorPrinted = false; 
    static bool forceReadNext = false; // Alternador para no atascar el bus
    
    if (millis() - lastPoll > intervalo) {
        lastPoll = millis();
        uint8_t result = 0;

        // ¿El usuario quiere prender/apagar el motor o resetear?
        bool hayPendiente = (targetMotorState != lastSentMotorState || resetRequested);

        // --- PRIORIDAD 1: ESCRIBIR (Si hay algo pendiente y nos toca) ---
        if (hayPendiente && !forceReadNext) {
            
            if (resetRequested) {
                result = node.writeSingleCoil(ADDR_COILS + 1, true);
                resetRequested = false; 
            } 
            else {
                result = node.writeSingleCoil(ADDR_COILS + 0, targetMotorState);
                if (result == node.ku8MBSuccess) {
                    lastSentMotorState = targetMotorState; // ¡Éxito! Guardamos el estado
                    extractorState.motorPrendido = targetMotorState; 
                }
            }

            if (result == node.ku8MBSuccess) {
                errorCount = 0;
                extractorState.commsOK = true;
                if (commsErrorPrinted) { Serial.println("✅ Modbus: Conexión PCB2 Restablecida (Escritura)."); commsErrorPrinted = false; }
            } else {
                errorCount++;
                forceReadNext = true; // Falló por ruido. En el siguiente ciclo forzamos lectura para no asfixiar el bus.
                
                // TOLERANCIA AL RUIDO: 15 intentos (1.5 segundos) antes de declarar cable roto
                if (errorCount > 15) { 
                    extractorState.commsOK = false;
                    if (!commsErrorPrinted) { Serial.print("⚠️ Modbus Error (Escritura): 0x"); Serial.println(result, HEX); commsErrorPrinted = true; }
                }
            }
        }
        // --- PRIORIDAD 2: LEER SENSORES ---
        else {
            result = node.readDiscreteInputs(ADDR_INPUTS, 6);
            
            if (result == node.ku8MBSuccess) {
                errorCount = 0;
                extractorState.commsOK = true; 
                if (commsErrorPrinted) { Serial.println("✅ Modbus: Conexión PCB2 Restablecida (Lectura)."); commsErrorPrinted = false; }

                uint8_t datos = node.getResponseBuffer(0);
                extractorState.zona1     = (datos >> 0) & 1;
                extractorState.zona2     = (datos >> 1) & 1;
                extractorState.zona3     = (datos >> 2) & 1;
                extractorState.zona4     = (datos >> 3) & 1;
                extractorState.fallaPE   = (datos >> 4) & 1;
                extractorState.fallaTrip = (datos >> 5) & 1;
            } else {
                errorCount++;
                if (errorCount > 15) { // 1.5 segundos de tolerancia
                    extractorState.commsOK = false;
                    if (!commsErrorPrinted) { Serial.print("⚠️ Modbus Error (Lectura): 0x"); Serial.println(result, HEX); commsErrorPrinted = true; }
                }
            }
            
            // Si leímos los sensores y había una escritura pendiente, liberamos el alternador
            // para que en el próximo ciclo (en 5ms) vuelva a intentar escribir.
            if (hayPendiente) forceReadNext = false;
        }
    }
}



