// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "filter_dynamic_attach.hpp"

namespace zlp {
    FilterDynamicAttach::FilterDynamicAttach(juce::AudioProcessor&,
                                             juce::AudioProcessorValueTreeState& parameters,
                                             Controller& controller, const size_t idx) :
        parameters_(parameters),
        controller_(controller),
        idx_(idx) {
        for (size_t i = 0; i < kIDs.size(); ++i) {
            const auto ID = kIDs[i] + std::to_string(idx_);
            parameters_.addParameterListener(ID, this);
            parameterChanged(ID, parameters.getRawParameterValue(ID)->load(std::memory_order::relaxed));
        }
    }

    FilterDynamicAttach::~FilterDynamicAttach() {
        for (size_t i = 0; i < kIDs.size(); ++i) {
            parameters_.removeParameterListener(kIDs[i] + std::to_string(idx_), this);
        }
    }

    void FilterDynamicAttach::parameterChanged(const juce::String& parameter_ID, const float value) {
        if (parameter_ID.startsWith(PDynamicON::kID)) {
            if (value > .5f) {
                // Check if the current filter type is HighPass or LowPass — if so, deny enabling dynamic
                const auto ftype_value = parameters_.getRawParameterValue(PFilterType::kID + std::to_string(idx_));
                if (ftype_value != nullptr) {
                    const auto ftype = choiceIndexToFilterType(static_cast<int>(std::round(ftype_value->load(std::memory_order::relaxed))));
                    if (ftype == zldsp::filter::kHighPass || ftype == zldsp::filter::kLowPass) {
                        // Force dynamic OFF for cut filters
                        if (auto* param = parameters_.getParameter(PDynamicON::kID + std::to_string(idx_))) {
                            param->setValueNotifyingHost(0.0f);
                        }
                        controller_.setDynamicON(idx_, false);
                        return;
                    }
                }
            }
            controller_.setDynamicON(idx_, value > .5f);
        } else if (parameter_ID.startsWith(PDynamicBypass::kID)) {
            controller_.setDynamicBypass(idx_, value > .5f);
        } else if (parameter_ID.startsWith(PDynamicLearn::kID)) {
            controller_.setDynamicLearn(idx_, value > .5f);
        } else if (parameter_ID.startsWith(PDynamicRelative::kID)) {
            controller_.setDynamicRelative(idx_, value > .5f);
        } else if (parameter_ID.startsWith(PSideSwap::kID)) {
            controller_.setDynamicSwap(idx_, value > .5f);
        } else if (parameter_ID.startsWith(PThreshold::kID)) {
            controller_.setDynamicThreshold(idx_, value);
        } else if (parameter_ID.startsWith(PKneeW::kID)) {
            controller_.setDynamicKnee(idx_, value);
        } else if (parameter_ID.startsWith(PAttack::kID)) {
            controller_.setDynamicAttack(idx_, value);
        } else if (parameter_ID.startsWith(PRelease::kID)) {
            controller_.setDynamicRelease(idx_, value);
        } else if (parameter_ID.startsWith(PDynamicRMSLength::kID)) {
            controller_.setDynamicRMSLength(idx_, value * 0.001f);
        } else if (parameter_ID.startsWith(PDynamicRMSMix::kID)) {
            controller_.setDynamicRMSMix(idx_, value * 0.01f);
        } else if (parameter_ID.startsWith(PDynamicSmooth::kID)) {
            controller_.setDynamicSmooth(idx_, value * 0.01f);
        } else if (parameter_ID.startsWith(PPitchTrackHarmonic::kID)) {
            controller_.setPitchTrackHarmonic(idx_, static_cast<int>(std::round(value)));
        } else if (parameter_ID.startsWith(PPitchTrack::kID)) {
            controller_.setPitchTrack(idx_, value > .5f);
            if (value > .5f) {
                if (auto* param = parameters_.getParameter(PPrecise::kID + std::to_string(idx_))) {
                    if (param->getValue() > .1f) {
                        param->setValueNotifyingHost(0.0f);
                    }
                }
            }
        } else if (parameter_ID.startsWith(PPrecise::kID)) {
            const int precise_mode = static_cast<int>(std::round(value));
            controller_.setPreciseON(idx_, precise_mode);
            if (precise_mode > 0) {
                if (auto* param = parameters_.getParameter(PPitchTrack::kID + std::to_string(idx_))) {
                    if (param->getValue() > .5f) {
                        param->setValueNotifyingHost(0.0f);
                    }
                }
            }
        } else if (parameter_ID.startsWith(PFilterType::kID)) {
            // When filter type changes to HighPass or LowPass, force dynamic OFF
            const auto ftype = choiceIndexToFilterType(static_cast<int>(std::round(value)));
            if (ftype == zldsp::filter::kHighPass || ftype == zldsp::filter::kLowPass) {
                if (auto* param = parameters_.getParameter(PDynamicON::kID + std::to_string(idx_))) {
                    if (param->getValue() > .5f) {
                        param->setValueNotifyingHost(0.0f);
                    }
                }
                controller_.setDynamicON(idx_, false);
            }
        }
    }
}
