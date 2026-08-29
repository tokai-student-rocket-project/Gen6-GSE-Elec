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
from datetime import datetime, timezone, timedelta

# Fix Windows console UTF-8 output issue for emojis
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

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

import csv
from datetime import datetime, timezone, timedelta

# 日本標準時 (JST: UTC+9) 定義
JST = timezone(timedelta(hours=9))

try:
    from flask import Flask, render_template_string, jsonify, request, send_file, logging
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
        self.auto_purge_enabled = True

        # MCU Health & Voltages (LaunchController3.0 & SatelliteController3.0)
        self.launch_voltage_V = 0.0
        self.launch_bus_voltage_V = 0.0
        self.sat_voltage_V = 0.0
        self.sat_bus_voltage_V = 0.0
        self.sat_bus_voltage_V = 0.0
        self.rs485_ok = True
        self.rocket_node_ok = True
        self.sat_armed = False
        self.raw_telemetry = ""

    def update_telemetry(self, cmd_b, fb_b, seq_b, press_f, limit_b, launch_v=None, launch_bus_v=None, sat_v=None, sat_bus_v=None):
        with self.lock:
            self.cmd_state = cmd_b
            self.fb_state = fb_b
            self.sequence_flag = seq_b
            self.pressure_MPa = press_f
            self.limit_switch_state = limit_b
            if launch_v is not None:
                self.launch_voltage_V = launch_v
            if launch_bus_v is not None:
                self.launch_bus_voltage_V = launch_bus_v
            if sat_v is not None:
                self.sat_voltage_V = sat_v
            if sat_bus_v is not None:
                self.sat_bus_voltage_V = sat_bus_v
            self.last_heartbeat_rx = time.time()
            self.connected = True
            
            self.emergency_stop = bool(seq_b & (1 << 0))
            self.fill_active = bool(seq_b & (1 << 1))
            self.ignition_active = bool(seq_b & (1 << 2))
            self.can_confirm = bool(seq_b & (1 << 3))
            self.mcu_wireless_ok = bool(seq_b & (1 << 4))
            self.rocket_node_ok = bool(seq_b & (1 << 5))
            self.sat_armed = bool(seq_b & (1 << 6))

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
                "rocket_node_ok": self.rocket_node_ok or self.demo_mode,
                "sat_armed": self.sat_armed or self.demo_mode,
                "armed_state": self.armed_state,
                "auto_purge": self.auto_purge_enabled,
                "limit_switch_ch5": bool(self.limit_switch_state & (1 << 5)),
                "launch_voltage_V": round(self.launch_voltage_V, 1),
                "launch_bus_voltage_V": round(self.launch_bus_voltage_V, 1),
                "sat_voltage_V": round(self.sat_voltage_V, 1),
                "sat_bus_voltage_V": round(self.sat_bus_voltage_V, 1),
                "valves_cmd": valves_cmd,
                "valves_fb": valves_fb,
                "raw_telemetry": self.raw_telemetry,
                "record_count": gse_logger.record_count if 'gse_logger' in globals() else 0,
                "current_log": gse_logger.current_filename if 'gse_logger' in globals() else "",
                "last_update": round(time.time() - self.last_heartbeat_rx, 1) if not self.demo_mode else 0.0
            }

# =========================================================================
# 自動データロガー (GSELogger - CSV 自動記録エンジン)
# =========================================================================
LOGS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")

class GSELogger:
    def __init__(self, logs_dir=LOGS_DIR):
        self.logs_dir = logs_dir
        os.makedirs(self.logs_dir, exist_ok=True)
        self.current_filename = f"gse_log_{datetime.now(JST).strftime('%Y%m%d_%H%M%S')}.csv"
        self.current_filepath = os.path.join(self.logs_dir, self.current_filename)
        self.record_count = 0
        self.lock = threading.Lock()
        self._clean_old_logs()
        self._init_csv()

    def _clean_old_logs(self, max_files=10):
        """保持するログの最大数を設定し、古いものを削除する (デフォルト: 最新10件のみ保持)"""
        try:
            files = [os.path.join(self.logs_dir, f) for f in os.listdir(self.logs_dir) if f.startswith("gse_log_") and f.endswith(".csv")]
            if len(files) > max_files:
                # 最終更新日時でソート (古い順)
                files.sort(key=os.path.getmtime)
                files_to_delete = files[:-max_files]
                for f in files_to_delete:
                    try:
                        os.remove(f)
                        print(f"[LOGGER] 容量節約のため古いログを削除しました: {os.path.basename(f)}")
                    except OSError as e:
                        print(f"[LOGGER] 古いログの削除に失敗しました: {e}")
        except Exception as e:
            print(f"[LOGGER] ログのクリーンアップ中にエラーが発生しました: {e}")

    def _init_csv(self):
        headers = [
            "timestamp_s", "date_jst", "time_jst", "datetime_jst", "connected", "demo_mode",
            "pressure_MPa", "vesim_mA", "emergency_stop", "fill_active",
            "ignition_active", "can_confirm", "armed_state", "auto_purge",
            "limit_ch5", "launch_voltage_V", "sat_voltage_V"
        ]
        for vname in VALVE_NAMES:
            headers.append(f"cmd_{vname}")
        for vname in VALVE_NAMES:
            headers.append(f"fb_{vname}")

        with self.lock:
            with open(self.current_filepath, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(headers)
        print(f"[LOGGER] Telemetry CSV logging initialized: {self.current_filepath}")

    def log_state(self, state_dict):
        now = time.time()
        now_jst = datetime.now(JST)
        date_str = now_jst.strftime('%Y-%m-%d')
        time_str = now_jst.strftime('%H:%M:%S.%f')[:-3]
        datetime_str = now_jst.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        row = [
            round(now, 3), date_str, time_str, datetime_str,
            1 if state_dict.get('connected') else 0,
            1 if state_dict.get('demo_mode') else 0,
            state_dict.get('pressure_MPa', 0.0),
            state_dict.get('vesim_current_mA', 4.0),
            1 if state_dict.get('emergency_stop') else 0,
            1 if state_dict.get('fill_active') else 0,
            1 if state_dict.get('ignition_active') else 0,
            1 if state_dict.get('can_confirm') else 0,
            1 if state_dict.get('armed_state') else 0,
            1 if state_dict.get('auto_purge') else 0,
            1 if state_dict.get('limit_switch_ch5') else 0,
            state_dict.get('launch_voltage_V', 12.4),
            state_dict.get('sat_voltage_V', 12.1)
        ]
        
        valves_cmd = state_dict.get('valves_cmd', {})
        valves_fb  = state_dict.get('valves_fb', {})
        for vname in VALVE_NAMES:
            row.append(1 if valves_cmd.get(vname) else 0)
        for vname in VALVE_NAMES:
            row.append(1 if valves_fb.get(vname) else 0)

        with self.lock:
            with open(self.current_filepath, "a", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(row)
            self.record_count += 1

gse_logger = GSELogger()

def logger_worker():
    """10Hz でテレメトリデータを CSV ファイルへ自動保存"""
    print("[LOGGER] Auto Data Logging Thread Running (10Hz)...")
    while True:
        try:
            d = gse_state.to_dict()
            gse_logger.log_state(d)
        except Exception as e:
            print(f"[LOGGER ERROR] {e}")
        time.sleep(0.1)

gse_state = GSEState()
ser_instance = None
ser_lock = threading.Lock()

# =========================================================================
# Serial & MsgPacketizer 通信処理
# =========================================================================
def auto_detect_serial_port():
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') + glob.glob('COM*')
    return ports[0] if ports else None

def crc8_smbus(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

def cobs_decode(buffer):
    out = bytearray()
    i = 0
    while i < len(buffer):
        code = buffer[i]
        if code == 0:
            break
        i += 1
        for _ in range(1, code):
            if i < len(buffer):
                out.append(buffer[i])
                i += 1
        if code < 0xFF and i < len(buffer) and buffer[i] != 0:
            out.append(0)
    return bytes(out)

def cobs_encode(source):
    dest = bytearray()
    dest.append(0)
    code = 1
    code_index = 0
    for b in source:
        if b == 0:
            dest[code_index] = code
            code = 1
            code_index = len(dest)
            dest.append(0)
        else:
            dest.append(b)
            code += 1
            if code == 0xFF:
                dest[code_index] = code
                code = 1
                code_index = len(dest)
                dest.append(0)
    dest[code_index] = code
    dest.append(0)
    return bytes(dest)

def send_msgpacketizer_packet(ser, packet_id, *args):
    """MsgPacketizer 形式 (COBS + CRC8 + MsgPack) でパケットを作成して送信"""
    if gse_state.demo_mode:
        simulate_handle_command(args[0] if len(args) > 0 else 0, args[1] if len(args) > 1 else 0)
        return True

    if ser is None or not ser.is_open:
        return False
    try:
        packed_args = b''
        for arg in args:
            packed_args += msgpack.packb(arg)
        
        payload = bytes([packet_id]) + packed_args
        crc = crc8_smbus(packed_args)
        frame = cobs_encode(payload + bytes([crc]))
        
        with ser_lock:
            ser.write(frame)
            ser.flush()
        return True
    except Exception as e:
        print(f"[SERIAL WRITE ERROR] {e}")
        return False

def serial_worker(port_name, baudrate=115200):
    global ser_instance
    unpacker = msgpack.Unpacker(raw=False)
    buffer = bytearray()
    
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
                buffer.extend(raw_bytes)
                while b'\x00' in buffer:
                    frame, buffer = buffer.split(b'\x00', 1)
                    if len(frame) == 0:
                        continue
                    
                    try:
                        decoded = cobs_decode(frame)
                        if len(decoded) < 2:
                            continue
                            
                        payload = decoded[:-1]
                        crc = decoded[-1]
                        
                        if crc8_smbus(payload[1:]) == crc:
                            packet_id = payload[0]
                            unpacker.feed(payload[1:])
                            
                            elements = []
                            for el in unpacker:
                                elements.append(el)
                                
                            msg = []
                            if len(elements) == 1 and isinstance(elements[0], list):
                                inner = elements[0]
                                if len(inner) > 0 and inner[0] == packet_id:
                                    msg = inner
                                else:
                                    msg = [packet_id] + inner
                            else:
                                msg = [packet_id] + elements
                                
                            if len(msg) > 0:
                                packet_id = msg[0]
                                if packet_id == PACKET_RASPI_TELEMETRY:
                                    gse_state.last_heartbeat_rx = time.time()
                                    gse_state.connected = True
                                    gse_state.raw_telemetry = str(msg)
                                    if len(msg) >= 6:
                                        cmd_b, fb_b, seq_b, press_f, limit_b = msg[1], msg[2], msg[3], msg[4], msg[5]
                                        launch_v = msg[6] if len(msg) >= 7 else None
                                        launch_bus_v = msg[7] if len(msg) >= 8 else None
                                        sat_v = msg[8] if len(msg) >= 9 else None
                                        sat_bus_v = msg[9] if len(msg) >= 10 else None
                                        
                                        gse_state.update_telemetry(cmd_b, fb_b, seq_b, press_f, limit_b, launch_v, launch_bus_v, sat_v, sat_bus_v)
                                    else:
                                        gse_state.raw_telemetry += " (Truncated!)"
                                elif packet_id == PACKET_RASPI_HEARTBEAT_L_TO_R:
                                    gse_state.last_heartbeat_rx = time.time()
                                    gse_state.connected = True
                                elif packet_id == PACKET_RASPI_WIRELESS_STATUS:
                                    if len(msg) >= 2:
                                        gse_state.wireless_connected = (msg[1] == 1)
                    except Exception as parse_e:
                        unpacker = msgpack.Unpacker(raw=False)

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
            gse_state.can_confirm = False
            purge_bit = (1 << 7) if gse_state.auto_purge_enabled else 0
            gse_state.cmd_state = (1 << 2) | (1 << 6) | purge_bit  # DUMP, CLOSE, (PURGE if Auto Purge ON)
            sim_target_pressure = 0.0

        elif cmd_type == CMD_PEACEFUL_STOP:
            print("\n[SIMULATOR] ⏹️ PEACEFUL STOP TRIGGERED!")
            gse_state.emergency_stop = False
            gse_state.fill_active = False
            gse_state.ignition_active = False
            gse_state.can_confirm = False
            gse_state.cmd_state = 0
            sim_target_pressure = 0.0

        elif cmd_type == CMD_ARM_SAFETY:
            gse_state.armed_state = bool(param != 0)
            print(f"\n[SIMULATOR] 🛡️ SAFETY ARM SET TO: {'ARMED' if gse_state.armed_state else 'DISARMED'}")

        elif cmd_type == CMD_FILL_START:
            if not gse_state.armed_state:
                print("\n[SIMULATOR REJECT] Cannot Fill: Safety not ARMED!")
                return
            dump_on = bool(gse_state.cmd_state & (1 << 2))
            if not dump_on:
                print("\n[SIMULATOR REJECT] Cannot Fill: DUMP valve must be ON before sequence start!")
                return
            print("\n[SIMULATOR] ⛽ FILL SEQUENCE STARTED (Filling N2O)...")
            gse_state.fill_active = True
            gse_state.can_confirm = False  # 充填開始時はcan_confirmはFalse。圧力が上がって充填完了後にTrueへ移行
            gse_state.cmd_state &= ~(1 << 2)  # DUMP AUTO OFF (シークエンス開始前にDUMPを自動OFF/CLOSE)
            gse_state.cmd_state |= (1 << 1)   # FILL ON
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

            # 充填シーケンス中、圧力が 3.8 MPa を超えたら充填完了 (can_confirm = True) へ移行
            if gse_state.fill_active and gse_state.pressure_MPa >= 3.8:
                if not gse_state.can_confirm:
                    print("\n[SIMULATOR] ⛽ N2O FILLING COMPLETE -> CAN CONFIRM READY FOR IGNITION!")
                    gse_state.can_confirm = True

            # 電圧ジッター
            gse_state.launch_voltage_V = max(0.0, min(13.8, 12.4 + random.uniform(-0.05, 0.05)))
            gse_state.launch_bus_voltage_V = max(0.0, min(13.8, 12.0 + random.uniform(-0.02, 0.02)))
            gse_state.sat_voltage_V = max(0.0, min(13.8, 12.1 + random.uniform(-0.05, 0.05)))
            gse_state.sat_bus_voltage_V = max(0.0, min(13.8, 12.0 + random.uniform(-0.02, 0.02)))

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
            gse_state.raw_telemetry = "[SIMULATED PACKET]"

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
                with gse_state.lock:
                    dump_on = bool(gse_state.cmd_state & (1 << 2))
                if not dump_on:
                    print("[CLI REJECT] Sequence start blocked: DUMP valve must be ON (開放) before starting sequence!")
                else:
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
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800;900&family=JetBrains+Mono:wght@400;700&family=Orbitron:wght@700;900&family=Share+Tech+Mono&display=swap');
        :root {
            --bg: #f1f5f9;
            --bg-card: #ffffff;
            --bg-card-alt: #f8fafc;
            --border: #cbd5e1;
            --blue: #0284c7;
            --green: #16a34a;
            --red: #dc2626;
            --orange: #ea580c;
            --yellow: #ca8a04;
            --purple: #9333ea;
            --text: #0f172a;
            --text-dim: #64748b;
            --text-bright: #020617;
        }
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            font-family: 'Inter', system-ui, sans-serif;
            background: var(--bg);
            color: var(--text);
            padding: 10px;
            min-height: 100vh;
        }

        /* ===== REC Badge & Log Button ===== */
        .rec-badge {
            padding: 3px 8px; border-radius: 12px; font-weight: 700; font-size: 0.7rem;
            background: rgba(239,68,68,0.15); color: var(--red); border: 1px solid rgba(239,68,68,0.4);
            display: flex; align-items: center; gap: 5px;
            animation: pulse-rec 2s infinite;
        }
        @keyframes pulse-rec {
            0% { opacity: 1; }
            50% { opacity: 0.6; }
            100% { opacity: 1; }
        }
        .btn-log-download {
            background: var(--bg-card-alt); color: var(--blue); border: 1px solid var(--blue);
            border-radius: 6px; padding: 4px 10px; font-size: 0.75rem; font-weight: 700;
            cursor: pointer; transition: all 0.2s;
        }
        .btn-log-download:hover { background: rgba(56,189,248,0.15); }

        /* ===== Log Modal ===== */
        .modal-overlay {
            display: none; position: fixed; top:0; left:0; width:100vw; height:100vh;
            background: rgba(0,0,0,0.7); backdrop-filter: blur(4px);
            z-index: 9999; justify-content: center; align-items: center;
        }
        .modal-overlay.open { display: flex; }
        .modal-content {
            background: var(--bg-card); border: 1px solid var(--border);
            border-radius: 12px; width: 90%; max-width: 680px; max-height: 80vh;
            display: flex; flex-direction: column; overflow: hidden;
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
        }
        .modal-header {
            padding: 12px 16px; border-bottom: 1px solid var(--border);
            display: flex; justify-content: space-between; align-items: center;
            font-size: 0.95rem; font-weight: 700; color: var(--blue);
        }
        .modal-body {
            padding: 14px; overflow-y: auto; flex: 1;
        }
        .log-table {
            width: 100%; border-collapse: collapse; font-size: 0.75rem;
        }
        .log-table th, .log-table td {
            padding: 8px 10px; text-align: left; border-bottom: 1px solid var(--border);
        }
        .log-table th { color: var(--text-dim); text-transform: uppercase; }
        .btn-dl-sm {
            padding: 4px 10px; background: #2563eb; color: #fff; border: none;
            border-radius: 4px; font-weight: 600; font-size: 0.72rem; cursor: pointer;
            text-decoration: none; display: inline-block;
        }
        .btn-dl-sm:hover { background: #3b82f6; }

        /* ===== Header ===== */
        .header {
            display: flex; justify-content: space-between; align-items: center;
            padding: 8px 16px;
            background: linear-gradient(135deg, var(--bg-card) 0%, var(--bg-card-alt) 100%);
            border: 1px solid var(--border);
            border-radius: 10px;
            margin-bottom: 8px;
        }
        .header-title {
            font-size: 1.05rem; font-weight: 700; color: var(--blue);
            display: flex; align-items: center; gap: 8px;
        }
        .mode-tag {
            background: #6366f1; padding: 2px 7px; border-radius: 4px;
            font-size: 0.68rem; font-weight: 600; color: #fff; letter-spacing: 0.5px;
        }

        /* ===== Toyota Andon Board (トヨタ式 アンドン表示板) ===== */
        .andon-board {
            background: linear-gradient(180deg, #e2e8f0 0%, #cbd5e1 100%);
            border: 2px solid var(--border);
            border-radius: 10px;
            padding: 8px 12px;
            margin-bottom: 8px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.1);
        }
        .andon-header {
            display: flex; justify-content: space-between; align-items: center;
            font-size: 0.72rem; font-weight: 800; color: var(--blue);
            border-bottom: 1px solid var(--border); padding-bottom: 4px; margin-bottom: 6px;
            letter-spacing: 1px;
        }
        .pokayoke-badge {
            background: rgba(234,179,8,0.15); color: var(--yellow);
            border: 1px solid var(--yellow); border-radius: 4px;
            padding: 2px 8px; font-size: 0.68rem; font-weight: 700;
            transition: all 0.3s;
        }
        .pokayoke-badge.safe { background: rgba(34,197,94,0.15); color: var(--green); border-color: var(--green); }
        .pokayoke-badge.lock { background: rgba(239,68,68,0.15); color: var(--red); border-color: var(--red); }

        .andon-grid {
            display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px;
        }
        .andon-item {
            background: #ffffff; border: 1px solid #94a3b8; border-radius: 6px;
            padding: 6px 8px; display: flex; align-items: center; gap: 8px;
            opacity: 0.35; transition: all 0.3s;
        }
        .andon-item.active {
            opacity: 1; border-color: var(--blue);
            box-shadow: 0 0 15px rgba(2,132,199,0.3);
            transform: translateY(-1px);
        }
        .andon-light {
            width: 14px; height: 14px; border-radius: 50%; background: #94a3b8;
            flex-shrink: 0; box-shadow: inset 0 1px 2px rgba(0,0,0,0.3);
        }
        .andon-item.active .andon-light.green  { background: #22c55e; box-shadow: 0 0 12px #22c55e, 0 0 20px #22c55e; }
        .andon-item.active .andon-light.blue   { background: #3b82f6; box-shadow: 0 0 12px #3b82f6, 0 0 20px #3b82f6; }
        .andon-item.active .andon-light.yellow { background: #eab308; box-shadow: 0 0 12px #eab308, 0 0 20px #eab308; }
        .andon-item.active .andon-light.red    { background: #ef4444; box-shadow: 0 0 12px #ef4444, 0 0 24px #ef4444; animation: pulse-red 0.8s infinite; }

        .andon-step { font-size: 0.72rem; font-weight: 800; color: var(--text-bright); line-height: 1.1; }
        .andon-sub { font-size: 0.55rem; color: var(--text-dim); font-weight: 600; }
        .conn-badge {
            padding: 4px 12px; border-radius: 16px; font-weight: 600; font-size: 0.75rem;
            transition: all 0.3s;
        }
        .conn-ok { background: rgba(34,197,94,0.15); color: var(--green); border: 1px solid rgba(34,197,94,0.4); }
        .conn-ng { background: rgba(239,68,68,0.15); color: var(--red); border: 1px solid rgba(239,68,68,0.4); }

        /* ===== Grid ===== */
        .main-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            grid-template-rows: auto auto;
            gap: 8px;
        }
        @media (max-width: 900px) { .main-grid { grid-template-columns: 1fr; } }

        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 12px 14px;
        }
        .card h3 {
            font-size: 0.8rem; font-weight: 600; color: var(--text-dim);
            text-transform: uppercase; letter-spacing: 0.8px;
            margin-bottom: 8px;
            padding-bottom: 4px;
            border-bottom: 1px solid var(--border);
        }

        /* ===== Pressure Display (Digital 7-Segment LED Panel) ===== */
        .pressure-box {
            background: #f8fafc;
            border: 2px solid rgba(2, 132, 199, 0.4);
            border-radius: 8px;
            padding: 8px 12px;
            text-align: center;
            margin: 6px 0;
            box-shadow: inset 0 0 10px rgba(0, 0, 0, 0.1);
        }
        .pressure-title {
            font-size: 0.68rem; font-weight: 700;
            color: var(--blue); letter-spacing: 1px;
            text-transform: uppercase; margin-bottom: 2px;
        }
        .pressure-value {
            font-family: 'Share Tech Mono', 'Orbitron', monospace;
            font-size: 3.2rem; font-weight: 900;
            color: #0369a1;
            text-shadow: none;
            line-height: 1.0; margin: 4px 0;
            letter-spacing: 3px;
        }
        .pressure-unit {
            font-size: 1.1rem; color: #0284c7;
            margin-left: 6px; font-weight: 700;
            text-shadow: none;
        }
        .vesim-ma {
            font-family: 'Share Tech Mono', monospace;
            font-size: 0.75rem; font-weight: 700; color: #6d28d9;
            text-shadow: none;
            background: #f5f3ff; border: 1px solid rgba(109,40,217,0.3);
            border-radius: 4px; padding: 1px 6px; display: inline-block;
        }

        /* ===== Status LEDs ===== */
        .led-row {
            display: flex; gap: 14px; justify-content: center;
            flex-wrap: wrap;
            margin: 8px 0 6px;
        }
        .led-item {
            display: flex; align-items: center; gap: 5px;
            font-size: 0.72rem; font-weight: 500; color: var(--text-dim);
        }
        .led {
            width: 9px; height: 9px; border-radius: 50%;
            background: #cbd5e1; box-shadow: inset 0 2px 4px rgba(0,0,0,0.2);
            transition: all 0.3s;
        }
        .led-on-green { background: var(--green); box-shadow: 0 0 8px var(--green); }
        .led-on-red   { background: var(--red);   box-shadow: 0 0 8px var(--red); }
        .led-on-yellow{ background: var(--yellow); box-shadow: 0 0 8px var(--yellow); }

        /* ===== Toggle Switch (Safety & Valves) ===== */
        .toggle-row {
            display: flex; align-items: center; justify-content: space-between;
            padding: 6px 0;
        }
        .toggle-label { font-size: 0.85rem; font-weight: 600; }
        .toggle-sub { font-size: 0.68rem; color: var(--text-dim); }
        .toggle-track {
            width: 48px; height: 26px;
            background: #cbd5e1;
            border-radius: 13px;
            position: relative;
            cursor: pointer;
            transition: background 0.3s;
            flex-shrink: 0;
        }
        .toggle-track.on { background: var(--green); box-shadow: 0 0 10px rgba(34,197,94,0.4); }
        .toggle-track.disabled { opacity: 0.35; pointer-events: none; }
        .toggle-knob {
            width: 20px; height: 20px;
            background: #fff;
            border-radius: 50%;
            position: absolute;
            top: 3px; left: 3px;
            transition: left 0.2s;
            box-shadow: 0 1px 3px rgba(0,0,0,0.3);
        }
        .toggle-track.on .toggle-knob { left: 25px; }

        /* ===== E-STOP ===== */
        .estop-zone {
            margin: 8px 0;
            text-align: center;
        }
        .estop-btn {
            width: 105px; height: 105px;
            border-radius: 50%;
            border: 5px solid #991b1b;
            background: radial-gradient(circle at 40% 35%, #f87171, #dc2626 50%, #991b1b);
            color: #fff;
            font-size: 0.8rem; font-weight: 800;
            cursor: pointer;
            text-transform: uppercase;
            letter-spacing: 1px;
            line-height: 1.2;
            box-shadow: 0 4px 16px rgba(239,68,68,0.4), inset 0 2px 4px rgba(255,255,255,0.2);
            transition: all 0.15s;
            position: relative;
        }
        .estop-btn:active:not(.locked) {
            transform: scale(0.95);
            box-shadow: 0 2px 8px rgba(239,68,68,0.3), inset 0 4px 8px rgba(0,0,0,0.3);
        }
        .estop-btn.locked {
            background: radial-gradient(circle at 40% 35%, #7f1d1d, #450a0a 50%, #1c0505);
            border-color: #450a0a;
            box-shadow: inset 0 4px 12px rgba(0,0,0,0.6);
            cursor: not-allowed;
        }
        .estop-reset-btn {
            display: none;
            width: 100%;
            margin-top: 10px;
            padding: 8px 12px;
            background: #ffffff;
            color: var(--orange);
            border: 1px solid var(--orange);
            border-radius: 6px;
            font-size: 0.8rem; font-weight: 700;
            text-align: center;
            white-space: nowrap;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 0 10px rgba(249,115,22,0.2);
        }
        .estop-reset-btn:hover {
            background: rgba(249,115,22,0.15);
            box-shadow: 0 0 15px rgba(249,115,22,0.4);
        }
        .estop-reset-btn.visible { display: block; }

        /* ===== Momentary (Tact) Button ===== */
        .tact-btn {
            width: 100%;
            padding: 10px 14px;
            margin: 4px 0;
            border: none;
            border-radius: 7px;
            font-size: 0.85rem; font-weight: 700;
            cursor: pointer;
            transition: all 0.1s;
            position: relative;
            user-select: none;
        }
        .tact-btn:active:not(:disabled) {
            transform: scale(0.97);
            filter: brightness(0.85);
        }
        .tact-btn:disabled {
            opacity: 0.3;
            cursor: not-allowed;
        }
        .tact-seq {
            background: linear-gradient(135deg, #3b82f6, #2563eb);
            color: #fff;
        }
        .tact-confirm {
            background: linear-gradient(135deg, #d97706, #b45309);
            color: #fff;
        }
        .tact-confirm.ready {
            background: linear-gradient(135deg, #ea580c, #dc2626) !important;
            animation: pulse-confirm 1.5s infinite;
        }
        @keyframes pulse-confirm {
            0% { box-shadow: 0 0 6px rgba(234,88,12,0.4); }
            50% { box-shadow: 0 0 20px rgba(234,88,12,0.9); }
            100% { box-shadow: 0 0 6px rgba(234,88,12,0.4); }
        }
        .tact-peace {
            background: #e2e8f0;
            color: var(--text);
        }
        .tact-zero {
            background: #f1f5f9;
            color: var(--text-dim);
            border: 1px solid var(--border);
        }

        /* ===== Timers (High-Contrast Digital 7-Seg LED Panel) ===== */
        .timer-row {
            display: flex; gap: 8px; margin: 8px 0;
        }
        .timer-box {
            flex: 1;
            background: #f8fafc;
            border: 2px solid rgba(22, 163, 74, 0.4);
            border-radius: 8px;
            padding: 6px 8px;
            text-align: center;
            box-shadow: inset 0 0 10px rgba(0, 0, 0, 0.1);
        }
        .timer-label {
            font-size: 0.65rem; font-weight: 800;
            color: #15803d; letter-spacing: 1px;
        }
        .timer-value {
            font-family: 'Share Tech Mono', monospace;
            font-size: 1.45rem; font-weight: 900;
            color: #15803d;
            text-shadow: none;
            background: #f0fdf4;
            border: 1px solid rgba(22,163,74,0.3);
            border-radius: 4px;
            padding: 3px 0;
            margin-top: 4px;
            letter-spacing: 2px;
        }

        /* Sequence status indicator */
        .seq-status {
            text-align: center;
            padding: 5px 8px;
            border-radius: 6px;
            font-size: 0.72rem;
            font-weight: 600;
            margin: 6px 0;
            background: var(--bg);
            border: 1px solid var(--border);
        }
        .seq-fill { border-color: #3b82f6; color: #0284c7; }
        .seq-ign  { border-color: var(--orange); color: var(--orange); }
        .seq-estop{ border-color: var(--red); color: var(--red); background: rgba(239,68,68,0.1); }
        .seq-idle { color: var(--text-dim); }

        /* ===== Prominent Large Status Banner ===== */
        .status-banner-large {
            text-align: center;
            padding: 10px 12px;
            border-radius: 8px;
            font-size: 1.0rem;
            font-weight: 800;
            letter-spacing: 0.8px;
            margin-bottom: 8px;
            background: #ffffff;
            border: 2px solid var(--border);
            transition: all 0.3s;
        }
        .status-banner-large.idle    { background: #f8fafc; border-color: #cbd5e1; color: var(--text-dim); }
        .status-banner-large.armed   { background: rgba(202,138,4,0.1); border-color: var(--yellow); color: var(--yellow); box-shadow: 0 0 10px rgba(202,138,4,0.15); }
        .status-banner-large.fill    { background: rgba(2,132,199,0.1); border-color: var(--blue); color: var(--blue); box-shadow: 0 0 10px rgba(2,132,199,0.15); }
        .status-banner-large.ready   { background: rgba(234,88,12,0.1); border-color: var(--orange); color: var(--orange); box-shadow: 0 0 15px rgba(234,88,12,0.2); }
        .status-banner-large.ignite  { background: rgba(220,38,38,0.1); border-color: var(--red); color: var(--red); box-shadow: 0 0 15px rgba(220,38,38,0.25); }
        .status-banner-large.estop   { background: rgba(239,68,68,0.3); border-color: var(--red); color: #fff; box-shadow: 0 0 30px rgba(239,68,68,0.7); animation: pulse-red 1s infinite; }
        @keyframes pulse-red {
            0% { box-shadow: 0 0 10px rgba(239,68,68,0.4); }
            50% { box-shadow: 0 0 30px rgba(239,68,68,0.9); }
            100% { box-shadow: 0 0 10px rgba(239,68,68,0.4); }
        }

        /* ===== MCU Health Grid ===== */
        .mcu-status-grid {
            display: grid; grid-template-columns: 1fr 1fr; gap: 6px; margin: 8px 0;
        }
        .mcu-box {
            background: var(--bg);
            border: 1px solid var(--border);
            border-radius: 6px;
            padding: 6px 8px;
        }
        .mcu-header {
            font-size: 0.72rem; font-weight: 700; color: var(--blue);
            border-bottom: 1px solid var(--border); padding-bottom: 3px; margin-bottom: 4px;
        }
        .mcu-metric {
            display: flex; justify-content: space-between; align-items: center;
            font-size: 0.68rem; margin: 2px 0;
        }
        .mcu-label { color: var(--text-dim); }
        .mcu-val { font-family: 'JetBrains Mono', monospace; font-weight: 600; }
        .mcu-tag {
            font-size: 0.58rem; padding: 1px 4px; border-radius: 3px; font-weight: 700;
        }
        .mcu-tag.ok { background: rgba(34,197,94,0.2); color: var(--green); }
        .mcu-tag.warn { background: rgba(239,68,68,0.2); color: var(--red); }

        /* ===== Valve Grid (4 cols on Desktop for 14" Full HD Single Screen) ===== */
        .valve-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 6px; }
        @media (min-width: 900px) {
            .valve-grid { grid-template-columns: repeat(4, 1fr); }
        }
        .valve-item {
            background: var(--bg);
            border: 1px solid var(--border);
            border-radius: 6px;
            padding: 8px 10px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 6px;
        }
        .valve-name {
            font-size: 0.8rem; font-weight: 700;
            min-width: 55px;
        }
        .valve-leds {
            display: flex; gap: 4px; align-items: center;
        }
        .valve-led-label {
            font-size: 0.55rem; color: var(--text-dim); font-weight: 500;
        }
        .valve-led {
            width: 10px; height: 10px; border-radius: 50%;
            background: #334155;
            transition: all 0.3s;
        }
        .valve-led.cmd-on { background: var(--blue); box-shadow: 0 0 6px var(--blue); }
        .valve-led.fb-on  { background: var(--green); box-shadow: 0 0 6px var(--green); }
    </style>
</head>
<body>
    <!-- ===== Header ===== -->
    <div class="header">
        <div class="header-title">
            🚀 Gen6 GSE Remote Control
            <span id="modeTag" class="mode-tag">REAL</span>
        </div>
        <div style="display:flex; align-items:center; gap:10px;">
            <div class="rec-badge" id="recBadge">● REC (0)</div>
            <button class="btn-log-download" onclick="openLogModal()">📊 ログ (CSV) 一覧 / DL</button>
            <div id="connBadge" class="conn-badge conn-ng">● CONNECTING...</div>
        </div>
    </div>

    <!-- ===== Toyota Andon Status Board (アンドン表示板) ===== -->
    <div class="andon-board">
        <div class="andon-header">
            <span>🚥 ステータスボード</span>
            <span class="pokayoke-badge" id="pokayokeBadge">🛡️ [保護中] セーフティ解除待ち</span>
        </div>
        <div class="andon-grid">
            <div class="andon-item active" id="andon1">
                <div class="andon-light green"></div>
                <div class="andon-text">
                    <div class="andon-step">1. 待機・正常</div>
                    <div class="andon-sub">STANDBY / SAFE</div>
                </div>
            </div>
            <div class="andon-item" id="andon2">
                <div class="andon-light blue"></div>
                <div class="andon-text">
                    <div class="andon-step">2. 充填工程</div>
                    <div class="andon-sub">FILLING ACTIVE</div>
                </div>
            </div>
            <div class="andon-item" id="andon3">
                <div class="andon-light yellow"></div>
                <div class="andon-text">
                    <div class="andon-step">3. 点火シーケンス</div>
                    <div class="andon-sub">IGNITION SEQUENCE</div>
                </div>
            </div>
            <div class="andon-item" id="andon4">
                <div class="andon-light red"></div>
                <div class="andon-text">
                    <div class="andon-step">4. 非常停止</div>
                    <div class="andon-sub">ANDON EMERGENCY</div>
                </div>
            </div>
        </div>
    </div>

    <div class="main-grid">
        <!-- ===== Left Top: Status & Pressure & MCU Health ===== -->
        <div class="card">
            <h3>📊 SYSTEM STATUS & CONTROLLER HEALTH</h3>

            <!-- Large Prominent Status Banner -->
            <div class="status-banner-large idle" id="largeStatusBanner">
                SYSTEM STANDBY
            </div>

            <!-- Pressure Display (Digital 7-Segment LED Panel) -->
            <div class="pressure-box">
                <div class="pressure-title">亜酸化窒素 圧力 (N2O PRESSURE)</div>
                <div class="pressure-value">
                    <span id="pressureVal">0.000</span>
                    <span class="pressure-unit">MPa</span>
                </div>
            </div>

            <div class="led-row">
                <div class="led-item"><div class="led" id="ledCom"></div>COM</div>
                <div class="led-item"><div class="led" id="ledErr"></div>ERR</div>
                <div class="led-item"><div class="led" id="ledArm"></div>ARM</div>
            </div>

            <!-- MCU Voltage & Status Grid -->
            <div class="mcu-status-grid">
                <div class="mcu-box">
                    <div class="mcu-header"> Launch Controller</div>
                    <div class="mcu-metric">
                        <span class="mcu-label">入 力 電 圧:</span>
                        <span class="mcu-val" id="launchVolts">12.4 V</span>
                        <span class="mcu-tag ok" id="launchVoltTag">正常</span>
                    </div>
                    <div class="mcu-metric">
                        <span class="mcu-label">12Vバス電圧:</span>
                        <span class="mcu-val" id="launchBusVolts">12.0 V</span>
                        <span class="mcu-tag ok" id="launchBusVoltTag">正常</span>
                    </div>
                    <div class="mcu-metric">
                        <span class="mcu-label">RS485通信:</span>
                        <span class="mcu-val" id="rs485Val">● 接続</span>
                    </div>
                    <div class="mcu-metric">
                        <span class="mcu-label">アビオニクス:</span>
                        <span class="mcu-val" id="rocketNodeVal">● 結合</span>
                    </div>

                </div>

                <div class="mcu-box">
                    <div class="mcu-header">🚀 Satellite Controller</div>
                    <div class="mcu-metric">
                        <span class="mcu-label">入力電圧:</span>
                        <span class="mcu-val" id="satVolts">0.0 V</span>
                        <span class="mcu-tag warn" id="satVoltTag">N/A</span>
                    </div>
                    <div class="mcu-metric">
                        <span class="mcu-label">12Vバス電圧:</span>
                        <span class="mcu-val" id="satBusVolts">0.0 V</span>
                        <span class="mcu-tag warn" id="satBusVoltTag">N/A</span>
                    </div>
                    <div class="mcu-metric" style="margin-top:6px; border-top:1px solid #333; padding-top:6px;">
                        <span class="mcu-label">アームド状態:</span>
                        <span class="mcu-val" id="satArmedVal" style="font-weight:bold;">---</span>
                        <span class="mcu-tag warn" id="satArmedTag">N/A</span>
                    </div>
                </div>
            </div>

            <button class="tact-btn tact-zero" onmousedown="sendCmdOnce(7, 0)">圧力センサーゼロ点校正実行</button>
        </div>

        <!-- ===== Right Top: Safety & E-STOP & Sequence ===== -->
        <div class="card">
            <h3>🎛️ Safety & Sequence Control</h3>

            <!-- Safety Toggle Switch -->
            <div class="toggle-row">
                <div>
                    <div class="toggle-label">🛡️ セーフティ (SAFETY)</div>
                    <div class="toggle-sub">トグルスイッチ: 解除しないと操作不可</div>
                </div>
                <div class="toggle-track" id="safetyToggle" onclick="toggleSafety()">
                    <div class="toggle-knob"></div>
                </div>
            </div>

            <!-- Auto Purge Toggle Switch -->
            <div class="toggle-row">
                <div>
                    <div class="toggle-label">🧯 自動パージ (AUTO PURGE)</div>
                    <div class="toggle-sub">ON: 緊急停止・点火後にパージ弁（N₂）を自動開放</div>
                </div>
                <div class="toggle-track on" id="autoPurgeToggle" onclick="toggleAutoPurge()">
                    <div class="toggle-knob"></div>
                </div>
            </div>

            <!-- E-STOP -->
            <div class="estop-zone">
                <button class="estop-btn" id="estopBtn" onclick="pressEstop()">
                    緊急停止
                </button>
                <br>
                <button class="estop-reset-btn" id="estopResetBtn" onclick="resetEstop()">
                    🔓 エマージェンシーストップ解除 (回転リセット)
                </button>
            </div>

            <!-- Sequence Status -->
            <div class="seq-status seq-idle" id="seqStatus">IDLE: シーケンス待機中</div>

            <!-- Timers (High-Contrast Digital 7-Seg LED Displays) -->
            <div class="timer-row">
                <div class="timer-box">
                    <div class="timer-label">⏱️ T+ SEQ (全体経過)</div>
                    <div class="timer-value" id="timerSeq">00:00</div>
                </div>
                <div class="timer-box">
                    <div class="timer-label">⛽ T+ FILL (充填経過)</div>
                    <div class="timer-value" id="timerFill">00:00</div>
                </div>
                <div class="timer-box">
                    <div class="timer-label">🚀 T+ OPEN (開放経過)</div>
                    <div class="timer-value" id="timerOpen">00:00</div>
                </div>
            </div>

            <!-- Sequence Start (Tact/Momentary) -->
            <button class="tact-btn tact-seq" id="btnSeqStart"
                    onmousedown="sendCmdOnce(3, 0)" disabled>
                ⛽ シーケンス開始 (SEQUENCE START)
            </button>

            <!-- Confirm Ignition / Fill Confirm Button -->
            <button class="tact-btn tact-confirm" id="btnConfirm"
                    onclick="confirmAndIgnite()" disabled>
                ⛽ 充填確認 / 🔥 点火 (CONFIRM & IGNITE)
            </button>

            <!-- Peaceful Stop -->
            <button class="tact-btn tact-peace" id="btnPeace"
                    onmousedown="sendCmdOnce(2, 0)" disabled>
                ⏹️ 通常停止 (PEACEFUL STOP)
            </button>
        </div>

        <!-- ===== Left Bottom: Valve Toggles ===== -->
        <div class="card" style="grid-column: 1 / -1;">
            <div style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid var(--border); padding-bottom:4px; margin-bottom:8px;">
                <h3 style="border-bottom:none; margin-bottom:0; padding-bottom:0;">⚙️ 電磁弁 手動操作 & フィードバック (Solenoid Valves)</h3>
                <span class="pokayoke-badge" id="pokayokeValveTag" style="font-size:0.65rem;">🔒 セーフティ施錠中</span>
            </div>
            <div class="valve-grid" id="valveGrid"></div>

            <!-- RAW Data Collapsible -->
            <div style="margin-top: 15px; border-top: 1px dashed var(--border); padding-top: 10px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                    <div style="font-size:0.85rem; font-weight:700; color:var(--text-dim); cursor:pointer; display:flex; align-items:center; gap:5px; user-select:none;" onclick="toggleRawData()">
                        <span id="rawDataArrow" style="transition:transform 0.2s;">▶</span> RAW Telemetry Data
                    </div>
                    <div style="font-size:1.1rem; color:var(--blue); cursor:pointer;" onclick="toggleRawHelp()" title="RAWデータの見方">
                        ℹ️
                    </div>
                </div>
                
                <!-- Help Box (Hidden by default) -->
                <div id="rawHelpBox" style="display:none; margin-top:8px; padding:10px; background:var(--bg-card-alt); border:1px solid var(--blue); border-radius:6px; font-size:0.75rem; color:var(--text-dim); line-height:1.5;">
                    <strong style="color:var(--blue);">[ データの見方 (MsgPackフォーマット) ]</strong><br>
                    配列内に左から順に以下のデータが格納されています。<br>
                    <ol style="margin-left:20px; margin-top:4px; margin-bottom:8px;">
                        <li><code>packetId</code>: パケットID (テレメトリは 35)</li>
                        <li><code>cmdState</code>: 操作コマンド (10進数)</li>
                        <li><code>fbState</code>: 機体側からの電磁弁状態フィードバック (10進数)</li>
                        <li><code>seqFlag</code>: 自動シーケンス・通信状態フラグ (10進数)</li>
                        <li><code>pressure</code>: N2Oタンク圧力 [MPa]</li>
                        <li><code>dummyLimitSwitch</code>: リミットスイッチ状態 (現状0固定)</li>
                        <li><code>launchV</code>: 操作卓側 入力電圧 [V]</li>
                        <li><code>launchBusV</code>: 操作卓側 12Vバス電圧 [V]</li>
                        <li><code>satV</code>: サテライト側 入力電圧 [V]</li>
                        <li><code>satBusV</code>: サテライト側 12Vバス電圧 [V]</li>
                    </ol>

                    <strong style="color:var(--blue);">[ 詳細: 頻出する状態値（チートシート） ]</strong><br>
                    <div style="margin-top:4px; padding:6px 10px; background:#e2e8f0; border-radius:4px; color:var(--text); font-size:0.8rem; line-height:1.6;">
                        計算不要で一目で状態が分かるように、よく出る数値をリスト化しました！<br>
                        
                        <b style="color:var(--purple); margin-top:10px; display:inline-block;">■ 第3要素: fbState (電磁弁の実際の状態)</b><br>
                        <ul style="margin:2px 0 6px 20px; padding:0;">
                            <li><b>0</b> : 全ての弁が閉鎖（待機）、または機体と通信断絶</li>
                            <li><b>2</b> : <code>FILL (充填弁)</code> のみ開放</li>
                            <li><b>4</b> : <code>DUMP (排出弁)</code> のみ開放</li>
                            <li><b>6</b> : <code>FILL</code> と <code>DUMP</code> の両方が開放</li>
                            <li><b>128</b> : <code>PURGE (パージ弁)</code> のみ開放</li>
                            <li><b>142</b> : <code>PURGE</code>, <code>OXYGEN</code>, <code>DUMP</code>, <code>FILL</code> の4つが同時に開放</li>
                        </ul>

                        <b style="color:var(--purple); margin-top:10px; display:inline-block;">■ 第4要素: seqFlag (通信・システム状態)</b><br>
                        <ul style="margin:2px 0 6px 20px; padding:0;">
                            <li><b>16</b> : 無線(RasPi)のみ繋がっている（有線は断絶、アームドOFF）</li>
                            <li><b>48</b> : 有線も無線も正常だが、アームドはOFF（待機状態）</li>
                            <li><b>112</b> : <b>【完全正常】</b> 通信OK ＆ アームドON（シーケンス開始可能！）</li>
                            <li><b>113</b> : 上記の正常状態 ＋ <b>エマージェンシーストップ発動中</b></li>
                            <li><b>114</b> : 上記の正常状態 ＋ <b>充填シーケンス実行中</b></li>
                            <li><b>116</b> : 上記の正常状態 ＋ <b>点火シーケンス実行中</b></li>
                        </ul>
                    </div>
                </div>
                
                <!-- Data Box (Hidden by default) -->
                <div id="rawDataContainer" style="display:none; margin-top:8px;">
                    <div class="mcu-val" id="rawTelemetry" style="font-size:0.85rem; font-family:'JetBrains Mono', 'Share Tech Mono', monospace; color:var(--text-bright); word-break:break-all; background:#f1f5f9; padding:8px 10px; border:1px solid var(--border); border-radius:6px; box-shadow:inset 0 1px 3px rgba(0,0,0,0.05); min-height:36px; display:flex; align-items:center;">
                        ---
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Log Modal Overlay -->
    <div class="modal-overlay" id="logModal" onclick="if(event.target===this)closeLogModal()">
        <div class="modal-content">
            <div class="modal-header">
                <span>📁 GSE Telemetry CSV Logs (自動保存ログ一覧)</span>
                <span style="cursor:pointer; font-size:1.2rem; color:var(--text-dim);" onclick="closeLogModal()">✕</span>
            </div>
            <div class="modal-body">
                <div style="margin-bottom:12px; display:flex; justify-content:space-between; align-items:center;">
                    <span style="font-size:0.75rem; color:var(--text-dim);">保存場所: <code>tools/raspi_gse_control/logs/</code></span>
                    <a href="/api/logs/latest" target="_blank" class="btn-dl-sm" style="padding:6px 12px; font-size:0.78rem; background:#22c55e;">
                        📥 最新ログ (現在記録中) をDL
                    </a>
                </div>
                <table class="log-table">
                    <thead>
                        <tr>
                            <th>ファイル名</th>
                            <th>作成日時</th>
                            <th>サイズ</th>
                            <th>ステータス</th>
                            <th>操作</th>
                        </tr>
                    </thead>
                    <tbody id="logTableBody">
                        <tr><td colspan="5">ログ一覧を読み込み中...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
    </div>

    <script>
        /* ===== RAW Data UI ===== */
        let rawDataOpen = false;
        let rawHelpOpen = false;
        function toggleRawData() {
            rawDataOpen = !rawDataOpen;
            document.getElementById('rawDataContainer').style.display = rawDataOpen ? 'block' : 'none';
            document.getElementById('rawDataArrow').style.transform = rawDataOpen ? 'rotate(90deg)' : 'rotate(0deg)';
        }
        function toggleRawHelp() {
            rawHelpOpen = !rawHelpOpen;
            document.getElementById('rawHelpBox').style.display = rawHelpOpen ? 'block' : 'none';
        }

        /* ===== State ===== */
        let armed = false;
        let autoPurge = true;
        let estopLocked = false;
        let cmdSentThisPress = false;
        let fillActiveState = false;
        let ignitionActiveState = false;
        let canConfirmState = false;
        const VALVE_NAMES = ["SHIFT","FILL","DUMP","OXYGEN","IGNITER","OPEN","CLOSE","PURGE"];
        let valveStates = {};
        VALVE_NAMES.forEach(v => valveStates[v] = false);

        function isSequenceActive() {
            return fillActiveState || ignitionActiveState;
        }

        /* ===== Timer state ===== */
        let seqStartTime = null;   // シーケンス開始時刻
        let fillStartTime = null;  // FILL ON 時刻
        let fillElapsed = null;    // FILL停止後の確定秒数
        let openStartTime = null;  // OPEN ON 時刻
        let openElapsed = null;    // OPEN停止（通常停止/エマスト含む）後の確定秒数
        let prevFillOn = false;
        let prevOpenOn = false;
        let prevSeqActive = false;

        /* ===== Init valve grid ===== */
        function buildValveGrid() {
            const grid = document.getElementById('valveGrid');
            grid.innerHTML = '';
            VALVE_NAMES.forEach(name => {
                const item = document.createElement('div');
                item.className = 'valve-item';
                item.innerHTML = `
                    <span class="valve-name">${name}</span>
                    <div class="valve-leds">
                        <span class="valve-led-label">CMD</span>
                        <div class="valve-led" id="cmd_${name}"></div>
                        <span class="valve-led-label">FB</span>
                        <div class="valve-led" id="fb_${name}"></div>
                    </div>
                    <div class="toggle-track ${armed ? '' : 'disabled'}" id="vt_${name}" onclick="toggleValve('${name}')">
                        <div class="toggle-knob"></div>
                    </div>`;
                grid.appendChild(item);
            });
        }
        buildValveGrid();

        /* ===== API helpers ===== */
        function sendCmd(cmdType, param) {
            fetch('/api/command', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({cmd_type: cmdType, param: param})
            }).then(r=>r.json()).then(r=>{
                console.log(r);
                if (r.status && r.status !== 'ok') {
                    alert(r.message || ('コマンド実行が拒否されました (エラー: ' + r.status + ')'));
                }
            }).catch(e=>console.error(e));
        }

        /* Momentary: fire only once per press */
        function sendCmdOnce(cmdType, param) {
            if (cmdSentThisPress) return;
            cmdSentThisPress = true;
            sendCmd(cmdType, param);
            setTimeout(() => { cmdSentThisPress = false; }, 300);
        }

        /* ===== Safety toggle ===== */
        function toggleSafety() {
            armed = !armed;
            sendCmd(5, armed ? 1 : 0);
            updateSafetyUI();
        }
        function updateSafetyUI() {
            const el = document.getElementById('safetyToggle');
            if (armed) { el.classList.add('on'); } else { el.classList.remove('on'); }

            const seqActive = isSequenceActive();
            const dumpIsOn = valveStates['DUMP'] || false;

            // Enable/disable sequence start based on safety, estop, sequence state, AND DUMP valve ON status
            const btnSeq = document.getElementById('btnSeqStart');
            if (seqActive || estopLocked) {
                btnSeq.disabled = true;
                btnSeq.innerText = "⛽ シーケンス開始 (SEQUENCE START)";
            } else if (!armed) {
                btnSeq.disabled = true;
                btnSeq.innerText = "🔒 シーケンス開始 (要 セーフティ解除)";
            } else if (!dumpIsOn) {
                btnSeq.disabled = true;
                btnSeq.innerText = "⚠️ シーケンス開始 (要 DUMP ON 開放)";
            } else {
                btnSeq.disabled = false;
                btnSeq.innerText = "⛽ シーケンス開始 (SEQUENCE START)";
            }

            document.getElementById('btnConfirm').disabled  = !armed || estopLocked || !seqActive;
            document.getElementById('btnPeace').disabled    = !armed || estopLocked;

            // Highlight confirm button when ready
            const btnConf = document.getElementById('btnConfirm');
            if (canConfirmState) {
                btnConf.classList.add('ready');
                btnConf.innerText = "⛽ 目視確認 OK → 🔥 点火開始 (IGNITE)";
            } else {
                btnConf.classList.remove('ready');
                btnConf.innerText = "⛽ 充填確認 / 🔥 点火 (CONFIRM & IGNITE)";
            }

            // Update valve toggles: DISABLE during automatic sequences to prevent overwriting MCU sequence
            VALVE_NAMES.forEach(name => {
                const vt = document.getElementById('vt_' + name);
                if (vt) {
                    if (armed && !estopLocked && !seqActive) {
                        vt.classList.remove('disabled');
                    } else {
                        vt.classList.add('disabled');
                    }
                }
            });
        }

        /* ===== Auto Purge toggle ===== */
        function toggleAutoPurge() {
            autoPurge = !autoPurge;
            fetch('/api/auto_purge_toggle', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({state: autoPurge})
            }).then(r=>r.json()).then(r=>{
                if (r.auto_purge !== undefined) autoPurge = r.auto_purge;
                updateAutoPurgeUI();
            }).catch(e=>console.error(e));
        }
        function updateAutoPurgeUI() {
            const el = document.getElementById('autoPurgeToggle');
            if (el) {
                if (autoPurge) { el.classList.add('on'); } else { el.classList.remove('on'); }
            }
        }

        /* ===== E-STOP (latching) ===== */
        function pressEstop() {
            if (estopLocked) return;
            estopLocked = true;
            sendCmd(1, 0);

            const btn = document.getElementById('estopBtn');
            btn.classList.add('locked');
            document.getElementById('estopResetBtn').classList.add('visible');
            updateSafetyUI();
        }
        function resetEstop() {
            if (!confirm('【エマージェンシーストップ解除確認】\\n緊急停止状態を解除しますか？\\nスイッチを回転させて回路を復帰させる操作に相当します。')) return;
            estopLocked = false;
            sendCmd(2, 0); // Peaceful stop to clear flags

            const btn = document.getElementById('estopBtn');
            btn.classList.remove('locked');
            document.getElementById('estopResetBtn').classList.remove('visible');
            updateSafetyUI();
        }

        /* ===== Valve toggles ===== */
        function toggleValve(name) {
            if (!armed || estopLocked || isSequenceActive()) return;
            valveStates[name] = !valveStates[name];

            // Update toggle UI
            const vt = document.getElementById('vt_' + name);
            if (valveStates[name]) { vt.classList.add('on'); } else { vt.classList.remove('on'); }

            // Send valve toggle
            fetch('/api/valve_toggle', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({valve: name, state: valveStates[name] ? 1 : 0})
            }).then(r=>r.json()).then(r=>console.log(r)).catch(e=>console.error(e));
        }

        /* ===== Confirm + Ignite (1-button with dialog) ===== */
        function confirmAndIgnite() {
            if (!armed || estopLocked || !isSequenceActive()) return;
            if (!confirm('【最終確認 (CONFIRM / IGNITE)】\\n\\n⚠️  3人同時押し確認スイッチの代替操作です。\\n本当に実行しますか？')) return;
            sendCmd(4, 0);
        }

        /* ===== Telemetry polling ===== */
        function updateTelemetry() {
            fetch('/api/telemetry').then(r=>r.json()).then(data => {
                // Connection badge
                const badge = document.getElementById('connBadge');
                const modeTag = document.getElementById('modeTag');
                if (data.demo_mode) { modeTag.innerText = 'DEMO'; modeTag.style.background = '#8b5cf6'; }
                if (data.connected) {
                    badge.className = 'conn-badge conn-ok';
                    badge.innerText = data.demo_mode ? '● DEMO ACTIVE' : '● LINK OK';
                } else {
                    badge.className = 'conn-badge conn-ng';
                    badge.innerText = '● DISCONNECTED';
                }

                // REC badge count update
                if (data.record_count !== undefined) {
                    document.getElementById('recBadge').innerText = '● REC (' + data.record_count + ')';
                }

                // Pressure & Sensor mA
                document.getElementById('pressureVal').innerText = data.pressure_MPa.toFixed(3);
                if (data.vesim_current_mA !== undefined) {
                    document.getElementById('vesimCurrentVal').innerText = data.vesim_current_mA.toFixed(2) + ' mA';
                }

                // MCU Voltages & Status Health
                if (data.launch_voltage_V !== undefined) {
                    document.getElementById('launchVolts').innerText = data.launch_voltage_V.toFixed(1) + ' V';
                    const lTag = document.getElementById('launchVoltTag');
                    if (data.launch_voltage_V < 11.5) {
                        lTag.className = 'mcu-tag warn'; lTag.innerText = '低電圧';
                    } else {
                        lTag.className = 'mcu-tag ok'; lTag.innerText = '正常';
                    }
                }
                if (data.launch_bus_voltage_V !== undefined) {
                    document.getElementById('launchBusVolts').innerText = data.launch_bus_voltage_V.toFixed(1) + ' V';
                    const lbTag = document.getElementById('launchBusVoltTag');
                    if (data.launch_bus_voltage_V < 11.0) {
                        lbTag.className = 'mcu-tag warn'; lbTag.innerText = '低電圧';
                    } else {
                        lbTag.className = 'mcu-tag ok'; lbTag.innerText = '正常';
                    }
                }
                if (data.sat_voltage_V !== undefined && data.sat_voltage_V !== null) {
                    document.getElementById('satVolts').innerText = data.sat_voltage_V.toFixed(1) + ' V';
                    const sTag = document.getElementById('satVoltTag');
                    if (data.sat_voltage_V < 0.1) {
                        sTag.className = 'mcu-tag warn'; sTag.innerText = '0V/停電';
                    } else if (data.sat_voltage_V < 10.5) {
                        sTag.className = 'mcu-tag warn'; sTag.innerText = '低電圧';
                    } else {
                        sTag.className = 'mcu-tag ok'; sTag.innerText = '正常';
                    }
                } else {
                    document.getElementById('satVolts').innerText = '--- V';
                    const sTag = document.getElementById('satVoltTag');
                    sTag.className = 'mcu-tag warn'; sTag.innerText = 'N/A';
                }
                
                if (data.sat_bus_voltage_V !== undefined && data.sat_bus_voltage_V !== null) {
                    document.getElementById('satBusVolts').innerText = data.sat_bus_voltage_V.toFixed(1) + ' V';
                    const sbTag = document.getElementById('satBusVoltTag');
                    if (data.sat_bus_voltage_V < 0.1) {
                        sbTag.className = 'mcu-tag warn'; sbTag.innerText = '0V/停電';
                    } else if (data.sat_bus_voltage_V < 11.0) {
                        sbTag.className = 'mcu-tag warn'; sbTag.innerText = '低電圧';
                    } else {
                        sbTag.className = 'mcu-tag ok'; sbTag.innerText = '正常';
                    }
                } else {
                    document.getElementById('satBusVolts').innerText = '--- V';
                    const sbTag = document.getElementById('satBusVoltTag');
                    sbTag.className = 'mcu-tag warn'; sbTag.innerText = 'N/A';
                }

                if (data.sat_armed !== undefined) {
                    const saVal = document.getElementById('satArmedVal');
                    const saTag = document.getElementById('satArmedTag');
                    if (data.sat_armed) {
                        saVal.innerText = 'ON (アームド)'; saVal.style.color = 'var(--green)';
                        saTag.className = 'mcu-tag ok'; saTag.innerText = '準備完了';
                    } else {
                        saVal.innerText = 'OFF (セーフティ)'; saVal.style.color = 'var(--red)';
                        saTag.className = 'mcu-tag warn'; saTag.innerText = '⚠️ アームド待機中';
                    }
                }

                if (data.rocket_node_ok !== undefined) {
                    const rsVal = document.getElementById('rs485Val');
                    const rnVal = document.getElementById('rocketNodeVal');
                    if (data.rocket_node_ok) {
                        rsVal.innerText = '● 接続'; rsVal.style.color = 'var(--green)';
                        rnVal.innerText = '● 結合'; rnVal.style.color = 'var(--green)';
                    } else {
                        rsVal.innerText = '○ 切断'; rsVal.style.color = 'var(--red)';
                        rnVal.innerText = '○ 分離'; rnVal.style.color = 'var(--red)';
                    }
                }


                if (data.raw_telemetry !== undefined) {
                    document.getElementById('rawTelemetry').innerText = data.raw_telemetry;
                }

                // Prominent Large Status Banner Update
                const lsb = document.getElementById('largeStatusBanner');
                if (data.emergency_stop) {
                    lsb.className = 'status-banner-large estop';
                    lsb.innerText = '🚨 EMERGENCY STOP 発動中';
                } else if (data.ignition_active) {
                    lsb.className = 'status-banner-large ignite';
                    lsb.innerText = '🔥 IGNITING 自動点火シーケンス中';
                } else if (data.can_confirm) {
                    lsb.className = 'status-banner-large ready';
                    lsb.innerText = '⛽ CONFIRM READY 充填完了・点火準備OK';
                } else if (data.fill_active) {
                    lsb.className = 'status-banner-large fill';
                    lsb.innerText = '⛽ FILLING 遠隔自動充填中...';
                } else if (data.armed_state) {
                    lsb.className = 'status-banner-large armed';
                    lsb.innerText = '🛡️ ARMED セーフティ解除済み';
                } else {
                    lsb.className = 'status-banner-large idle';
                    lsb.innerText = 'SYSTEM STANDBY (待機中)';
                }

                // Toyota Andon & Poka-Yoke Status Update
                const a1 = document.getElementById('andon1');
                const a2 = document.getElementById('andon2');
                const a3 = document.getElementById('andon3');
                const a4 = document.getElementById('andon4');
                const pyBadge = document.getElementById('pokayokeBadge');
                const pyValveTag = document.getElementById('pokayokeValveTag');

                if (a1 && a2 && a3 && a4) {
                    a1.classList.remove('active');
                    a2.classList.remove('active');
                    a3.classList.remove('active');
                    a4.classList.remove('active');

                    if (data.emergency_stop) {
                        a4.classList.add('active');
                        if (pyBadge) {
                            pyBadge.className = 'pokayoke-badge lock';
                            pyBadge.innerText = '🔴 エマージェンシーストップ 発動中';
                        }
                        if (pyValveTag) pyValveTag.innerText = '⛔ 停止中: 全ての弁を自動で安全方向へ';
                    } else if (data.ignition_active) {
                        a3.classList.add('active');
                        if (pyBadge) {
                            pyBadge.className = 'pokayoke-badge safe';
                            pyBadge.innerText = '🔥 [点火シーケンス実行中] 燃焼制御中...';
                        }
                        if (pyValveTag) pyValveTag.innerText = '⛔ 点火中: 手動弁操作ロック中';
                    } else if (data.can_confirm) {
                        a2.classList.add('active');
                        if (pyBadge) {
                            pyBadge.className = 'pokayoke-badge safe';
                            pyBadge.innerText = '🟠 [充填完了] 目視確認待ち ➔ 確認後に点火シーケンス開始可能';
                        }
                        if (pyValveTag) pyValveTag.innerText = '⛔ 充填完了: 手動弁操作ロック中';
                    } else if (data.fill_active) {
                        a2.classList.add('active');
                        if (pyBadge) {
                            pyBadge.className = 'pokayoke-badge safe';
                            pyBadge.innerText = '🔵 [2. 充填工程実行中] N2O自動充填中... (手動弁保護中)';
                        }
                        if (pyValveTag) pyValveTag.innerText = '⛔ 充填中: 手動弁トグル禁止';
                    } else {
                        a1.classList.add('active');
                        const dumpIsOn = (data.valves_cmd && data.valves_cmd['DUMP']) || false;
                        if (!data.armed_state) {
                            if (pyBadge) {
                                pyBadge.className = 'pokayoke-badge';
                                pyBadge.innerText = '🔒 セーフティ解除なしではシーケンス起動不可';
                            }
                            if (pyValveTag) pyValveTag.innerText = '🔒 セーフティ施錠中';
                        } else if (!dumpIsOn) {
                            if (pyBadge) {
                                pyBadge.className = 'pokayoke-badge lock';
                                pyBadge.innerText = '⚠️ DUMP弁(排出弁)をON(閉鎖)に設定しないとシーケンスは起動できません';
                            }
                            if (pyValveTag) pyValveTag.innerText = '⚠️ シーケンス準備: DUMP弁をONに設定してください';
                        } else {
                            if (pyBadge) {
                                pyBadge.className = 'pokayoke-badge safe';
                                pyBadge.innerText = '🛡️ DUMP ON確認完了 ➔ シーケンス開始可能';
                            }
                            if (pyValveTag) pyValveTag.innerText = '🛡️ 通常手動トグル可能';
                        }
                    }
                }

                // Status LEDs
                setLed('ledCom', data.connected, 'green');
                setLed('ledErr', data.emergency_stop, 'red');
                setLed('ledArm', data.armed_state, 'yellow');

                // Sync sequence & armed states from telemetry
                armed = data.armed_state;
                fillActiveState = data.fill_active || false;
                ignitionActiveState = data.ignition_active || false;
                canConfirmState = data.can_confirm || false;

                updateSafetyUI();
                if (data.auto_purge !== undefined) {
                    autoPurge = data.auto_purge;
                    updateAutoPurgeUI();
                }

                // Sequence status
                const ss = document.getElementById('seqStatus');
                if (data.emergency_stop) {
                    ss.className = 'seq-status seq-estop'; ss.innerText = '🚨 EMERGENCY STOP 発動中';
                } else if (data.ignition_active) {
                    ss.className = 'seq-status seq-ign'; ss.innerText = '🔥 点火シーケンス実行中...';
                } else if (data.fill_active) {
                    ss.className = 'seq-status seq-fill';
                    ss.innerText = data.can_confirm
                        ? '⛽ 充填完了 → 目視確認待ち (点火可能)'
                        : '⛽ 充填シーケンス実行中...';
                } else {
                    ss.className = 'seq-status seq-idle'; ss.innerText = 'IDLE: シーケンス待機中';
                }

                // ===== Timer tracking =====
                const now = Date.now();
                const seqActive = data.fill_active || data.ignition_active;
                const fillOn = data.valves_cmd['FILL'] || false;
                const openOn = data.valves_cmd['OPEN'] || false;

                // T+ SEQ: シーケンス開始からの経過秒
                if (seqActive && !prevSeqActive) { seqStartTime = now; }
                if (!seqActive && !data.emergency_stop) { seqStartTime = null; }
                prevSeqActive = seqActive;

                // T+ FILL: FILL ON からの経過 / FILL OFF で停止・確定
                if (fillOn && !prevFillOn) { fillStartTime = now; fillElapsed = null; }
                if (!fillOn && prevFillOn && fillStartTime) { fillElapsed = (now - fillStartTime) / 1000; fillStartTime = null; }
                prevFillOn = fillOn;

                // T+ OPEN: OPEN ON からの経過 / OPEN OFF (通常停止/エマスト含む) で停止・確定表示
                if (openOn && !prevOpenOn) { openStartTime = now; openElapsed = null; }
                if (!openOn && prevOpenOn && openStartTime) { openElapsed = (now - openStartTime) / 1000; openStartTime = null; }
                prevOpenOn = openOn;

                // Timer display update
                updateTimerDisplay('timerSeq',  seqStartTime,  null,         seqActive);
                updateTimerDisplay('timerFill', fillStartTime, fillElapsed,  fillOn);
                updateTimerDisplay('timerOpen', openStartTime, openElapsed,  openOn);

                // Valve CMD/FB LEDs
                VALVE_NAMES.forEach(name => {
                    const cmdEl = document.getElementById('cmd_' + name);
                    const fbEl  = document.getElementById('fb_' + name);
                    if (cmdEl) {
                        cmdEl.className = 'valve-led' + (data.valves_cmd[name] ? ' cmd-on' : '');
                    }
                    if (fbEl) {
                        fbEl.className = 'valve-led' + (data.valves_fb[name] ? ' fb-on' : '');
                    }

                    // Sync valve toggle UI from telemetry cmd state
                    valveStates[name] = data.valves_cmd[name];
                    const vt = document.getElementById('vt_' + name);
                    if (vt) {
                        if (valveStates[name]) { vt.classList.add('on'); } else { vt.classList.remove('on'); }
                    }
                });

            }).catch(e => console.error(e));
        }

        function setLed(id, on, color) {
            const el = document.getElementById(id);
            el.className = 'led' + (on ? (' led-on-' + color) : '');
        }

        function updateTimerDisplay(id, startTime, frozenSec, isActive) {
            const el = document.getElementById(id);
            let sec = null;
            if (frozenSec !== null && frozenSec !== undefined) {
                sec = frozenSec;
            } else if (startTime) {
                sec = (Date.now() - startTime) / 1000;
            }

            if (sec !== null) {
                const m = Math.floor(sec / 60);
                const s = Math.floor(sec % 60);
                const ms = Math.floor((sec % 1) * 10);
                el.innerText = String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0') + '.' + ms;
                el.className = 'timer-value' + (isActive ? ' active' : (frozenSec !== null ? ' warn' : ''));
            } else {
                el.innerText = '--:--';
                el.className = 'timer-value';
            }
        }

        /* ===== Log Modal JS ===== */
        function openLogModal() {
            document.getElementById('logModal').classList.add('open');
            fetchLogList();
        }
        function closeLogModal() {
            document.getElementById('logModal').classList.remove('open');
        }
        function fetchLogList() {
            fetch('/api/logs/list').then(r=>r.json()).then(data => {
                const tbody = document.getElementById('logTableBody');
                tbody.innerHTML = '';
                if (!data.logs || data.logs.length === 0) {
                    tbody.innerHTML = '<tr><td colspan="5">ログファイルが見つかりません</td></tr>';
                    return;
                }
                data.logs.forEach(item => {
                    const tr = document.createElement('tr');
                    const statusTag = item.current
                        ? `<span style="color:var(--green); font-weight:bold;">● 記録中 (${item.records || 0}件)</span>`
                        : `<span style="color:var(--text-dim);">保存済み</span>`;
                    tr.innerHTML = `
                        <td><code>${item.filename}</code></td>
                        <td>${item.mtime}</td>
                        <td>${item.size_kb} KB</td>
                        <td>${statusTag}</td>
                        <td>
                            <a href="/viewer?log=${item.filename}" target="_blank" class="btn-dl-sm" style="background:#0ea5e9; margin-right:5px; text-decoration:none;">📊 解析</a>
                            <a href="/api/logs/download/${item.filename}" target="_blank" class="btn-dl-sm" style="text-decoration:none;">📥 DL</a>
                        </td>
                    `;
                    tbody.appendChild(tr);
                });
            }).catch(e=>console.error(e));
        }

        setInterval(updateTelemetry, 200);
    </script>
</body>
</html>
"""

VIEWER_HTML = """
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <title>GSE Telemetry Log Viewer</title>
    <script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/PapaParse/5.4.1/papaparse.min.js"></script>
    <style>
        body { font-family: 'Inter', sans-serif; margin: 0; padding: 15px; background: #f1f5f9; color: #0f172a; }
        .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid #cbd5e1; padding-bottom: 10px; margin-bottom: 15px; }
        .loading { text-align: center; font-size: 1.2rem; margin-top: 50px; color: #64748b; font-weight: bold; }
        .chart-container { width: 100%; height: 80vh; background: #ffffff; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); padding: 10px; border: 1px solid #e2e8f0; }
        .btn { padding: 6px 16px; background: #0284c7; color: white; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; }
        .btn:hover { background: #0369a1; }
    </style>
</head>
<body>
    <div class="header">
        <h2 style="margin:0; color:#0f172a;">📊 GSE Log Analyzer</h2>
        <div style="display:flex; align-items:center; gap:15px;">
            <span style="font-size:0.9rem; color:#475569;">Target Log: <strong>{{ filename }}</strong></span>
            <button class="btn" onclick="window.close()">閉じる</button>
        </div>
    </div>
    <div id="loading" class="loading">⏳ CSV データを読み込み、解析中です...</div>
    <div id="chartContainer" class="chart-container" style="display:none;"></div>
    
    <script>
        const filename = '{{ filename }}';
        if (!filename || filename === 'None') {
            document.getElementById('loading').innerText = "❌ エラー: ログファイルが指定されていません。";
        } else {
            Papa.parse('/api/logs/download/' + filename, {
                download: true,
                header: true,
                dynamicTyping: true,
                skipEmptyLines: true,
                complete: function(results) {
                    document.getElementById('loading').style.display = 'none';
                    document.getElementById('chartContainer').style.display = 'block';
                    renderChart(results.data);
                },
                error: function(err) {
                    document.getElementById('loading').innerText = "❌ データの読み込みに失敗しました: " + err.message;
                }
            });
        }

        function renderChart(data) {
            // 時系列データ (datetime_jst or timestamp_s)
            const timeData = data.map(row => row.datetime_jst || row.timestamp_s);
            
            // アナログ値
            const pressureData = data.map(row => row.pressure_MPa);
            const launchVoltData = data.map(row => row.launch_voltage_V);
            const satVoltData = data.map(row => row.sat_voltage_V);
            
            // デジタルフラグ
            const armedData = data.map(row => row.armed_state ? 1 : 0);
            const estopData = data.map(row => row.emergency_stop ? 1 : 0);
            const fillActData = data.map(row => row.fill_active ? 1 : 0);
            const ignActData = data.map(row => row.ignition_active ? 1 : 0);

            const traces = [
                {
                    x: timeData, y: pressureData,
                    name: 'Tank Pressure [MPa]',
                    type: 'scatter', mode: 'lines',
                    line: {color: '#0284c7', width: 2.5},
                    yaxis: 'y1'
                },
                {
                    x: timeData, y: launchVoltData,
                    name: 'Launch Voltage [V]',
                    type: 'scatter', mode: 'lines',
                    line: {color: '#ea580c', width: 2},
                    yaxis: 'y2'
                },
                {
                    x: timeData, y: satVoltData,
                    name: 'Sat Voltage [V]',
                    type: 'scatter', mode: 'lines',
                    line: {color: '#16a34a', width: 2},
                    yaxis: 'y2'
                },
                {
                    x: timeData, y: armedData,
                    name: 'Armed',
                    type: 'scatter', mode: 'lines',
                    line: {color: '#ca8a04', width: 1.5, dash: 'dot'},
                    yaxis: 'y3'
                }
            ];

            // Valve States (CMD and FB)
            const VALVE_NAMES = ["SHIFT","FILL","DUMP","OXYGEN","IGNITER","OPEN","CLOSE","PURGE"];
            const vColors = ['#64748b', '#3b82f6', '#8b5cf6', '#0ea5e9', '#f43f5e', '#f59e0b', '#10b981', '#a855f7'];
            
            VALVE_NAMES.forEach((vname, idx) => {
                // Add FB (Feedback) Trace
                traces.push({
                    x: timeData, 
                    y: data.map(row => row[`fb_${vname}`] ? 1 : 0),
                    name: `${vname} FB`,
                    type: 'scatter', mode: 'lines',
                    line: {color: vColors[idx], width: 1.5, dash: 'solid'},
                    yaxis: 'y3',
                    visible: 'legendonly' // デフォルトは非表示
                });
                // Add CMD (Command) Trace
                traces.push({
                    x: timeData, 
                    y: data.map(row => row[`cmd_${vname}`] ? 1 : 0),
                    name: `${vname} CMD`,
                    type: 'scatter', mode: 'lines',
                    line: {color: vColors[idx], width: 1.5, dash: 'dot'},
                    yaxis: 'y3',
                    visible: 'legendonly' // デフォルトは非表示
                });
            });

            // Add E-STOP at the end so it draws on top
            traces.push({
                x: timeData, y: estopData,
                name: 'E-STOP',
                type: 'scatter', mode: 'lines',
                line: {color: '#dc2626', width: 2},
                yaxis: 'y3',
                fill: 'tozeroy', fillcolor: 'rgba(220, 38, 38, 0.1)'
            });

            const layout = {
                title: 'GSE Telemetry Data over Time',
                plot_bgcolor: '#ffffff',
                paper_bgcolor: '#ffffff',
                hovermode: 'x unified',
                margin: { l: 60, r: 60, t: 50, b: 50 },
                legend: { orientation: 'h', y: -0.15 },
                xaxis: { 
                    title: 'Time',
                    showgrid: true, gridcolor: '#e2e8f0'
                },
                yaxis: { 
                    title: 'Pressure [MPa]', 
                    titlefont: {color: '#0284c7'},
                    tickfont: {color: '#0284c7'},
                    range: [-0.5, 12],
                    showgrid: true, gridcolor: '#e2e8f0'
                },
                yaxis2: {
                    title: 'Voltage [V]',
                    titlefont: {color: '#16a34a'},
                    tickfont: {color: '#16a34a'},
                    anchor: 'x', overlaying: 'y', side: 'right',
                    range: [-1, 16],
                    showgrid: false
                },
                yaxis3: {
                    title: 'Digital Flags',
                    anchor: 'free', overlaying: 'y', side: 'right', position: 0.95,
                    range: [0, 1.1],
                    showgrid: false, showticklabels: false
                }
            };

            Plotly.newPlot('chartContainer', traces, layout, {responsive: true});
        }
    </script>
</body>
</html>
"""

@app.route('/viewer')
def log_viewer():
    """解析ビューアのHTMLを返す"""
    log_filename = request.args.get('log', '')
    return render_template_string(VIEWER_HTML, filename=log_filename)

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
        if cmd_type == CMD_ARM_SAFETY:
            with gse_state.lock:
                gse_state.armed_state = bool(param != 0)

        if cmd_type == CMD_FILL_START or cmd_type == CMD_IGNITION_START:
            with gse_state.lock:
                dump_on = bool(gse_state.cmd_state & (1 << 2))
                sat_armed = gse_state.sat_armed or gse_state.demo_mode
                seq_active = gse_state.fill_active or gse_state.ignition_active
            if not sat_armed:
                print("[SERVER REJECT] Sequence start blocked: Satellite is not armed!")
                return jsonify({
                    "status": "blocked_sat_not_armed",
                    "message": "シーケンス開始エラー: サテライト側のアームドスイッチがONになっていません（Poka-yoke）。"
                }), 403
            if cmd_type == CMD_FILL_START and not dump_on:
                print("[SERVER REJECT] Sequence start blocked: DUMP valve is OFF!")
                return jsonify({
                    "status": "blocked_dump_off",
                    "message": "シーケンス開始エラー: DUMP弁(排出弁)をONに設定してからシーケンスを開始してください。"
                }), 403
            if cmd_type == CMD_IGNITION_START and not seq_active:
                print("[SERVER REJECT] Ignition start blocked: Sequence not active!")
                return jsonify({
                    "status": "blocked_no_seq",
                    "message": "点火エラー: シーケンスが開始されていないため、確認・点火操作は無効化されています（Poka-yoke）。"
                }), 403

        success = send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, cmd_type, param)
        return jsonify({"status": "ok" if success else "failed"})
    return jsonify({"status": "invalid_request"}), 400

@app.route('/api/valve_toggle', methods=['POST'])
def api_valve_toggle():
    """個別電磁弁のトグル操作エンドポイント（物理トグルスイッチの再現）"""
    data = request.get_json() or {}
    valve_name = data.get('valve', '').upper()
    valve_state = int(data.get('state', 0))

    if valve_name not in VALVE_NAMES:
        return jsonify({"status": "invalid_valve", "valid": VALVE_NAMES}), 400

    # 安全保護: 自動シーケンス進行中およびセーフティ未解除時は手動弁トグルを拒否（上書き防止・誤操作防止）
    with gse_state.lock:
        if gse_state.fill_active or gse_state.ignition_active:
            print("[SERVER REJECT] Valve toggle blocked during active automatic sequence!")
            return jsonify({"status": "blocked", "message": "自動シーケンス実行中のため手動弁操作はロックされています"}), 403
        if not gse_state.armed_state and not gse_state.demo_mode:
            print("[SERVER REJECT] Valve toggle blocked: Safety not ARMED!")
            return jsonify({"status": "blocked", "message": "セーフティが解除(ARMED)されていないため手動弁操作はできません"}), 403

    idx = VALVE_NAMES.index(valve_name)

    with gse_state.lock:
        if valve_state:
            gse_state.cmd_state |= (1 << idx)
        else:
            gse_state.cmd_state &= ~(1 << idx)
        cmd_b = gse_state.cmd_state

    success = send_msgpacketizer_packet(ser_instance, PACKET_RASPI_COMMAND, CMD_VALVE_CONTROL, cmd_b)
    return jsonify({"status": "ok" if success else "failed", "valve": valve_name, "state": valve_state})

@app.route('/api/auto_purge_toggle', methods=['POST'])
def api_auto_purge_toggle():
    """自動パージ機能のON/OFF切り替えエンドポイント"""
    data = request.get_json() or {}
    state = bool(data.get('state', True))
    with gse_state.lock:
        gse_state.auto_purge_enabled = state
    print(f"[SERVER] Auto Purge set to: {'ENABLED' if state else 'DISABLED'}")
    return jsonify({"status": "ok", "auto_purge": gse_state.auto_purge_enabled})

# =========================================================================
# Log Download API エンドポイント
# =========================================================================
@app.route('/api/logs/list')
def api_logs_list():
    """保存されている CSV ログファイルの一覧を返却"""
    files = []
    try:
        if os.path.exists(LOGS_DIR):
            for fname in sorted(os.listdir(LOGS_DIR), reverse=True):
                if fname.endswith(".csv"):
                    fpath = os.path.join(LOGS_DIR, fname)
                    size_kb = round(os.path.getsize(fpath) / 1024.0, 1)
                    mtime = datetime.fromtimestamp(os.path.getmtime(fpath), tz=JST).strftime('%Y-%m-%d %H:%M:%S')
                    is_current = (fname == gse_logger.current_filename)
                    files.append({
                        "filename": fname,
                        "size_kb": size_kb,
                        "mtime": mtime,
                        "current": is_current,
                        "records": gse_logger.record_count if is_current else None
                    })
    except Exception as e:
        print(f"[API LOGS ERROR] {e}")
    return jsonify({"logs": files, "current_file": gse_logger.current_filename, "record_count": gse_logger.record_count})

@app.route('/api/logs/download/<path:filename>')
def api_logs_download(filename):
    """指定された CSV ログファイルをブラウザへ直接ダウンロード"""
    safe_path = os.path.normpath(os.path.join(LOGS_DIR, filename))
    if not safe_path.startswith(os.path.abspath(LOGS_DIR)):
        return jsonify({"status": "forbidden"}), 403
    if not os.path.exists(safe_path):
        return jsonify({"status": "not_found"}), 404
    return send_file(safe_path, as_attachment=True, download_name=filename, mimetype="text/csv")

@app.route('/api/logs/latest')
def api_logs_latest():
    """現在書き込み中の最新 CSV ログファイルをワンクリックダウンロード"""
    return api_logs_download(gse_logger.current_filename)

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

    # 自動データロガースレッドの起動 (10Hz CSV 記録)
    t_log = threading.Thread(target=logger_worker, daemon=True)
    t_log.start()

    # CLI プロンプトの起動
    t_cli = threading.Thread(target=cli_worker, daemon=True)
    t_cli.start()

    print(f"\n[RASPI GSE SERVER] Starting Web Remote Control Dashboard at http://0.0.0.0:{args.web_port}")
    app.run(host='0.0.0.0', port=args.web_port, debug=False)

if __name__ == '__main__':
    main()
