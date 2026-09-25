#!/usr/bin/env python3
"""
ZeroMQ Multi-Cell RF Channel Emulator:
Connects eNB 1 (port 2000) and eNB 2 (port 2100) to the UE (port 2001).
Applies dynamic RF attenuation factors alpha_1(t) and alpha_2(t) received from
attenuation_controller.py over IPC/ZMQ to simulate realistic spatial mobility.
"""

import zmq
import numpy as np
import threading
import time

class ChannelEmulator:
    def __init__(self, enb1_tx_port=2000, enb2_tx_port=2100, ue_rx_port=2001, ctrl_port=5555):
        self.context = zmq.Context()
        self.enb1_sub = self.context.socket(zmq.SUB)
        self.enb1_sub.connect(f"tcp://localhost:{enb1_tx_port}")
        self.enb1_sub.setsockopt(zmq.SUBSCRIBE, b"")

        self.enb2_sub = self.context.socket(zmq.SUB)
        self.enb2_sub.connect(f"tcp://localhost:{enb2_tx_port}")
        self.enb2_sub.setsockopt(zmq.SUBSCRIBE, b"")

        self.ue_pub = self.context.socket(zmq.PUB)
        self.ue_pub.bind(f"tcp://*:{ue_rx_port}")

        self.ctrl_sub = self.context.socket(zmq.PULL)
        self.ctrl_sub.bind(f"tcp://*:{ctrl_port}")

        self.att_cell1 = 0.0 # dB attenuation
        self.att_cell2 = 40.0 # dB attenuation
        self.running = True

    def control_listener(self):
        while self.running:
            try:
                msg = self.ctrl_sub.recv_json(flags=zmq.NOBLOCK)
                self.att_cell1 = float(msg.get("att_cell1", self.att_cell1))
                self.att_cell2 = float(msg.get("att_cell2", self.att_cell2))
            except zmq.Again:
                time.sleep(0.01)

    def run(self):
        print("[Emulator] Started ZMQ Multi-Cell RF Channel Emulator.")
        t = threading.Thread(target=self.control_listener, daemon=True)
        t.start()

        poller = zmq.Poller()
        poller.register(self.enb1_sub, zmq.POLLIN)
        poller.register(self.enb2_sub, zmq.POLLIN)

        while self.running:
            socks = dict(poller.poll(timeout=100))
            scale1 = 10.0 ** (-self.att_cell1 / 20.0)
            scale2 = 10.0 ** (-self.att_cell2 / 20.0)

            if self.enb1_sub in socks:
                raw1 = self.enb1_sub.recv()
                samples1 = np.frombuffer(raw1, dtype=np.complex64) * scale1
                self.ue_pub.send(samples1.astype(np.complex64).tobytes())

            if self.enb2_sub in socks:
                raw2 = self.enb2_sub.recv()
                samples2 = np.frombuffer(raw2, dtype=np.complex64) * scale2
                self.ue_pub.send(samples2.astype(np.complex64).tobytes())

if __name__ == "__main__":
    emulator = ChannelEmulator()
    try:
        emulator.run()
    except KeyboardInterrupt:
        print("[Emulator] Shutting down...")
