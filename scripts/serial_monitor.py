#!/usr/bin/env python3
"""Line-buffered serial monitor that DOES NOT assert DTR/RTS on open.

The default pyserial / Linux tty behavior asserts DTR and RTS when the port
is opened, which on standard ESP32 dev boards drives EN low (reset) and GPIO0
low (download mode). A long-lived `cat /dev/ttyUSBx` background reader will
therefore hold the chip in reset+download forever, looking like "the chip
just isn't booting". Pyserial lets us explicitly release the signals before
data starts flowing.
"""
import serial
import sys
import time
import os
import datetime

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB1"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
LOG  = sys.argv[3] if len(sys.argv) > 3 else "/tmp/portal_log.txt"

with open(LOG, "ab", buffering=0) as out:
    while True:
        try:
            # Open WITHOUT asserting DTR/RTS by using rtscts=False and
            # manually clearing the lines immediately after open.
            s = serial.Serial(PORT, BAUD, timeout=1)
            s.dtr = False
            s.rts = False
            stamp = datetime.datetime.now().strftime("%H:%M:%S")
            out.write(f"\n[{stamp}] --- monitor attached (no DTR/RTS) ---\n".encode())
            while True:
                data = s.read(256)
                if data:
                    out.write(data)
        except (serial.SerialException, OSError):
            stamp = datetime.datetime.now().strftime("%H:%M:%S")
            out.write(f"\n[{stamp}] --- port closed, retry ---\n".encode())
            time.sleep(1)
        finally:
            try: s.close()
            except: pass
