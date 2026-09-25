#include "predictive_ho/stock_a3_engine.h"
#include <iostream>

namespace lte {

StockA3Engine::StockA3Engine(const HandoverConfig& config)
    : config_(config),
      serving_filter_(config.l3_filter_k),
      neighbor_filter_(config.l3_filter_k) {}

void StockA3Engine::on_measurement_report(const MeasurementReport& report) {
    if (handover_triggered_) {
        return; // Handover already initiated
    }

    latest_report_ = report;

    // Apply 3GPP Layer 3 Filtering (TS 36.331)
    float filtered_serving = serving_filter_.filter(report.serving_rsrp_dbm);
    float filtered_neighbor = neighbor_filter_.filter(report.neighbor_rsrp_dbm);

    // 3GPP TS 36.331 Event A3 conditions
    // Entering: Mn - Hys > Ms + A3_Offset
    // Leaving:  Mn + Hys < Ms + A3_Offset
    bool entering_condition = (filtered_neighbor - config_.hysteresis_db) > 
                              (filtered_serving + config_.a3_offset_db);
    bool leaving_condition  = (filtered_neighbor + config_.hysteresis_db) < 
                              (filtered_serving + config_.a3_offset_db);

    if (entering_condition) {
        if (!condition_entered_) {
            condition_entered_ = true;
            condition_entry_timestamp_ms_ = report.timestamp_ms;
        }
    } else if (leaving_condition) {
        // Condition ceased; reset Time-To-Trigger (TTT) timer
        condition_entered_ = false;
        condition_entry_timestamp_ms_ = 0.0;
    }
}

bool StockA3Engine::should_trigger_handover(HandoverEvent& out_event) {
    if (handover_triggered_ || !condition_entered_ || !latest_report_.has_value()) {
        return false;
    }

    double duration_ms = latest_report_->timestamp_ms - condition_entry_timestamp_ms_;
    if (duration_ms >= config_.time_to_trigger_ms) {
        handover_triggered_ = true;

        out_event.timestamp_ms = latest_report_->timestamp_ms;
        out_event.ue_id = latest_report_->ue_id;
        out_event.source_pci = latest_report_->serving_pci;
        out_event.target_pci = latest_report_->neighbor_pci;
        out_event.serving_rsrp_at_trigger = latest_report_->serving_rsrp_dbm;
        out_event.neighbor_rsrp_at_trigger = latest_report_->neighbor_rsrp_dbm;
        out_event.algorithm = "Stock_Event_A3";
        out_event.predicted_crossover_ms = 0.0;
        return true;
    }

    return false;
}

void StockA3Engine::reset() {
    condition_entered_ = false;
    condition_entry_timestamp_ms_ = 0.0;
    handover_triggered_ = false;
    latest_report_.reset();
    serving_filter_.reset();
    neighbor_filter_.reset();
}

} // namespace lte
