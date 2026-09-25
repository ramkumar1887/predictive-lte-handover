#pragma once

#include "types.h"
#include <random>
#include <vector>

namespace lte {

class ChannelSimulator {
public:
    explicit ChannelSimulator(ScenarioType scenario, uint32_t seed = 42);

    // Advance simulation by step_ms and generate measurement report
    MeasurementReport step(double current_time_ms, double step_ms);

    // Get true underlying path loss without fast fading (for analysis)
    void get_true_rsrp(double time_ms, float& out_serving, float& out_neighbor) const;

    double get_total_duration_ms() const { return duration_ms_; }
    double get_degradation_onset_ms() const { return degradation_onset_ms_; }

private:
    ScenarioType scenario_;
    double duration_ms_ = 60000.0; // 60 seconds default
    double degradation_onset_ms_ = 10000.0;

    std::mt19937 rng_;
    std::normal_distribution<float> shadow_dist_;
    std::normal_distribution<float> fast_fading_dist_;
};

} // namespace lte
