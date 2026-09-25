#include "predictive_ho/channel_model.h"
#include <cmath>
#include <algorithm>

namespace lte {

ChannelSimulator::ChannelSimulator(ScenarioType scenario, uint32_t seed)
    : scenario_(scenario), rng_(seed), shadow_dist_(0.0f, 1.2f), fast_fading_dist_(0.0f, 1.0f) {
    if (scenario_ == ScenarioType::FAST_FADING_CROSSOVER) {
        duration_ms_ = 50000.0;
        degradation_onset_ms_ = 10000.0;
        shadow_dist_ = std::normal_distribution<float>(0.0f, 1.8f);
    } else if (scenario_ == ScenarioType::SLOW_GRADUAL_CROSSOVER) {
        duration_ms_ = 60000.0;
        degradation_onset_ms_ = 10000.0;
        shadow_dist_ = std::normal_distribution<float>(0.0f, 1.2f);
    } else { // BOUNDARY_OSCILLATION
        duration_ms_ = 45000.0;
        degradation_onset_ms_ = 5000.0;
        shadow_dist_ = std::normal_distribution<float>(0.0f, 2.2f);
    }
}

void ChannelSimulator::get_true_rsrp(double time_ms, float& out_serving, float& out_neighbor) const {
    if (scenario_ == ScenarioType::FAST_FADING_CROSSOVER) {
        // High-velocity vehicle trajectory (~120 km/h)
        if (time_ms < degradation_onset_ms_) {
            out_serving = -78.0f;
            out_neighbor = -115.0f;
        } else {
            double elapsed = time_ms - degradation_onset_ms_;
            out_serving = -78.0f - static_cast<float>(elapsed * 0.0022);
            out_neighbor = -115.0f + static_cast<float>(elapsed * 0.0022);

            out_serving = std::max(-130.0f, out_serving);
            out_neighbor = std::min(-75.0f, out_neighbor);
        }
    } else if (scenario_ == ScenarioType::SLOW_GRADUAL_CROSSOVER) {
        // Pedestrian mobility (~5 km/h)
        if (time_ms < degradation_onset_ms_) {
            out_serving = -82.0f;
            out_neighbor = -110.0f;
        } else {
            double elapsed = time_ms - degradation_onset_ms_;
            out_serving = -82.0f - static_cast<float>(elapsed * 0.0009);
            out_neighbor = -110.0f + static_cast<float>(elapsed * 0.0009);

            out_serving = std::max(-128.0f, out_serving);
            out_neighbor = std::min(-80.0f, out_neighbor);
        }
    } else {
        // BOUNDARY_OSCILLATION
        double t_sec = time_ms / 1000.0;
        float oscillation = static_cast<float>(3.5 * std::sin(2.0 * 3.14159265 * t_sec / 12.0));
        out_serving = -96.0f + oscillation;
        out_neighbor = -96.0f - oscillation;
    }
}

MeasurementReport ChannelSimulator::step(double current_time_ms, double /*step_ms*/) {
    float true_serving = 0.0f, true_neighbor = 0.0f;
    get_true_rsrp(current_time_ms, true_serving, true_neighbor);

    // Apply log-normal shadowing and fast fading
    float serving_fading = shadow_dist_(rng_) + 0.5f * fast_fading_dist_(rng_);
    float neighbor_fading = shadow_dist_(rng_) + 0.5f * fast_fading_dist_(rng_);

    MeasurementReport report;
    report.timestamp_ms = current_time_ms;
    report.ue_id = 1;
    report.serving_pci = 1;
    report.serving_rsrp_dbm = true_serving + serving_fading;
    report.serving_rsrq_db = -10.0f;
    report.neighbor_pci = 2;
    report.neighbor_rsrp_dbm = true_neighbor + neighbor_fading;
    report.neighbor_rsrq_db = -12.0f;

    return report;
}

} // namespace lte
