// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "left_control_panel.hpp"
#include "BinaryData.h"

namespace zlpanel {
    LeftControlPanel::LeftControlPanel(PluginProcessor& p,
                                       zlgui::UIBase& base,
                                       const multilingual::TooltipHelper& tooltip_helper) :
        p_ref_(p), base_(base), updater_(),
        left_drawable_(juce::Drawable::createFromImageData(BinaryData::left_arrow_svg, BinaryData::left_arrow_svgSize)),
        left_button_(base, left_drawable_.get(), nullptr),
        band_label_("", ""),
        right_drawable_(juce::Drawable::createFromImageData(BinaryData::right_arrow_svg, BinaryData::right_arrow_svgSize)),
        right_button_(base, right_drawable_.get(), nullptr),
        close_drawable_(juce::Drawable::createFromImageData(BinaryData::close_svg, BinaryData::close_svgSize)),
        close_button_(base, close_drawable_.get(), nullptr, tooltip_helper.getToolTipText(multilingual::kBandOff)),
        bypass_drawable_(juce::Drawable::createFromImageData(BinaryData::bypass_svg, BinaryData::bypass_svgSize)),
        bypass_button_(base, bypass_drawable_.get(), bypass_drawable_.get(), tooltip_helper.getToolTipText(multilingual::kBandBypass)),
        ftype_box_(zlp::PFilterType::kChoices, base, tooltip_helper.getToolTipText(multilingual::kBandType)),
        slope_box_(juce::StringArray{
                       "6 dB/oct", "12 dB/oct", "24 dB/oct", "36 dB/oct",
                       "48 dB/oct", "72 dB/oct", "96 dB/oct"
                   }, base, tooltip_helper.getToolTipText(multilingual::kBandSlope)),
        stereo_box_(zlp::PLRMode::kChoices, base, tooltip_helper.getToolTipText(multilingual::kBandStereoMode)),
        gain_slider_("", base, tooltip_helper.getToolTipText(multilingual::kBandGain), 1.25f),
        freq_slider_("", base, tooltip_helper.getToolTipText(multilingual::kBandFreq), 1.25f),
        q_slider_("", base, tooltip_helper.getToolTipText(multilingual::kBandQ), 1.25f),
        label_laf_(base),
        freq_label_("", "FREQ"),
        gain_label_("", "GAIN"),
        q_label_("", "Q"),
        max_db_idx_ref_(*p.parameters_NA_.getRawParameterValue(zlstate::PEQMaxDB::kID)) {
        close_button_.setBufferedToImage(true);
        addAndMakeVisible(close_button_);
        close_button_.getButton().onClick = [this]() {
            juce::Component::SafePointer<LeftControlPanel> safeThis(this);
            juce::MessageManager::callAsync([safeThis]() {
                if (safeThis == nullptr) return;
                auto* self = safeThis.getComponent();
                if (const auto c_band = self->base_.getSelectedBand(); c_band < zlp::kBandNum) {
                    band_helper::turnOffBand(self->p_ref_, c_band, self->base_.getSelectedBandSet());
                    const auto band1 = band_helper::findClosestBand<true>(self->p_ref_, c_band);
                    const auto band2 = band_helper::findClosestBand<false>(self->p_ref_, c_band);
                    if (band1 < zlp::kBandNum) {
                        self->base_.setSelectedBand(band1);
                    } else if (band2 < zlp::kBandNum) {
                        self->base_.setSelectedBand(band2);
                    } else {
                        self->base_.setSelectedBand(zlp::kBandNum);
                    }
                }
            });
        };

        bypass_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        bypass_button_.setBufferedToImage(true);
        addAndMakeVisible(bypass_button_);
        bypass_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                updateValue(p_ref_.parameters_.getParameter(zlp::PFilterStatus::kID + std::to_string(c_band)),
                            bypass_button_.getToggleState() ? 1.f : .5f);
            }
        };

        label_laf_.setFontScale(1.3f);
        for (auto& l : {&freq_label_, &gain_label_, &q_label_}) {
            l->setLookAndFeel(&label_laf_);
            l->setJustificationType(juce::Justification::centred);
            l->setInterceptsMouseClicks(false, false);
            l->setBufferedToImage(true);
        }

        left_button_.setBufferedToImage(true);
        addAndMakeVisible(left_button_);
        left_button_.getButton().onClick = [this]() {
            const auto c_band = base_.getSelectedBand();
            const auto next_band = band_helper::findClosestBand<false>(p_ref_, c_band);
            if (next_band < zlp::kBandNum) {
                base_.setSelectedBand(next_band);
            }
        };

        band_label_.setLookAndFeel(&label_laf_);
        band_label_.setJustificationType(juce::Justification::centred);
        band_label_.setInterceptsMouseClicks(false, false);
        band_label_.setBufferedToImage(true);
        addAndMakeVisible(band_label_);

        right_button_.setBufferedToImage(true);
        addAndMakeVisible(right_button_);
        right_button_.getButton().onClick = [this]() {
            const auto c_band = base_.getSelectedBand();
            const auto next_band = band_helper::findClosestBand<true>(p_ref_, c_band);
            if (next_band < zlp::kBandNum) {
                base_.setSelectedBand(next_band);
            }
        };

        const auto popup_option = juce::PopupMenu::Options().withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::upwards);
        slope_box_.getLAF().setOption(popup_option);
        ftype_box_.getLAF().setOption(popup_option);
        stereo_box_.getLAF().setOption(popup_option);
        
        slope_box_.getLAF().setItemJustification(juce::Justification::centredRight);
        for (auto& box : {&ftype_box_, &slope_box_, &stereo_box_}) {
            box->setBufferedToImage(true);
            addChildComponent(box);
            box->setVisible(false);
        }

        freq_slider_.permitted_characters_ = "-0123456789.kK#ABCDEFG";
        freq_slider_.string_formatter_ = freq_note::getFrequencyFromNote;
        
        gain_slider_.setShowSlider2(false);

        q_slider_.setBufferedToImage(true);

        setInterceptsMouseClicks(false, true);
    }

    LeftControlPanel::~LeftControlPanel() = default;

    int LeftControlPanel::getIdealWidth() const {
        return 240;
    }

    void LeftControlPanel::paint(juce::Graphics& g) {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        
        // Semi-transparent rounded card background
        g.setColour(juce::Colour(255, 255, 255).withAlpha(0.02f));
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Subtle outline highlight
        g.setColour(juce::Colour(255, 255, 255).withAlpha(0.05f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void LeftControlPanel::resized() {
        const auto font_size = base_.getFontSize();
        const auto padding = getPaddingSize(font_size);
        auto bounds = getLocalBounds();

        // 1. Top row (Header / Navigation)
        auto top_row = bounds.removeFromTop(30);
        close_button_.setBounds(top_row.removeFromRight(30));
        top_row.removeFromRight(padding);
        
        bypass_button_.setBounds(top_row.removeFromLeft(30));
        top_row.removeFromLeft(padding);
        
        auto nav_w = 20;
        left_button_.setBounds(top_row.removeFromLeft(nav_w));
        right_button_.setBounds(top_row.removeFromRight(nav_w));
        band_label_.setBounds(top_row);
        
        bounds.removeFromTop(padding / 2);
        
        ftype_box_.setBounds({});
        slope_box_.setBounds({});
        stereo_box_.setBounds({});
    }

    void LeftControlPanel::repaintCallBack() {
        updater_.updateComponents();
    }

    void LeftControlPanel::repaintCallBackSlow() {
        if (ftype_ptr_ != nullptr) {
            const auto ftype_idx = static_cast<int>(std::round(ftype_ptr_->load(std::memory_order::relaxed)));
            const auto ftype = zlp::choiceIndexToFilterType(ftype_idx);
            if (static_cast<int>(ftype) != c_ftype_) {
                c_ftype_ = static_cast<int>(ftype);
                const auto slope_6_disabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kBandPass)
                    || (ftype == zldsp::filter::kNotch);
                enableSlope6(!slope_6_disabled);

                const auto gain_enabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kLowShelf)
                    || (ftype == zldsp::filter::kHighShelf)
                    || (ftype == zldsp::filter::kTiltShelf)
                    || (ftype == zldsp::filter::kFlatTilt);
                gain_slider_.setEditable(gain_enabled);
                enableGain(gain_enabled);

                enableSlope(c_ftype_ != zldsp::filter::kFlatTilt);
                enableQ(c_ftype_ != zldsp::filter::kFlatTilt);
            }
        }
        const auto max_db_idx = max_db_idx_ref_.load(std::memory_order::relaxed);
        if (std::abs(max_db_idx - c_max_db_idx_) > .1f) {
            c_max_db_idx_ = max_db_idx;
            updateGainSliderDraggingDistance();
        }
        
        if (filter_status_ptr_ != nullptr) {
            const auto filter_on = filter_status_ptr_->load(std::memory_order::relaxed) > 1.5f;
            if (filter_on != bypass_button_.getToggleState()) {
                bypass_button_.getButton().setToggleState(filter_on, juce::dontSendNotification);
            }
        }
    }

    void LeftControlPanel::updateBand() {
        if (base_.getSelectedBand() < zlp::kBandNum) {
            const auto band_s = std::to_string(base_.getSelectedBand());
            updateFreqMax(freq_max_);
            
            ftype_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                ftype_box_.getBox(), p_ref_.parameters_, zlp::PFilterType::kID + band_s, updater_);
            slope_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                slope_box_.getBox(), p_ref_.parameters_, zlp::POrder::kID + band_s, updater_);
            stereo_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                stereo_box_.getBox(), p_ref_.parameters_, zlp::PLRMode::kID + band_s, updater_);
            q_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                q_slider_.getSlider1(), p_ref_.parameters_, zlp::PQ::kID + band_s, updater_);
            q_slider_.setComponentID(zlp::PQ::kID + band_s);
            
            gain_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                gain_slider_.getSlider1(), p_ref_.parameters_, zlp::PGain::kID + band_s, updater_);
            target_gain_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                gain_slider_.getSlider2(), p_ref_.parameters_, zlp::PTargetGain::kID + band_s, updater_);
            
            updateGainSliderComponentID();
            gain_slider_.visibilityChanged();
            q_slider_.visibilityChanged();
            
            band_label_.setText(band_s, juce::dontSendNotification);
            filter_status_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PFilterStatus::kID + band_s);
            ftype_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PFilterType::kID + band_s);
        } else {
            freq_attachment_.reset();
            gain_attachment_.reset();
            target_gain_attachment_.reset();
            ftype_attachment_.reset();
            slope_attachment_.reset();
            stereo_attachment_.reset();
            q_attachment_.reset();
            ftype_ptr_ = nullptr;
            filter_status_ptr_ = nullptr;
            band_label_.setText("", juce::dontSendNotification);
        }
    }

    void LeftControlPanel::updateFreqMax(const double freq_max) {
        freq_max_ = freq_max;
        if (base_.getSelectedBand() < zlp::kBandNum) {
            freq_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                freq_slider_.getSlider1(), p_ref_.parameters_,
                zlp::PFreq::kID + std::to_string(base_.getSelectedBand()),
                zlp::getLogMidRange(10.0, freq_max, 1000.0, 0.1),
                updater_);
            freq_slider_.setComponentID(zlp::PFreq::kID + std::to_string(base_.getSelectedBand()));
            freq_slider_.visibilityChanged();
        } else {
            freq_attachment_.reset();
        }
    }

    void LeftControlPanel::turnOnOffDynamic(const bool dynamic_on) {
        dynamic_on_ = dynamic_on;
        gain_slider_.setShowSlider2(dynamic_on);
        updateGainSliderComponentID();
    }

    void LeftControlPanel::updateGainSliderDraggingDistance() {
        const auto font_size = base_.getFontSize();
        if (c_max_db_idx_ < .5f) {
            gain_slider_.setMouseDragSensitivity(
                static_cast<int>(std::round(5.f * font_size * kSliderDraggingDistanceScale)));
        } else if (c_max_db_idx_ < 1.5f) {
            gain_slider_.setMouseDragSensitivity(
                static_cast<int>(std::round(2.5f * font_size * kSliderDraggingDistanceScale)));
        } else {
            gain_slider_.setMouseDragSensitivity(
                static_cast<int>(std::round(font_size * kSliderDraggingDistanceScale)));
        }
    }

    void LeftControlPanel::updateGainSliderComponentID() {
        const auto band = base_.getSelectedBand();
        if (band < zlp::kBandNum && !dynamic_on_) {
            gain_slider_.setComponentID(zlp::PGain::kID + std::to_string(band));
        } else {
            gain_slider_.setComponentID("");
        }
    }

    void LeftControlPanel::enableSlope6(const bool f) {
        if (!f && slope_box_.getBox().getSelectedId() == 1) {
            slope_box_.getBox().setSelectedId(2, juce::sendNotificationSync);
        }
        slope_box_.getBox().setItemEnabled(1, f);
    }

    void LeftControlPanel::enableSlope(const bool f) {
        slope_box_.setEditable(f);
    }

    void LeftControlPanel::enableGain(const bool f) {
        gain_label_.setAlpha(f ? 1.f : .5f);
    }

    void LeftControlPanel::enableQ(const bool f) {
        q_slider_.setEditable(f);
        q_label_.setAlpha(f ? 1.f : .5f);
    }
}
