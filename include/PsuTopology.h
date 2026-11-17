#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Connection topology is a physical wiring setting, not a software switch.
// Supported: up to 3 independent CAN-connected Flatpack2 rectifiers.
// A single PSU behaves identically in either mode.
class PsuTopology {
public:
    enum class Mode : uint8_t { SERIES = 0, PARALLEL = 1 };

    void load(Mode defaultMode, uint8_t defaultCount, const char* storageNamespace) {
        namespace_ = storageNamespace;
        mode_ = defaultMode;
        count_ = validCount(defaultCount) ? defaultCount : 1;
        Preferences prefs;
        if (prefs.begin(namespace_, true)) {
            uint8_t m = prefs.getUChar("mode", static_cast<uint8_t>(mode_));
            uint8_t n = prefs.getUChar("count", count_);
            if (m <= 1 && validCount(n)) {
                mode_ = static_cast<Mode>(m);
                count_ = n;
            }
            prefs.end();
        }
    }

    bool configure(Mode mode, uint8_t count) {
        if (static_cast<uint8_t>(mode) > 1 || !validCount(count))
            return false;
        Preferences prefs;
        if (!prefs.begin(namespace_, false)) return false;
        bool ok = prefs.putUChar("mode", static_cast<uint8_t>(mode)) == 1;
        ok = prefs.putUChar("count", count) == 1 && ok;
        prefs.end();
        if (ok) { mode_ = mode; count_ = count; }
        return ok;
    }

    Mode mode() const { return mode_; }
    uint8_t count() const { return count_; }
    uint8_t seriesCount() const { return mode_ == Mode::SERIES ? count_ : 1; }
    uint8_t parallelCount() const { return mode_ == Mode::PARALLEL ? count_ : 1; }
    const char* name() const { return mode_ == Mode::SERIES ? "series" : "parallel"; }

private:
    static bool validCount(uint8_t count) { return count >= 1 && count <= 3; }
    const char* namespace_ = "psu-topology";
    Mode mode_ = Mode::SERIES;
    uint8_t count_ = 1;
};
