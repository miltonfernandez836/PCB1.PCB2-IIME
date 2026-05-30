import websocket
import json
import time
import threading
import math
import csv 
import random
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider, Button 
from matplotlib.animation import FuncAnimation

# --- CONFIGURACIÓN ---
ESP_IP = "192.168.1.177" 
PORT = 81

# Configuración Visual
WINDOW_SECONDS = 20  
# UPDATE_INTERVAL: 200ms es un buen equilibrio (5 cuadros/segundo). 
# Si sigue muy lento al inicio, puedes subirlo a 300.
UPDATE_INTERVAL = 200 

FILENAME = f"datalog_FINAL_{int(time.time())}.csv" 

# Variables Globales
start_time = time.time()
times = []
voltajes_pc = []; voltajes_esp = []     
# Listas para señales digitales
up_signals = []; down_signals = []
motor_signals = []   # Estado real (PCB2)
manual_signals = []  # Estado botón (HMI)
purge_signals = []   # Estado timer (Lógica)

current_sim_v = 0.0; esp_feedback_v = 0.0; current_setpoint = 0.0 
modo_onda = False; motor_activo = False; last_state = "IDLE"  
ws_app = None

# CSV FINAL
csv_file = open(FILENAME, mode='w', newline='')
csv_writer = csv.writer(csv_file, delimiter=',')
csv_writer.writerow(["Tiempo(s)", "Voltaje_PC", "Voltaje_ESP32", "Setpoint_V", "Estado", 
                     "UP", "DOWN", "MOTOR_REAL", "MANUAL_CMD", "PURGA_TIMER"])

print(f"--- MONITOR THC FINAL (VISIBILIDAD MAXIMA) ---")
print(f"📁 Guardando datos en: {FILENAME}")

# --- COMUNICACIÓN ---
def on_message(ws, message):
    global current_setpoint, last_state, esp_feedback_v
    try:
        data = json.loads(message)
        
        # Leemos datos
        if "voltaje_arco" in data: esp_feedback_v = float(data["voltaje_arco"])
        if "voltaje_setpoint" in data: current_setpoint = float(data["voltaje_setpoint"])
        if "estado_thc" in data: last_state = data["estado_thc"]
        
        up_val = 1 if data.get("up", False) else 0
        down_val = 1 if data.get("down", False) else 0
        
        motor_val = 1 if data.get("motor", False) else 0   
        man_val   = 1 if data.get("man_fan", False) else 0 
        purg_val  = 1 if data.get("purging", False) else 0 
            
        now = time.time() - start_time
        
        # Guardar en CSV
        csv_writer.writerow([f"{now:.2f}", f"{current_sim_v:.2f}", f"{esp_feedback_v:.2f}", f"{current_setpoint:.2f}",
                             last_state, up_val, down_val, motor_val, man_val, purg_val])
        csv_file.flush() 
        
        # Guardar en listas para gráfica
        times.append(now)
        voltajes_pc.append(current_sim_v); voltajes_esp.append(esp_feedback_v)
        up_signals.append(up_val); down_signals.append(down_val)
        
        motor_signals.append(motor_val)
        manual_signals.append(man_val)
        purge_signals.append(purg_val)
        
        # Limpieza de memoria (Ventana deslizante)
        limit = now - WINDOW_SECONDS
        while times and times[0] < limit:
            times.pop(0); voltajes_pc.pop(0); voltajes_esp.pop(0); 
            up_signals.pop(0); down_signals.pop(0); 
            motor_signals.pop(0); manual_signals.pop(0); purge_signals.pop(0)
                
    except Exception as e: pass

def on_error(ws, error): print(f"❌ Error: {error}")
def on_close(ws, close_status_code, close_msg): print("🔌 Desconectado"); csv_file.close() 
def on_open(ws): print("✅ CONECTADO.")

def send_voltage(v):
    if ws_app and ws_app.sock and ws_app.sock.connected:
        ws_app.send(json.dumps({"sim_val": v}))

# --- FÍSICA SIMULADA ---
VELOCIDAD_MOTOR = 0.5 

def physics_loop():
    global current_sim_v
    t = 0
    while True:
        if modo_onda and current_setpoint > 0:
            # --- 2. ONDA MÁS RÁPIDA + PICOS DE RUIDO ALEATORIO ---
            noise = math.sin(t * 10.0) * 10 + random.uniform(-15, 15)
            current_sim_v = current_setpoint + noise
            send_voltage(current_sim_v)
            t += 0.05
        elif motor_activo:
            if "SUBIENDO" in last_state: current_sim_v += VELOCIDAD_MOTOR
            elif "BAJANDO" in last_state: current_sim_v -= VELOCIDAD_MOTOR
            current_sim_v = max(0, min(current_sim_v, 300))
            send_voltage(current_sim_v)
            
        # --- 3. ACELERAR EL BUCLE A 50ms ---
        time.sleep(0.05) 

# --- INTERFAZ GRÁFICA ---
def run_dashboard():
    global ws_app, modo_onda, current_sim_v

    ws_app = websocket.WebSocketApp(f"ws://{ESP_IP}:{PORT}/", on_open=on_open, on_message=on_message, on_error=on_error, on_close=on_close)
    
    wst = threading.Thread(target=ws_app.run_forever)
    wst.daemon = True; wst.start()
    
    threading.Thread(target=physics_loop, daemon=True).start()

    plt.style.use('dark_background') 
    
    # CONFIGURACIÓN DE PANELES (3 Filas)
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 10), sharex=True, gridspec_kw={'height_ratios': [3, 1.5, 1.5]})
    plt.subplots_adjust(bottom=0.20) 
    fig.canvas.manager.set_window_title('THC Monitor - Panel de Control')

    # --- PANEL 1: CONTROL ANALÓGICO (Voltajes) ---
    
    # === CAMBIO DE ESTILO PARA VISIBILIDAD ===
    # 1. Simulador PC (Naranja): Guiones largos (--)
    line_pc, = ax1.plot([], [], label='Simulador PC', color='#ff6d00', lw=2.5, ls='--', alpha=0.8)
    
    # 2. Lectura Real (Amarilla): Guion-Punto (-.)
    line_esp, = ax1.plot([], [], label='Lectura Real (ESP32)', color='#ffff00', lw=2.0, ls='-.') 
    
    # 3. Setpoint (Verde): Punteado fino (:)
    line_sp = ax1.axhline(y=0, color='#00e676', alpha=0.9, ls=':', lw=2, label='Setpoint (Objetivo)')
    
    ax1.legend(loc='upper right', facecolor='#333333', fontsize='medium')
    ax1.grid(True, alpha=0.2)
    ax1.set_ylim(-10, 320)
    ax1.set_ylabel("Voltaje de Arco (V)", fontsize=12, color='white')
    ax1.set_title("Dinámica de Control de Altura", color='white', fontsize=14, fontweight='bold')

    # --- PANEL 2: SISTEMA DE EXTRACCIÓN (Diagnóstico) ---
    ax2.set_title("Estado del Sistema de Extracción", color='white', fontsize=12)
    ax2.set_ylim(-0.1, 1.3); ax2.set_yticks([])
    ax2.grid(True, alpha=0.1)
    ax2.text(0.02, 0.8, "Prioridad Lógica:", transform=ax2.transAxes, color='white', fontsize=9)

    # --- PANEL 3: ACCIONES DE CONTROL (Pulsos UP/DOWN) ---
    ax3.set_title("Acciones de Control Z (Salidas Digitales)", color='white', fontsize=12)
    ax3.set_ylim(-0.1, 1.1)
    ax3.set_yticks([0, 1])
    ax3.set_yticklabels(['OFF', 'ON'])
    ax3.set_ylabel("Lógica", fontsize=12, color='white')
    ax3.grid(True, alpha=0.1)

    # --- CONTROLES INTERACTIVOS ---
    ax_slider = plt.axes([0.15, 0.08, 0.7, 0.03], facecolor='#333333')
    slider = Slider(ax_slider, 'V. Simulado', 0.0, 300.0, valinit=0.0, color='#00bcd4')
    slider.label.set_color('white')
    def update_slider(val): global current_sim_v; current_sim_v = val; send_voltage(val)
    slider.on_changed(update_slider)

    C_OFF = '#333333'; C_ON_ONDA = '#00bcd4'; C_ON_MOTOR = '#4caf50' 
    
    ax_btn_onda = plt.axes([0.05, 0.02, 0.1, 0.05])
    btn_onda = Button(ax_btn_onda, 'Ruido: OFF', color=C_OFF, hovercolor='#555555')
    btn_onda.label.set_color('white')
    
    ax_btn_motor = plt.axes([0.17, 0.02, 0.1, 0.05])
    btn_motor = Button(ax_btn_motor, 'Motor Z: OFF', color=C_OFF, hovercolor='#555555')
    btn_motor.label.set_color('white')

    def toggle_onda(event):
        global modo_onda; modo_onda = not modo_onda
        btn_onda.label.set_text("Ruido: ON" if modo_onda else "Ruido: OFF")
        btn_onda.color = C_ON_ONDA if modo_onda else C_OFF
        btn_onda.label.set_color('black' if modo_onda else 'white')

    def toggle_motor(event):
        global motor_activo; motor_activo = not motor_activo
        btn_motor.label.set_text("Motor Z: AUTO" if motor_activo else "Motor Z: OFF")
        btn_motor.color = C_ON_MOTOR if motor_activo else C_OFF
        btn_motor.label.set_color('black' if motor_activo else 'white')

    btn_onda.on_clicked(toggle_onda)
    btn_motor.on_clicked(toggle_motor)

    ax_btn_fan = plt.axes([0.85, 0.02, 0.1, 0.05])
    btn_fan = Button(ax_btn_fan, 'Fan Manual', color='#ff9800', hovercolor='#fb8c00')
    btn_fan.label.set_color('white')
    btn_fan.on_clicked(lambda e: ws_app.send(json.dumps({"cmd": "toggle_fan"})) if ws_app.sock else None)

    def create_btn(pos, text, val):
        ax_btn = plt.axes(pos)
        btn = Button(ax_btn, text, color='#222222', hovercolor='#444444')
        btn.label.set_color('#00e5ff') 
        for s in ax_btn.spines.values(): s.set_edgecolor('#00e5ff')
        btn.on_clicked(lambda x: slider.set_val(val))
        return btn
    create_btn([0.45, 0.02, 0.1, 0.05], 'Set: 80V', 80)
    create_btn([0.60, 0.02, 0.1, 0.05], 'Set: 120V', 120)
    create_btn([0.75, 0.02, 0.1, 0.05], 'Set: 160V', 160)

    # --- ANIMACIÓN ---
    def update_plot(frame):
        if not times: return line_pc, line_esp, line_sp 
        
        # 1. Voltajes
        line_pc.set_data(times, voltajes_pc)
        line_esp.set_data(times, voltajes_esp)
        line_sp.set_ydata([current_setpoint, current_setpoint])
        
        # 2. Diagnóstico Motor (Capas lógicas)
        while ax2.collections: ax2.collections[0].remove()
        
        # Capa 1: Manual (Fondo)
        ax2.fill_between(times, 0, manual_signals, facecolor='#ff9800', alpha=0.4, label='HMI: Manual CMD')
        # Capa 2: Purga (Fondo)
        ax2.fill_between(times, 0, purge_signals, facecolor='#03a9f4', alpha=0.4, label='Lógica: Purga Timer')
        # Capa 3: Motor Real (Línea Sólida/Rayada)
        ax2.fill_between(times, 0, motor_signals, facecolor='none', edgecolor='#00ff00', hatch='///', linewidth=1.5, label='Hardware: Motor ON')
        
        if not ax2.get_legend(): ax2.legend(loc='upper left', fontsize='small', framealpha=0.8, facecolor='#222')

        # 3. Pulsos Z (Lógica de Control)
        while ax3.collections: ax3.collections[0].remove()
        ax3.fill_between(times, 0, up_signals, color='#00e5ff', alpha=0.8, step='mid', label='UP (Subir)')
        ax3.fill_between(times, 0, down_signals, color='#ff4081', alpha=0.8, step='mid', label='DOWN (Bajar)')
        
        if not ax3.get_legend(): ax3.legend(loc='upper right', fontsize='small', framealpha=0.8, facecolor='#222')

        now = times[-1]
        ax1.set_xlim(max(0, now - WINDOW_SECONDS), max(1, now))
        return line_pc, line_esp, line_sp

    ani = FuncAnimation(fig, update_plot, interval=UPDATE_INTERVAL, blit=False)
    plt.show()

if __name__ == "__main__":
    try: run_dashboard()
    except: csv_file.close()








    