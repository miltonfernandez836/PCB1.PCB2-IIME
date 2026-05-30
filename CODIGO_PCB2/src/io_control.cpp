#include "io_control.h"

// --- VARIABLES PARA ANTI-REBOTE (DEBOUNCE) ---
// 1. Tiempo del último cambio detectado
static unsigned long lastDebounceTime[4] = {0, 0, 0, 0}; 

// 2. Última lectura CRUDA (con ruido)
static bool lastRawReading[4] = {false, false, false, false};

// 3. Último estado ESTABLE (Filtrado) - ¡Esto faltaba!
static bool estadoEstable[4] = {false, false, false, false};

const int DEBOUNCE_DELAY = 20; //


void setupIO() {
    // 1. Configurar Entradas
    pinMode(PIN_SENS_Z1, INPUT_PULLUP);
    pinMode(PIN_SENS_Z2, INPUT_PULLUP);
    pinMode(PIN_SENS_Z3, INPUT_PULLUP);
    pinMode(PIN_SENS_Z4, INPUT_PULLUP);
    
    pinMode(PIN_PE_REMOTO, INPUT_PULLUP); 
    pinMode(PIN_TRIP, INPUT_PULLUP);      

    // 2. Configurar Salidas
    digitalWrite(PIN_VALV_1, HIGH);
    digitalWrite(PIN_VALV_2, HIGH);
    digitalWrite(PIN_VALV_3, HIGH);
    digitalWrite(PIN_VALV_4, HIGH);
    digitalWrite(PIN_MOTOR,  HIGH);

    pinMode(PIN_VALV_1, OUTPUT);
    pinMode(PIN_VALV_2, OUTPUT);
    pinMode(PIN_VALV_3, OUTPUT);
    pinMode(PIN_VALV_4, OUTPUT);
    pinMode(PIN_MOTOR,  OUTPUT);
}

// --- FUNCIÓN DE FILTRADO CORREGIDA ---
// Lee el pin, filtra el ruido y devuelve true/false limpio
bool leerLimpio(int pin, int indice) {
    // 1. Leer el pin físico (Invertido porque usas INPUT_PULLUP/NPN)
    bool lecturaActual = !digitalRead(pin); 
    
    // 2. Si la lectura cambió respecto a la última vez (sea ruido o real)...
    
    if (lecturaActual != lastRawReading[indice]) {
        lastDebounceTime[indice] = millis(); 
    }
    
    // 3. Guardamos esta lectura cruda para la próxima comparación
    lastRawReading[indice] = lecturaActual;
    
    // 4. Si ha pasado suficiente tiempo desde el último cambio...
    if ((millis() - lastDebounceTime[indice]) > DEBOUNCE_DELAY) {
        // Significa que la señal es estable.
        // Si es diferente a lo que teníamos guardado como "Estable", actualizamos.
        if (lecturaActual != estadoEstable[indice]) {
            estadoEstable[indice] = lecturaActual;
        }
    }
    
    // 5. Devolvemos el valor estable (no el crudo)
    return estadoEstable[indice]; 
}

void updateIO() {
    // --- 1. LEER ENTRADAS USANDO EL FILTRO ---
    // AHORA SÍ usamos leerLimpio con su índice correspondiente (0 a 3)
    
    systemState.sensZ1 = leerLimpio(PIN_SENS_Z1, 0);
    systemState.sensZ2 = leerLimpio(PIN_SENS_Z2, 1);
    systemState.sensZ3 = leerLimpio(PIN_SENS_Z3, 2);
    systemState.sensZ4 = leerLimpio(PIN_SENS_Z4, 3);

    // Leer Seguridad (Estas suelen ser más firmes, pero podrias filtrarlas también si quieres)
    // Por ahora las dejamos directas para respuesta inmediata de emergencia
    systemState.peRemoto = (digitalRead(PIN_PE_REMOTO) == HIGH);
    systemState.trip     = (digitalRead(PIN_TRIP) == LOW);

    // Resumen de fallas
    if (systemState.peRemoto || systemState.trip) {
        systemState.enFalla = true;
    }

    // --- 2. GESTIÓN DE RESET ---
    if (systemState.enFalla) {
        if (!systemState.peRemoto && !systemState.trip && systemState.cmdReset) {
            systemState.enFalla = false; 
            systemState.cmdReset = false; 
        }
    }

    // --- 3. LÓGICA DE CONTROL ---
    if (systemState.enFalla) {
        systemState.valv1 = false;
        systemState.valv2 = false;
        systemState.valv3 = false;
        systemState.valv4 = false;
        systemState.motor = false;
    } 
    else {
        // Espejo de Sensores (Ahora usan el valor limpio)
        systemState.valv1 = systemState.sensZ1;
        systemState.valv2 = systemState.sensZ2;
        systemState.valv3 = systemState.sensZ3;
        systemState.valv4 = systemState.sensZ4;

        systemState.motor = systemState.cmdMotor;
    }

    // --- 4. ESCRITURA FÍSICA ---
    digitalWrite(PIN_VALV_1, systemState.valv1 ? LOW : HIGH);
    digitalWrite(PIN_VALV_2, systemState.valv2 ? LOW : HIGH);
    digitalWrite(PIN_VALV_3, systemState.valv3 ? LOW : HIGH);
    digitalWrite(PIN_VALV_4, systemState.valv4 ? LOW : HIGH);
    digitalWrite(PIN_MOTOR,  systemState.motor ? LOW : HIGH);
}