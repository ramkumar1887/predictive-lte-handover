#!/usr/bin/env python3
"""
Attenuation Controller:
Drives the RF Channel Emulator by sending dynamic attenuation updates (dB)
representing vehicle mobility trajectories:
  1. Fast Crossover (~120 km/h)
  2. Slow Crossover (~5 km/h pedestrian)
  3. Boundary Oscillation (flapping test)
"""

import zmq
import time
import math
import sys

def run_trajectory(scenario="fast", duration_s=60.0, ctrl_port=5555):
    context = zmq.Context()
    sock = context.socket(zmq.PUSH)
    sock.connect(f"tcp://localhost:{ctrl_port}")

    print(f"[Controller] Launching scenario: {scenario.upper()} for {duration_s} seconds...")
    start_time = time.time()

    while True:
        elapsed = time.time() - start_time
        if elapsed > duration_s:
            break

        if scenario == "fast":
            # Steep crossover between 15s and 30s
            progress = max(0.0, min(1.0, (elapsed - 15.0) / 15.0))
            att1 = 0.0 + progress * 40.0   # 0 dB -> 40 dB
            att2 = 40.0 - progress * 40.0  # 40 dB -> 0 dB
        elif scenario == "slow":
            # Gradual crossover across 50s
            progress = max(0.0, min(1.0, (elapsed - 10.0) / 40.0))
            att1 = 0.0 + progress * 35.0
            att2 = 35.0 - progress * 35.0
        else: # oscillation
            # Lingers at cell boundary (both ~15 dB) with sinusoidal variation
            wave = 6.0 * math.sin(2.0 * math.pi * elapsed / 10.0)
            att1 = 18.0 + wave
            att2 = 18.0 - wave

        payload = {"att_cell1": att1, "att_cell2": att2, "elapsed": elapsed}
        sock.send_json(payload)
        time.sleep(0.1) # 100 ms update interval

    print(f"[Controller] Completed scenario {scenario}.")

if __name__ == "__main__":
    scen = sys.argv[1] if len(sys.argv) > 1 else "fast"
    run_trajectory(scen)
