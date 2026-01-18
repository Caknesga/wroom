import serial
import time

PORT = "COM8"      # update if needed
BAUD = 115200

print("Opening serial port...")
ser = serial.Serial()
ser.port = PORT
ser.baudrate = BAUD
ser.timeout = 1

# Open port safely
while True:
    try:
        ser.open()
        break
    except Exception:
        print("Waiting for device...")
        time.sleep(1)

print("Connected. Reset ESP32 to start capture.")

with open("audio2.txt", "w") as f:
    while True:
        try:
            if ser.in_waiting:
                line = ser.readline().decode(errors="ignore").strip()
                if line:
                    f.write(line + "\n")
        except KeyboardInterrupt:
            print("Stopping capture.")
            break
        except Exception as e:
            print("Serial error, waiting for reconnect...")
            try:
                ser.close()
            except:
                pass
            time.sleep(1)
            while True:
                try:
                    ser.open()
                    print("Reconnected.")
                    break
                except:
                    time.sleep(1)

ser.close()
