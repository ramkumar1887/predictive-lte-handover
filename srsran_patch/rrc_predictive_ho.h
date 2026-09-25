#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <cmath>

namespace srsenb {

struct RrcTimedMeasurement {
    double timestamp_s;
    float serving_rsrp;
    float neighbor_rsrp;
    uint32_t neighbor_pci;
};

/**
 * Drop-in helper class for srsenb RRC mobility handling.
 * Integrates into srsenb/src/stack/rrc/rrc_ue.cc to calculate linear trend slopes
 * and trigger predictive LTE handover before 3GPP Event A3 TTT expiration.
 */
class RrcPredictiveHoEvaluator {
public:
    RrcPredictiveHoEvaluator(bool enabled = true,
                             size_t buffer_size = 6,
                             int min_samples = 3,
                             double lookahead_s = 0.48,
                             float min_slope = 0.001f);

    void add_report(double timestamp_s, float serving_rsrp, float neighbor_rsrp, uint32_t neighbor_pci);

    // Evaluates whether trend meets predictive trigger condition
    bool should_trigger(float a3_offset_db, uint32_t& out_target_pci, double& out_pred_crossover_s);

    void reset();

    static double compute_slope(const std::vector<double>& times, const std::vector<float>& values);

private:
    bool enabled_;
    size_t buffer_size_;
    int min_samples_;
    double lookahead_s_;
    float min_slope_;

    std::deque<RrcTimedMeasurement> buffer_;
    int consecutive_valid_trends_ = 0;
};

} // namespace srsenb
