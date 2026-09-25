#!/usr/bin/env python3
"""
Launcher for Predictive LTE Handover Application Dashboard
"""
import runpy
import os

if __name__ == "__main__":
    script_path = os.path.join(os.path.dirname(__file__), "scripts", "dashboard.py")
    runpy.run_path(script_path, run_name="__main__")
