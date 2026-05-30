#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WebSocketsServer.h> // Librería: Links2004/WebSockets
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <Preferences.h>

// Cabeceras propias
#include "web_server.h"
#include "thc_logic.h"
#include "modbus_master.h"

// --- CONFIGURACIÓN W5500 ---
#define W5500_CS    5
#define W5500_RST   33
// MOSI=23, MISO=19, SCK=18 (SPI Default)

// --- RED ETHERNET (IP ESTÁTICA) ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// --- SERVIDORES ---
EthernetServer server(80);          // Servidor Web (HTML) en puerto 80
WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket en puerto 81

// --- Variables Externas ---
extern Preferences preferences;
extern float deadband;
extern float targetV;
extern float vFilt;
extern bool  arcOK;
extern String estadoTHCStr;
extern ExtractorData extractorState;

// --- Control simple de conexiones WebSocket ---
static bool wsConnected[4] = {false};

/* =========================================================
   MANEJO DE EVENTOS WEBSOCKET (Links2004)
   ========================================================= */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            // Solo mostrar si estaba realmente conectado
            if (wsConnected[num]) {
                Serial.printf("[%u] Cliente Desconectado\n", num);
                wsConnected[num] = false;
            }
            break;

        case WStype_CONNECTED:
            {
                wsConnected[num] = true;
                Serial.printf("[%u] Cliente Conectado!\n", num);

                // --- ENVIAR ESTADO INICIAL ---
                StaticJsonDocument<300> statusDoc;
                statusDoc["voltaje_arco"] = (int)(vFilt * 10) / 10.0;
                statusDoc["voltaje_setpoint"] = targetV;
                statusDoc["deadband"] = deadband;
                statusDoc["estado_thc"] = estadoTHCStr;
                statusDoc["arc_ok"] = arcOK;

                statusDoc["z1"] = extractorState.zona1;
                statusDoc["z2"] = extractorState.zona2;
                statusDoc["z3"] = extractorState.zona3;
                statusDoc["z4"] = extractorState.zona4;
                statusDoc["motor"] = extractorState.motorPrendido;
                statusDoc["err_ext"] = (extractorState.fallaPE || extractorState.fallaTrip);
                
                statusDoc["man_fan"] = thcIsManualFanOn();
                statusDoc["purging"] = thcIsPurging();


                String output;
                serializeJson(statusDoc, output);
                webSocket.sendTXT(num, output); // Enviar solo a este cliente
            }
            break;

        case WStype_TEXT:
            {
                // Procesar JSON recibido
                StaticJsonDocument<200> docIn;
                DeserializationError error = deserializeJson(docIn, payload);

                if (error) {
                    Serial.print("Error JSON: "); Serial.println(error.c_str());
                    return;
                }

                // --- COMANDOS ---
                if (docIn.containsKey("set_thickness")) {
                    float mm = docIn["set_thickness"];
                    if (setTargetByThickness(mm)) {
                        Serial.printf("HMI: Espesor %.1fmm\n", mm);
                        preferences.putFloat("targetV", targetV);
                    }
                }

                if (docIn.containsKey("set_voltage")) {
                    float v = docIn["set_voltage"];
                    if (setTargetByVoltage(v)) {
                        Serial.printf("HMI: Manual %.1fV\n", v);
                        preferences.putFloat("targetV", v);
                    }
                }

                if (docIn.containsKey("set_deadband")) {
                    float db = docIn["set_deadband"];
                    if (db >= 0.5f && db <= 10.0f) {
                        deadband = db;
                        preferences.putFloat("deadband", db);
                    }
                }

                if (docIn.containsKey("cmd")) {
                    String comando = docIn["cmd"];
                    if (comando == "reset_alarm") {
                        resetAlarm();
                        Serial.println("HMI: Reset Alarmas");
                    }
                    
                    else if (comando == "stop") {
                        forceStopFromHMI(); // <--- LLAMA A LA NUEVA FUNCIÓN
                    }

                    else if (comando == "toggle_fan") {
                      toggleManualFan();
                    }
                    
                }

                // --- COMANDOS DE SIMULACIÓN (NUEVO) ---
                if (docIn.containsKey("sim_val")) {
                    // Si recibimos un valor, activamos la simulación automáticamente
                    float val = docIn["sim_val"];
                    setSimMode(true); 
                    setSimVoltage(val);
                }
                
                // Comando para apagar simulación y volver a la realidad
                if (docIn.containsKey("sim_stop")) {
                    setSimMode(false);
                    Serial.println("Simulación detenida. Volviendo a sensores reales.");
                }



            }
            break;
    }
}

/* =========================================================
   CONFIGURACIÓN (SETUP)
   ========================================================= */
void setupWebServer() {
    Serial.println("\n--- INICIANDO ETHERNET W5500 ---");

    // 1. SPI y Reset Hardware
    SPI.begin(18, 19, 23, 5); 
    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW); delay(100);
    digitalWrite(W5500_RST, HIGH); delay(200);

    // 2. Iniciar Ethernet
    Ethernet.init(W5500_CS);
    Ethernet.begin(mac, ip, myDns, gateway, subnet);

    // Verificación
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("❌ ERROR: W5500 no detectado.");
    } else {
        Serial.print("✅ Ethernet OK. IP: ");
        Serial.println(Ethernet.localIP());
    }

    // 3. Iniciar SPIFFS
    if (!SPIFFS.begin(true)) Serial.println("❌ Fallo SPIFFS");
    else Serial.println("✅ SPIFFS Montado");

    // 4. Iniciar Servidores
    server.begin(); // HTTP (80)
    webSocket.begin(); // WS (81)
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("Servicios iniciados: HTTP(80), WS(81)");
}

/* =========================================================
   BUCLE PRINCIPAL (LOOP)
   ¡Esto es vital! Ethernet no es asíncrono.
   ========================================================= */
void runWebServer() {
    // 1. Mantener vivo el WebSocket
    webSocket.loop();

    // 2. Atender peticiones HTTP (Servir index.html)
    EthernetClient client = server.available();
    
    if (client) {
        boolean currentLineIsBlank = true;
        String req = "";
        
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                if (req.length() < 50) req += c; // Guardamos un poquito para ver qué pide

                if (c == '\n' && currentLineIsBlank) {
                    // Fin de cabeceras HTTP, enviamos respuesta
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println();

                    // LEER Y ENVIAR index.html DESDE SPIFFS
                    if (SPIFFS.exists("/index.html")) {
                        File file = SPIFFS.open("/index.html", "r");
                        while (file.available()) {
                            // Leemos en bloques de 64 bytes para eficiencia
                            uint8_t buf[64];
                            int bytesRead = file.read(buf, 64);
                            client.write(buf, bytesRead);
                        }
                        file.close();
                    } else {
                        client.println("<h1>Error: index.html no encontrado en SPIFFS</h1>");
                    }
                    break;
                }
                if (c == '\n') {
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            }
        }
        delay(1);
        client.stop();
    }
}