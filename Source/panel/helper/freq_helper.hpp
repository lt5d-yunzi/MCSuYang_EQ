// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cmath>

namespace zlpanel::freq_helper {
    inline double getSliderMax(const double sample_rate) {
        // Allow up to 20 kHz, but clamp to 0.49 * sample_rate to prevent digital filter Nyquist instability.
        return std::min(20000.0, sample_rate * 0.49);
    }

    inline double getFFTMax(const double sample_rate) {
        if (sample_rate > 352000.0) {
            return 176000.0;
        } else if (sample_rate > 176000.0) {
            return 88000.0;
        } else if (sample_rate > 88000.0) {
            return 44000.0;
        } else {
            // Ensure visual axis goes up to at least 30 kHz
            return std::max(30000.0, sample_rate * 0.5);
        }
    }
}
