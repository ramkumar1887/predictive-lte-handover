#include "predictive_ho/predictive_a3_engine.h"
#include <numeric>
#include <cmath>
#include <iostream>

namespace lte {

PredictiveA3Engine::PredictiveA3Engine(const HandoverConfig& config)
    : config_(config),
      serving_filter_(config.l3_filter_k),
      neighbor_filter_(config.l3_filter_k) {}

double PredictiveA3Engine::compute_slope(const std::vector<double>& times, const std::vector<float>& values) {
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

void PredictiveA3Engine::on_measurement_report(const MeasurementReport& report) {
    if (handover_triggered_) {
        return;
    }

    latest_report_ = report;

    // Apply L3 Filter
    float filtered_serving = serving_filter_.filter(report.serving_rsrp_dbm);
    float filtered_neighbor = neighbor_filter_.filter(report.neighbor_rsrp_dbm);

    // Maintain rolling buffer
    history_.push_back({report.timestamp_ms, filtered_serving, filtered_neighbor});
    while (history_.size() > config_.rolling_buffer_size) {
        history_.pop_front();
    }

    // Evaluate Trend over rolling buffer
    if (history_.size() >= 3) {
        std::vector<double> times;
        std::vector<float> serving_vals, neighbor_vals;
        times.reserve(history_.size());
        serving_vals.reserve(history_.size());
        neighbor_vals.reserve(history_.size());

        for (const auto& sample : history_) {
            times.push_back(sample.timestamp_ms);
            serving_vals.push_back(sample.serving_rsrp);
            neighbor_vals.push_back(sample.neighbor_rsrp);
        }

        double serving_slope = compute_slope(times, serving_vals);
        double neighbor_slope = compute_slope(times, neighbor_vals);

        // Check if serving is degrading and neighbor is rising with significant slope
        if (serving_slope < -config_.min_slope_abs && neighbor_slope > config_.min_slope_abs) {
            consecutive_valid_trends_++;
        } else {
            consecutive_valid_trends_ = 0;
        }
    }

    // Also track standard 3GPP A3 as safety backstop
    bool entering_condition = (filtered_neighbor - config_.hysteresis_db) > 
                              (filtered_serving + config_.a3_offset_db);
    bool leaving_condition  = (filtered_neighbor + config_.hysteresis_db) < 
                              (filtered_serving + config_.a3_offset_db);

    if (entering_condition) {
        if (!stock_condition_entered_) {
            stock_condition_entered_ = true;
            stock_condition_entry_timestamp_ms_ = report.timestamp_ms;
        }
    } else if (leaving_condition) {
        stock_condition_entered_ = false;
        stock_condition_entry_timestamp_ms_ = 0.0;
    }
}

bool PredictiveA3Engine::should_trigger_handover(HandoverEvent& out_event) {
    if (handover_triggered_ || !latest_report_.has_value()) {
        return false;
    }

    // 1. Check Predictive Trigger condition
    if (history_.size() >= 3 && consecutive_valid_trends_ >= config_.min_trend_samples) {
        std::vector<double> times;
        std::vector<float> serving_vals, neighbor_vals;
        for (const auto& sample : history_) {
            times.push_back(sample.timestamp_ms);
            serving_vals.push_back(sample.serving_rsrp);
            neighbor_vals.push_back(sample.neighbor_rsrp);
        }

        double s_slope = compute_slope(times, serving_vals);
        double n_slope = compute_slope(times, neighbor_vals);

        float latest_serving = history_.back().serving_rsrp;
        float latest_neighbor = history_.back().neighbor_rsrp;

        // Condition for crossover: Mn(t + dt) = Ms(t + dt) + A3_Offset
        // Mn_0 + n_slope * dt = Ms_0 + s_slope * dt + A3_Offset
        // dt * (n_slope - s_slope) = Ms_0 - Mn_0 + A3_Offset
        double relative_slope = n_slope - s_slope;
        if (relative_slope > 0.0) {
            double time_to_crossover = (latest_serving - latest_neighbor + config_.a3_offset_db) / relative_slope;

            // If crossover is projected to occur within lookahead window
            if (time_to_crossover > 0.0 && time_to_crossover <= config_.lookahead_ms) {
                handover_triggered_ = true;
                out_event.timestamp_ms = latest_report_->timestamp_ms;
                out_event.ue_id = latest_report_->ue_id;
                out_event.source_pci = latest_report_->serving_pci;
                out_event.target_pci = latest_report_->neighbor_pci;
                out_event.serving_rsrp_at_trigger = latest_report_->serving_rsrp_dbm;
                out_event.neighbor_rsrp_at_trigger = latest_report_->neighbor_rsrp_dbm;
                out_event.algorithm = "Predictive_Trend_A3";
                out_event.predicted_crossover_ms = time_to_crossover;
                return true;
            }
        }
    }

    // 2. Safety Backstop: Standard Stock A3 TTT expiration
    if (stock_condition_entered_) {
        double duration = latest_report_->timestamp_ms - stock_condition_entry_timestamp_ms_;
        if (duration >= config_.time_to_trigger_ms) {
            handover_triggered_ = true;
            out_event.timestamp_ms = latest_report_->timestamp_ms;
            out_event.ue_id = latest_report_->ue_id;
            out_event.source_pci = latest_report_->serving_pci;
            out_event.target_pci = latest_report_->neighbor_pci;
            out_event.serving_rsrp_at_trigger = latest_report_->serving_rsrp_dbm;
            out_event.neighbor_rsrp_at_trigger = latest_report_->neighbor_rsrp_dbm;
            out_event.algorithm = "Stock_A3_Fallback";
            out_event.predicted_crossover_ms = 0.0;
            return true;
        }
    }

    return false;
}

void PredictiveA3Engine::reset() {
    history_.clear();
    consecutive_valid_trends_ = 0;
    handover_triggered_ = false;
    stock_condition_entered_ = false;
    stock_condition_entry_timestamp_ms_ = 0.0;
    latest_report_.reset();
    serving_filter_.reset();
    neighbor_filter_.reset();
}

} // namespace lte
