#include "predictive_ho/metrics.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>

namespace lte {

MetricsTracker::MetricsTracker(const std::string& algo_name, const std::string& scenario_name,
                               double degradation_onset_ms, float poor_threshold_dbm)
    : algo_name_(algo_name),
      scenario_name_(scenario_name),
      degradation_onset_ms_(degradation_onset_ms),
      poor_threshold_dbm_(poor_threshold_dbm) {
    metrics_.algorithm_name = algo_name;
    metrics_.scenario_name = scenario_name;
    metrics_.degradation_onset_ms = degradation_onset_ms;
}

void MetricsTracker::record_step(double timestamp_ms, float serving_rsrp, float neighbor_rsrp,
                                 uint32_t active_pci, bool ho_trigger, bool ho_complete) {
    float active_rsrp = (active_pci == 1) ? serving_rsrp : neighbor_rsrp;

    if (last_timestamp_ms_ > 0.0) {
        double delta_ms = timestamp_ms - last_timestamp_ms_;
        if (active_rsrp < poor_threshold_dbm_) {
            metrics_.time_below_threshold_ms += delta_ms;
        }
    }
    last_timestamp_ms_ = timestamp_ms;

    trace_.push_back({
        timestamp_ms,
        serving_rsrp,
        neighbor_rsrp,
        active_rsrp,
        active_pci,
        ho_trigger,
        ho_complete
    });
}

void MetricsTracker::record_handover(const HandoverEvent& event, double completion_ms, float rsrp_at_comp) {
    metrics_.handover_count++;
    if (!metrics_.handover_succeeded) {
        metrics_.handover_succeeded = true;
        metrics_.handover_trigger_ms = event.timestamp_ms;
        metrics_.handover_completion_ms = completion_ms;
        metrics_.rsrp_at_trigger = event.serving_rsrp_at_trigger;
        metrics_.rsrp_at_completion = rsrp_at_comp;
        metrics_.handover_latency_ms = completion_ms - degradation_onset_ms_;
    }
}

SimulationMetrics MetricsTracker::finalize(double /*total_sim_time_ms*/) {
    return metrics_;
}

void MetricsTracker::export_csv(const std::string& filepath) const {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "Failed to open CSV for export: " << filepath << std::endl;
        return;
    }

    out << "timestamp_ms,serving_rsrp,neighbor_rsrp,active_rsrp,active_pci,ho_trigger,ho_complete\n";
    for (const auto& pt : trace_) {
        out << std::fixed << std::setprecision(2)
            << pt.timestamp_ms << ","
            << pt.serving_rsrp_raw << ","
            << pt.neighbor_rsrp_raw << ","
            << pt.active_rsrp << ","
            << pt.active_pci << ","
            << (pt.is_handover_trigger ? 1 : 0) << ","
            << (pt.is_handover_complete ? 1 : 0) << "\n";
    }
}

} // namespace lte
