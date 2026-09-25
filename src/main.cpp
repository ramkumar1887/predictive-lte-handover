#include "predictive_ho/stock_a3_engine.h"
#include "predictive_ho/predictive_a3_engine.h"
#include "predictive_ho/channel_model.h"
#include "predictive_ho/metrics.h"

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>

using namespace lte;

struct RunResult {
    SimulationMetrics stock_metrics;
    SimulationMetrics pred_metrics;
};

RunResult run_scenario(ScenarioType scenario_type, const std::string& scenario_name) {
    const double step_ms = 100.0;      // 100 ms measurement report interval (10 Hz)
    const double rach_delay_ms = 50.0; // LTE RACH + RRC Reconfiguration latency

    // 1. Stock A3 Run
    HandoverConfig stock_cfg;
    stock_cfg.enable_predictive = false;
    stock_cfg.a3_offset_db = 3.0f;
    stock_cfg.hysteresis_db = 1.0f;
    stock_cfg.time_to_trigger_ms = 320.0;

    ChannelSimulator stock_sim(scenario_type, 1337);
    StockA3Engine stock_engine(stock_cfg);
    MetricsTracker stock_tracker("Stock_Event_A3", scenario_name, stock_sim.get_degradation_onset_ms());

    uint32_t current_pci = 1;
    bool ho_in_progress = false;
    double ho_complete_time = -1.0;
    HandoverEvent triggered_event{};

    for (double t = 0.0; t <= stock_sim.get_total_duration_ms(); t += step_ms) {
        MeasurementReport report = stock_sim.step(t, step_ms);
        report.serving_pci = current_pci;
        report.neighbor_pci = (current_pci == 1) ? 2 : 1;

        bool is_trigger_step = false;
        bool is_complete_step = false;

        if (ho_in_progress && t >= ho_complete_time) {
            current_pci = triggered_event.target_pci;
            ho_in_progress = false;
            is_complete_step = true;
            stock_tracker.record_handover(triggered_event, t, report.serving_rsrp_dbm);
        }

        if (!ho_in_progress) {
            stock_engine.on_measurement_report(report);
            if (stock_engine.should_trigger_handover(triggered_event)) {
                ho_in_progress = true;
                ho_complete_time = t + rach_delay_ms;
                is_trigger_step = true;
            }
        }

        stock_tracker.record_step(t, report.serving_rsrp_dbm, report.neighbor_rsrp_dbm,
                                  current_pci, is_trigger_step, is_complete_step);
    }

    SimulationMetrics stock_m = stock_tracker.finalize(stock_sim.get_total_duration_ms());
    stock_tracker.export_csv("results/trace_" + scenario_name + "_stock.csv");

    // 2. Predictive A3 Run
    HandoverConfig pred_cfg = stock_cfg;
    pred_cfg.enable_predictive = true;
    pred_cfg.lookahead_ms = 480.0;
    pred_cfg.rolling_buffer_size = 6;
    pred_cfg.min_trend_samples = 3;
    pred_cfg.min_slope_abs = 0.0010f; // 1.0 dB / sec

    ChannelSimulator pred_sim(scenario_type, 1337);
    PredictiveA3Engine pred_engine(pred_cfg);
    MetricsTracker pred_tracker("Predictive_Trend_A3", scenario_name, pred_sim.get_degradation_onset_ms());

    current_pci = 1;
    ho_in_progress = false;
    ho_complete_time = -1.0;

    for (double t = 0.0; t <= pred_sim.get_total_duration_ms(); t += step_ms) {
        MeasurementReport report = pred_sim.step(t, step_ms);
        report.serving_pci = current_pci;
        report.neighbor_pci = (current_pci == 1) ? 2 : 1;

        bool is_trigger_step = false;
        bool is_complete_step = false;

        if (ho_in_progress && t >= ho_complete_time) {
            current_pci = triggered_event.target_pci;
            ho_in_progress = false;
            is_complete_step = true;
            pred_tracker.record_handover(triggered_event, t, report.serving_rsrp_dbm);
        }

        if (!ho_in_progress) {
            pred_engine.on_measurement_report(report);
            if (pred_engine.should_trigger_handover(triggered_event)) {
                ho_in_progress = true;
                ho_complete_time = t + rach_delay_ms;
                is_trigger_step = true;
            }
        }

        pred_tracker.record_step(t, report.serving_rsrp_dbm, report.neighbor_rsrp_dbm,
                                 current_pci, is_trigger_step, is_complete_step);
    }

    SimulationMetrics pred_m = pred_tracker.finalize(pred_sim.get_total_duration_ms());
    pred_tracker.export_csv("results/trace_" + scenario_name + "_predictive.csv");

    return {stock_m, pred_m};
}

void print_comparison_table(const std::vector<std::pair<std::string, RunResult>>& results) {
    std::cout << "\n"
              << "========================================================================================================\n"
              << "                      PREDICTIVE LTE HANDOVER EVALUATION BENCHMARK (srsRAN 3GPP A3)                    \n"
              << "========================================================================================================\n";
    std::cout << std::left
              << std::setw(22) << "Scenario"
              << std::setw(17) << "Algorithm"
              << std::setw(18) << "Trigger Time (s)"
              << std::setw(16) << "HO Latency (ms)"
              << std::setw(18) << "RSRP @ Trigger"
              << std::setw(16) << "Time < -110dBm"
              << std::setw(10) << "HO Count"
              << "\n";
    std::cout << "--------------------------------------------------------------------------------------------------------\n";

    for (const auto& [name, res] : results) {
        // Stock Row
        std::cout << std::left
                  << std::setw(22) << name
                  << std::setw(17) << "Stock Event A3"
                  << std::fixed << std::setprecision(2)
                  << std::setw(18) << (res.stock_metrics.handover_trigger_ms / 1000.0)
                  << std::setw(16) << res.stock_metrics.handover_latency_ms
                  << std::setw(18) << (std::to_string(static_cast<int>(res.stock_metrics.rsrp_at_trigger)) + " dBm")
                  << std::setw(16) << (std::to_string(static_cast<int>(res.stock_metrics.time_below_threshold_ms)) + " ms")
                  << std::setw(10) << res.stock_metrics.handover_count
                  << "\n";

        // Predictive Row
        std::cout << std::left
                  << std::setw(22) << ""
                  << std::setw(17) << "Predictive A3"
                  << std::fixed << std::setprecision(2)
                  << std::setw(18) << (res.pred_metrics.handover_trigger_ms / 1000.0)
                  << std::setw(16) << res.pred_metrics.handover_latency_ms
                  << std::setw(18) << (std::to_string(static_cast<int>(res.pred_metrics.rsrp_at_trigger)) + " dBm")
                  << std::setw(16) << (std::to_string(static_cast<int>(res.pred_metrics.time_below_threshold_ms)) + " ms")
                  << std::setw(10) << res.pred_metrics.handover_count
                  << "\n";

        // Delta calculation
        double latency_saved = res.stock_metrics.handover_latency_ms - res.pred_metrics.handover_latency_ms;
        double poor_saved = res.stock_metrics.time_below_threshold_ms - res.pred_metrics.time_below_threshold_ms;
        double rsrp_gain = res.pred_metrics.rsrp_at_trigger - res.stock_metrics.rsrp_at_trigger;

        std::cout << "  --> Performance Delta: "
                  << "Latency: -" << latency_saved << " ms | "
                  << "RSRP Gain: +" << std::fixed << std::setprecision(1) << rsrp_gain << " dB | "
                  << "Degraded Time Saved: -" << poor_saved << " ms\n";
        std::cout << "--------------------------------------------------------------------------------------------------------\n";
    }
}

int main() {
    std::cout << "============================================================\n";
    std::cout << " Starting Predictive LTE Handover Simulation Engine (C++) \n";
    std::cout << "============================================================\n";

    std::vector<std::pair<std::string, RunResult>> results;

    std::cout << "\n[1/3] Simulating Scenario: Fast Vehicle Highway Crossover...\n";
    results.push_back({"Fast_Crossover", run_scenario(ScenarioType::FAST_FADING_CROSSOVER, "Fast_Crossover")});

    std::cout << "[2/3] Simulating Scenario: Slow Pedestrian Walk Crossover...\n";
    results.push_back({"Slow_Crossover", run_scenario(ScenarioType::SLOW_GRADUAL_CROSSOVER, "Slow_Crossover")});

    std::cout << "[3/3] Simulating Scenario: Boundary Flapping & Oscillation...\n";
    results.push_back({"Boundary_Oscillation", run_scenario(ScenarioType::BOUNDARY_OSCILLATION, "Boundary_Oscillation")});

    print_comparison_table(results);

    std::cout << "\nTrace files exported to results/ directory successfully.\n";
    return 0;
}
