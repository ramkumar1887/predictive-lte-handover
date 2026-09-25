#pragma once

#include "types.h"
#include <cmath>
#include <memory>
#include <string>

namespace lte {

class IHandoverEngine {
public:
    virtual ~IHandoverEngine() = default;

    // Ingest a new measurement report from UE
    virtual void on_measurement_report(const MeasurementReport& report) = 0;

    // Check if handover execution condition is fulfilled
    virtual bool should_trigger_handover(HandoverEvent& out_event) = 0;

    // Reset internal state (timers, buffers)
    virtual void reset() = 0;

    // Get human-readable algorithm name
    virtual std::string get_name() const = 0;

    // Get current configuration
    virtual const HandoverConfig& get_config() const = 0;
};

// 3GPP TS 36.331 Layer 3 IIR Filter: Fn = (1 - a) * Fn-1 + a * Mn
class Layer3Filter {
public:
    explicit Layer3Filter(float k = 4.0f) {
        set_coefficient(k);
    }

    void set_coefficient(float k) {
        a_ = std::pow(0.5f, k / 4.0f);
    }

    float filter(float measurement, bool is_first = false) {
        if (is_first || !initialized_) {
            filtered_val_ = measurement;
            initialized_ = true;
        } else {
            filtered_val_ = (1.0f - a_) * filtered_val_ + a_ * measurement;
        }
        return filtered_val_;
    }

    void reset() {
        initialized_ = false;
        filtered_val_ = 0.0f;
    }

private:
    float a_ = 0.5f;
    float filtered_val_ = 0.0f;
    bool initialized_ = false;
};

} // namespace lte
