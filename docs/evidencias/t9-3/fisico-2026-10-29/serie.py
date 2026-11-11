import json, sys, threading, time
import serial

port = serial.Serial()
port.port = 'COM5'
port.baudrate = 115200
port.timeout = .3
port.dtr = False
port.rts = False
port.open()
port.reset_input_buffer()

def commands():
    for text in sys.stdin:
        port.write(text.strip().encode() + b'\n')
        port.flush()

threading.Thread(target=commands, daemon=True).start()
while True:
    data = port.readline()
    if data:
        print(json.dumps({'timestamp': time.time(), 'line': data.decode('utf8', errors='replace').strip()}), flush=True)
