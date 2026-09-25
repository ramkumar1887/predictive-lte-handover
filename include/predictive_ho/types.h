#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lte {

// Identifies an LTE Physical Cell
struct CellId {
    uint32_t pci;         // Physical Cell ID (0..503)
    uint32_t earfcn;      // E-UTRA Absolute Radio Frequency Channel Number
    std::string name;
};

// Represents an RRC Measurement Report transmitted by UE (TS 36.331)
struct MeasurementReport {
    double timestamp_ms;       // Simulation / absolute time in ms
    uint32_t ue_id;            // User Equipment Identifier
    uint32_t serving_pci;      // Serving Cell PCI
    float serving_rsrp_dbm;    // Reference Signal Received Power of Serving Cell
    float serving_rsrq_db;     // Reference Signal Received Quality of Serving Cell
    uint32_t neighbor_pci;     // Candidate Neighbor PCI
    float neighbor_rsrp_dbm;   // Reference Signal Received Power of Neighbor Cell
    float neighbor_rsrq_db;    // Reference Signal Received Quality of Neighbor Cell
};

// Handover trigger decision record
struct HandoverEvent {
    double timestamp_ms;
    uint32_t ue_id;
    uint32_t source_pci;
    uint32_t target_pci;
    float serving_rsrp_at_trigger;
    float neighbor_rsrp_at_trigger;
    std::string algorithm;      // "Stock_A3" or "Predictive_Trend"
    double predicted_crossover_ms;
};

// Configuration parameters for Handover Decision Logic
struct HandoverConfig {
    // 3GPP Standard Event A3 parameters (TS 36.331 Section 5.5.4.4)
    float a3_offset_db = 3.0f;          // A3 offset: neighbor must exceed serving by this margin (dB)
    float hysteresis_db = 1.0f;         // Hysteresis margin to prevent rapid flapping (dB)
    double time_to_trigger_ms = 320.0;  // Duration entering condition must hold before trigger (ms)
    float l3_filter_k = 4.0f;           // L3 Filter coefficient k: a = (1/2)^(k/4)

    // Predictive Handover parameters
    bool enable_predictive = false;     // Toggle between Stock and Predictive
    size_t rolling_buffer_size = 6;     // Rolling window depth for linear regression
    int min_trend_samples = 3;          // Minimum consistent slope samples required
    double lookahead_ms = 480.0;        // Predictive forecast time horizon (ms)
    float min_slope_abs = 0.015f;       // Minimum required degradation rate (|dBm/ms|)
    float poor_coverage_rsrp_dbm = -110.0f; // Critical QoS degradation threshold (dBm)
};

enum class ScenarioType {
    FAST_FADING_CROSSOVER,
    SLOW_GRADUAL_CROSSOVER,
    BOUNDARY_OSCILLATION
};

} // namespace lte
