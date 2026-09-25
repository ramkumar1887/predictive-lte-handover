#pragma once

#include "handover_engine.h"
#include <deque>
#include <optional>

namespace lte {

struct TimedSample {
    double timestamp_ms;
    float serving_rsrp;
    float neighbor_rsrp;
};

/**
 * Predictive LTE Handover Engine:
 * Employs Ordinary Least Squares (OLS) linear regression over a rolling
 * measurement report buffer to estimate d(RSRP_s)/dt and d(RSRP_n)/dt.
 *
 * Triggers early handover if:
 * 1. Serving cell trend is negative (slope_s < -min_slope)
 * 2. Neighbor cell trend is positive (slope_n > +min_slope)
 * 3. Consistent trend has been sustained for >= min_trend_samples (safeguard)
 * 4. Extrapolated crossover time T_crossover is within lookahead window (e.g. 480ms)
 *
 * Also maintains Stock A3 evaluation as safety backstop.
 */
class PredictiveA3Engine : public IHandoverEngine {
public:
    explicit PredictiveA3Engine(const HandoverConfig& config);

    void on_measurement_report(const MeasurementReport& report) override;
    bool should_trigger_handover(HandoverEvent& out_event) override;
    void reset() override;
    std::string get_name() const override { return "Predictive_Trend_A3"; }
    const HandoverConfig& get_config() const override { return config_; }

    // Exposed for testing & validation
    static double compute_slope(const std::vector<double>& times, const std::vector<float>& values);

private:
    HandoverConfig config_;
    Layer3Filter serving_filter_;
    Layer3Filter neighbor_filter_;

    std::deque<TimedSample> history_;
    int consecutive_valid_trends_ = 0;
    bool handover_triggered_ = false;
    std::optional<MeasurementReport> latest_report_;

    // Stock fallback tracking
    bool stock_condition_entered_ = false;
    double stock_condition_entry_timestamp_ms_ = 0.0;
};

} // namespace lte
