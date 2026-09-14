#pragma once

#include <Preferences.h>
#include "FPort85ReportSettings.h"

namespace multibus::lorawan {

class FPort85ReportSettingsStore {
public:
    bool begin() {
        return prefs_.begin("multibus", false);
    }

    void end() {
        prefs_.end();
    }

    bool load(ReportIntervalSettings& settings) const {
        if (!prefs_.isKey(kKey)) {
            settings = ReportIntervalSettings{};
            return true;
        }
        settings.seconds = prefs_.getUShort(kKey, kDefaultReportIntervalSeconds);
        return validReportIntervalSettings(settings);
    }

    bool save(const ReportIntervalSettings& settings) {
        if (!validReportIntervalSettings(settings)) return false;
        return prefs_.putUShort(kKey, settings.seconds) == sizeof(uint16_t);
    }

private:
    static constexpr const char* kKey = "lw_report_s";
    mutable Preferences prefs_;
};

} // namespace multibus::lorawan
