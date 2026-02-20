import tkinter as tk
import customtkinter as ctk
import serial
import serial.tools.list_ports
import threading
import time
import math

# --- Configuration ---
VALVE_MAP = {
    "SHIFT": 0,
    "FILL": 1,
    "DUMP": 2,
    "OXYGEN": 3,
    "IGNITER": 4,
    "OPEN": 5,
    "CLOSE": 6,
    "PURGE": 7
}

# Font Settings
FONT_FAMILY = "Meiryo UI" 
FONT_BOLD = (FONT_FAMILY, 12, "bold")
FONT_LARGE = (FONT_FAMILY, 16, "bold")
FONT_TITLE = (FONT_FAMILY, 24, "bold")

ctk.set_appearance_mode("Light")
ctk.set_default_color_theme("blue")

class SolenoidVisualizer(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("Gen6-GSE Solenoid Visualizer")
        self.geometry("1500x1000")

        self.tank_level = 0.0 # Initial State: Empty
        self.has_liquid = False
        self.drain_timer = 300
        
        self.cmd_state = 0
        self.fb_state = 0
        self.ser = None
        self.running = True

        self.setup_ui()
        self.start_serial_thread()
        
        self.is_auto_running = False

    def setup_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        # High-level controls
        self.top_frame = ctk.CTkFrame(self, corner_radius=0, fg_color="#f0f0f0")
        self.top_frame.grid(row=0, column=0, padx=0, pady=0, sticky="ew")

        self.title_label = ctk.CTkLabel(self.top_frame, text="GEN6-GSE SYSTEM MONITOR", font=FONT_TITLE, text_color="#333")
        self.title_label.pack(side="left", padx=30, pady=15)

        self.port_menu = ctk.CTkOptionMenu(self.top_frame, values=self.get_ports(), corner_radius=5, font=FONT_BOLD, width=200)
        self.port_menu.pack(side="left", padx=10)

        self.connect_btn = ctk.CTkButton(self.top_frame, text="接続 (CONNECT)", command=self.toggle_connect, corner_radius=5, font=FONT_BOLD, width=150)
        self.connect_btn.pack(side="left", padx=10)

        self.status_label = ctk.CTkLabel(self.top_frame, text="● DISCONNECTED", text_color="#e74c3c", font=FONT_LARGE)
        self.status_label.pack(side="left", padx=20)
        
        # Debug Mode Checkbox
        self.debug_mode = tk.BooleanVar(value=False)
        self.debug_check = ctk.CTkCheckBox(self.top_frame, text="DEBUG MODE", variable=self.debug_mode, command=self.toggle_debug_mode, font=FONT_BOLD)
        self.debug_check.pack(side="right", padx=20)

        # Content Frame (Splits Canvas and Debug Panel)
        self.content_frame = ctk.CTkFrame(self, fg_color="#ffffff")
        self.content_frame.grid(row=1, column=0, sticky="nsew")
        self.content_frame.grid_columnconfigure(0, weight=1)
        self.content_frame.grid_rowconfigure(0, weight=1)

        # Canvas for Drawing
        self.canvas = tk.Canvas(self.content_frame, bg="#ffffff", highlightthickness=0)
        self.canvas.grid(row=0, column=0, padx=0, pady=0, sticky="nsew")
        
        # Debug Panel (Initially Hidden)
        self.debug_frame = ctk.CTkFrame(self.content_frame, width=200, corner_radius=0)
        # It will be grid() in toggle_debug_mode
        
        # Auto Sequence Button
        self.auto_btn = ctk.CTkButton(self.debug_frame, text="AUTO LAUNCH", command=self.toggle_auto_sequence, fg_color="#e74c3c", font=FONT_BOLD)
        self.auto_btn.pack(pady=20, padx=20, fill="x")

        self.debug_vars = {}
        valid_keys = ["FILL", "DUMP", "OXYGEN", "IGNITER", "OPEN", "PURGE"]
        for key in valid_keys:
            var = tk.BooleanVar(value=False)
            self.debug_vars[key] = var
            cb = ctk.CTkCheckBox(self.debug_frame, text=key, variable=var, font=FONT_BOLD)
            cb.pack(pady=10, padx=20, anchor="w")

    def get_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports] if ports else ["No Ports Found"]

    def toggle_connect(self):
        if self.ser is None:
            port = self.port_menu.get()
            try:
                self.ser = serial.Serial(port, 115200, timeout=0.1)
                self.status_label.configure(text="● CONNECTED", text_color="#2ecc71")
                self.connect_btn.configure(text="切断 (DISCONNECT)", fg_color="#95a5a6")
            except Exception as e:
                self.status_label.configure(text="● CONNECTION FAILED", text_color="#e74c3c")
        else:
            self.ser.close()
            self.ser = None
            self.status_label.configure(text="● DISCONNECTED", text_color="#e74c3c")
            self.connect_btn.configure(text="接続 (CONNECT)", fg_color=['#3B8ED0', '#1F6AA5'])

    def toggle_debug_mode(self):
        if self.debug_mode.get():
            self.debug_frame.grid(row=0, column=1, sticky="ns")
            self.status_label.configure(text="● DEBUG MODE", text_color="#e67e22")
            if self.ser: self.ser.close() # Close serial in debug mode
            self.connect_btn.configure(state="disabled")
        else:
            self.debug_frame.grid_forget()
            self.status_label.configure(text="● DISCONNECTED", text_color="#e74c3c")
            self.connect_btn.configure(state="normal")
            self.ser = None
            self.cmd_state = 0
            self.fb_state = 0

    def start_serial_thread(self):
        thread = threading.Thread(target=self.serial_loop, daemon=True)
        thread.start()

    def serial_loop(self):
        while self.running:
            if not self.debug_mode.get():
                if self.ser and self.ser.is_open:
                    try:
                        line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                        if line.startswith("V_DATA:"):
                            data_part = line.split(":")[1]
                            data = data_part.split(",")
                            self.cmd_state = int(data[0])
                            self.fb_state = int(data[1])
                    except Exception as e:
                        pass
            time.sleep(0.01)

    def update_debug_state(self):
        if self.debug_mode.get():
            # Construct state integer from checkboxes
            state = 0
            for key, var in self.debug_vars.items():
                if var.get():
                    if key in VALVE_MAP:
                        state |= (1 << VALVE_MAP[key])
            
            self.cmd_state = state
            self.fb_state = state # Simulate perfect feedback

    def toggle_auto_sequence(self):
        if not self.is_auto_running:
            self.is_auto_running = True
            self.auto_btn.configure(text="STOP AUTO", fg_color="#95a5a6")
            threading.Thread(target=self.run_auto_sequence, daemon=True).start()
        else:
            self.is_auto_running = False
            self.auto_btn.configure(text="AUTO LAUNCH", fg_color="#e74c3c")

    def run_auto_sequence(self):
        # Reset
        for key in self.debug_vars:
            self.debug_vars[key].set(False)
        
        # 1. Setup: DUMP Closed
        if not self.is_auto_running: return
        self.debug_vars["DUMP"].set(True) # Closed (Solenoid ON)
        time.sleep(1.0)
        
        # 2. Fill Start
        if not self.is_auto_running: return
        self.debug_vars["FILL"].set(True) # Open
        time.sleep(5.0) # Filling
        
        # 3. Ignition Seq Start (Simulated T=0 relative to logic)
        # 4. Oxygen Open (T+4.5s)
        if not self.is_auto_running: return
        time.sleep(4.5)
        self.debug_vars["OXYGEN"].set(True)
        
        # 5. Igniter On (T+6.0s -> +1.5s from prev)
        if not self.is_auto_running: return
        time.sleep(1.5)
        self.debug_vars["IGNITER"].set(True)
        
        # 6. Burn Start (T+10.0s -> +4.0s from prev)
        if not self.is_auto_running: return
        time.sleep(4.0)
        self.debug_vars["FILL"].set(False) # Close Fill
        self.debug_vars["OPEN"].set(True) # Open Main
        
        # 7. Cutoff (T+10.5s -> +0.5s from prev)
        if not self.is_auto_running: return
        time.sleep(0.5)
        self.debug_vars["OXYGEN"].set(False)
        self.debug_vars["IGNITER"].set(False)
        
        # 8. Purge Start (T+20.5s -> +10.0s from prev)
        if not self.is_auto_running: return
        time.sleep(10.0)
        self.debug_vars["PURGE"].set(True)
        
        # 9. End (T+25.5s -> +5.0s from prev)
        if not self.is_auto_running: return
        time.sleep(5.0)
        self.debug_vars["PURGE"].set(False)
        self.debug_vars["OPEN"].set(False)
        self.debug_vars["DUMP"].set(False) # Open (Solenoid OFF)
        
        # Finish
        self.is_auto_running = False
        self.auto_btn.configure(text="AUTO LAUNCH", fg_color="#e74c3c")

    def draw_diagram(self):
        self.update_debug_state()
        self.canvas.delete("all")
        # Scale Factor ~0.75
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        if w < 100: return 

        # Modern Palette
        C_BG = "#ffffff"
        C_PIPE = "#ecf0f1"      # Very light grey for skeleton
        C_TEXT = "#2c3e50"
        C_N2O = "#0984e3"       # Blue
        C_O2 = "#7f8c8d"        # Grey
        C_N2 = "#00b894"        # Green
        C_SIGNAL = "#e056fd"    # Bright Purple
        C_VALVE_OFF = "#ffffff"
        C_VALVE_ON = "#e67e22"  # Orange
        C_BOX = "#bdc3c7"
        
        # --- Helper functions ---
        def draw_pipe(x1, y1, x2, y2, active=False, color="#0984e3", width=15, active_width=9, show_skeleton=True): # Scaled widths 20->15, 12->9
            if show_skeleton:
                self.canvas.create_line(x1, y1, x2, y2, fill=C_PIPE, width=width, capstyle="round")
            if active:
                self.canvas.create_line(x1, y1, x2, y2, fill=color, width=active_width, capstyle="round")

        def draw_pilot(x1, y1, x2, y2, active=False):
            self.canvas.create_line(x1, y1, x2, y2, fill="#f0f3f4", width=4)
            if active:
                self.canvas.create_line(x1, y1, x2, y2, fill=C_N2, width=2)

        def draw_signal_line(x1, y1, x2, y2, active=False):
            self.canvas.create_line(x1, y1, x2, y2, fill="#f0f3f4", width=3, dash=(8,5))
            if active:
                self.canvas.create_line(x1, y1, x2, y2, fill=C_SIGNAL, width=3)

        def draw_valve(x, y, name, active=False, orientation="H", label_offset=None):
            color = C_VALVE_ON if active else C_VALVE_OFF
            size = 18 # Scaled 24->18
            if orientation == "H":
                points = [x-size, y-size, x+size, y+size, x-size, y+size, x+size, y-size]
            else:
                points = [x-size, y-size, x+size, y-size, x-size, y+size, x+size, y+size]
            
            # Valve body
            self.canvas.create_polygon(points, fill=color, outline=C_TEXT, width=2)
            
            # Label box above valve or offset
            if label_offset:
                lx, ly = x + label_offset[0], y + label_offset[1]
                self.canvas.create_text(lx, ly, text=name, fill=C_TEXT, font=FONT_BOLD)
            else:
                label_y = y - size - 20
                self.canvas.create_text(x, label_y, text=name, fill=C_TEXT, font=FONT_BOLD)

        def draw_box(x1, y1, x2, y2, label, color=C_BOX):
            self.canvas.create_rectangle(x1, y1, x2, y2, outline=color, width=2, dash=(6,6)) # Scaled dash
            self.canvas.create_text(x1+10, y1+15, text=label, fill=color, font=(FONT_FAMILY, 11), anchor="w") # Scaled font 14->11

        def draw_tank(x, y, label, color, level=1.0):
            w, h = 34, 82 # Scaled 45->34, 110->82
            
            # --- 1. Draw Empty Container (Background) ---
            self.canvas.create_oval(x-w, y+h-22, x+w, y+h, fill="#ffffff", outline=color, width=2)
            self.canvas.create_rectangle(x-w, y-h+11, x+w, y+h-11, fill="#ffffff", outline=color, width=2)
            self.canvas.create_oval(x-w, y-h, x+w, y-h+22, fill="#ffffff", outline=color, width=2)

            # --- 2. Draw Liquid (Foreground) ---
            if level > 0.01:
                liquid_h = (2 * h - 22) * level 
                y_surface = (y + h - 11) - liquid_h
                rect_top = max(y_surface, y-h+11)
                
                self.canvas.create_oval(x-w, y+h-22, x+w, y+h, fill=color, outline=color, width=0)
                self.canvas.create_rectangle(x-w, rect_top, x+w, y+h-11, fill=color, outline=color, width=0)
                
                surface_y_center = y_surface
                self.canvas.create_oval(x-w, surface_y_center-11, x+w, surface_y_center+11, fill=color, outline=color, width=0)

            # --- 3. Re-draw Outline ---
            self.canvas.create_oval(x-w, y+h-22, x+w, y+h, fill=None, outline=color, width=2)
            self.canvas.create_rectangle(x-w, y-h+11, x+w, y+h-11, fill=None, outline=color, width=2)
            self.canvas.create_oval(x-w, y-h, x+w, y-h+22, fill=None, outline=color, width=2)
            
            text_color = "#ffffff" if level > 0.6 else color 
            self.canvas.create_text(x, y, text=label, fill=text_color, font=FONT_BOLD, justify="center") # Keep font bold (12)

        def draw_nozzle(x, y):
            points = [x-38, y, x+38, y, x+52, y+68, x-52, y+68] # Scaled ~0.75
            self.canvas.create_polygon(points, fill="#dfe6e9", outline=C_TEXT, width=2)

        # --- Coordinates (Scaled ~0.75) ---
        X_LEFT = 120
        X_SATELLITE = 270
        X_PNEUMATIC = 540
        X_UMB_BOX = 840
        X_AIRFRAME = 1000
        
        Y_O2 = 110 
        Y_N2O = 390
        Y_UMB_SIGNALS_OPEN = 525
        Y_UMB_SIGNALS_IGN = 600
        Y_N2 = 700 
        
        # Derived
        ax = X_AIRFRAME + 112 # +150 -> +112
        y_engine_top = Y_N2O + 150 # +200 -> +150
        eng_height = 150 # 200 -> 150
        eng_y_start = y_engine_top + 60 # +80 -> +60

        # --- State Logic ---
        fb = self.fb_state
        is_fill_sol_on = bool(fb & (1 << VALVE_MAP["FILL"]))
        is_dump_sol_on = bool(fb & (1 << VALVE_MAP["DUMP"]))
        is_o2_sol_on = bool(fb & (1 << VALVE_MAP["OXYGEN"]))
        is_purge_sol_on = bool(fb & (1 << VALVE_MAP["PURGE"]))
        is_ign_on = bool(fb & (1 << VALVE_MAP["IGNITER"]))
        is_open_signal_on = bool(fb & (1 << VALVE_MAP["OPEN"]))

        is_fill_open = is_fill_sol_on
        is_dump_open = not is_dump_sol_on
        is_internal_supply_open = not is_open_signal_on
        is_internal_main_open = is_open_signal_on

        # --- DRAWING ---

        # 1. GN2 System
        draw_tank(X_LEFT, Y_N2, "GN2\n0.65MPa", C_N2)
        
        # Manifold
        draw_pipe(X_LEFT + 38, Y_N2, X_SATELLITE + 45, Y_N2, active=True, color=C_N2) # +50->38, +60->45
        draw_pipe(X_SATELLITE + 45, Y_N2 - 75, X_SATELLITE + 45, Y_N2 + 75, active=True, color=C_N2) # +-100->75

        # Satellite Box
        draw_box(X_SATELLITE, Y_N2 - 150, X_SATELLITE + 188, Y_N2 + 150, "SATELLITE - 電磁弁") # +-200->150, +250->188

        labels_sol = ["FILL", "DUMP", "PURGE"]
        for i in range(3):
            vx = X_SATELLITE + 98 # +130->98
            vy = Y_N2 + (i - 1) * 75 # *100->75
            sol_active = [is_fill_sol_on, is_dump_sol_on, is_purge_sol_on][i]
            
            # Input pipe
            draw_pipe(X_SATELLITE + 45, vy, vx - 18, vy, active=True, color=C_N2) # -24->18
            
            # Valve
            draw_valve(vx, vy, labels_sol[i], active=sol_active)
            
            # Pilots
            if i == 0: # FILL
                path_y = 480 # 620 -> 480 (approx)
                draw_pilot(vx + 18, vy, vx + 105, vy, active=is_fill_sol_on) # +24->18, +140->105
                draw_pilot(vx + 105, vy, vx + 105, path_y, active=is_fill_sol_on)
                draw_pilot(vx + 105, path_y, X_PNEUMATIC - 60, path_y, active=is_fill_sol_on) # -80->-60
                draw_pilot(X_PNEUMATIC - 60, path_y, X_PNEUMATIC - 60, Y_N2O - 15, active=is_fill_sol_on) # -20->-15
                
            elif i == 1: # DUMP
                draw_pilot(vx + 18, vy, X_PNEUMATIC + 38, vy, active=is_dump_sol_on) # +50->+38
                draw_pilot(X_PNEUMATIC + 38, vy, X_PNEUMATIC + 38, Y_N2O - 52, active=is_dump_sol_on) # -70->-52
                draw_pilot(X_PNEUMATIC + 38, Y_N2O - 52, X_PNEUMATIC + 75, Y_N2O - 52, active=is_dump_sol_on) # +100->+75
            
            elif i == 2: # PURGE
                 draw_pipe(vx + 18, vy, ax, vy, active=is_purge_sol_on, color=C_N2, width=11, active_width=6) # 15->11, 8->6
                 draw_pipe(ax, vy, ax, eng_y_start + eng_height, active=is_purge_sol_on, color=C_N2, width=11, active_width=6)
                 
                 p_text = "消火 ON" if is_purge_sol_on else "消火 OFF"
                 p_color = "#e74c3c" if is_purge_sol_on else "#bdc3c7"
                 self.canvas.create_text(ax - 110, vy - 22, text=p_text, fill=p_color, font=("Meiryo UI", 15, "bold")) # -150->-110, -30->-22, 20->15

        # 2. LN2O System
        draw_tank(X_LEFT, Y_N2O, "LN2O\n5.5MPa", C_N2O)
        draw_pipe(X_LEFT + 38, Y_N2O, X_PNEUMATIC - 68, Y_N2O, active=True, color=C_N2O) # +50->38, -90->-68
        
        # FILL
        draw_box(X_PNEUMATIC - 98, Y_N2O - 112, X_PNEUMATIC + 112, Y_N2O + 135, "空圧弁ユニット") # -130->-98, -150->-112, +150->+112, +180->+135
        draw_valve(X_PNEUMATIC - 60, Y_N2O, "FILL (NC)", active=is_fill_sol_on) # -80->-60

        # To Airframe
        ground_flow_active = is_fill_open or (is_dump_open and is_internal_supply_open and self.tank_level > 0.0)
        draw_pipe(X_PNEUMATIC - 52, Y_N2O, X_AIRFRAME, Y_N2O, active=ground_flow_active, color=C_N2O) # -70->-52
        
        # Dump
        draw_valve(X_PNEUMATIC + 60, Y_N2O - 52, "DUMP (NO)", active=is_dump_sol_on) # +80->+60, -70->-52
        draw_pipe(X_PNEUMATIC + 60, Y_N2O - 26, X_PNEUMATIC + 60, Y_N2O, active=ground_flow_active, color=C_N2O) # -35->-26
        
        # Exhaust
        is_exhaust_active = is_dump_open and ground_flow_active
        draw_pipe(X_PNEUMATIC + 75, Y_N2O - 52, X_PNEUMATIC + 135, Y_N2O - 52, active=is_exhaust_active, color=C_N2O) # +100->+75, +180->+135
        self.canvas.create_text(X_PNEUMATIC + 210, Y_N2O - 52, text="● 排気 (EXHAUST)", fill="#e67e22", font=FONT_BOLD) # +280->+210

        # 3. Oxygen System
        draw_tank(X_LEFT, Y_O2, "GO2\n0.2MPa", C_O2)
        draw_pipe(X_LEFT + 38, Y_O2, X_SATELLITE + 128, Y_O2, active=True, color=C_O2) # +50->38, +170->128
        
        draw_box(X_SATELLITE + 75, Y_O2 - 75, X_SATELLITE + 210, Y_O2 + 45, "電磁弁ユニット(O2)") # +100->75, -100->-75, +280->210, +60->45
        draw_valve(X_SATELLITE + 131, Y_O2, "O2 VALVE", active=is_o2_sol_on) # +175->131
        draw_pipe(X_LEFT + 292, Y_O2, X_AIRFRAME + 112, Y_O2, active=is_o2_sol_on, color=C_O2) # +390->292, +150->112

        # 4. Signal Lines
        self.canvas.create_text(X_UMB_BOX - 75, Y_UMB_SIGNALS_OPEN - 11, text="OPEN SIGNAL ▶", fill=C_SIGNAL, font=FONT_BOLD) # -100->-75, -15->-11
        draw_signal_line(X_SATELLITE + 450, Y_UMB_SIGNALS_OPEN, X_UMB_BOX, Y_UMB_SIGNALS_OPEN, active=is_open_signal_on) # +600->450
        
        self.canvas.create_text(X_UMB_BOX - 75, Y_UMB_SIGNALS_IGN - 11, text="IGN SIGNAL ▶", fill=C_SIGNAL, font=FONT_BOLD)
        draw_signal_line(X_SATELLITE + 450, Y_UMB_SIGNALS_IGN, X_UMB_BOX, Y_UMB_SIGNALS_IGN, active=is_ign_on)

        # 5. Umbilical Area
        draw_box(X_UMB_BOX - 150, Y_UMB_SIGNALS_OPEN - 45, X_UMB_BOX + 60, Y_UMB_SIGNALS_IGN + 45, "UMBILICAL", color="#e67e22") # -200->-150, -60->-45, +80->+60

        # Simulation Logic
        is_filling = ground_flow_active and is_internal_supply_open and not is_dump_open
        is_draining = (is_dump_open and is_internal_supply_open) or is_internal_main_open
        
        if is_filling: 
             self.tank_level += 0.02
             if self.tank_level > 1.0:
                 self.tank_level = 0.0 
             self.has_liquid = True
             
        elif self.has_liquid:
            if is_draining:
                 self.tank_level -= 0.05
                 if self.tank_level <= 0.0:
                     self.tank_level = 0.0
                     self.has_liquid = False
            else:
                self.tank_level = 1.0 
        else:
            self.tank_level = 0.0

        # 6. Airframe & Engine
        draw_box(X_AIRFRAME - 60, 38, X_AIRFRAME + 262, 712, "機体 (AIRFRAME) & モータ") # -80->-60, 50->38, +350->262, 950->712
        
        # Signals Entering
        draw_signal_line(X_UMB_BOX, Y_UMB_SIGNALS_OPEN, X_AIRFRAME + 38, Y_UMB_SIGNALS_OPEN, active=is_open_signal_on) # +50->38
        draw_signal_line(X_UMB_BOX, Y_UMB_SIGNALS_IGN, X_AIRFRAME + 38, Y_UMB_SIGNALS_IGN, active=is_ign_on)

        # Pipes Entering
        draw_pipe(X_AIRFRAME, Y_N2O, ax, Y_N2O, active=ground_flow_active and is_internal_supply_open, color=C_N2O)

        # Supply Valve
        draw_valve(X_AIRFRAME + 38, Y_N2O, "供給路弁（NO）", active=not is_internal_supply_open) # +50->38
        draw_signal_line(X_AIRFRAME + 38, Y_UMB_SIGNALS_OPEN, X_AIRFRAME + 38, Y_N2O + 19, active=is_open_signal_on) # +25->19

        # Internal Tank
        draw_tank(ax, Y_N2O - 150, "1000cc\nTank", C_N2O, level=self.tank_level) # -200->-150
        
        # Tank Exit Pipe
        tank_out_active = (ground_flow_active and is_internal_supply_open) or (self.has_liquid and is_internal_main_open)
        draw_pipe(ax, Y_N2O, ax, Y_N2O - 64, active=tank_out_active, color=C_N2O) # -85->-64
        
        # Main Valve
        draw_valve(ax, y_engine_top, "主流路弁（NC）", active=is_internal_main_open, label_offset=(-60, 0)) # -80->-60

        draw_pipe(ax, Y_N2O, ax, y_engine_top - 19, active=is_internal_main_open, color=C_N2O, show_skeleton=True) # -25->-19

        # Downstream
        draw_pipe(ax, y_engine_top + 19, ax, y_engine_top + 60, active=is_internal_main_open, color=C_N2O, show_skeleton=True) # +25->19, +80->60
        
        # Signal links
        draw_signal_line(X_AIRFRAME + 38, Y_UMB_SIGNALS_OPEN, ax, Y_UMB_SIGNALS_OPEN, active=is_open_signal_on)
        draw_signal_line(ax, Y_UMB_SIGNALS_OPEN, ax, y_engine_top + 19, active=is_open_signal_on)

        # -- ENGINE --
        eng_width = 105 # 140 -> 105
        
        # 1. Combustion Chamber Case
        self.canvas.create_rectangle(ax - eng_width//2, eng_y_start, ax + eng_width//2, eng_y_start + eng_height, 
                                     fill="#f9f9f9", outline=C_TEXT, width=2) # width 3->2
        
        # 2. Fuel Grains
        grain_width = 30 # 40->30
        self.canvas.create_rectangle(ax - eng_width//2 + 4, eng_y_start + 8, ax - eng_width//2 + 4 + grain_width, eng_y_start + eng_height - 8, 
                                     fill="#2c3e50", outline=C_TEXT)
        self.canvas.create_rectangle(ax + eng_width//2 - 4 - grain_width, eng_y_start + 8, ax + eng_width//2 - 4, eng_y_start + eng_height - 8, 
                                     fill="#2c3e50", outline=C_TEXT)
        
        self.canvas.create_text(ax + 90, eng_y_start + 75, text="固体燃料\n(GRAIN)", fill="#7f8c8d", font=FONT_BOLD) # +120->90, +100->75

        # 3. Nozzle
        draw_nozzle(ax, eng_y_start + eng_height)

        # 4. Flows inside Engine
        if is_internal_main_open:
             self.canvas.create_line(ax, eng_y_start, ax, eng_y_start + eng_height + 38, fill=C_N2O, width=8, arrow=tk.LAST) # +50->38, width 10->8

        # O2 Path
        o2_x_start = ax
        o2_x_route = ax + 75 # +100->75
        o2_y_inject = eng_y_start + 22 # +30->22
        
        points_o2 = [
            o2_x_start, Y_O2,
            o2_x_route, Y_O2,
            o2_x_route, o2_y_inject,
            ax + 15, o2_y_inject # +20->15
        ]
        
        self.canvas.create_line(points_o2, fill=C_PIPE, width=15, capstyle="round", joinstyle="round") # 20->15
        
        if is_o2_sol_on:
             self.canvas.create_line(points_o2, fill=C_O2, width=9, capstyle="round", joinstyle="round") # 12->9

        # Ignition Signal (Left Side)
        draw_signal_line(X_AIRFRAME + 38, Y_UMB_SIGNALS_IGN, ax - 75, Y_UMB_SIGNALS_IGN, active=is_ign_on) # +50->38, ax-100->ax-75
        draw_signal_line(ax - 75, Y_UMB_SIGNALS_IGN, ax - 75, eng_y_start + 112, active=is_ign_on) # +150->112
        draw_signal_line(ax - 75, eng_y_start + 112, ax, eng_y_start + 112, active=is_ign_on)

        if is_ign_on:
             self.canvas.create_oval(ax-30, eng_y_start+112-30, ax+30, eng_y_start+112+30, outline="#e74c3c", width=3) # 40->30
             self.canvas.create_text(ax, eng_y_start + 112, text="💥", font=("Arial", 68)) # 90->68

        # Metadata
        self.canvas.create_text(w-50, 40, text=f"CMD: {bin(self.cmd_state)}", fill="#95a5a6", font=("Consolas", 14, "bold"), anchor="e") # w-200? w-50 is tight, stick to w-150 scaled
        self.canvas.create_text(w-50, 70, text=f"FB:  {bin(self.fb_state)}", fill="#95a5a6", font=("Consolas", 14, "bold"), anchor="e")

        self.after(50, self.draw_diagram) 




if __name__ == "__main__":
    app = SolenoidVisualizer()
    app.after(100, app.draw_diagram)
    app.mainloop()
    app.running = False
