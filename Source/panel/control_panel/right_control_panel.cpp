// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//

#include "right_control_panel.hpp"
#include "BinaryData.h"

namespace zlpanel {
    RightControlPanel::RightControlPanel(PluginProcessor& p,
                                         zlgui::UIBase& base,
                                         const multilingual::TooltipHelper& tooltip_helper) :
        p_ref_(p), base_(base), updater_(),
        control_background_(base),
        dynamic_button_(base, juce::String::fromUTF8("\xe5\xbc\x80\xe5\x85\xb3"), tooltip_helper.getToolTipText(multilingual::kBandDynamic)),
        delete_button_(base, juce::String::fromUTF8("\xe5\x88\xa0"), juce::String::fromUTF8("删除当前频点")),
        band_bypass_button_(base, juce::String::fromUTF8("\xe5\xbc\x80\xe5\x85\xb3"), tooltip_helper.getToolTipText(multilingual::kBandBypass)),
        band_solo_button_(base, juce::String::fromUTF8("\xe7\x8b\xac\xe5\xa5\x8f"), tooltip_helper.getToolTipText(multilingual::kBandSolo)),
        active_band_label_("", ""),
        filter_type_box_(juce::StringArray{
            juce::String::fromUTF8("\xe5\xb3\xb0\xe8\xb0\xb7"), // 峰谷
            juce::String::fromUTF8("\xe4\xbd\x8e\xe5\x88\x87"), // 低切
            juce::String::fromUTF8("\xe4\xbd\x8e\xe6\x9e\xb6"), // 低架
            juce::String::fromUTF8("\xe9\xab\x98\xe6\x9e\xb6"), // 高架
            juce::String::fromUTF8("\xe9\xab\x98\xe5\x88\x87"), // 高切
            juce::String::fromUTF8("\xe5\x80\xbe\xe6\x96\x9c\xe6\x90\x81\xe6\x9e\xb6"), // 倾斜搁架
            juce::String::fromUTF8("\xe5\xb9\xb3\xe5\x9d\xa6\xe5\x80\xbe\xe6\x96\x9c")  // 平坦倾斜
        }, base, tooltip_helper.getToolTipText(multilingual::kBandType)),
        lr_mode_box_(juce::StringArray{
            juce::String::fromUTF8("\xe7\xab\x8b\xe4\xbd\x93\xe5\xa3\xb0"), // 立体声
            juce::String::fromUTF8("\xe5\xb7\xa6\xe5\xa3\xb0\xe9\x81\x93"), // 左声道
            juce::String::fromUTF8("\xe5\x8f\xb3\xe5\xa3\xb0\xe9\x81\x93"), // 右声道
            juce::String::fromUTF8("\xe4\xb8\xad\xe7\xbd\xae"),             // 中置
            juce::String::fromUTF8("\xe4\xbe\xa7\xe8\xbe\xb9")              // 侧边
        }, base, tooltip_helper.getToolTipText(multilingual::kBandStereoMode)),
        order_box_(juce::StringArray{
            "6 dB/oct", "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct"
        }, base, tooltip_helper.getToolTipText(multilingual::kBandSlope)),
        bypass_drawable_(juce::Drawable::createFromImageData(BinaryData::bypass_svg, BinaryData::bypass_svgSize)),
        bypass_button_(base, bypass_drawable_.get(), bypass_drawable_.get(), tooltip_helper.getToolTipText(multilingual::kBandDynamicBypass)),
        auto_button_(base, juce::String::fromUTF8("\xe8\x87\xaa\xe5\x8a\xa8"), tooltip_helper.getToolTipText(multilingual::kBandDynamicAuto)),
        link_button_(base, juce::String::fromUTF8("\xe5\x85\xb3\xe8\x81\x94"), tooltip_helper.getToolTipText(multilingual::kBandDynamicSideLink)),
        track_button_(base, juce::String::fromUTF8("\xe8\xbf\xbd\xe8\xb8\xaa"), juce::String::fromUTF8("\xe5\xbc\x80\xe5\x90\xaf/\xe5\x85\xb3\xe9\x97\xad\xe9\x9f\xb3\xe9\xab\x98\xe8\xbf\xbd\xe8\xb8\xaa")),
        precise_button_(base, juce::String::fromUTF8("\xe7\xb2\xbe\xe5\xaf\x86"), juce::String::fromUTF8("\xe5\xbc\x00\xe5\x90\xaf/\xe5\x85\xb3\xe9\x97\xad\xe7\xb2\xbe\xe5\xaf\x86\xe6\xa8\xa1\xe5\xbc\x8f")),
        harmonic_box_(zlp::PPitchTrackHarmonic::kChoices, base, juce::String::fromUTF8("\xe9\x80\x89\xe6\x8b\xa9\xe8\xbf\xbd\xe8\xb8\xaa\xe7\x9a\x84\xe8\xb0\x90\xe6\xb3\xa2")),
        static_freq_slider_("Freq", base, juce::String::fromUTF8("双击输入数值，上下拖拽调节频率。")),
        static_gain_slider_("Gain", base, juce::String::fromUTF8("双击输入数值，上下拖拽调节增益。")),
        static_q_slider_("Q", base, juce::String::fromUTF8("双击输入数值，上下拖拽调节宽度。")),
        th_slider_("Threshold", base, tooltip_helper.getToolTipText(multilingual::kBandDynamicThreshold)),
        attack_slider_("Attack", base, tooltip_helper.getToolTipText(multilingual::kBandDynamicAttack)),
        release_slider_("Release", base, tooltip_helper.getToolTipText(multilingual::kBandDynamicRelease)),
        range_slider_("Amount", base, juce::String::fromUTF8("双击输入数值，上下拖拽调节处理量。")),
        label_laf_(base),
        dynamic_label_("", "DYNAMIC"),
        knob_label_laf_(base),
        static_freq_label_("", juce::String::fromUTF8("频率")),
        static_gain_label_("", juce::String::fromUTF8("增益")),
        static_q_label_("", juce::String::fromUTF8("宽度")),
        th_label_("", juce::String::fromUTF8("\xe9\x98\x88\xe5\x80\xbc")),
        attack_label_("", juce::String::fromUTF8("\xe5\x90\xaf\xe5\x8a\xa8")),
        release_label_("", juce::String::fromUTF8("\xe9\x87\x8a\xe6\x94\xbe")),
        range_label_("", juce::String::fromUTF8("\xe5\xa4\x84\xe7\x90\x86\xe9\x87\x8f")) {
        control_background_.setVisible(false);
        addAndMakeVisible(control_background_);

        label_laf_.setFontScale(1.0f);

        for (auto* b : {&dynamic_button_, &auto_button_, &link_button_, &track_button_, &precise_button_}) {
            b->getLAF().setFontScale(0.95f);
            b->getLAF().setJustification(juce::Justification::centred);
            b->getLAF().setUseBandColour(true);
            b->getButton().setToggleable(true);
            b->getButton().setClickingTogglesState(true);
            b->setBufferedToImage(true);
            addAndMakeVisible(b);
        }

        delete_button_.getLAF().setFontScale(0.95f);
        delete_button_.getLAF().setJustification(juce::Justification::centred);
        delete_button_.getLAF().setUseBandColour(true);
        delete_button_.getButton().setToggleable(false);
        delete_button_.getButton().setClickingTogglesState(false);
        delete_button_.setBufferedToImage(true);
        addAndMakeVisible(delete_button_);
        delete_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                band_helper::turnOffBand(p_ref_, c_band, base_.getSelectedBandSet());
                const auto band1 = band_helper::findClosestBand<true>(p_ref_, c_band);
                const auto band2 = band_helper::findClosestBand<false>(p_ref_, c_band);
                if (band1 < zlp::kBandNum) {
                    base_.setSelectedBand(band1);
                } else if (band2 < zlp::kBandNum) {
                    base_.setSelectedBand(band2);
                } else {
                    base_.setSelectedBand(zlp::kBandNum);
                }
            }
        };

        for (auto* b : {&band_bypass_button_, &band_solo_button_}) {
            b->getLAF().setFontScale(0.95f);
            b->getLAF().setJustification(juce::Justification::centred);
            b->getLAF().setUseBandColour(true);
            b->getButton().setToggleable(true);
            b->getButton().setClickingTogglesState(true);
            b->setBufferedToImage(true);
            addAndMakeVisible(b);
        }

        band_bypass_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                updateValue(p_ref_.parameters_.getParameter(zlp::PFilterStatus::kID + std::to_string(c_band)),
                            band_bypass_button_.getButton().getToggleState() ? 1.f : .5f);
            }
        };

        band_solo_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                const auto current_solo = base_.getSoloWholeIdx();
                if (current_solo == c_band) {
                    base_.setSoloWholeIdx(2 * zlp::kBandNum);
                } else {
                    base_.setSoloWholeIdx(c_band);
                }
            }
        };

        active_band_label_.setLookAndFeel(&knob_label_laf_);
        active_band_label_.setJustificationType(juce::Justification::centred);
        active_band_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
        active_band_label_.setBufferedToImage(true);
        addAndMakeVisible(active_band_label_);
        active_band_label_.onClick = [this]() {
            juce::PopupMenu menu;
            menu.setLookAndFeel(&knob_label_laf_);
            for (size_t band = 0; band < zlp::kBandNum; ++band) {
                if (getValue(p_ref_.parameters_, zlp::PFilterStatus::kID + std::to_string(band)) >= .5f) {
                    const auto is_selected = (base_.getSelectedBand() == band);
                    menu.addItem(static_cast<int>(band + 1),
                                 juce::String::fromUTF8("\xe9\xa2\x91\xe6\xae\xb5 ") + std::to_string(band + 1),
                                 true,
                                 is_selected);
                }
            }
            
            auto options = juce::PopupMenu::Options()
                .withTargetComponent(&active_band_label_)
                .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::upwards);

            menu.showMenuAsync(options, [this](int result) {
                if (result > 0) {
                    base_.setSelectedBand(static_cast<size_t>(result - 1));
                }
            });
        };

        const auto popup_option = juce::PopupMenu::Options().withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::upwards).withMinimumNumColumns(1);

        for (auto* box : {&filter_type_box_, &lr_mode_box_, &order_box_}) {
            box->getLAF().setFontScale(0.85f);
            box->getLAF().setOption(popup_option);
            box->setBufferedToImage(true);
            addAndMakeVisible(box);
        }

        dynamic_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                band_helper::turnOnOffDynamic(p_ref_, c_band, dynamic_button_.getButton().getToggleState());
            }
            if (!dynamic_button_.getButton().getToggleState() && base_.getSoloWholeIdx() < 2 * zlp::kBandNum) {
                base_.setSoloWholeIdx(2 * zlp::kBandNum);
            }
        };

        bypass_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        bypass_button_.setBufferedToImage(true);
        addChildComponent(bypass_button_);
        bypass_button_.setVisible(false);

        auto_button_.getButton().onClick = [this]() {
            if (base_.getSelectedBand() < zlp::kBandNum) {
                turnOnOffAuto();
            }
        };



        harmonic_box_.getLAF().setFontScale(0.9f);
        harmonic_box_.getLAF().setOption(popup_option);
        harmonic_box_.setBufferedToImage(true);
        addAndMakeVisible(harmonic_box_);

        th_slider_.setBufferedToImage(true);
        th_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f", std::round(v));
            return std::string(buf);
        };
        addAndMakeVisible(th_slider_);

        attack_slider_.setBufferedToImage(true);
        attack_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f", std::round(v));
            return std::string(buf);
        };
        addAndMakeVisible(attack_slider_);

        release_slider_.setBufferedToImage(true);
        release_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f", std::round(v));
            return std::string(buf);
        };
        addAndMakeVisible(release_slider_);

        range_slider_.getSlider1().setRange(-12.0, 12.0, 0.01);
        range_slider_.setBufferedToImage(true);
        range_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f", v);
            return std::string(buf);
        };
        addAndMakeVisible(range_slider_);

        range_slider_.getSlider1().onValueChange = [this]() {
            if (is_syncing_range_) {
                return;
            }
            if (const auto band = base_.getSelectedBand(); band < zlp::kBandNum) {
                const auto band_s = std::to_string(band);
                auto* target_gain_param = p_ref_.parameters_.getParameter(zlp::PTargetGain::kID + band_s);
                if (target_gain_param != nullptr && gain_param_ptr_ != nullptr) {
                    const float current_gain = gain_param_ptr_->load(std::memory_order::relaxed);
                    const float new_target_gain = std::clamp(current_gain + static_cast<float>(range_slider_.getSlider1().getValue()), -30.0f, 30.0f);
                    updateValue(target_gain_param, target_gain_param->convertTo0to1(new_target_gain));
                }
            }
        };

        label_laf_.setFontScale(1.3f);
        dynamic_label_.setLookAndFeel(&label_laf_);
        dynamic_label_.setJustificationType(juce::Justification::centred);
        dynamic_label_.setBufferedToImage(true);
        addChildComponent(dynamic_label_);

        static_freq_slider_.setBufferedToImage(true);
        static_freq_slider_.value_formatter_ = [](double v) {
            char buf[32];
            if (v < 1000.0) {
                snprintf(buf, sizeof(buf), "%.0f", v);
            } else {
                double kv = v / 1000.0;
                if (std::abs(kv - std::round(kv)) < 0.001) {
                    snprintf(buf, sizeof(buf), "%.0fk", kv);
                } else if (std::abs(kv * 10.0 - std::round(kv * 10.0)) < 0.01) {
                    snprintf(buf, sizeof(buf), "%.1fk", kv);
                } else {
                    snprintf(buf, sizeof(buf), "%.2fk", kv);
                }
            }
            return std::string(buf);
        };
        addAndMakeVisible(static_freq_slider_);

        static_gain_slider_.setBufferedToImage(true);
        static_gain_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", v);
            return std::string(buf);
        };
        addAndMakeVisible(static_gain_slider_);

        static_q_slider_.setBufferedToImage(true);
        static_q_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", v);
            return std::string(buf);
        };
        addAndMakeVisible(static_q_slider_);

        knob_label_laf_.setFontScale(1.05f);
        for (auto* l : {&static_freq_label_, &static_gain_label_, &static_q_label_, &th_label_, &attack_label_, &release_label_, &range_label_}) {
            l->setLookAndFeel(&knob_label_laf_);
            l->setJustificationType(juce::Justification::centred);
            l->setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.65f));
            l->setBufferedToImage(true);
            addAndMakeVisible(l);
        }



        setInterceptsMouseClicks(false, true);
    }

    RightControlPanel::~RightControlPanel() {
        range_slider_.setLookAndFeel(nullptr);
    }

    int RightControlPanel::getIdealWidth() const {
        return 800;
    }

    void RightControlPanel::paint(juce::Graphics& g) {
        g.setColour(base_.getTextColour().withAlpha(0.12f));
        g.drawVerticalLine(static_cast<int>(std::round(left_divider_x_)), 6.0f, static_cast<float>(getHeight()) - 6.0f);
        g.drawVerticalLine(static_cast<int>(std::round(divider_x_)), 6.0f, static_cast<float>(getHeight()) - 6.0f);

        if (base_.getSelectedBand() < zlp::kBandNum) {
            const auto band_color = base_.getColourMap1(base_.getSelectedBand());
            auto label_bounds = active_band_label_.getBounds().toFloat();
            
            const float bg_alpha = active_band_label_.isHovered() ? 0.16f : 0.08f;
            const float border_alpha = active_band_label_.isHovered() ? 0.65f : 0.4f;

            g.setColour(band_color.withAlpha(bg_alpha));
            g.fillRoundedRectangle(label_bounds, 4.0f);
            
            g.setColour(band_color.withAlpha(border_alpha));
            g.drawRoundedRectangle(label_bounds, 4.0f, 1.0f);
        }
    }

    void RightControlPanel::resized() {
        const auto font_size = base_.getFontSize();
        auto bounds = getLocalBounds();

        // Reduce vertical padding to maximize vertical space in 120px height
        bounds.reduce(12, 6);

        // New Section on the very left (width 126)
        auto new_section = bounds.removeFromLeft(126);
        auto new_section_area = new_section.withSizeKeepingCentre(126, 82);
        
        auto left_col = new_section_area.removeFromLeft(52);
        new_section_area.removeFromLeft(8);
        auto right_col = new_section_area; // width 66

        // Layout Left Column: Switch, Solo, active band label
        band_bypass_button_.setBounds(left_col.removeFromTop(22));
        left_col.removeFromTop(8);
        band_solo_button_.setBounds(left_col.removeFromTop(22));
        left_col.removeFromTop(8);
        active_band_label_.setBounds(left_col.removeFromTop(22));

        // Layout Right Column: filter type, lr mode, order combo boxes
        filter_type_box_.setBounds(right_col.removeFromTop(22));
        right_col.removeFromTop(8);
        lr_mode_box_.setBounds(right_col.removeFromTop(22));
        right_col.removeFromTop(8);
        order_box_.setBounds(right_col.removeFromTop(22));

        // Spacing and left divider line
        bounds.removeFromLeft(8);
        left_divider_x_ = static_cast<float>(bounds.getX());
        bounds.removeFromLeft(8);

        // sfreq, sgain, sq
        auto sfreq_col = bounds.removeFromLeft(64);
        bounds.removeFromLeft(8);

        auto sgain_col = bounds.removeFromLeft(64);
        bounds.removeFromLeft(8);

        auto sq_col = bounds.removeFromLeft(64);
        
        // Right divider line
        bounds.removeFromLeft(8);
        divider_x_ = static_cast<float>(bounds.getX());
        bounds.removeFromLeft(8);

        // Column 1: Buttons
        auto btn_col = bounds.removeFromLeft(102);
        auto btn_area = btn_col.withSizeKeepingCentre(102, 82);
        
        // Row 1
        auto row1 = btn_area.removeFromTop(22);
        dynamic_button_.setBounds(row1.removeFromLeft(48));
        row1.removeFromLeft(6); // gap between columns
        track_button_.setBounds(row1.removeFromLeft(48));
        
        btn_area.removeFromTop(8); // row gap
        
        // Row 2
        auto row2 = btn_area.removeFromTop(22);
        auto_button_.setBounds(row2.removeFromLeft(48));
        row2.removeFromLeft(6); // gap between columns
        harmonic_box_.setBounds(row2.removeFromLeft(48));
        
        btn_area.removeFromTop(8); // row gap
        
        // Row 3
        auto row3 = btn_area.removeFromTop(22);
        link_button_.setBounds(row3.removeFromLeft(48));
        row3.removeFromLeft(6); // gap
        precise_button_.setBounds(row3.removeFromLeft(48));

        dynamic_label_.setBounds({});
        bypass_button_.setBounds({});

        // Spacing before threshold knob
        bounds.removeFromLeft(10);

        // th, attack, release, range
        auto th_col = bounds.removeFromLeft(64);
        bounds.removeFromLeft(8);
        
        auto att_col = bounds.removeFromLeft(64);
        bounds.removeFromLeft(8);
        
        auto rel_col = bounds.removeFromLeft(64);
        bounds.removeFromLeft(8);

        auto range_col = bounds;

        const int knob_size = 64;
        const int label_h = 16;
        const int group_h = knob_size + 2 + label_h;

        auto sfreq_group = sfreq_col.withSizeKeepingCentre(sfreq_col.getWidth(), group_h);
        static_freq_slider_.setBounds(sfreq_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        sfreq_group.removeFromTop(2);
        static_freq_label_.setBounds(sfreq_group.removeFromTop(label_h).translated(0, 7));

        auto sgain_group = sgain_col.withSizeKeepingCentre(sgain_col.getWidth(), group_h);
        static_gain_slider_.setBounds(sgain_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        sgain_group.removeFromTop(2);
        static_gain_label_.setBounds(sgain_group.removeFromTop(label_h).translated(0, 7));

        auto sq_group = sq_col.withSizeKeepingCentre(sq_col.getWidth(), group_h);
        static_q_slider_.setBounds(sq_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        sq_group.removeFromTop(2);
        static_q_label_.setBounds(sq_group.removeFromTop(label_h).translated(0, 7));

        auto th_group = th_col.withSizeKeepingCentre(th_col.getWidth(), group_h);
        th_slider_.setBounds(th_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        th_group.removeFromTop(2); // spacing
        th_label_.setBounds(th_group.removeFromTop(label_h).translated(0, 7));

        auto att_group = att_col.withSizeKeepingCentre(att_col.getWidth(), group_h);
        attack_slider_.setBounds(att_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        att_group.removeFromTop(2); // spacing
        attack_label_.setBounds(att_group.removeFromTop(label_h).translated(0, 7));

        auto rel_group = rel_col.withSizeKeepingCentre(rel_col.getWidth(), group_h);
        release_slider_.setBounds(rel_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        rel_group.removeFromTop(2); // spacing
        release_label_.setBounds(rel_group.removeFromTop(label_h).translated(0, 7));

        auto range_group = range_col.withSizeKeepingCentre(range_col.getWidth(), group_h);
        range_slider_.setBounds(range_group.removeFromTop(knob_size).withSizeKeepingCentre(knob_size, knob_size));
        range_group.removeFromTop(2); // spacing
        range_label_.setBounds(range_group.removeFromTop(label_h).translated(0, 7));

        delete_button_.setBounds(getWidth() - 28, 6, 20, 18);

        const auto dragging_distance = getSliderDraggingDistance(font_size);
        static_freq_slider_.setMouseDragSensitivity(dragging_distance);
        static_gain_slider_.setMouseDragSensitivity(dragging_distance);
        static_q_slider_.setMouseDragSensitivity(dragging_distance);
        th_slider_.setMouseDragSensitivity(dragging_distance);
        attack_slider_.setMouseDragSensitivity(dragging_distance);
        release_slider_.setMouseDragSensitivity(dragging_distance);
        range_slider_.setMouseDragSensitivity(dragging_distance);
    }

    void RightControlPanel::repaintCallBack() {
        updater_.updateComponents();

        if (gain_param_ptr_ != nullptr && target_gain_param_ptr_ != nullptr) {
            const float gain_val = gain_param_ptr_->load(std::memory_order::relaxed);
            const float target_gain_val = target_gain_param_ptr_->load(std::memory_order::relaxed);
            const float relative_range = target_gain_val - gain_val;
            if (std::abs(range_slider_.getSlider1().getValue() - relative_range) > 1e-4) {
                juce::ScopedValueSetter<bool> svs(is_syncing_range_, true);
                range_slider_.getSlider1().setValue(relative_range, juce::sendNotification);
            }
        }
    }

    void RightControlPanel::repaintCallBackSlow() {
        bool filter_on = true;
        if (filter_status_ptr_ != nullptr) {
            filter_on = filter_status_ptr_->load(std::memory_order::relaxed) > 1.5f;
            if (filter_on != band_bypass_button_.getButton().getToggleState()) {
                band_bypass_button_.getButton().setToggleState(filter_on, juce::dontSendNotification);
            }
        }

        if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
            const auto solo_active = (base_.getSoloWholeIdx() == c_band);
            if (solo_active != band_solo_button_.getButton().getToggleState()) {
                band_solo_button_.getButton().setToggleState(solo_active, juce::dontSendNotification);
            }
        }

        if (ftype_param_ptr_ != nullptr) {
            const auto ftype_idx = static_cast<int>(std::round(ftype_param_ptr_->load(std::memory_order::relaxed)));
            const auto ftype = zlp::choiceIndexToFilterType(ftype_idx);
            if (static_cast<int>(ftype) != c_ftype_) {
                c_ftype_ = static_cast<int>(ftype);
                const auto slope_6_disabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kBandPass)
                    || (ftype == zldsp::filter::kNotch);
                enableSlope6(!slope_6_disabled);
                enableSlope(c_ftype_ != zldsp::filter::kFlatTilt);

                const auto q_enabled = (c_ftype_ != zldsp::filter::kFlatTilt);
                static_q_slider_.setEnabled(q_enabled);
                static_q_label_.setEnabled(q_enabled);

                const auto gain_enabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kLowShelf)
                    || (ftype == zldsp::filter::kHighShelf)
                    || (ftype == zldsp::filter::kTiltShelf)
                    || (ftype == zldsp::filter::kFlatTilt);
                static_gain_slider_.setEnabled(gain_enabled);
                static_gain_label_.setEnabled(gain_enabled);
            }
        }

        if (dynamic_on_ptr_ != nullptr) {
            const auto dynamic_on = dynamic_on_ptr_->load(std::memory_order::relaxed) > .5f;

            const auto precise_mode = precise_on_ptr_ ? static_cast<int>(std::round(precise_on_ptr_->load(std::memory_order::relaxed))) : 0;

            // 高低切（HighPass/LowPass）不允许开启动态EQ
            const bool is_cut_filter = (c_ftype_ == zldsp::filter::kHighPass || c_ftype_ == zldsp::filter::kLowPass);
            const bool dynamic_should_enable = filter_on && dynamic_on && !is_cut_filter;
            const bool dynamic_button_should_enable = filter_on && !is_cut_filter;
            
            if (dynamic_button_should_enable != c_dynamic_btn_enabled_ || 
                dynamic_should_enable != c_dynamic_enabled_ || 
                precise_mode != c_precise_mode_) {
                
                c_dynamic_btn_enabled_ = dynamic_button_should_enable;
                c_dynamic_enabled_ = dynamic_should_enable;
                c_precise_mode_ = precise_mode;
                
                dynamic_button_.setEnabled(c_dynamic_btn_enabled_);
                
                th_slider_.setEnabled(c_dynamic_enabled_);
                attack_slider_.setEnabled(c_dynamic_enabled_);
                release_slider_.setEnabled(c_dynamic_enabled_);
                th_label_.setEnabled(c_dynamic_enabled_);
                attack_label_.setEnabled(c_dynamic_enabled_);
                release_label_.setEnabled(c_dynamic_enabled_);
                range_slider_.setEnabled(c_dynamic_enabled_);
                range_label_.setEnabled(c_dynamic_enabled_);
                auto_button_.setEnabled(c_dynamic_enabled_);
                link_button_.setEnabled(c_dynamic_enabled_);
                bypass_button_.setEnabled(c_dynamic_enabled_);
                track_button_.setEnabled(c_dynamic_enabled_);
                precise_button_.setEnabled(c_dynamic_enabled_);
                harmonic_box_.setEnabled(c_dynamic_enabled_);
            }
        }
    }

    void RightControlPanel::updateBand() {
        if (base_.getSelectedBand() < zlp::kBandNum) {
            const auto band_s = std::to_string(base_.getSelectedBand());
            dynamic_on_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PDynamicON::kID + band_s);
            
            dynamic_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                dynamic_button_.getButton(), p_ref_.parameters_, zlp::PDynamicON::kID + band_s, updater_,
                juce::dontSendNotification);
            
            bypass_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                bypass_button_.getButton(), p_ref_.parameters_, zlp::PDynamicBypass::kID + band_s, updater_);
            auto_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                auto_button_.getButton(), p_ref_.parameters_, zlp::PDynamicLearn::kID + band_s, updater_,
                juce::dontSendNotification);
            link_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                link_button_.getButton(), p_ref_.parameters_, zlp::PSideLink::kID + band_s, updater_);
            track_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                track_button_.getButton(), p_ref_.parameters_, zlp::PPitchTrack::kID + band_s, updater_,
                juce::dontSendNotification);
            precise_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                precise_button_.getButton(), p_ref_.parameters_, zlp::PPrecise::kID + band_s, updater_,
                juce::dontSendNotification);
            harmonic_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                harmonic_box_.getBox(), p_ref_.parameters_, zlp::PPitchTrackHarmonic::kID + band_s, updater_);
            precise_on_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PPrecise::kID + band_s);
            c_precise_mode_ = -1;
            c_dynamic_btn_enabled_ = false;
            c_dynamic_enabled_ = false;

            filter_type_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                filter_type_box_.getBox(), p_ref_.parameters_, zlp::PFilterType::kID + band_s, updater_);
            lr_mode_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                lr_mode_box_.getBox(), p_ref_.parameters_, zlp::PLRMode::kID + band_s, updater_);
            order_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                order_box_.getBox(), p_ref_.parameters_, zlp::POrder::kID + band_s, updater_);

            filter_status_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PFilterStatus::kID + band_s);
            ftype_param_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PFilterType::kID + band_s);
            c_ftype_ = -1;
            const auto band_color = base_.getColourMap1(base_.getSelectedBand());
            active_band_label_.setColour(juce::Label::textColourId, band_color);
            active_band_label_.setText(juce::String::fromUTF8("\xe9\xa2\x91\xe6\xae\xb5 ") + std::to_string(base_.getSelectedBand() + 1), juce::dontSendNotification);
            band_bypass_button_.repaint();
            band_solo_button_.repaint();
            active_band_label_.repaint();
            repaint();

            static_freq_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                static_freq_slider_.getSlider1(), p_ref_.parameters_, zlp::PFreq::kID + band_s, updater_);
            static_freq_slider_.setComponentID(zlp::PFreq::kID + band_s);
            
            static_gain_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                static_gain_slider_.getSlider1(), p_ref_.parameters_, zlp::PGain::kID + band_s, updater_);
            static_gain_slider_.setComponentID(zlp::PGain::kID + band_s);
            
            static_q_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                static_q_slider_.getSlider1(), p_ref_.parameters_, zlp::PQ::kID + band_s, updater_);
            static_q_slider_.setComponentID(zlp::PQ::kID + band_s);

            th_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                th_slider_.getSlider1(), p_ref_.parameters_, zlp::PThreshold::kID + band_s, updater_);
            th_slider_.setComponentID(zlp::PThreshold::kID + band_s);
            attack_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                attack_slider_.getSlider1(), p_ref_.parameters_, zlp::PAttack::kID + band_s, updater_);
            attack_slider_.setComponentID(zlp::PAttack::kID + band_s);
            release_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                release_slider_.getSlider1(), p_ref_.parameters_, zlp::PRelease::kID + band_s, updater_);
            release_slider_.setComponentID(zlp::PRelease::kID + band_s);

            filter_type_box_.visibilityChanged();
            lr_mode_box_.visibilityChanged();
            order_box_.visibilityChanged();
            static_freq_slider_.visibilityChanged();
            static_gain_slider_.visibilityChanged();
            static_q_slider_.visibilityChanged();
            th_slider_.visibilityChanged();
            attack_slider_.visibilityChanged();
            release_slider_.visibilityChanged();
            range_slider_.visibilityChanged();

            filter_type_box_.repaint();
            lr_mode_box_.repaint();
            order_box_.repaint();
            static_freq_slider_.repaint();
            static_gain_slider_.repaint();
            static_q_slider_.repaint();
            th_slider_.repaint();
            attack_slider_.repaint();
            release_slider_.repaint();
            range_slider_.repaint();

            auto* ftype_param = p_ref_.parameters_.getParameter(zlp::PSideFilterType::kID + band_s);
            if (ftype_param != nullptr) {
                updateValue(ftype_param, ftype_param->convertTo0to1(0.f));
            }
            auto* order_param = p_ref_.parameters_.getParameter(zlp::PSideOrder::kID + band_s);
            if (order_param != nullptr) {
                updateValue(order_param, order_param->convertTo0to1(1.f));
            }
            auto* knee_param = p_ref_.parameters_.getParameter(zlp::PKneeW::kID + band_s);
            if (knee_param != nullptr) {
                updateValue(knee_param, knee_param->getDefaultValue());
            }

            gain_param_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PGain::kID + band_s);
            target_gain_param_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PTargetGain::kID + band_s);
            if (gain_param_ptr_ != nullptr && target_gain_param_ptr_ != nullptr) {
                const float gain_val = gain_param_ptr_->load(std::memory_order::relaxed);
                const float target_gain_val = target_gain_param_ptr_->load(std::memory_order::relaxed);
                const float diff = target_gain_val - gain_val;
                juce::ScopedValueSetter<bool> svs(is_syncing_range_, true);
                range_slider_.getSlider1().setValue(diff, juce::sendNotification);

            }
        } else {
            dynamic_on_ptr_ = nullptr;
            precise_on_ptr_ = nullptr;
            gain_param_ptr_ = nullptr;
            target_gain_param_ptr_ = nullptr;
            filter_status_ptr_ = nullptr;
            ftype_param_ptr_ = nullptr;
            c_precise_mode_ = -1;
            c_ftype_ = -1;
            c_dynamic_btn_enabled_ = false;
            c_dynamic_enabled_ = false;
            active_band_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
            active_band_label_.setText("", juce::dontSendNotification);
            band_bypass_button_.repaint();
            band_solo_button_.repaint();
            active_band_label_.repaint();
            repaint();
            dynamic_attachment_.reset();
            bypass_attachment_.reset();
            auto_attachment_.reset();
            link_attachment_.reset();
            track_attachment_.reset();
            precise_attachment_.reset();
            harmonic_attachment_.reset();
            filter_type_attachment_.reset();
            lr_mode_attachment_.reset();
            order_attachment_.reset();
            static_freq_attachment_.reset();
            static_gain_attachment_.reset();
            static_q_attachment_.reset();
            th_attachment_.reset();
            attack_attachment_.reset();
            release_attachment_.reset();
        }
    }

    void RightControlPanel::updateFreqMax(double freq_max) {
        freq_max_ = freq_max;
    }

    void RightControlPanel::turnOnOffAuto() {
        const auto band = base_.getSelectedBand();
        if (band < zlp::kBandNum) {
            if (!auto_button_.getButton().getToggleState()) {
                auto* th_para = p_ref_.parameters_.getParameter(zlp::PThreshold::kID + std::to_string(band));
                updateValue(th_para,
                            th_para->convertTo0to1(static_cast<float>(p_ref_.getController().getLearnedThreshold(band))));
            } else {
                auto* th_para = p_ref_.parameters_.getParameter(zlp::PThreshold::kID + std::to_string(band));
                updateValue(th_para, th_para->getDefaultValue());
            }
        }
    }

    void RightControlPanel::mouseDown(const juce::MouseEvent&) {
        if (const auto band = base_.getSelectedBand(); band < zlp::kBandNum) {
            updateValue(p_ref_.parameters_.getParameter(zlp::PSideLink::kID + std::to_string(band)), 0.f);
        }
    }

    void RightControlPanel::enableSlope6(const bool f) {
        if (!f && order_box_.getBox().getSelectedId() == 1) {
            order_box_.getBox().setSelectedId(2, juce::sendNotificationSync);
        }
        order_box_.getBox().setItemEnabled(1, f);
    }

    void RightControlPanel::enableSlope(const bool f) {
        order_box_.setEditable(f);
    }
}
