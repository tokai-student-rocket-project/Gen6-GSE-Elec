#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Gen6 GSE System - Raspberry Pi 4 Wireless Remote Control & Telemetry Server
Launch3.0 / Satellite3.0 対応 無線遠隔制御ブリッジ，Web ダッシュボード & デモ・シミュレータ

機能:
- Serial (115200 bps USB FT232RL) 経由で ATmega2560 と MsgPacketizer パケットを送受信
- 【デモ/シミュレータモード (--demo)】: 実機マイコンがない状態でもターミナル上でデバッグ動作
- 【インタラクティブ CLI デバッグプロンプト (--cli)】: ターミナル上で直接コマンド入力・状態確認が可能
- 【Web インターフェース (Flask, Port 5000)】: ブラウザからリアルタイム監視・遠隔操作
"""

import sys
import os
import time
import json
import threading
import glob
import argparse
import random

try:
    import serial
except ImportError:
    print("[ERROR] 'pyserial' package is missing. Install with: pip install pyserial")
    sys.exit(1)

try:
    import msgpack
except ImportError:
    print("[ERROR] 'msgpack' package is missing. Install with: pip install msgpack")
    sys.exit(1)

try:
    from flask import Flask, render_template_string, jsonify, request, logging
    import logging
    log = logging.getLogger('werkzeug')
    log.setLevel(logging.ERROR)
except ImportError:
    print("[ERROR] 'flask' package is missing. Install with: pip install flask")
    sys.exit(1)

# =========================================================================
# 通信定数・パケット ID 定義 (Launch3.0 / Satellite3.0 仕様準拠)
# =========================================================================
PACKET_RASPI_COMMAND          = 0x20  # (32) RasPi -> Launch3 遠隔制御コマンド
PACKET_RASPI_HEARTBEAT_L_TO_R = 0x21  # (33) Launch3 -> RasPi 生存確認
PACKET_RASPI_HEARTBEAT_R_TO_L = 0x22  # (34) RasPi -> Launch3 生存確認
PACKET_RASPI_TELEMETRY        = 0x23  # (35) Telemetry パケット
PACKET_RASPI_WIRELESS_STATUS  = 0x24  # (36) 無線ステータス同期

# コマンド種別
CMD_EMERGENCY_STOP = 1
CMD_PEACEFUL_STOP  = 2
CMD_FILL_START     = 3
CMD_IGNITION_START = 4
CMD_ARM_SAFETY     = 5
CMD_VALVE_CONTROL  = 6
CMD_ZERO_CALIB     = 7

VALVE_NAMES = ["SHIFT", "FILL", "DUMP", "OXYGEN", "IGNITER", "OPEN", "CLOSE", "PURGE"]

# =========================================================================
# グローバルステート
# =========================================================================
class GSEState:
    def __init__(self):
        self.lock = threading.Lock()
        self.connected = False
        self.demo_mode = False
        self.last_heartbeat_rx = 0.0
        self.port_name = "/dev/ttyUSB0"
        
        # Telemetry Data
        self.cmd_state = 0
        self.fb_state = 0
        self.sequence_flag = 0
        self.pressure_MPa = 0.0
        self.limit_switch_state = (1 << 5)  # ch5 デフォルト CLOSED (SAFE)
        
        # Unpacked Status
        self.emergency_stop = False
        self.fill_active = False
        self.ignition_active = False
        self.can_confirm = False
        self.mcu_wireless_ok = False
        self.armed_state = False

    def update_telemetry(self, cmd_b, fb_b, seq_b, press_f, limit_b):
        with self.lock:
            self.cmd_state = cmd_b
            self.fb_state = fb_b
            self.sequence_flag = seq_b
            self.pressure_MPa = press_f
            self.limit_switch_state = limit_b
            self.last_heartbeat_rx = time.time()
            self.connected = True
            
            self.emergency_stop = bool(seq_b & (1 << 0))
            self.fill_active = bool(seq_b & (1 << 1))
            self.ignition_active = bool(seq_b & (1 << 2))
            self.can_confirm = bool(seq_b & (1 << 3))
            self.mcu_wireless_ok = bool(seq_b & (1 << 4))

    def to_dict(self):
        with self.lock:
            valves_cmd = {VALVE_NAMES[i]: bool(self.cmd_state & (1 << i)) for i in range(8)}
            valves_fb  = {VALVE_NAMES[i]: bool(self.fb_state & (1 << i)) for i in range(8)}
            return {
                "demo_mode": self.demo_mode,
                "connected": self.connected and (self.demo_mode or (time.time() - self.last_heartbeat_rx < 4.0)),
                "pressure_MPa": round(self.pressure_MPa, 3),
                "emergency_stop": self.emergency_stop,
                "fill_active": self.fill_active,
                "ignition_active": self.ignition_active,
                "can_confirm": self.can_confirm,
                "mcu_wireless_ok": self.mcu_wireless_ok or self.demo_mode,
                "armed_state": self.armed_state,
                "limit_switch_ch5": bool(self.limit_switch_state & (1 << 5)),
                "valves_cmd": valves_cmd,
                "valves_fb": valves_fb,
                "last_update": round(time.time() - self.last_heartbeat_rx, 1) if not self.demo_mode else 0.0
            }

gse_state = GSEState()
ser_instance = None
ser_lock = threading.Lock()

# =========================================================================
# Serial & MsgPacketizer 通信処理
# =========================================================================
def auto_detect_serial_port():
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') + glob.glob('COM*')
    return ports[0] if ports else None

def send_msgpacketizer_packet(ser, packet_id, *args):
    """MsgPacketizer 形式 (MsgPack 配列 [packet_id, args...]) でパケットを作成して送信"""
    if gse_state.demo_mode:
        # デモモード時はシミュレータにコマンドを直接転送
        simulate_handle_command(args[0] if len(args) > 0 else 0, args[1] if len(args) > 1 else 0)
        return True

    if ser is None or not ser.is_open:
        return False
    try:
        data_list = [packet_id] + list(args)
        packed_data = msgpack.packb(data_list)
        with ser_lock:
            ser.write(packed_data)
            ser.flush()
        return True
    except Exception as e:
        print(f"[SERIAL WRITE ERROR] {e}")
        return False

def serial_worker(port_name, baudrate=115200):
    global ser_instance
    unpacker = msgpack.Unpacker(raw=False)
    
    print(f"[SERIAL] Starting Serial Worker on {port_name} ({baudrate} bps)...")
    
    while not gse_state.demo_mode:
        try:
            if ser_instance is None or not ser_instance.is_open:
                current_port = port_name
                if not os.path.exists(current_port) and not current_port.startswith("COM"):
                    detected = auto_detect_serial_port()
                    if detected:
                        current_port = detected
                
                print(f"[SERIAL] Connecting to {current_port}...")
                ser_instance = serial.Serial(current_port, baudrate, timeout=0.1)
                gse_state.port_name = current_port
                print(f"[SERIAL] Connected to {current_port} successfully.")
            
            raw_bytes = ser_instance.read(1024)
            if raw_bytes:
                unpacker.feed(raw_bytes)
                for msg in unpacker:
                    if isinstance(msg, list) and len(msg) > 0:
                        packet_id = msg[0]
                        if packet_id == PACKET_RASPI_TELEMETRY and len(msg) >= 6:
                            cmd_b, fb_b, seq_b, press_f, limit_b = msg[1], msg[2], msg[3], msg[4], msg[5]
                            gse_state.update_telemetry(cmd_b, fb_b, seq_b, press_f, limit_b)
                        elif packet_id == PACKET_RASPI_HEARTBEAT_L_TO_R:
                            gse_state.last_heartbeat_rx = time.time()
                            gse_state.connected = True

        except Exception as e:
            print(f"[SERIAL ERROR] {e}")
            if ser_instance:
                try:
                    ser_instance.close()
                except:
                    pass
                ser_instance = None
            gse_state.connected = False
            time.sleep(2.0)
            
        time.sleep(0.01)

def heartbeat_worker():
    """2Hz で Raspberry Pi -> MCU の生存確認パケットを送信"""
    while True:
        if not gse_state.demo_mode and ser_instance and ser_instance.is_open:
            send_msgpacketizer_packet(ser_instance, PACKET_RASPI_HEARTBEAT_R_TO_L)
        time.sleep(0.5)

# =========================================================================
# 実機不使用デモ・シミュレータ処理 (--demo)
# =========================================================================
sim_target_pressure = 0.0

def simulate_handle_command(cmd_type, param):
    global sim_target_pressure
    with gse_state.lock:
        if cmd_type == CMD_EMERGENCY_STOP:
            print("\n[SIMULATOR] 🚨 EMERGENCY STOP TRIGGERED!")
            gse_state.emergency_stop = True
            gse_state.fill_active = False
            gse_state.ignition_active = False
            gse_state.cmd_state = (1 << 2) | (1 << 7) | (1 << 6)  # DUMP, PURGE, CLOSE ON
            sim_target_pressure = 0.0

        elif cmd_type == CMD_PEACEFUL_STOP:
            print("\n[SIMULATOR] ⏹️ PEACEFUL STOP TRIGGERED!")
            gse_state.emergency_stop = False
            gse_state.fill_active = False
            gse_state.ignition_active = False
            gse_state.cmd_state = 0
            sim_target_pressure = 0.0

        elif cmd_type == CMD_ARM_SAFETY:
            gse_state.armed_state = bool(param != 0)
            print(f"\n[SIMULATOR] 🛡️ SAFETY ARM SET TO: {'ARMED' if gse_state.armed_state else 'DISARMED'}")

        elif cmd_type == CMD_FILL_START:
            if not gse_state.armed_state:
                print("\n[SIMULATOR REJECT] Cannot Fill: Safety not ARMED!")
                return
            print("\n[SIMULATOR] ⛽ FILL SEQUENCE STARTED!")
            gse_state.fill_active = True
            gse_state.cmd_state |= (1 << 1)  # FILL ON
            sim_target_pressure = 4.50

        elif cmd_type == CMD_IGNITION_START:
            if not gse_state.armed_state:
                print("\n[SIMULATOR REJECT] Cannot Ignite: Safety not ARMED!")
                return
            print("\n[SIMULATOR] 🔥 IGNITION SEQUENCE STARTED!")
            gse_state.ignition_active = True
            gse_state.cmd_state |= (1 << 3) | (1 << 4) | (1 << 5)  # O2, IGNITER, OPEN ON

        elif cmd_type == CMD_VALVE_CONTROL:
            if not gse_state.armed_state:
                print("\n[SIMULATOR REJECT] Valve control blocked: Safety not ARMED!")
                return
            gse_state.cmd_state = param
            print(f"\n[SIMULATOR] Valve cmd set to: 0x{param:02X}")

        elif cmd_type == CMD_ZERO_CALIB:
            print("\n[SIMULATOR] Zero Calibration Done.")
            gse_state.pressure_MPa = 0.0

def simulator_worker():
    """デモモード用物理ダイナミクスシミュレータ"""
    global sim_target_pressure
    print("[SIMULATOR] Demo Simulator Engine Running...")
    gse_state.connected = True
    gse_state.mcu_wireless_ok = True

    while True:
        with gse_state.lock:
            # 圧力のアプローチ計算
            dp = (sim_target_pressure - gse_state.pressure_MPa) * 0.1
            noise = random.uniform(-0.005, 0.005)
            gse_state.pressure_MPa = max(0.0, gse_state.pressure_MPa + dp + noise)

            # フィードバック状態を追従させる
            if gse_state.armed_state:
                gse_state.fb_state = gse_state.cmd_state
            else:
                gse_state.fb_state = 0

            seq_flag = 0
            if gse_state.emergency_stop: seq_flag |= (1 << 0)
            if gse_state.fill_active: seq_flag |= (1 << 1)
            if gse_state.ignition_active: seq_flag |= (1 << 2)
            if gse_state.can_confirm: seq_flag |= (1 << 3)
            seq_flag |= (1 << 4) # Wireless OK
            gse_state.sequence_flag = seq_flag

        time.sleep(0.1)

# =========================================================================
# インタラクティブ CLI デバッグ shell (--cli)
# =========================================================================
def print_cli_help():
    print("""
===================================================================
 🛠️  Raspberry Pi GSE CLI Debug Commands
===================================================================
  status             : 現在のリアルタイムステータスを表示
  arm <on|off>       : セーフティ装置の ARM / DISARM 切替
  fill               : 充填シーケンス開始 (FILL Sequence)
  ignite             : 点火シーケンス開始 (IGNITION Sequence)
  estop              : 緊急停止 (EMERGENCY STOP)
  peace              : 通常停止 (PEACEFUL STOP)
  valve <name> <1|0> : 電磁弁手動トグル (FILL, DUMP, O2, IGNITER, OPEN, CLOSE, PURGE)
  limit <1|0>        : リミットスイッチ (ch5) の状態切替 (デモ時)
  zero               : ゼロ点校正実行
  help               : このヘルプを表示
  quit / exit        : プログラムを終了
===================================================================
""")

def print_cli_status():
    d = gse_state.to_dict()
    print("\n------------------- [ GSE CURRENT STATUS ] -------------------")
    print(f"  Mode           : {'[DEMO MOCK MODE]' if d['demo_mode'] else '[PHYSICAL HARDWARE MODE]'}")
    print(f"  Wireless Link  : {'● OK (CONNECTED)' if d['connected'] else '○ DISCONNECTED'}")
    print(f"  Safety Armed   : {'🛡️ ARMED (解除済み)' if d['armed_state'] else '🔒 DISARMED (施錠中)'}")
    print(f"  N2O Pressure   : {d['pressure_MPa']:.3f} MPa")
    print(f"  Limit Switch 5 : {'CLOSED (SAFE)' if d['limit_switch_ch5'] else 'OPEN'}")
    print(f"  Sequence State : Fill={d['fill_active']}, Ignite={d['ignition_active']}, E-Stop={d['emergency_stop']}")
    print("  Valves State   :")
    for vname in VALVE_NAMES:
        cmd = "ON " if d['valves_cmd'][vname] else "OFF"
        fb  = "ON " if d['valves_fb'][vname]  else "OFF"
        print(f"    - {vname:<8} : CMD={cmd} | FB={fb}")
    print("--------------------------------------------------------------\n")

def cli_worker():
    time.sleep(1.0)
    print_cli_help()
    print_cli_status()

    while True:
        try:
            cmd_str = input("GSE-Debug> ").strip()
            if not cmd_str:
                continue

            parts = cmd_str.split()
            main_cmd = parts[0].lower()

            if main_cmd in ["quit", "exit"]:
                print("Exiting GSE Debug Server...")
                os._exit(0)
            elif main_cmd == "help":
                print_cli_help()
            elif main_cmd == "status":
                print_cli_status()
            elif main_cmd == "estop":
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_EMERGENCY_STOP, 0)
                print("[CLI] Emergency Stop Sent.")
            elif main_cmd == "peace":
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_PEACEFUL_STOP, 0)
                print("[CLI] Peaceful Stop Sent.")
            elif main_cmd == "arm":
                arg = parts[1].lower() if len(parts) > 1 else "on"
                arm_val = 1 if arg in ["on", "1", "arm"] else 0
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_ARM_SAFETY, arm_val)
                print(f"[CLI] Safety Arm set to {arm_val}.")
            elif main_cmd == "fill":
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_FILL_START, 0)
                print("[CLI] Fill Sequence Start Sent.")
            elif main_cmd == "ignite":
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_IGNITION_START, 0)
                print("[CLI] Ignition Sequence Start Sent.")
            elif main_cmd == "zero":
                send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_ZERO_CALIB, 0)
                print("[CLI] Zero Calibration Requested.")
            elif main_cmd == "limit":
                arg = parts[1].lower() if len(parts) > 1 else "1"
                sw = (1 << 5) if arg in ["1", "on", "closed"] else 0
                with gse_state.lock:
                    gse_state.limit_switch_state = sw
                print(f"[CLI] Limit switch ch5 set to {'CLOSED' if sw else 'OPEN'}.")
            elif main_cmd == "valve":
                if len(parts) < 3:
                    print("Usage: valve <name> <1|0>")
                    continue
                vname = parts[1].upper()
                vval  = 1 if parts[2] in ["1", "on"] else 0
                if vname in VALVE_NAMES:
                    idx = VALVE_NAMES.index(vname)
                    with gse_state.lock:
                        if vval:
                            gse_state.cmd_state |= (1 << idx)
                        else:
                            gse_state.cmd_state &= ~(1 << idx)
                        cmd_b = gse_state.cmd_state
                    send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_VALVE_CONTROL, cmd_b)
                    print(f"[CLI] Valve {vname} set to {vval}.")
                else:
                    print(f"Unknown valve name: {vname}. Valid: {VALVE_NAMES}")
            else:
                print(f"Unknown command: '{main_cmd}'. Type 'help' for available commands.")
        except (KeyboardInterrupt, EOFError):
            print("\nExiting GSE Debug Server...")
            os._exit(0)

# =========================================================================
# Web UI (Flask)
# =========================================================================
app = Flask(__name__)

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Gen6 GSE Remote Control Center</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --accent-blue: #38bdf8;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --text-color: #f8fafc;
            --text-muted: #94a3b8;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            margin: 0;
            padding: 15px;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 20px;
            background: var(--card-bg);
            border-radius: 10px;
            margin-bottom: 15px;
        }
        .title { font-size: 1.4rem; font-weight: bold; color: var(--accent-blue); }
        .mode-tag { background: #6366f1; padding: 4px 8px; border-radius: 4px; font-size: 0.8rem; margin-left: 10px; }
        .status-badge {
            padding: 6px 14px;
            border-radius: 20px;
            font-weight: bold;
            font-size: 0.9rem;
        }
        .status-online { background: rgba(34, 197, 94, 0.2); color: var(--accent-green); border: 1px solid var(--accent-green); }
        .status-offline { background: rgba(239, 68, 68, 0.2); color: var(--accent-red); border: 1px solid var(--accent-red); }
        
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 15px; }
        .card { background: var(--card-bg); padding: 15px; border-radius: 10px; border: 1px solid #334155; }
        
        .pressure-display {
            font-size: 3.2rem;
            font-weight: bold;
            text-align: center;
            color: var(--accent-blue);
            margin: 10px 0;
        }
        .unit { font-size: 1.2rem; color: var(--text-muted); }
        
        .btn {
            width: 100%;
            padding: 14px;
            margin: 6px 0;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        .btn-estop { background: var(--accent-red); color: white; font-size: 1.2rem; }
        .btn-arm { background: var(--accent-orange); color: white; }
        .btn-fill { background: #3b82f6; color: white; }
        .btn-ignite { background: #d97706; color: white; }
        .btn-peace { background: #475569; color: white; }
        .btn:active { transform: scale(0.98); opacity: 0.9; }

        .valve-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-top: 10px; }
        .valve-box {
            padding: 10px;
            border-radius: 6px;
            background: #0f172a;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border: 1px solid #334155;
        }
        .indicator {
            width: 14px;
            height: 14px;
            border-radius: 50%;
            background: #475569;
        }
        .indicator-on { background: var(--accent-green); box-shadow: 0 0 8px var(--accent-green); }
    </style>
</head>
<body>
    <div class="header">
        <div class="title">🚀 Gen6 GSE Wireless Remote Control <span id="modeTag" class="mode-tag">REAL</span></div>
        <div id="statusBadge" class="status-badge status-offline">● CONNECTING...</div>
    </div>

    <div class="grid">
        <div class="card">
            <h3>N2O 配管圧力 (Pressure)</h3>
            <div class="pressure-display"><span id="pressureVal">0.000</span> <span class="unit">MPa</span></div>
            <p>リミットスイッチ (ch5): <strong id="limitSwitch">OFF</strong></p>
            <p>MCU 無線インターロック: <strong id="mcuInterlock">--</strong></p>
            <button class="btn btn-peace" onclick="sendCmd(7, 0)">ゼロ点校正要求 (Zero Calib)</button>
        </div>

        <div class="card">
            <h3>遠隔制御シーケンス (Control)</h3>
            <button class="btn btn-estop" onclick="sendCmd(1, 0)">🚨 緊急停止 (EMERGENCY STOP)</button>
            <button class="btn btn-arm" onclick="toggleArm()">🛡️ セーフティ解除 (ARM / DISARM)</button>
            <button class="btn btn-fill" onclick="sendCmd(3, 0)">⛽ 充填シーケンス開始 (FILL)</button>
            <button class="btn btn-ignite" onclick="confirmIgnition()">🔥 点火シーケンス開始 (IGNITION)</button>
            <button class="btn btn-peace" onclick="sendCmd(2, 0)">⏹️ 通常停止 (PEACEFUL STOP)</button>
        </div>

        <div class="card">
            <h3>電磁弁状態 (Solenoid Feedback)</h3>
            <div class="valve-grid" id="valveGrid"></div>
        </div>
    </div>

    <script>
        let isArmed = false;

        function updateData() {
            fetch('/api/telemetry')
                .then(r => r.json())
                .then(data => {
                    const badge = document.getElementById('statusBadge');
                    const modeTag = document.getElementById('modeTag');
                    
                    if (data.demo_mode) {
                        modeTag.innerText = "DEMO SIMULATOR";
                        modeTag.style.background = "#8b5cf6";
                    }

                    if (data.connected) {
                        badge.className = "status-badge status-online";
                        badge.innerText = data.demo_mode ? "● DEMO MODE ACTIVE" : "● WIRELESS LINK OK";
                    } else {
                        badge.className = "status-badge status-offline";
                        badge.innerText = "● DISCONNECTED";
                    }

                    document.getElementById('pressureVal').innerText = data.pressure_MPa.toFixed(3);
                    document.getElementById('limitSwitch').innerText = data.limit_switch_ch5 ? "CLOSED (SAFE)" : "OPEN";
                    document.getElementById('limitSwitch').style.color = data.limit_switch_ch5 ? "#22c55e" : "#ef4444";
                    document.getElementById('mcuInterlock').innerText = data.mcu_wireless_ok ? "ACTIVE (OK)" : "DISCONNECTED";

                    const grid = document.getElementById('valveGrid');
                    grid.innerHTML = '';
                    for (const [name, on] of Object.entries(data.valves_fb)) {
                        const box = document.createElement('div');
                        box.className = 'valve-box';
                        box.innerHTML = `<span>${name}</span>
                            <div class="indicator ${on ? 'indicator-on' : ''}"></div>`;
                        grid.appendChild(box);
                    }
                })
                .catch(err => console.error(err));
        }

        function sendCmd(cmdType, param) {
            fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ cmd_type: cmdType, param: param })
            }).then(r => r.json()).then(res => console.log(res));
        }

        function toggleArm() {
            isArmed = !isArmed;
            sendCmd(5, isArmed ? 1 : 0);
            alert(isArmed ? "セーフティを ARM (解除) しました．" : "セーフティを DISARM (施錠) しました．");
        }

        function confirmIgnition() {
            if (confirm("【最終点火確認】本当に点火シーケンスを開始しますか？")) {
                sendCmd(4, 0);
            }
        }

        setInterval(updateData, 200);
    </script>
</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

@app.route('/api/telemetry')
def api_telemetry():
    return jsonify(gse_state.to_dict())

@app.route('/api/command', methods=['POST'])
def api_command():
    data = request.get_json() or {}
    cmd_type = data.get('cmd_type')
    param = data.get('param', 0)
    
    if cmd_type is not None:
        success = send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, cmd_type, param)
        return jsonify({"status": "ok" if success else "failed"})
    return jsonify({"status": "invalid_request"}), 400

# =========================================================================
# エントリポイント
# =========================================================================
def main():
    parser = argparse.ArgumentParser(description="Gen6 GSE Raspberry Pi 4 Remote Control Server")
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial Port (default: /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='Baudrate (default: 115200)')
    parser.add_argument('--web-port', type=int, default=5000, help='Web Server Port (default: 5000)')
    parser.add_argument('--demo', action='store_true', help='Run in Demo/Simulator mode without physical hardware')
    parser.add_argument('--cli', action='store_true', help='Enable interactive terminal CLI debug shell')
    args = parser.parse_args()

    if args.demo:
        gse_state.demo_mode = True
        print("\n" + "="*65)
        print(" 🎮 DEMO MODE ENABLED: Running without physical hardware!")
        print("="*65 + "\n")
        t_sim = threading.Thread(target=simulator_worker, daemon=True)
        t_sim.start()
    else:
        t_serial = threading.Thread(target=serial_worker, args=(args.port, args.baud), daemon=True)
        t_serial.start()

    t_hb = threading.Thread(target=heartbeat_worker, daemon=True)
    t_hb.start()

    # CLI プロンプトの起動
    t_cli = threading.Thread(target=cli_worker, daemon=True)
    t_cli.start()

    print(f"\n[RASPI GSE SERVER] Starting Web Remote Control Dashboard at http://0.0.0.0:{args.web_port}")
    app.run(host='0.0.0.0', port=args.web_port, debug=False)

if __name__ == '__main__':
    main()
