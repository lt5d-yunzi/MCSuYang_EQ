// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "response_panel.hpp"

namespace zlpanel {
    ResponsePanel::ResponsePanel(PluginProcessor& p,
                                 zlgui::UIBase& base,
                                 const multilingual::TooltipHelper& tooltip_helper) :
        Thread("response"),
        p_ref_(p), base_(base),
        gain_scale_(*p.parameters_.getRawParameterValue(zlp::PGainScale::kID)),
        single_panel_(p, base, message_not_off_indices_),
        sum_panel_(p, base),
        dragger_panel_(p, base, tooltip_helper),
        solo_panel_(p, base),
        eq_max_db_idx_ref_(*p.parameters_NA_.getRawParameterValue(zlstate::PEQMaxDB::kID)) {
        juce::ignoreUnused(base_, tooltip_helper);
        for (auto& f : precise_ideal_) {
            f.prepare(p.getSampleRate());
        }
        for (auto& band_array : precise_sub_ideal_) {
            for (auto& f : band_array) {
                f.prepare(p.getSampleRate());
            }
        }
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            const auto band_str = std::to_string(band);
            for (const auto& ID : kIDs) {
                const auto band_ID = ID + band_str;
                p_ref_.parameters_.addParameterListener(band_ID, this);
                parameterChanged(band_ID,
                                 p_ref_.parameters_.getRawParameterValue(band_ID)->load(std::memory_order::relaxed));
            }
        }
        p_ref_.parameters_.addParameterListener(zlp::PGainScale::kID, this);

        xs_.resize(kNumPoints);
        ws_.resize(kNumPoints);
        for (size_t i = 0; i < zlp::kBandNum; ++i) {
            base_mags_[i].resize(kNumPoints);
            target_mags_[i].resize(kNumPoints);
            dynamic_mags_[i].resize(kNumPoints);
        }
        for (size_t i = 0; i < 5; ++i) {
            sum_mags_[i].resize(kNumPoints);
            on_lr_indices_[i].reserve(zlp::kBandNum);
        }
        std::fill(band_center_idx_.begin(), band_center_idx_.end(), -1);
        addAndMakeVisible(single_panel_);
        addAndMakeVisible(sum_panel_);
        addAndMakeVisible(dragger_panel_);
        dragger_panel_.addChildComponent(solo_panel_);
        solo_panel_.setAlwaysOnTop(true);
        dragger_panel_.getFloatPopPanel().setAlwaysOnTop(true);
        dragger_panel_.getFloatPopPanel().toFront(true);
        dragger_panel_.getRightClickPanel().setAlwaysOnTop(true);
        dragger_panel_.getRightClickPanel().toFront(true);
        setInterceptsMouseClicks(false, true);
    }

    ResponsePanel::~ResponsePanel() {
        dragger_panel_.removeChildComponent(&solo_panel_);
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            const auto band_str = std::to_string(band);
            for (const auto& ID : kIDs) {
                p_ref_.parameters_.removeParameterListener(ID + band_str, this);
            }
        }
        p_ref_.parameters_.removeParameterListener(zlp::PGainScale::kID, this);
    }

    void ResponsePanel::paint(juce::Graphics& g) {
        // Draw top and bottom descriptor bars first (so they are behind the curves)
        const double freq_max = freq_helper::getFFTMax(sample_rate_.load(std::memory_order::relaxed));
        if (freq_max >= 10000.0) {
            const auto font_size = base_.getFontSize();
            const auto top_bar_height = font_size * 1.4f;
            const auto bottom_bar_height = font_size * 1.4f;

            const auto full_width = static_cast<float>(getWidth());
            const auto full_height = static_cast<float>(getHeight());
            
            // Scaled graph width
            const auto graph_width = full_width * kFFTSizeOverWidth;

            // 1. Draw top bar background and divider line
            const auto bar_bg_color = base_.getBackgroundColour().darker(0.15f);
            g.setColour(bar_bg_color);
            g.fillRect(0.f, 0.f, full_width, top_bar_height);

            g.setColour(base_.getColourByIdx(zlgui::ColourIdx::kGridColour).withAlpha(0.15f));
            g.drawHorizontalLine(static_cast<int>(top_bar_height), 0.f, full_width);

            // 2. Draw bottom bar background and divider line
            const auto bottom_bar_y = full_height - bottom_bar_height;
            g.setColour(bar_bg_color);
            g.fillRect(0.f, bottom_bar_y, full_width, bottom_bar_height);

            g.setColour(base_.getColourByIdx(zlgui::ColourIdx::kGridColour).withAlpha(0.15f));
            g.drawHorizontalLine(static_cast<int>(bottom_bar_y), 0.f, full_width);

            // Helper lambda for frequency to X mapping
            auto getX = [&](double freq) -> float {
                const auto p = std::log(freq * .1) / std::log(freq_max * .1);
                return static_cast<float>(p) * graph_width;
            };

            struct Descriptor {
                juce::String label;
                double start_freq;
                double end_freq;
            };

            const std::array<Descriptor, 6> top_descriptors = {
                Descriptor{ juce::String::fromUTF8("\xe5\x8e\x9a\xe9\x87\x8d\xe6\x84\x9f"), 10.0, 60.0 },
                Descriptor{ juce::String::fromUTF8("\xe6\xb8\xa9\xe6\x9a\x96\xe6\x84\x9f"), 60.0, 150.0 },
                Descriptor{ juce::String::fromUTF8("\xe8\xb4\xa8\xe6\x84\x9f"), 150.0, 500.0 },
                Descriptor{ juce::String::fromUTF8("\xe5\xad\x98\xe5\x9c\xa8\xe6\x84\x9f"), 500.0, 2000.0 },
                Descriptor{ juce::String::fromUTF8("\xe6\xb8\x85\xe6\x99\xb0\xe5\xba\xa6"), 2000.0, 8000.0 },
                Descriptor{ juce::String::fromUTF8("\xe7\xa9\xba\xe6\xb0\x94\xe6\x84\x9f"), 8000.0, freq_max }
            };

            const std::array<Descriptor, 6> bottom_descriptors = {
                Descriptor{ juce::String::fromUTF8("\xe8\xbd\xb0\xe9\xb8\xa3\xe6\x84\x9f"), 10.0, 50.0 },
                Descriptor{ juce::String::fromUTF8("\xe8\xbd\xb0\xe5\x93\x8d\xe6\x84\x9f"), 50.0, 150.0 },
                Descriptor{ juce::String::fromUTF8("\xe6\xb5\x91\xe6\xb5\x8a\xe6\x84\x9f"), 150.0, 400.0 },
                Descriptor{ juce::String::fromUTF8("\xe9\xbc\xbb\xe9\x9f\xb3"), 400.0, 1500.0 },
                Descriptor{ juce::String::fromUTF8("\xe5\x88\xba\xe8\x80\xb3\xe6\x84\x9f"), 1500.0, 6000.0 },
                Descriptor{ juce::String::fromUTF8("\xe5\x98\xb6\xe5\xa3\xb0"), 6000.0, freq_max }
            };

            // Draw top descriptors and dividers
            g.setFont(font_size * 0.95f);
            for (size_t i = 0; i < top_descriptors.size(); ++i) {
                const auto& desc = top_descriptors[i];
                float x_start = getX(desc.start_freq);
                float x_end = getX(desc.end_freq);
                if (x_start < 0.f) x_start = 0.f;
                if (x_end > graph_width) x_end = graph_width;

                auto rect = juce::Rectangle<float>(x_start, 0.f, x_end - x_start, top_bar_height);
                
                // Draw text
                g.setColour(base_.getTextColour().withAlpha(0.65f));
                g.drawText(desc.label, rect, juce::Justification::centred, false);

                // Draw divider if not the last item
                if (i < top_descriptors.size() - 1) {
                    g.setColour(base_.getColourByIdx(zlgui::ColourIdx::kGridColour).withAlpha(0.12f));
                    g.drawVerticalLine(static_cast<int>(x_end), 0.f, top_bar_height);
                }
            }

            // Draw bottom descriptors and dividers
            for (size_t i = 0; i < bottom_descriptors.size(); ++i) {
                const auto& desc = bottom_descriptors[i];
                float x_start = getX(desc.start_freq);
                float x_end = getX(desc.end_freq);
                if (x_start < 0.f) x_start = 0.f;
                if (x_end > graph_width) x_end = graph_width;

                auto rect = juce::Rectangle<float>(x_start, bottom_bar_y, x_end - x_start, bottom_bar_height);
                
                // Draw text
                g.setColour(base_.getTextColour().withAlpha(0.65f));
                g.drawText(desc.label, rect, juce::Justification::centred, false);

                // Draw divider if not the last item
                if (i < bottom_descriptors.size() - 1) {
                    g.setColour(base_.getColourByIdx(zlgui::ColourIdx::kGridColour).withAlpha(0.12f));
                    g.drawVerticalLine(static_cast<int>(x_end), bottom_bar_y, full_height);
                }
            }
        }

        const auto should_alpha = static_cast<float>(base_.getPanelProperty(zlgui::kCurveShouldTransparent)) > .5f;
        if (should_alpha) {
            g.beginTransparencyLayer(.5f);
        }
        single_panel_.paintDifferentStereo(g);
        sum_panel_.paintDifferentStereo(g);
        single_panel_.paintSameStereo(g);
        sum_panel_.paintSameStereo(g);
        if (should_alpha) {
            g.endTransparencyLayer();
        }
    }

    void ResponsePanel::resized() {
        const auto bound = getLocalBounds();
        const auto font_size = base_.getFontSize();
        const auto bottom_height = getBottomAreaHeight(font_size);
        single_panel_.setBounds(bound);
        sum_panel_.setBounds(bound);
        dragger_panel_.setBounds(bound);
        solo_panel_.setBounds(bound);
        side_y_ = static_cast<float>(bound.getHeight() - bottom_height) - font_size * kDraggerScale * .5f + 20.f;
        width_.store(static_cast<float>(bound.getWidth()), std::memory_order::relaxed);
        height_.store(static_cast<float>(bound.getHeight()), std::memory_order::relaxed);
        font_size_.store(base_.getFontSize(), std::memory_order::relaxed);
        to_update_bound_.signal();
    }

    void ResponsePanel::repaintCallBack() {
        bool any_dynamic_visuals = false;
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            if (p_ref_.getController().isPitchTrackingActive(band) ||
                p_ref_.getController().isPreciseActive(band)) {
                any_dynamic_visuals = true;
                break;
            }
        }
        if (any_dynamic_visuals) {
            notify();
        }
        updateDraggerPositions();
        updateDrawingParas();
        dragger_panel_.repaintCallBack();
    }

    void ResponsePanel::updateDraggerPositions() {
        updateSoloPosition();
        if (!message_to_update_draggers_total_.check()) {
            return;
        }
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            if (message_to_update_draggers_[band].check()) {
                dragger_panel_.updateFilterType(band, empty_[band].getFilterType());
                dragger_panel_.getDragger(band).updateButton(
                {points_[band][0].load(std::memory_order::relaxed),
                 points_[band][4].load(std::memory_order::relaxed)});
            }
        }
        updateFloatingPosition();
        updateTargetPosition();
        updateSidePosition();
    }

    void ResponsePanel::updateSoloPosition() {
        if (!solo_panel_.isVisible()) {
            return;
        }
        if (const auto band = base_.getSelectedBand(); band < zlp::kBandNum) {
            if (solo_panel_.isSoloSide()) {
                solo_panel_.updateX(side_points_[band][1].load(std::memory_order::relaxed),
                                    side_points_[band][2].load(std::memory_order::relaxed));
            } else {
                solo_panel_.updateX(points_[band][1].load(std::memory_order::relaxed),
                                    points_[band][2].load(std::memory_order::relaxed));
            }
        }
    }

    void ResponsePanel::updateTargetPosition() {
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            if (!dragger_panel_.isTargetHovering(band)) {
                dragger_panel_.getTargetDragger(band).updateButton(
                {points_[band][0].load(std::memory_order::relaxed),
                 points_[band][5].load(std::memory_order::relaxed)});
            }
        }
    }

    void ResponsePanel::updateSidePosition() {
        if (const auto band = base_.getSelectedBand(); band < zlp::kBandNum) {
            dragger_panel_.getSideDragger().updateButton(
            {side_points_[band][0].load(std::memory_order::relaxed),
             side_y_});
        }
    }

    void ResponsePanel::updateFloatingPosition() {
        if (const auto band = base_.getSelectedBand(); band < zlp::kBandNum) {
            dragger_panel_.getFloatPopPanel().updatePosition(
            {points_[band][0].load(std::memory_order::relaxed),
             points_[band][4].load(std::memory_order::relaxed)});
        }
    }

    void ResponsePanel::updateDrawingParas() {
        if (!message_to_update_panels_.check()) {
            return;
        }
        message_not_off_indices_.clear();
        const auto selected_band = base_.getSelectedBand();
        const auto selected_lr_mode = selected_band < zlp::kBandNum
            ? lr_modes_[selected_band].load(std::memory_order::relaxed)
            : 0;
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            const auto filter_status = filter_status_[band].load(std::memory_order::relaxed);
            if (filter_status != zlp::FilterStatus::kOff) {
                message_not_off_indices_.emplace_back(band);
                const auto dynamic_on = dynamic_ons_[band].load(std::memory_order::relaxed);
                const auto lr_mode = lr_modes_[band].load(std::memory_order::relaxed);
                const auto is_same_stereo = selected_band < zlp::kBandNum ? lr_mode == selected_lr_mode : true;
                single_panel_.updateDrawingParas(band, filter_status, dynamic_on, is_same_stereo);
                dragger_panel_.updateDrawingParas(band, filter_status, dynamic_on, is_same_stereo, lr_mode);
                dragger_panel_.getDragger(band).updateButton(
                {points_[band][0].load(std::memory_order::relaxed),
                 points_[band][4].load(std::memory_order::relaxed)});
            } else {
                single_panel_.updateDrawingParas(band, zlp::FilterStatus::kOff, false, false);
                dragger_panel_.updateDrawingParas(band, zlp::FilterStatus::kOff, false, false, 0);
                dragger_panel_.getDragger(band).updateButton({-1000.f, -1000.f});
            }
        }
        updateFloatingPosition();
        updateTargetPosition();
        updateSidePosition();
        for (int lr = 0; lr < 5; ++lr) {
            if (selected_band < zlp::kBandNum) {
                sum_panel_.updateDrawingParas(lr, lr == selected_lr_mode);
            } else {
                sum_panel_.updateDrawingParas(lr, true);
            }
        }
    }

    void ResponsePanel::repaintCallBackSlow() {
        dragger_panel_.repaintCallBackSlow();
    }

    void ResponsePanel::updateBand() {
        message_to_update_panels_.signal();
        dragger_panel_.updateBand();
        solo_panel_.updateBand();
        updateFloatingPosition();
        updateTargetPosition();
        updateSidePosition();
    }

    void ResponsePanel::updateSampleRate(const double sample_rate) {
        sample_rate_.store(sample_rate, std::memory_order::relaxed);
        dragger_panel_.updateSampleRate(sample_rate);
    }

    void ResponsePanel::run() {
        while (!threadShouldExit()) {
            const auto flag = wait(-1);
            juce::ignoreUnused(flag);
            if (threadShouldExit()) {
                break;
            }
            updateCurveParas();
            if (threadShouldExit()) {
                break;
            }
            if (!updateCurveMags()) {
                break;
            }
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                // Multi-point xs_ injection for accurate path drawing
                const int inject_idx = band_center_idx_[band];
                bool injected = (inject_idx >= 0 && c_filter_status_[band] != zlp::FilterStatus::kOff);
                int inject_start = 0, inject_end = -1;
                std::array<float, 2 * kInjectHalf + 1> orig_xs{};

                if (injected) {
                    inject_start = std::max(0, inject_idx - kInjectHalf);
                    inject_end = std::min(static_cast<int>(kNumPoints) - 1, inject_idx + kInjectHalf);
                    const auto interval_x = static_cast<float>(c_width_) / static_cast<float>(kNumPoints - 1);
                    const float idx_frac = band_center_idx_frac_[band];

                    for (int k = inject_start; k <= inject_end; ++k) {
                        orig_xs[k - inject_start] = xs_[k];
                        const auto offset = static_cast<float>(k - inject_idx) * static_cast<float>(kInjectStep);
                        xs_[k] = (idx_frac + offset) * interval_x;
                    }
                }

                single_panel_.run(band, c_filter_status_[band],
                                  to_update_base_y_flags_[band],
                                  to_update_target_y_flags_[band],
                                  xs_, c_k_, c_b_,
                                  base_mags_[band], target_mags_[band],
                                  points_[band][0].load(std::memory_order::relaxed),
                                  points_[band][3].load(std::memory_order::relaxed),
                                  points_[band][4].load(std::memory_order::relaxed),
                                  to_update_side_y_flags_[band],
                                  side_points_[band][1].load(std::memory_order::relaxed),
                                  side_points_[band][2].load(std::memory_order::relaxed));

                // Restore xs_ so next band and sum panel get a clean grid
                if (injected) {
                    for (int k = inject_start; k <= inject_end; ++k) {
                        xs_[k] = orig_xs[k - inject_start];
                    }
                }

                if (threadShouldExit()) {
                    break;
                }
            }
            // For sum curve: inject ALL active bands' center xs simultaneously
            {
                struct InjectInfo { int idx; float orig; };
                std::array<InjectInfo, zlp::kBandNum * (2 * kInjectHalf + 1)> sum_saves{};
                int num_saved = 0;
                const auto interval_x = static_cast<float>(c_width_) / static_cast<float>(kNumPoints - 1);

                for (size_t band = 0; band < zlp::kBandNum; ++band) {
                    const int idx = band_center_idx_[band];
                    if (idx >= 0 && c_filter_status_[band] != zlp::FilterStatus::kOff) {
                        const int s = std::max(0, idx - kInjectHalf);
                        const int e = std::min(static_cast<int>(kNumPoints) - 1, idx + kInjectHalf);
                        const float idx_frac = band_center_idx_frac_[band];
                        for (int k = s; k <= e; ++k) {
                            sum_saves[num_saved] = {k, xs_[k]};
                            ++num_saved;
                            const auto offset = static_cast<float>(k - idx) * static_cast<float>(kInjectStep);
                            xs_[k] = (idx_frac + offset) * interval_x;
                        }
                    }
                }

                for (size_t lr = 0; lr < 5; ++lr) {
                    sum_panel_.run(lr, to_update_lr_flags_[lr], is_lr_not_off_flags_[lr],
                                   on_lr_indices_[lr],
                                   xs_, c_k_, c_b_,
                                   dynamic_mags_);
                    if (threadShouldExit()) {
                        break;
                    }
                }

                // Restore all injected xs values
                for (int i = num_saved - 1; i >= 0; --i) {
                    xs_[sum_saves[i].idx] = sum_saves[i].orig;
                }
            }
        }
    }

    void ResponsePanel::parameterChanged(const juce::String& parameter_ID, const float value) {
        if (parameter_ID.startsWith(zlp::PGainScale::kID)) {
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                empty_[band].setGain(
                    std::clamp(
                        original_base_gains_[band].load(std::memory_order::relaxed) * value / 100.f, -30.f, 30.f));
                to_update_empty_flags_[band].signal();
                target_gains_[band].store(
                    std::clamp(
                        original_target_gains_[band].load(std::memory_order::relaxed) * value / 100.f, -30.f, 30.f),
                    std::memory_order::relaxed);
                to_update_target_gain_flags_[band].signal();
            }
            return;
        }
        const auto band = static_cast<size_t>(parameter_ID.getTrailingIntValue());
        if (parameter_ID.startsWith(zlp::PFilterStatus::kID)) {
            filter_status_[band].store(static_cast<zlp::FilterStatus>(std::round(value)), std::memory_order::relaxed);
            to_update_filter_status_.signal();
        } else if (parameter_ID.startsWith(zlp::PLRMode::kID)) {
            lr_modes_[band].store(static_cast<int>(std::round(value)), std::memory_order::relaxed);
            to_update_lr_modes_.signal();
        } else if (parameter_ID.startsWith(zlp::PFilterType::kID)) {
            empty_[band].setFilterType(zlp::choiceIndexToFilterType(static_cast<int>(std::round(value))));
            to_update_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::POrder::kID)) {
            empty_[band].setOrder(zlp::POrder::kOrderArray[static_cast<size_t>(std::round(value))]);
            to_update_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PFreq::kID)) {
            empty_[band].setFreq(value);
            to_update_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PGain::kID)) {
            original_base_gains_[band].store(value, std::memory_order::relaxed);
            empty_[band].setGain(
                std::clamp(value * gain_scale_.load(std::memory_order::relaxed) / 100.f, -30.f, 30.f));
            to_update_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PQ::kID)) {
            empty_[band].setQ(value);
            to_update_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PDynamicON::kID)) {
            dynamic_ons_[band].store(value > .5f, std::memory_order::relaxed);
            to_update_dynamic_ons_.signal();
        } else if (parameter_ID.startsWith(zlp::PTargetGain::kID)) {
            original_target_gains_[band].store(value, std::memory_order::relaxed);
            target_gains_[band].store(
                std::clamp(value * gain_scale_.load(std::memory_order::relaxed) / 100.f, -30.f, 30.f),
                std::memory_order::relaxed);
            to_update_target_gain_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PSideFilterType::kID)) {
            if (value < .5f) {
                side_empty_[band].setFilterType(zldsp::filter::kBandPass);
            } else if (value < 1.5f) {
                side_empty_[band].setFilterType(zldsp::filter::kLowPass);
            } else {
                side_empty_[band].setFilterType(zldsp::filter::kHighPass);
            }
            to_update_side_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PSideOrder::kID)) {
            side_empty_[band].setOrder(zlp::POrder::kOrderArray[static_cast<size_t>(std::round(value))]);
            to_update_side_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PSideFreq::kID)) {
            side_empty_[band].setFreq(value);
            to_update_side_empty_flags_[band].signal();
        } else if (parameter_ID.startsWith(zlp::PSideQ::kID)) {
            side_empty_[band].setQ(value);
            to_update_side_empty_flags_[band].signal();
        }
    }

    void ResponsePanel::updateCurveParas() {
        // update filter status
        if (to_update_filter_status_.check()) {
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                const auto new_filter_status = filter_status_[band].load(std::memory_order::relaxed);
                if (c_filter_status_[band] != new_filter_status) {
                    c_filter_status_[band] = new_filter_status;
                    to_update_base_y_flags_[band] = true;
                    to_update_lr_flags_[static_cast<size_t>(c_lr_modes_[band])] = true;
                }
            }
            to_update_lr_modes_.signal();
        }
        // update dynamic ons
        if (to_update_dynamic_ons_.check()) {
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                const auto dynamic_on = dynamic_ons_[band].load(std::memory_order::relaxed);
                if (c_dynamic_ons_[band] != dynamic_on) {
                    c_dynamic_ons_[band] = dynamic_ons_[band].load(std::memory_order::relaxed);
                    to_update_lr_flags_[static_cast<size_t>(c_lr_modes_[band])] = true;
                    if (!dynamic_on) {
                        dynamic_mags_[band] = base_mags_[band];
                    }
                }
            }
            message_to_update_panels_.signal();
        }
        // update lr modes for summing
        if (to_update_lr_modes_.check()) {
            for (auto& indices : on_lr_indices_) {
                indices.clear();
            }
            std::fill(is_lr_not_off_flags_.begin(), is_lr_not_off_flags_.end(), false);
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                const auto lr_mode = lr_modes_[band].load(std::memory_order::relaxed);
                if (lr_mode != c_lr_modes_[band]) {
                    to_update_lr_flags_[static_cast<size_t>(c_lr_modes_[band])] = true;
                    to_update_lr_flags_[static_cast<size_t>(lr_mode)] = true;
                    c_lr_modes_[band] = lr_mode;
                }
                if (c_filter_status_[band] != zlp::FilterStatus::kOff) {
                    if (c_filter_status_[band] == zlp::FilterStatus::kOn) {
                        on_lr_indices_[static_cast<size_t>(lr_mode)].emplace_back(band);
                    }
                    is_lr_not_off_flags_[static_cast<size_t>(lr_mode)] = true;
                }
            }
            message_to_update_panels_.signal();
        }
        // update sample rate
        if (const auto sample_rate = sample_rate_.load(std::memory_order::relaxed);
            std::abs(sample_rate - c_sample_rate_) > 1.0) {
            c_sample_rate_ = sample_rate;
            c_slider_max_ = freq_helper::getSliderMax(sample_rate);
            if (sample_rate < 40000.0) {
                return;
            }
            fft_max_ = freq_helper::getFFTMax(sample_rate);
            for (auto& f : ideal_) {
                f.prepare(sample_rate);
            }
            for (auto& f : side_ideal_) {
                f.prepare(sample_rate);
            }
            for (auto& f : precise_ideal_) {
                f.prepare(sample_rate);
            }
            for (auto& band_array : precise_sub_ideal_) {
                for (auto& f : band_array) {
                    f.prepare(sample_rate);
                }
            }
            const auto max_log_value = std::log(fft_max_ * 0.1) / static_cast<double>(kFFTSizeOverWidth);
            const auto interval_log_value = max_log_value / static_cast<double>(kNumPoints - 1);
            const auto freq_scale = 20.0 * std::numbers::pi / sample_rate;
            for (size_t i = 0; i < kNumPoints; ++i) {
                ws_[i] = static_cast<float>(std::exp(interval_log_value * static_cast<double>(i)) * freq_scale);
            }
            std::fill(to_update_base_y_flags_.begin(), to_update_base_y_flags_.end(), true);
        }
        // update width & xs
        if (to_update_bound_.check()) {
            c_width_ = width_.load(std::memory_order::relaxed);
            c_height_ = height_.load(std::memory_order::relaxed);
            c_font_size_ = font_size_.load(std::memory_order::relaxed);
            if (c_width_ < 1.f || c_height_ < 1.f) {
                return;
            }
            const auto interval_x_value = c_width_ / static_cast<float>(kNumPoints - 1);
            for (size_t i = 0; i < kNumPoints; ++i) {
                xs_[i] = static_cast<float>(i) * interval_x_value;
            }
            c_eq_max_db_idx_ = -1.f;
        }
        // update maximum db
        if (const auto eq_max_db_idx = eq_max_db_idx_ref_.load(std::memory_order::relaxed);
            std::abs(eq_max_db_idx - c_eq_max_db_idx_) > .1f) {
            c_eq_max_db_idx_ = eq_max_db_idx;
            const auto z = zlstate::PEQMaxDB::kDBs[static_cast<size_t>(std::round(eq_max_db_idx))];
            const auto h = c_height_ - static_cast<float>(getBottomAreaHeight(c_font_size_));
            const auto padding = c_font_size_ * kDraggerScale;
            const auto h1 = h * .5f;
            const auto h2 = h - padding;
            c_k_ = (h1 - h2) / z;
            c_b_ = h1;
            std::fill(to_update_base_y_flags_.begin(), to_update_base_y_flags_.end(), true);
        }

        // update db update flags
        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            to_update_base_y_flags_[band] = to_update_base_y_flags_[band]
                || to_update_empty_flags_[band].check()
                || p_ref_.getController().isPitchTrackingActive(band);
            to_update_target_y_flags_[band] = to_update_base_y_flags_[band]
                || to_update_target_gain_flags_[band].check();
            to_update_side_y_flags_[band] = to_update_base_y_flags_[band]
                || to_update_side_empty_flags_[band].check();
            const auto lr = c_lr_modes_[band];
            to_update_lr_flags_[static_cast<size_t>(lr)] = to_update_lr_flags_[static_cast<size_t>(lr)]
                || to_update_base_y_flags_[band] || c_dynamic_ons_[band];
        }
    }

    bool ResponsePanel::updateCurveMags() {
        // Precompute grid parameters for center frequency index calculation
        const auto max_log_value = std::log(fft_max_ * 0.1) / static_cast<double>(kFFTSizeOverWidth);
        const auto interval_log_value = max_log_value / static_cast<double>(kNumPoints - 1);
        const auto freq_scale = 20.0 * std::numbers::pi / c_sample_rate_;
        const auto interval_x_value = static_cast<double>(c_width_) / static_cast<double>(kNumPoints - 1);

        for (size_t band = 0; band < zlp::kBandNum; ++band) {
            if (c_filter_status_[band] != zlp::FilterStatus::kOff) {
                // Update per-band injection info when frequency changes
                if (to_update_base_y_flags_[band]) {
                    auto para = empty_[band].getParas();
                    if (p_ref_.getController().isPitchTrackingActive(band)) {
                        para.freq = p_ref_.getController().getCurrentFreq(band);
                    }
                    para.freq = std::min(para.freq, c_slider_max_);

                    const double center_w = para.freq * (2.0 * std::numbers::pi / c_sample_rate_);
                    const double idx_double = (std::log(center_w) - std::log(freq_scale)) / interval_log_value;
                    band_center_idx_[band] = std::clamp(
                        static_cast<int>(std::round(idx_double)), 0, static_cast<int>(kNumPoints - 1));
                    band_center_w_[band] = static_cast<float>(center_w);
                    band_center_x_[band] = static_cast<float>(idx_double * interval_x_value);
                    band_center_idx_frac_[band] = static_cast<float>(idx_double);
                }

                // Multi-point ws_ injection: replace center ± kInjectHalf points
                const int inject_idx = band_center_idx_[band];
                bool injected = false;
                int inject_start = 0, inject_end = -1;
                std::array<float, 2 * kInjectHalf + 1> orig_ws{};

                if (inject_idx >= 0) {
                    inject_start = std::max(0, inject_idx - kInjectHalf);
                    inject_end = std::min(static_cast<int>(kNumPoints) - 1, inject_idx + kInjectHalf);

                    // Save original ws values
                    for (int k = inject_start; k <= inject_end; ++k) {
                        orig_ws[k - inject_start] = ws_[k];
                    }

                    // Inject: place tightly-spaced points centered on the exact center frequency
                    const double cw = static_cast<double>(band_center_w_[band]);
                    for (int k = inject_start; k <= inject_end; ++k) {
                        const double offset = static_cast<double>(k - inject_idx) * kInjectStep;
                        ws_[k] = static_cast<float>(cw * std::exp(offset * interval_log_value));
                    }
                    injected = true;
                }

                // All magnitude computations for this band use the injected grid
                if (to_update_base_y_flags_[band]) {
                    auto para = empty_[band].getParas();
                    if (p_ref_.getController().isPitchTrackingActive(band)) {
                        para.freq = p_ref_.getController().getCurrentFreq(band);
                    }
                    para.freq = std::min(para.freq, c_slider_max_);
                    ideal_[band].forceUpdate(para);
                    ideal_[band].updateMagnitudeSquare(ws_, base_mags_[band]);
                    zldsp::vector::sqr_mag_to_db(base_mags_[band].data(), base_mags_[band].size());
                    if (!c_dynamic_ons_[band]) {
                        dynamic_mags_[band] = base_mags_[band];
                    }
                    const auto center_w = para.freq * (2.0 * std::numbers::pi / c_sample_rate_);
                    const float center_square_magnitude = zldsp::chore::squareGainToDecibels(
                        ideal_[band].getCenterMagnitudeSquare(static_cast<float>(center_w)));
                    const auto [left_x, center_x, right_x] = getLeftCenterRightX(para);

                    points_[band][0].store(static_cast<float>(center_x), std::memory_order::relaxed);
                    points_[band][1].store(static_cast<float>(left_x), std::memory_order::relaxed);
                    points_[band][2].store(static_cast<float>(right_x), std::memory_order::relaxed);
                    points_[band][3].store(c_k_ * center_square_magnitude + c_b_, std::memory_order::relaxed);
                    para.gain = original_base_gains_[band].load(std::memory_order::relaxed);
                    points_[band][4].store(c_k_ * getButtonMag(para) + c_b_, std::memory_order::relaxed);

                    if (threadShouldExit()) {
                        if (injected) {
                            for (int k = inject_start; k <= inject_end; ++k)
                                ws_[k] = orig_ws[k - inject_start];
                        }
                        return false;
                    }
                    message_to_update_draggers_[band].signal();
                    message_to_update_draggers_total_.signal();
                }
                if (to_update_target_y_flags_[band]) {
                    const auto target_gain = target_gains_[band].load(std::memory_order::relaxed);
                    ideal_[band].setGain(target_gain);
                    ideal_[band].updateCoeffs();
                    ideal_[band].updateMagnitudeSquare(ws_, target_mags_[band]);
                    zldsp::vector::sqr_mag_to_db(target_mags_[band].data(), target_mags_[band].size());
                    auto para = ideal_[band].getParas();
                    para.gain = original_target_gains_[band].load(std::memory_order::relaxed);
                    points_[band][5].store(c_k_ * getButtonMag(para) + c_b_, std::memory_order::relaxed);

                    if (threadShouldExit()) {
                        if (injected) {
                            for (int k = inject_start; k <= inject_end; ++k)
                                ws_[k] = orig_ws[k - inject_start];
                        }
                        return false;
                    }
                    message_to_update_target_dragger_.signal();
                    message_to_update_draggers_total_.signal();
                }
                if (to_update_side_y_flags_[band]) {
                    auto para = side_empty_[band].getParas();
                    if (p_ref_.getController().isPitchTrackingActive(band)) {
                        para.freq = p_ref_.getController().getCurrentFreq(band);
                    }
                    para.freq = std::min(para.freq, c_slider_max_);
                    const auto [left_x, center_x, right_x] = getLeftCenterRightX(para);
                    side_points_[band][0].store(center_x, std::memory_order::relaxed);
                    if (para.filter_type == zldsp::filter::kLowPass) {
                        side_points_[band][1].store(0.f, std::memory_order::relaxed);
                        side_points_[band][2].store(center_x, std::memory_order::relaxed);
                    } else if (para.filter_type == zldsp::filter::kHighPass) {
                        side_points_[band][1].store(center_x, std::memory_order::relaxed);
                        side_points_[band][2].store(c_width_, std::memory_order::relaxed);
                    } else {
                        side_points_[band][1].store(left_x, std::memory_order::relaxed);
                        side_points_[band][2].store(right_x, std::memory_order::relaxed);
                    }
                    message_to_update_side_dragger_.signal();
                    message_to_update_draggers_total_.signal();
                }
                if (c_dynamic_ons_[band]) {
                    const int precise_mode = p_ref_.getController().getPreciseMode(band);
                    if (precise_mode == 1) {
                        std::array<float, kNumPoints> sum_precise_mags_sq{};
                        for (size_t k = 0; k < kNumPoints; ++k) {
                            sum_precise_mags_sq[k] = 0.0f;
                        }

                        const int num_sub_bands = std::clamp(p_ref_.getController().getActiveSubBandCount(band), 10, 64);
                        for (size_t j = 0; j < static_cast<size_t>(num_sub_bands); ++j) {
                            const double sub_freq = p_ref_.getController().getSubFreq(band, j);
                            const double sub_gain = p_ref_.getController().getSubGain(band, j);
                            const double sub_q = p_ref_.getController().getSubQ(band, j);

                            zldsp::filter::FilterParameters precise_para;
                            precise_para.filter_type = zldsp::filter::kPeak;
                            precise_para.order = 2;
                            precise_para.freq = sub_freq;
                            precise_para.q = sub_q;
                            precise_para.gain = sub_gain;

                            precise_sub_ideal_[band][j].forceUpdate(precise_para);

                            std::array<float, kNumPoints> sub_mags_sq;
                            precise_sub_ideal_[band][j].updateMagnitudeSquare(ws_, sub_mags_sq);
                            zldsp::vector::sqr_mag_to_db(sub_mags_sq.data(), sub_mags_sq.size());

                            for (size_t k = 0; k < kNumPoints; ++k) {
                                sum_precise_mags_sq[k] += sub_mags_sq[k];
                            }
                        }

                        for (size_t k = 0; k < kNumPoints; ++k) {
                            dynamic_mags_[band][k] = base_mags_[band][k] + sum_precise_mags_sq[k];
                        }
                    } else {
                        ideal_[band].setGain(p_ref_.getController().getCurrentGain(band));
                        ideal_[band].updateCoeffs();
                        ideal_[band].updateMagnitudeSquare(ws_, dynamic_mags_[band]);
                        zldsp::vector::sqr_mag_to_db(dynamic_mags_[band].data(), dynamic_mags_[band].size());
                    }
                    if (threadShouldExit()) {
                        if (injected) {
                            for (int k = inject_start; k <= inject_end; ++k)
                                ws_[k] = orig_ws[k - inject_start];
                        }
                        return false;
                    }
                }

                // Restore the shared grid after this band's computations
                if (injected) {
                    for (int k = inject_start; k <= inject_end; ++k) {
                        ws_[k] = orig_ws[k - inject_start];
                    }
                }
            } else {
                points_[band][4].store(-10000.f, std::memory_order::relaxed);
            }
        }
        return true;
    }

    float ResponsePanel::getButtonMag(const zldsp::filter::FilterParameters& para) {
        if (para.filter_type == zldsp::filter::kPeak) {
            return static_cast<float>(para.gain);
        } else if (para.filter_type == zldsp::filter::kLowShelf
            || para.filter_type == zldsp::filter::kHighShelf
            || para.filter_type == zldsp::filter::kTiltShelf
            || para.filter_type == zldsp::filter::kFlatTilt) {
            return static_cast<float>(0.5 * para.gain);
        } else {
            return 0.f;
        }
    }

    std::tuple<float, float, float> ResponsePanel::getLeftCenterRightX(
        const zldsp::filter::FilterParameters& para) const {
        const auto freq_to_x_scale = 1.0 / std::log(
            fft_max_ * 0.1) * static_cast<double>(c_width_) * static_cast<double>(
            kFFTSizeOverWidth);
        const auto center_x = std::log(para.freq / 10.0) * freq_to_x_scale;

        switch (para.filter_type) {
        case zldsp::filter::kPeak:
        case zldsp::filter::kBandPass:
        case zldsp::filter::kNotch:
        default: {
            const auto bandwidth = para.freq / para.q;
            const auto left_f = 0.5 * bandwidth * (std::sqrt(4.0 * para.q * para.q + 1.0) - 1.0);
            const auto left_x = std::log(left_f / 10.0) * freq_to_x_scale;
            const auto right_f = left_f + bandwidth;
            const auto right_x = std::log(right_f / 10.0) * freq_to_x_scale;
            return std::make_tuple(static_cast<float>(left_x),
                                   static_cast<float>(center_x),
                                   static_cast<float>(right_x));
        }
        case zldsp::filter::kTiltShelf:
        case zldsp::filter::kFlatTilt: {
            const auto fixed_q = std::sqrt(2.0) * 0.03125;
            const auto bandwidth = para.freq / fixed_q;
            const auto left_f = 0.5 * bandwidth * (std::sqrt(4.0 * fixed_q * fixed_q + 1.0) - 1.0);
            const auto left_x = std::log(left_f / 10.0) * freq_to_x_scale;
            const auto right_f = left_f + bandwidth;
            const auto right_x = std::log(right_f / 10.0) * freq_to_x_scale;
            return std::make_tuple(static_cast<float>(left_x),
                                   static_cast<float>(center_x),
                                   static_cast<float>(right_x));
        }
        case zldsp::filter::kLowShelf:
        case zldsp::filter::kHighPass: {
            return std::make_tuple(0.f, static_cast<float>(center_x), static_cast<float>(center_x));
        }
        case zldsp::filter::kHighShelf:
        case zldsp::filter::kLowPass: {
            return std::make_tuple(static_cast<float>(center_x), static_cast<float>(center_x), c_width_);
        }
        }
    }

    void ResponsePanel::turnMatchON(const bool match_on) {
        dragger_panel_.setVisible(!match_on);
    }
}
