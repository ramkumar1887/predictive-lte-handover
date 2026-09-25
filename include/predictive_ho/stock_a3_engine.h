#pragma once

#include "handover_engine.h"
#include <optional>

namespace lte {

/**
 * Implements standard 3GPP TS 36.331 Event A3:
 * Entering condition:  Mn - Hys > Ms + A3_Offset
 * Leaving condition:   Mn + Hys < Ms + A3_Offset
 * Time-To-Trigger (TTT) timer must expire while entering condition remains valid.
 */
class StockA3Engine : public IHandoverEngine {
public:
    explicit StockA3Engine(const HandoverConfig& config);

    void on_measurement_report(const MeasurementReport& report) override;
    bool should_trigger_handover(HandoverEvent& out_event) override;
    void reset() override;
    std::string get_name() const override { return "Stock_Event_A3"; }
    const HandoverConfig& get_config() const override { return config_; }

private:
    HandoverConfig config_;
    Layer3Filter serving_filter_;
    Layer3Filter neighbor_filter_;

    bool condition_entered_ = false;
    double condition_entry_timestamp_ms_ = 0.0;
    std::optional<MeasurementReport> latest_report_;
    bool handover_triggered_ = false;
};

} // namespace lte
