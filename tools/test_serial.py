import serial
import time
import msgpack
import sys

def test_port(port):
    try:
        ser = serial.Serial(port, 115200, timeout=2)
        print(f"Connected to {port}. Listening for 5 seconds...")
        start = time.time()
        unpacker = msgpack.Unpacker()
        while time.time() - start < 5:
            if ser.in_waiting > 0:
                raw = ser.read(ser.in_waiting)
                try:
                    unpacker.feed(raw)
                    for msg in unpacker:
                        print(f"[{port}] Parsed MsgPack: {msg}")
                except Exception as e:
                    print(f"[{port}] Raw data: {raw} (Parse error: {e})")
        ser.close()
    except Exception as e:
        print(f"Could not connect to {port}: {e}")

test_port('COM3')
test_port('COM4')
