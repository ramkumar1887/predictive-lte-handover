#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace lte {

struct SimulationMetrics {
    std::string algorithm_name;
    std::string scenario_name;
    double degradation_onset_ms = 0.0;
    double handover_trigger_ms = 0.0;
    double handover_completion_ms = 0.0;
    bool handover_succeeded = false;
    double handover_latency_ms = 0.0;        // From degradation onset to HO completion
    float rsrp_at_trigger = 0.0f;            // Serving RSRP when HO command was issued
    float rsrp_at_completion = 0.0f;         // Serving RSRP when new cell RACH completed
    double time_below_threshold_ms = 0.0;    // Time spent where serving RSRP < -110 dBm
    int handover_count = 0;                  // Total HO events (detecting ping-pongs)
};

struct TracePoint {
    double timestamp_ms;
    float serving_rsrp_raw;
    float neighbor_rsrp_raw;
    float active_rsrp;
    uint32_t active_pci;
    bool is_handover_trigger;
    bool is_handover_complete;
};

class MetricsTracker {
public:
    MetricsTracker(const std::string& algo_name, const std::string& scenario_name,
                   double degradation_onset_ms, float poor_threshold_dbm = -110.0f);

    void record_step(double timestamp_ms, float serving_rsrp, float neighbor_rsrp,
                     uint32_t active_pci, bool ho_trigger, bool ho_complete);

    void record_handover(const HandoverEvent& event, double completion_ms, float rsrp_at_comp);

    SimulationMetrics finalize(double total_sim_time_ms);

    void export_csv(const std::string& filepath) const;

    const std::vector<TracePoint>& get_trace() const { return trace_; }

private:
    std::string algo_name_;
    std::string scenario_name_;
    double degradation_onset_ms_;
    float poor_threshold_dbm_;
    SimulationMetrics metrics_;
    std::vector<TracePoint> trace_;
    double last_timestamp_ms_ = 0.0;
};

} // namespace lte
