#!/usr/bin/env python3
"""
Reads simulation CSV traces from results/ and generates high-resolution
publication-ready comparative plots:
  1. RSRP vs Time Trajectory (Stock A3 vs Predictive A3)
  2. Latency & Degradation 4-Metric Comparative Bar Chart across all 3 scenarios
"""

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "results")
PLOTS_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "plots")
os.makedirs(PLOTS_DIR, exist_ok=True)

def plot_scenario(scenario_name):
    stock_path = os.path.join(RESULTS_DIR, f"trace_{scenario_name}_stock.csv")
    pred_path = os.path.join(RESULTS_DIR, f"trace_{scenario_name}_predictive.csv")

    if not os.path.exists(stock_path) or not os.path.exists(pred_path):
        print(f"Skipping {scenario_name}: trace files not found.")
        return

    df_stock = pd.read_csv(stock_path)
    df_pred = pd.read_csv(pred_path)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 7), sharex=True, sharey=True)

    # Plot Stock A3
    t_sec_s = df_stock['timestamp_ms'] / 1000.0
    ax1.plot(t_sec_s, df_stock['serving_rsrp'], label='Serving Cell 1 (PCI 1)', color='#1f77b4', lw=1.6)
    ax1.plot(t_sec_s, df_stock['neighbor_rsrp'], label='Neighbor Cell 2 (PCI 2)', color='#ff7f0e', lw=1.6, ls='--')
    ax1.axhline(-110, color='red', ls=':', alpha=0.7, label='Critical QoS Threshold (-110 dBm)')

    ho_triggers_s = df_stock[df_stock['ho_trigger'] == 1]
    for _, row in ho_triggers_s.iterrows():
        ax1.axvline(row['timestamp_ms'] / 1000.0, color='darkgreen', lw=2.2, label='Stock A3 Trigger')
        ax1.text(row['timestamp_ms'] / 1000.0 + 0.5, -85, f"Trigger @ {row['timestamp_ms']/1000.0:.1f}s\n{row['serving_rsrp']:.1f} dBm",
                 bbox=dict(boxstyle="round,pad=0.3", fc="yellow", alpha=0.6))

    ax1.set_title(f"Stock 3GPP Event A3 Handover - {scenario_name.replace('_', ' ')}", fontsize=12, fontweight='bold')
    ax1.set_ylabel("RSRP (dBm)")
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='lower left', fontsize=9)

    # Plot Predictive A3
    t_sec_p = df_pred['timestamp_ms'] / 1000.0
    ax2.plot(t_sec_p, df_pred['serving_rsrp'], label='Serving Cell 1 (PCI 1)', color='#1f77b4', lw=1.6)
    ax2.plot(t_sec_p, df_pred['neighbor_rsrp'], label='Neighbor Cell 2 (PCI 2)', color='#ff7f0e', lw=1.6, ls='--')
    ax2.axhline(-110, color='red', ls=':', alpha=0.7, label='Critical QoS Threshold (-110 dBm)')

    ho_triggers_p = df_pred[df_pred['ho_trigger'] == 1]
    for _, row in ho_triggers_p.iterrows():
        ax2.axvline(row['timestamp_ms'] / 1000.0, color='purple', lw=2.2, label='Predictive HO Trigger')
        ax2.text(row['timestamp_ms'] / 1000.0 + 0.5, -85, f"Predictive Trigger @ {row['timestamp_ms']/1000.0:.1f}s\n{row['serving_rsrp']:.1f} dBm",
                 bbox=dict(boxstyle="round,pad=0.3", fc="#bbf7d0", alpha=0.8))

    ax2.set_title(f"Predictive Trend-Based Handover - {scenario_name.replace('_', ' ')}", fontsize=12, fontweight='bold')
    ax2.set_xlabel("Time (seconds)")
    ax2.set_ylabel("RSRP (dBm)")
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='lower left', fontsize=9)

    plt.tight_layout()
    out_file = os.path.join(PLOTS_DIR, f"rsrp_comparison_{scenario_name}.png")
    plt.savefig(out_file, dpi=200)
    plt.close()
    print(f"Saved plot: {out_file}")

def plot_summary_barchart():
    scenarios = ['Fast_Crossover', 'Slow_Crossover', 'Boundary_Oscillation']
    display_names = ['Fast Crossover\n(120 km/h)', 'Slow Walk\n(5 km/h)', 'Boundary\nOscillation']
    
    # 4 metrics extracted from benchmark:
    # 1. Handover Latency (ms)
    stock_latency = [9900.0, 18400.0, 3300.0]
    pred_latency  = [7900.0, 18400.0, 900.0]
    
    # 2. RSRP at Trigger (dBm)
    stock_rsrp = [-99.0, -97.0, -99.0]
    pred_rsrp  = [-96.0, -97.0, -97.0]
    
    # 3. Time spent below -110 dBm (ms)
    stock_time_poor = [0.0, 0.0, 0.0]
    pred_time_poor  = [0.0, 0.0, 0.0]
    
    # 4. Handover count (stability/ping-pong)
    stock_ho_count = [1, 1, 1]
    pred_ho_count  = [1, 1, 1]
    
    fig, axes = plt.subplots(2, 2, figsize=(13, 9))
    x = np.arange(len(scenarios))
    width = 0.35
    
    # Subplot 1: Latency
    ax = axes[0, 0]
    r1 = ax.bar(x - width/2, stock_latency, width, label='Stock Event A3', color='#e06666')
    r2 = ax.bar(x + width/2, pred_latency, width, label='Predictive A3', color='#6aa84f')
    ax.set_ylabel('Latency (ms)')
    ax.set_title('1. Degradation Onset to Handover Completion', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(display_names, fontsize=9)
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.legend()
    for r in r1 + r2:
        h = r.get_height()
        ax.annotate(f"{h:.0f}ms", xy=(r.get_x() + r.get_width() / 2, h),
                    xytext=(0, 3), textcoords="offset points", ha='center', va='bottom', fontsize=8)

    # Subplot 2: RSRP at Trigger
    ax = axes[0, 1]
    r1 = ax.bar(x - width/2, stock_rsrp, width, label='Stock Event A3', color='#e06666')
    r2 = ax.bar(x + width/2, pred_rsrp, width, label='Predictive A3', color='#6aa84f')
    ax.set_ylabel('RSRP (dBm)')
    ax.set_title('2. Serving RSRP at Trigger Time (Higher is Better)', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(display_names, fontsize=9)
    ax.set_ylim(-105, -92)
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.legend(loc='lower right')
    for r in r1 + r2:
        h = r.get_height()
        ax.annotate(f"{h:.1f} dBm", xy=(r.get_x() + r.get_width() / 2, h),
                    xytext=(0, 3), textcoords="offset points", ha='center', va='bottom', fontsize=8)

    # Subplot 3: Time Below -110 dBm
    ax = axes[1, 0]
    ax.bar(x - width/2, stock_time_poor, width, label='Stock Event A3', color='#e06666')
    ax.bar(x + width/2, pred_time_poor, width, label='Predictive A3', color='#6aa84f')
    ax.set_ylabel('Time (ms)')
    ax.set_title('3. Time Spent Below Poor Signal Threshold (-110 dBm)', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(display_names, fontsize=9)
    ax.set_ylim(0, 50)
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.legend()

    # Subplot 4: Total Handover Count
    ax = axes[1, 1]
    r1 = ax.bar(x - width/2, stock_ho_count, width, label='Stock Event A3', color='#e06666')
    r2 = ax.bar(x + width/2, pred_ho_count, width, label='Predictive A3', color='#6aa84f')
    ax.set_ylabel('Count')
    ax.set_title('4. Total Handover Count (Ping-Pong Stability Check)', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(display_names, fontsize=9)
    ax.set_ylim(0, 3)
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.legend()
    for r in r1 + r2:
        h = r.get_height()
        ax.annotate(f"{h}", xy=(r.get_x() + r.get_width() / 2, h),
                    xytext=(0, 3), textcoords="offset points", ha='center', va='bottom', fontsize=8)

    plt.suptitle('Predictive vs Stock 3GPP A3 Handover - 4 Metrics Comparison', fontsize=14, fontweight='bold')
    plt.tight_layout()
    out_file = os.path.join(PLOTS_DIR, "metrics_summary_barchart.png")
    plt.savefig(out_file, dpi=200)
    plt.close()
    print(f"Saved summary bar chart: {out_file}")

if __name__ == "__main__":
    scenarios = ["Fast_Crossover", "Slow_Crossover", "Boundary_Oscillation"]
    for s in scenarios:
        plot_scenario(s)
    plot_summary_barchart()
    print("All plots generated in plots/ directory.")
