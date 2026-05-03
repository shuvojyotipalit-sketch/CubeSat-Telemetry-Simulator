"""
CubeSat Telemetry Simulator — Ground Station (Python)

Reads CSV telemetry from receiver Arduino over Serial,
parses packets, logs to file, and plots live dashboard.

Requirements:
    pip install pyserial matplotlib

Usage:
    1. Change PORT to your receiver Arduino's COM port
    2. Run: python ground_station.py
"""

import serial
import time
import csv
import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
from datetime import datetime

# Configuration 
PORT       = 'COM7'        
BAUD       = 9600
MAX_POINTS = 60            
LOG_FILE   = 'telemetry_log.csv'

#  Data buffers 
seqs   = deque(maxlen=MAX_POINTS)
temps  = deque(maxlen=MAX_POINTS)
hums   = deque(maxlen=MAX_POINTS)
volts  = deque(maxlen=MAX_POINTS)
rssis  = deque(maxlen=MAX_POINTS)
snrs   = deque(maxlen=MAX_POINTS)

# Counters 
total_pkts  = [0]
lost_pkts   = [0]
error_pkts  = [0]

# Serial connection 
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"[OK] Connected to {PORT} at {BAUD} baud")
except Exception as e:
    print(f"[FAIL] Cannot open {PORT}: {e}")
    print("Check: 1) correct COM port  2) Arduino is plugged in  3) Serial Monitor is closed")
    exit()

# CSV log file 
log_exists = os.path.exists(LOG_FILE)
log_file   = open(LOG_FILE, 'a', newline='')
log_writer = csv.writer(log_file)
if not log_exists:
    log_writer.writerow(['timestamp', 'seq', 'temp', 'humidity',
                         'voltage', 'rssi', 'snr', 'lost', 'errors'])
print(f"[OK] Logging to {LOG_FILE}")

#Plot setup 
fig, axes = plt.subplots(4, 1, figsize=(11, 9))
fig.suptitle('CubeSat Ground Station — Live Telemetry', fontsize=13, fontweight='bold')
plt.subplots_adjust(hspace=0.5)

colors = ['#E24B4A', '#378ADD', '#1D9E75', '#BA7517']
labels = ['Temperature (°C)', 'Humidity (%)', 'Voltage (V)', 'RSSI (dBm)']

def parse_line(line):
    """Parse one CSV line from receiver. Returns tuple or None."""
    try:
        line = line.strip()
        if not line or line.startswith('=') or line.startswith('[') or line.startswith('seq'):
            return None
        parts = line.split(',')
        if len(parts) != 8:
            return None
        seq      = int(parts[0])
        temp     = float(parts[1])
        humidity = float(parts[2])
        voltage  = float(parts[3])
        rssi     = int(parts[4])
        snr      = float(parts[5])
        lost     = int(parts[6])
        errors   = int(parts[7])
        return seq, temp, humidity, voltage, rssi, snr, lost, errors
    except (ValueError, IndexError):
        return None

def animate(frame):
    """Called by matplotlib every 500ms to refresh the plot."""

    # Read all available lines from Serial
    while ser.in_waiting:
        try:
            raw = ser.readline().decode('utf-8', errors='ignore')
        except Exception:
            continue

        # Print raw line to terminal for debugging
        print(f"[RAW] {raw.strip()}")

        parsed = parse_line(raw)
        if parsed is None:
            continue

        seq, temp, humidity, voltage, rssi, snr, lost, errors = parsed

        # Update buffers
        seqs.append(seq)
        temps.append(temp)
        hums.append(humidity)
        volts.append(voltage)
        rssis.append(rssi)
        snrs.append(snr)
        total_pkts[0] += 1
        lost_pkts[0]   = lost
        error_pkts[0]  = errors

        # Log to CSV file
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        log_writer.writerow([timestamp, seq, temp, humidity,
                              voltage, rssi, snr, lost, errors])
        log_file.flush()

    # Re-draw all 4 subplots
    data_sets = [temps, hums, volts, rssis]
    units     = ['°C', '%', 'V', 'dBm']

    for i, ax in enumerate(axes):
        ax.clear()
        ax.set_facecolor('#f8f8f8')
        ax.grid(True, alpha=0.4, linewidth=0.5)

        if seqs:
            ax.plot(list(seqs), list(data_sets[i]),
                    color=colors[i], linewidth=1.8, marker='o',
                    markersize=3, markerfacecolor=colors[i])
            ax.set_ylabel(labels[i], fontsize=9)
            ax.set_xlabel('Packet #', fontsize=8)

            # Show current value on right side
            current = list(data_sets[i])[-1]
            ax.set_title(
                f"{labels[i]}  —  current: {current:.1f} {units[i]}"
                f"  |  pkts: {total_pkts[0]}  lost: {lost_pkts[0]}  errors: {error_pkts[0]}",
                fontsize=9, loc='left'
            )

    plt.tight_layout(rect=[0, 0, 1, 0.96])

# ─────────────────────────────────────────────────────────
print()
print("============================")
print("  Ground Station RUNNING")
print("============================")
print(f"  Port     : {PORT}")
print(f"  Log file : {LOG_FILE}")
print(f"  Ctrl+C to stop")
print()

ani = animation.FuncAnimation(fig, animate, interval=500, cache_frame_data=False)

try:
    plt.show()
except KeyboardInterrupt:
    pass
finally:
    print("\n[INFO] Closing connection...")
    ser.close()
    log_file.close()
    print(f"[INFO] Telemetry saved to {LOG_FILE}")
    print("[INFO] Ground station stopped.")
