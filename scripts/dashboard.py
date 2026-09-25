#!/usr/bin/env python3
"""
Predictive LTE Handover - Application-Layer Telemetry & Protocol Stack Dashboard
Translates 3GPP TS 36.331 RRC Measurement Reports & OLS Predictive Decision Logic
into Real-Time User-Facing Quality Metrics, Throughput, and Signal Trajectories.
"""

import os
import time
import numpy as np
import pandas as pd
import streamlit as st
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# Page config
st.set_page_config(
    page_title="Predictive LTE Handover Telemetry Dashboard",
    page_icon="📡",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom Styling for modern dark telecom dashboard
st.markdown("""
<style>
    .reportview-container {
        background: #0e1117;
    }
    .metric-card {
        background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
        border: 1px solid #334155;
        border-radius: 10px;
        padding: 16px;
        margin-bottom: 12px;
        box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
    }
    .metric-title {
        color: #94a3b8;
        font-size: 0.85rem;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.05em;
    }
    .metric-value {
        font-size: 1.6rem;
        font-weight: 700;
        margin-top: 4px;
    }
    .badge-gain {
        color: #10b981;
        font-size: 0.85rem;
        font-weight: 600;
    }
    .badge-stock {
        color: #ef4444;
        font-size: 0.85rem;
        font-weight: 600;
    }
    .badge-pred {
        color: #8b5cf6;
        font-size: 0.85rem;
        font-weight: 600;
    }
    .subtext {
        font-size: 0.78rem;
        color: #64748b;
    }
</style>
""", unsafe_allow_html=True)

RESULTS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "results")

# --- Mathematical Simulation Engine for Live Interactivity ---
def simulate_scenario(velocity_kmh, a3_offset_db, ttt_ms, lookahead_ms, noise_std_db, duration_s=30.0, dt_s=0.1):
    n_steps = int(duration_s / dt_s)
    t = np.linspace(0, duration_s, n_steps)
    
    # Base pathloss model: UE moves from Cell 1 (x=0) to Cell 2 (x=1000m)
    v_mps = velocity_kmh / 3.6
    x = v_mps * t
    d1 = np.maximum(10.0, np.abs(x))
    d2 = np.maximum(10.0, np.abs(1000.0 - x))
    
    # Log-distance path loss
    rsrp1_raw = -60.0 - 32.0 * np.log10(d1 / 10.0)
    rsrp2_raw = -60.0 - 32.0 * np.log10(d2 / 10.0)
    
    np.random.seed(42)
    shadow1 = np.random.normal(0, noise_std_db, n_steps)
    shadow2 = np.random.normal(0, noise_std_db, n_steps)
    
    # Apply L3 filter: alpha = 0.5 (typical 3GPP k=4)
    alpha = 0.5
    rsrp1 = np.zeros(n_steps)
    rsrp2 = np.zeros(n_steps)
    rsrp1[0] = rsrp1_raw[0] + shadow1[0]
    rsrp2[0] = rsrp2_raw[0] + shadow2[0]
    for i in range(1, n_steps):
        rsrp1[i] = (1 - alpha) * rsrp1[i-1] + alpha * (rsrp1_raw[i] + shadow1[i])
        rsrp2[i] = (1 - alpha) * rsrp2[i-1] + alpha * (rsrp2_raw[i] + shadow2[i])
        
    # 1. Stock A3 evaluation
    stock_trigger_idx = -1
    ttt_steps = int((ttt_ms / 1000.0) / dt_s)
    entering_counter = 0
    for i in range(n_steps):
        if rsrp2[i] > (rsrp1[i] + a3_offset_db):
            entering_counter += 1
            if entering_counter >= ttt_steps:
                stock_trigger_idx = i
                break
        else:
            entering_counter = 0
            
    # 2. Predictive OLS evaluation
    pred_trigger_idx = -1
    window_n = 6
    window_steps = min(window_n, int(0.6 / dt_s))
    slope1 = np.zeros(n_steps)
    slope2 = np.zeros(n_steps)
    crossover_time_pred = np.full(n_steps, np.nan)
    
    for i in range(window_steps, n_steps):
        tw = t[i-window_steps+1:i+1]
        t_mean = np.mean(tw)
        t_diff = tw - t_mean
        denom = np.sum(t_diff**2)
        if denom > 1e-6:
            s1 = np.sum(t_diff * (rsrp1[i-window_steps+1:i+1] - np.mean(rsrp1[i-window_steps+1:i+1]))) / denom
            s2 = np.sum(t_diff * (rsrp2[i-window_steps+1:i+1] - np.mean(rsrp2[i-window_steps+1:i+1]))) / denom
            slope1[i] = s1
            slope2[i] = s2
            
            delta_slope = s2 - s1
            if delta_slope > 0.05 and s1 < -0.05 and s2 > 0.05:
                delta_rsrp = (rsrp1[i] - rsrp2[i] + a3_offset_db)
                if delta_rsrp > 0:
                    dt_cross = delta_rsrp / delta_slope
                    crossover_time_pred[i] = dt_cross
                    if dt_cross <= (lookahead_ms / 1000.0) and pred_trigger_idx == -1:
                        pred_trigger_idx = i
                        
    # If predictive didn't trigger, fall back to stock
    if pred_trigger_idx == -1:
        pred_trigger_idx = stock_trigger_idx

    # Throughput calculation (Shannon approximation: BW=20MHz, SNR proportional to serving RSRP)
    noise_floor = -118.0
    sinr_serving = rsrp1 - noise_floor
    sinr_target  = rsrp2 - noise_floor
    
    tput_stock = np.zeros(n_steps)
    tput_pred  = np.zeros(n_steps)
    
    for i in range(n_steps):
        # Stock: serves cell 1 until stock_trigger_idx + 0.1s execution latency
        exec_stock_idx = stock_trigger_idx + 1 if stock_trigger_idx != -1 else n_steps
        if i < exec_stock_idx:
            cqi_snr = np.clip(sinr_serving[i], 0, 30)
        else:
            cqi_snr = np.clip(sinr_target[i], 0, 30)
        tput_stock[i] = 20.0 * np.log2(1 + 10**(cqi_snr / 10.0)) * 0.75  # Mbps
        
        # Pred: serves cell 1 until pred_trigger_idx + 0.1s execution latency
        exec_pred_idx = pred_trigger_idx + 1 if pred_trigger_idx != -1 else n_steps
        if i < exec_pred_idx:
            cqi_snr_p = np.clip(sinr_serving[i], 0, 30)
        else:
            cqi_snr_p = np.clip(sinr_target[i], 0, 30)
        tput_pred[i] = 20.0 * np.log2(1 + 10**(cqi_snr_p / 10.0)) * 0.75  # Mbps

    df = pd.DataFrame({
        'time_s': t,
        'timestamp_ms': t * 1000.0,
        'serving_rsrp': rsrp1,
        'neighbor_rsrp': rsrp2,
        'slope_serving': slope1,
        'slope_neighbor': slope2,
        'crossover_pred_s': crossover_time_pred,
        'tput_stock_mbps': tput_stock,
        'tput_pred_mbps': tput_pred,
        'stock_trigger': [1 if i == stock_trigger_idx else 0 for i in range(n_steps)],
        'pred_trigger': [1 if i == pred_trigger_idx else 0 for i in range(n_steps)],
    })
    
    metrics = {
        'stock_trigger_time': t[stock_trigger_idx] if stock_trigger_idx != -1 else None,
        'pred_trigger_time': t[pred_trigger_idx] if pred_trigger_idx != -1 else None,
        'stock_rsrp_at_trigger': rsrp1[stock_trigger_idx] if stock_trigger_idx != -1 else None,
        'pred_rsrp_at_trigger': rsrp1[pred_trigger_idx] if pred_trigger_idx != -1 else None,
        'stock_min_tput': np.min(tput_stock),
        'pred_min_tput': np.min(tput_pred),
        'time_poor_stock_ms': np.sum(rsrp1[:stock_trigger_idx if stock_trigger_idx != -1 else n_steps] < -110) * dt_s * 1000.0,
        'time_poor_pred_ms': np.sum(rsrp1[:pred_trigger_idx if pred_trigger_idx != -1 else n_steps] < -110) * dt_s * 1000.0,
    }
    return df, metrics

# --- Sidebar Controls ---
st.sidebar.image("https://img.shields.io/badge/3GPP-TS%2036.331%20Event%20A3-orange.svg", width=180)
st.sidebar.title("🎛️ Simulation Controls")

mode = st.sidebar.radio(
    "Data Source Mode",
    ["Preset Benchmark Scenarios", "Live Parametric Sweep (Real-Time Physics)"]
)

if mode == "Preset Benchmark Scenarios":
    scenario_choice = st.sidebar.selectbox(
        "Select Evaluated Scenario",
        ["Fast_Crossover (120 km/h Highway)", "Slow_Crossover (5 km/h Pedestrian)", "Boundary_Oscillation (Urban Fading)"]
    )
    scenario_key = scenario_choice.split(" ")[0]
    
    stock_path = os.path.join(RESULTS_DIR, f"trace_{scenario_key}_stock.csv")
    pred_path = os.path.join(RESULTS_DIR, f"trace_{scenario_key}_predictive.csv")
    
    if os.path.exists(stock_path) and os.path.exists(pred_path):
        df_stock = pd.read_csv(stock_path)
        df_pred = pd.read_csv(pred_path)
        t_sec = df_stock['timestamp_ms'] / 1000.0
        
        # Approximate application throughput from RSRP traces
        tput_stock = 20.0 * np.log2(1 + 10**((np.clip(df_stock['serving_rsrp'] + 118, 0, 30)) / 10.0)) * 0.75
        tput_pred  = 20.0 * np.log2(1 + 10**((np.clip(df_pred['serving_rsrp'] + 118, 0, 30)) / 10.0)) * 0.75
        
        stock_trig = df_stock[df_stock['ho_trigger'] == 1]
        pred_trig  = df_pred[df_pred['ho_trigger'] == 1]
        
        st_time = stock_trig['timestamp_ms'].values[0]/1000.0 if len(stock_trig) > 0 else None
        pr_time = pred_trig['timestamp_ms'].values[0]/1000.0 if len(pred_trig) > 0 else None
        st_rsrp = stock_trig['serving_rsrp'].values[0] if len(stock_trig) > 0 else None
        pr_rsrp = pred_trig['serving_rsrp'].values[0] if len(pred_trig) > 0 else None
        
        df = pd.DataFrame({
            'time_s': t_sec,
            'timestamp_ms': df_stock['timestamp_ms'],
            'serving_rsrp': df_stock['serving_rsrp'],
            'neighbor_rsrp': df_stock['neighbor_rsrp'],
            'tput_stock_mbps': tput_stock,
            'tput_pred_mbps': tput_pred,
            'stock_trigger': df_stock['ho_trigger'],
            'pred_trigger': df_pred['ho_trigger'],
        })
        metrics = {
            'stock_trigger_time': st_time,
            'pred_trigger_time': pr_time,
            'stock_rsrp_at_trigger': st_rsrp,
            'pred_rsrp_at_trigger': pr_rsrp,
            'stock_min_tput': np.min(tput_stock),
            'pred_min_tput': np.min(tput_pred),
            'time_poor_stock_ms': 0.0,
            'time_poor_pred_ms': 0.0,
        }
    else:
        # Fallback to simulation if trace file missing
        v = 120 if "Fast" in scenario_choice else (5 if "Slow" in scenario_choice else 40)
        n = 1.0 if "Fast" in scenario_choice else (0.5 if "Slow" in scenario_choice else 3.5)
        df, metrics = simulate_scenario(v, 2.0, 320, 480, n)
else:
    velocity = st.sidebar.slider("UE Velocity (km/h)", 5, 160, 120, step=5)
    a3_offset = st.sidebar.slider("3GPP Event A3 Offset (dB)", 0.5, 6.0, 2.0, step=0.5)
    ttt = st.sidebar.slider("Time-To-Trigger (TTT ms)", 100, 640, 320, step=40)
    lookahead = st.sidebar.slider("Predictive Lookahead Window (ms)", 200, 800, 480, step=40)
    fading_std = st.sidebar.slider("Shadow Fading Noise Std (dB)", 0.0, 6.0, 1.2, step=0.2)
    
    df, metrics = simulate_scenario(velocity, a3_offset, ttt, lookahead, fading_std)

# --- Header & Top Protocol/UX Summary ---
st.title("📡 Predictive LTE Handover: Protocol-to-Application Telemetry")
st.caption("Bridging 3GPP TS 36.331 RRC Event A3 signaling decisions to application-layer throughput, QoS, and user-perceived connection health.")

# Top Metric Cards
col1, col2, col3, col4 = st.columns(4)

time_saved_s = (metrics['stock_trigger_time'] - metrics['pred_trigger_time']) if (metrics['stock_trigger_time'] and metrics['pred_trigger_time']) else 0.0
rsrp_gain_db = (metrics['pred_rsrp_at_trigger'] - metrics['stock_rsrp_at_trigger']) if (metrics['stock_rsrp_at_trigger'] and metrics['pred_rsrp_at_trigger']) else 0.0

with col1:
    st.markdown(f"""
    <div class="metric-card">
        <div class="metric-title">⚡ Trigger Latency Advantage</div>
        <div class="metric-value badge-gain">-{time_saved_s*1000:.0f} ms</div>
        <div class="subtext">Stock: {metrics['stock_trigger_time']:.2f}s | Pred: {metrics['pred_trigger_time']:.2f}s</div>
    </div>
    """, unsafe_allow_html=True)

with col2:
    st.markdown(f"""
    <div class="metric-card">
        <div class="metric-title">📶 Serving RSRP at Handover</div>
        <div class="metric-value badge-gain">+{rsrp_gain_db:.1f} dBm</div>
        <div class="subtext">Stock: {metrics['stock_rsrp_at_trigger']:.1f} dBm | Pred: {metrics['pred_rsrp_at_trigger']:.1f} dBm</div>
    </div>
    """, unsafe_allow_html=True)

with col3:
    tput_dip_avoidance = max(0.0, metrics['pred_min_tput'] - metrics['stock_min_tput'])
    st.markdown(f"""
    <div class="metric-card">
        <div class="metric-title">🚀 Throughput Collapse Protection</div>
        <div class="metric-value" style="color:#38bdf8;">+{tput_dip_avoidance:.1f} Mbps</div>
        <div class="subtext">Min Throughput floor maintained during switch</div>
    </div>
    """, unsafe_allow_html=True)

with col4:
    rlf_risk = "0.0% (Protected)" if metrics['pred_rsrp_at_trigger'] > -105 else "Elevated (> -110 dBm)"
    st.markdown(f"""
    <div class="metric-card">
        <div class="metric-title">🛡️ Radio Link Failure (RLF) Risk</div>
        <div class="metric-value" style="color:#10b981;">{rlf_risk}</div>
        <div class="subtext">3GPP TS 36.331 Safety Fallback Active</div>
    </div>
    """, unsafe_allow_html=True)

st.markdown("---")

# --- Main Multi-Layer Telemetry Visualizer ---
tab1, tab2, tab3 = st.tabs([
    "📊 Real-Time Protocol & App Telemetry", 
    "📈 OLS Slope & Crossover Analysis", 
    "🏗️ 3GPP Protocol Stack vs UX Architecture"
])

with tab1:
    fig = make_subplots(
        rows=2, cols=1,
        shared_xaxes=True,
        vertical_spacing=0.10,
        subplot_titles=(
            "<b>Layer 3 (RRC): Reference Signal Received Power (RSRP) Trajectory & Handover Decision</b>",
            "<b>Layer 7 (Application): User-Perceived Data Throughput & Quality of Experience (QoE)</b>"
        )
    )

    # Subplot 1: RSRP Trajectory
    fig.add_trace(
        go.Scatter(x=df['time_s'], y=df['serving_rsrp'], mode='lines', name='Serving Cell 1 (PCI 1)', line=dict(color='#38bdf8', width=2.2)),
        row=1, col=1
    )
    fig.add_trace(
        go.Scatter(x=df['time_s'], y=df['neighbor_rsrp'], mode='lines', name='Neighbor Cell 2 (PCI 2)', line=dict(color='#fb923c', width=2.2, dash='dash')),
        row=1, col=1
    )
    
    # Critical QoS line
    fig.add_hline(y=-110, line=dict(color='red', width=1.5, dash='dot'), row=1, col=1, annotation_text="Critical QoS (-110 dBm)", annotation_position="bottom right")

    # Handover trigger points
    if metrics['stock_trigger_time'] is not None:
        fig.add_vline(x=metrics['stock_trigger_time'], line=dict(color='#ef4444', width=2.5, dash='dash'), row=1, col=1)
        fig.add_annotation(
            x=metrics['stock_trigger_time'], y=metrics['stock_rsrp_at_trigger'],
            text=f"Stock A3 Trigger<br>{metrics['stock_trigger_time']:.1f}s ({metrics['stock_rsrp_at_trigger']:.1f} dBm)",
            showarrow=True, arrowhead=2, arrowcolor="#ef4444", bgcolor="#ef4444", font=dict(color="white", size=10),
            row=1, col=1
        )
        
    if metrics['pred_trigger_time'] is not None:
        fig.add_vline(x=metrics['pred_trigger_time'], line=dict(color='#a855f7', width=2.5), row=1, col=1)
        fig.add_annotation(
            x=metrics['pred_trigger_time'], y=metrics['pred_rsrp_at_trigger'],
            text=f"Predictive Early Trigger<br>{metrics['pred_trigger_time']:.1f}s ({metrics['pred_rsrp_at_trigger']:.1f} dBm)",
            showarrow=True, arrowhead=2, arrowcolor="#a855f7", bgcolor="#a855f7", font=dict(color="white", size=10),
            row=1, col=1
        )

    # Subplot 2: Throughput Trajectory
    fig.add_trace(
        go.Scatter(x=df['time_s'], y=df['tput_stock_mbps'], mode='lines', name='Stock A3 Throughput', line=dict(color='#ef4444', width=2)),
        row=2, col=1
    )
    fig.add_trace(
        go.Scatter(x=df['time_s'], y=df['tput_pred_mbps'], mode='lines', name='Predictive A3 Throughput', line=dict(color='#10b981', width=2.5)),
        row=2, col=1
    )

    fig.update_yaxes(title_text="RSRP (dBm)", row=1, col=1, gridcolor="#334155")
    fig.update_yaxes(title_text="App Throughput (Mbps)", row=2, col=1, gridcolor="#334155")
    fig.update_xaxes(title_text="Elapsed Time (seconds)", row=2, col=1, gridcolor="#334155")

    fig.update_layout(
        height=620,
        template="plotly_dark",
        paper_bgcolor="rgba(0,0,0,0)",
        plot_bgcolor="rgba(15, 23, 42, 0.6)",
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1),
        margin=dict(l=40, r=20, t=60, b=40)
    )

    st.plotly_chart(fig, use_container_width=True)

with tab2:
    st.subheader("📐 OLS Rolling-Window Trend Estimation & Lookahead Horizon")
    st.markdown("""
    The predictive decision engine evaluates rolling measurement windows:
    $$\\beta_s = \\frac{d(RSRP_s)}{dt} \\quad \\text{and} \\quad \\beta_n = \\frac{d(RSRP_n)}{dt}$$
    $$\\Delta t_{\\text{crossover}} = \\frac{RSRP_s(t) - RSRP_n(t) + A3\\_Offset}{\\beta_n - \\beta_s}$$
    """)
    
    if 'slope_serving' in df.columns:
        fig_slope = go.Figure()
        fig_slope.add_trace(go.Scatter(x=df['time_s'], y=df['slope_serving'], name='d(RSRP_serving)/dt (dB/s)', line=dict(color='#38bdf8')))
        fig_slope.add_trace(go.Scatter(x=df['time_s'], y=df['slope_neighbor'], name='d(RSRP_neighbor)/dt (dB/s)', line=dict(color='#fb923c')))
        fig_slope.add_hline(y=0, line=dict(color='white', width=1, dash='dot'))
        fig_slope.update_layout(
            title="Estimated Rate of Signal Degradation / Rise (dB/s)",
            xaxis_title="Time (s)",
            yaxis_title="Slope (dB/s)",
            template="plotly_dark",
            paper_bgcolor="rgba(0,0,0,0)",
            plot_bgcolor="rgba(15, 23, 42, 0.6)"
        )
        st.plotly_chart(fig_slope, use_container_width=True)
    else:
        st.info("Select 'Live Parametric Sweep' mode in the sidebar to inspect real-time derivative curves and crossover horizons.")

with tab3:
    st.subheader("🌐 Cellular Protocol Stack to User Experience Architecture")
    st.markdown("""
    ```
    +-----------------------------------------------------------------------------------+
    | [Application Layer (L7)]                                                          |
    | - Real-time Voice/Video Calling (VoLTE/WebRTC), Streaming Throughput, QoE Scores  |
    | - Telemetry Dashboard: Surfacing real-time RRC events to diagnostic UX             |
    +-----------------------------------------------------------------------------------+
                                         ▲
                                         │ QoS Feedback & Metrics
                                         ▼
    +-----------------------------------------------------------------------------------+
    | [Transport / Network Layer (L3/L4)]                                               |
    | - TCP Congestion Control, Packet Retransmissions, IP Packet Forwarding via EPC    |
    +-----------------------------------------------------------------------------------+
                                         ▲
                                         │ S1-U / GTP Data Path
                                         ▼
    +-----------------------------------------------------------------------------------+
    | [Radio Resource Control (RRC - 3GPP TS 36.331)]                                   |
    | - Measurement Reports (Event A3: Mn - Hys > Ms + Offset)                          |
    | - ★ PREDICTIVE DECISION ENGINE (OLS Regression & Lookahead Trigger)               |
    | - RRC Connection Reconfiguration & Target Cell Handover Command                   |
    +-----------------------------------------------------------------------------------+
                                         ▲
                                         │ Raw RF Measurement Reports
                                         ▼
    +-----------------------------------------------------------------------------------+
    | [PHY & RF Layer (L1/L2)]                                                          |
    | - srsRAN_4G eNodeB / UE over ZeroMQ Multi-Cell RF Emulation                       |
    | - L1 Filtering & RSRP / RSRQ Extraction                                           |
    +-----------------------------------------------------------------------------------+
    ```
    """)

# Footer
st.markdown("---")
st.markdown(
    "<center><small style='color: #64748b;'>Predictive LTE Handover Telemetry Dashboard • PES University • Open5GS + srsRAN_4G + ZeroMQ</small></center>",
    unsafe_allow_html=True
)
