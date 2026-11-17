#pragma once

// Pylontech low-voltage CAN V1.2 passive decoder.
// Standard 11-bit identifiers, little-endian payloads, 500 kbit/s.
// Intentionally independent of ESP-IDF/TWAI for host-side unit tests.
#include <stdint.h>
#include <math.h>

class PylontechBms {
public:
    struct Data {
        float chargeVoltageV = 0.0f;       // 0x351, 0.1 V
        float chargeLimitA = 0.0f;         // 0x351, 0.1 A
        float dischargeLimitA = 0.0f;
        float packVoltageV = 0.0f;         // 0x356, 0.01 V
        float packCurrentA = 0.0f;         // 0x356, signed 0.1 A
        float temperatureC = 0.0f;        // 0x356, signed 0.1 deg C
        uint8_t soc = 0;                 // 0x355, percent
        uint8_t soh = 0;
        uint8_t protection0 = 0;         // 0x359
        uint8_t protection1 = 0;
        uint8_t warning0 = 0;
        uint8_t warning1 = 0;
        bool chargeEnable = false;       // 0x35C bit 7
        bool dischargeEnable = false;    // 0x35C bit 6
    };

    static constexpr uint32_t FRESH_MS = 3500;

    // Returns true for a recognized and well-formed BMS data frame.
    bool ingest(uint32_t id, bool extended, bool remote, const uint8_t* d,
                uint8_t len, uint32_t nowMs) {
        if (extended || remote || !d) return false;
        switch (id) {
            case 0x351: {
                if (len < 6) return false;
                float voltage = u16(d) * 0.1f;
                float charge = s16(d + 2) * 0.1f;
                float discharge = s16(d + 4) * 0.1f;
                // Reject corrupted values rather than preserving their timestamps.
                if (voltage < 43.5f || voltage > 58.4f ||
                    charge < 0.0f || charge > 500.0f ||
                    discharge < 0.0f || discharge > 500.0f) return false;
                data_.chargeVoltageV = voltage;
                data_.chargeLimitA = charge;
                data_.dischargeLimitA = discharge;
                stamp(0, nowMs); return true;
            }
            case 0x355: {
                if (len < 4) return false;
                uint16_t soc = u16(d), soh = u16(d + 2);
                if (soc > 100 || soh > 100) return false;
                data_.soc = static_cast<uint8_t>(soc);
                data_.soh = static_cast<uint8_t>(soh);
                stamp(1, nowMs); return true;
            }
            case 0x356: {
                if (len < 6) return false;
                float voltage = s16(d) * 0.01f;
                float current = s16(d + 2) * 0.1f;
                float temp = s16(d + 4) * 0.1f;
                if (voltage < 40.0f || voltage > 60.0f ||
                    temp < -40.0f || temp > 90.0f) return false;
                data_.packVoltageV = voltage;
                data_.packCurrentA = current;
                data_.temperatureC = temp;
                stamp(2, nowMs); return true;
            }
            case 0x359:
                if (len < 4) return false;
                data_.protection0 = d[0]; data_.protection1 = d[1];
                data_.warning0 = d[2]; data_.warning1 = d[3];
                stamp(3, nowMs); return true;
            case 0x35C:
                if (len < 1) return false;
                data_.chargeEnable = (d[0] & 0x80u) != 0;
                data_.dischargeEnable = (d[0] & 0x40u) != 0;
                stamp(4, nowMs); return true;
            default:
                return false;
        }
    }

    // Require each safety-critical frame; do not infer permission from SOC.
    bool valid(uint32_t nowMs) const {
        if (received_ != 0x1Fu) return false;
        for (unsigned i = 0; i < 5; ++i)
            if (static_cast<uint32_t>(nowMs - lastMs_[i]) > FRESH_MS)
                return false;
        return true;
    }

    bool canCharge(uint32_t nowMs) const {
        if (!valid(nowMs) || !data_.chargeEnable ||
            data_.chargeLimitA < 0.1f) return false;
        // The fallback output is 43.7 V; under-voltage is not a safe
        // condition for direct connection without a separate DC switch.
        if (data_.packVoltageV <= 43.7f ||
            data_.packVoltageV > 57.5f) return false;
        if (data_.temperatureC < 0.0f || data_.temperatureC > 45.0f)
            return false;
        // Pylontech V1.2 protection bits: undervolt/overvolt/temp,
        // charge overcurrent, internal BMS error. Reject any asserted
        // protection bit, including manufacturer-specific bits.
        if (data_.protection0 || data_.protection1) return false;
        // Charge-related warning bits in 0x359:
        // B2 bits 1..4 (voltage / temperature); B3 bits 0/3.
        if ((data_.warning0 & 0x1Eu) || (data_.warning1 & 0x09u))
            return false;
        return true;
    }

    const Data& data() const { return data_; }
    uint8_t receivedMask() const { return received_; }

private:
    Data data_{};
    uint32_t lastMs_[5] = {};
    uint8_t received_ = 0;

    static uint16_t u16(const uint8_t* d) {
        return static_cast<uint16_t>(d[0]) |
               (static_cast<uint16_t>(d[1]) << 8);
    }
    static int16_t s16(const uint8_t* d) {
        return static_cast<int16_t>(u16(d));
    }
    void stamp(unsigned idx, uint32_t ms) {
        lastMs_[idx] = ms;
        received_ |= static_cast<uint8_t>(1u << idx);
    }
};
