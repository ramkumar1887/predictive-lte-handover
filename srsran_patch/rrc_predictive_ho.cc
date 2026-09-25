#include "rrc_predictive_ho.h"

namespace srsenb {

RrcPredictiveHoEvaluator::RrcPredictiveHoEvaluator(bool enabled,
                                                 size_t buffer_size,
                                                 int min_samples,
                                                 double lookahead_s,
                                                 float min_slope)
    : enabled_(enabled),
      buffer_size_(buffer_size),
      min_samples_(min_samples),
      lookahead_s_(lookahead_s),
      min_slope_(min_slope) {}

double RrcPredictiveHoEvaluator::compute_slope(const std::vector<double>& times, const std::vector<float>& values) {
    if (times.size() < 2 || times.size() != values.size()) {
        return 0.0;
    }

    size_t n = times.size();
    double sum_t = 0.0, sum_y = 0.0, sum_ty = 0.0, sum_tt = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum_t += times[i];
        sum_y += values[i];
        sum_ty += times[i] * values[i];
        sum_tt += times[i] * times[i];
    }

    double denominator = (n * sum_tt - sum_t * sum_t);
    if (std::abs(denominator) < 1e-9) {
        return 0.0;
    }

    return (n * sum_ty - sum_t * sum_y) / denominator;
}

void RrcPredictiveHoEvaluator::add_report(double timestamp_s, float serving_rsrp, float neighbor_rsrp, uint32_t neighbor_pci) {
    if (!enabled_) return;

    buffer_.push_back({timestamp_s, serving_rsrp, neighbor_rsrp, neighbor_pci});
    while (buffer_.size() > buffer_size_) {
        buffer_.pop_front();
    }

    if (buffer_.size() >= 3) {
        std::vector<double> times;
        std::vector<float> s_vals, n_vals;
        for (const auto& m : buffer_) {
            times.push_back(m.timestamp_s);
            s_vals.push_back(m.serving_rsrp);
            n_vals.push_back(m.neighbor_rsrp);
        }

        double s_slope = compute_slope(times, s_vals);
        double n_slope = compute_slope(times, n_vals);

        if (s_slope < -min_slope_ && n_slope > min_slope_) {
            consecutive_valid_trends_++;
        } else {
            consecutive_valid_trends_ = 0;
        }
    }
}

bool RrcPredictiveHoEvaluator::should_trigger(float a3_offset_db, uint32_t& out_target_pci, double& out_pred_crossover_s) {
    if (!enabled_ || buffer_.size() < 3 || consecutive_valid_trends_ < min_samples_) {
        return false;
    }

    std::vector<double> times;
    std::vector<float> s_vals, n_vals;
    for (const auto& m : buffer_) {
        times.push_back(m.timestamp_s);
        s_vals.push_back(m.serving_rsrp);
        n_vals.push_back(m.neighbor_rsrp);
    }

    double s_slope = compute_slope(times, s_vals);
    double n_slope = compute_slope(times, n_vals);
    double rel_slope = n_slope - s_slope;

    if (rel_slope <= 0.0) {
        return false;
    }

    float latest_serving = buffer_.back().serving_rsrp;
    float latest_neighbor = buffer_.back().neighbor_rsrp;
    out_target_pci = buffer_.back().neighbor_pci;

    double dt = (latest_serving - latest_neighbor + a3_offset_db) / rel_slope;
    if (dt > 0.0 && dt <= lookahead_s_) {
        out_pred_crossover_s = dt;
        return true;
    }

    return false;
}

void RrcPredictiveHoEvaluator::reset() {
    buffer_.clear();
    consecutive_valid_trends_ = 0;
}

} // namespace srsenb
