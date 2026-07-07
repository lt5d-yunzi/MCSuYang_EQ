// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "control_panel.hpp"
#include "BinaryData.h"

namespace zlpanel {
    ControlPanel::ControlPanel(PluginProcessor& p, zlgui::UIBase& base,
                               const multilingual::TooltipHelper& tooltip_helper) :
        p_ref_(p), base_(base), updater_(),
        right_control_panel_(p, base, tooltip_helper),
        soothe_slider_("Smooth", base, tooltip_helper.getToolTipText(multilingual::kBandDynamic)),
        soothe_label_("", "SOOTHE"),
        soothe_laf_(base) {
        addAndMakeVisible(mouse_event_eater_);


        right_control_panel_.setBufferedToImage(true);
        addAndMakeVisible(right_control_panel_);

        soothe_laf_.setFontScale(1.4f);
        soothe_label_.setLookAndFeel(&soothe_laf_);
        soothe_label_.setJustificationType(juce::Justification::centred);
        soothe_label_.setBufferedToImage(true);

        soothe_slider_.setBufferedToImage(true);


        setInterceptsMouseClicks(false, true);
        setVisible(true);
    }

    ControlPanel::~ControlPanel() = default;

    int ControlPanel::getIdealWidth() const {
        return 800;
    }

    int ControlPanel::getIdealHeight() const {
        return 120;
    }

    void ControlPanel::paint(juce::Graphics& g) {
        auto bounds = getLocalBounds().toFloat();
        
        juce::ColourGradient grad(juce::Colour(12, 22, 28).withAlpha(0.92f), bounds.getX(), bounds.getY(),
                                  juce::Colour(6, 10, 14).withAlpha(0.96f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 8.0f);
        
        const auto accent_color = base_.getSelectedBand() < zlp::kBandNum ? base_.getColourMap1(base_.getSelectedBand()) : juce::Colour(140, 175, 210);
        g.setColour(accent_color.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.2f);
        
        g.setColour(juce::Colour(255, 255, 255).withAlpha(0.04f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.0f);
    }

    void ControlPanel::resized() {
        auto bounds = getLocalBounds();
        right_control_panel_.setBounds(bounds);
    }

    void ControlPanel::repaintCallBack() {
        right_control_panel_.repaintCallBack();
    }

    void ControlPanel::repaintCallBackSlow() {
        right_control_panel_.repaintCallBackSlow();
        updater_.updateComponents();

        const auto has_selection = base_.getSelectedBand() < zlp::kBandNum;
        if (isVisible() != has_selection) {
            setVisible(has_selection);
        }
        if (isEnabled() != has_selection) {
            setEnabled(has_selection);
        }
    }

    void ControlPanel::updateBand() {
        const auto has_selection = base_.getSelectedBand() < zlp::kBandNum;
        if (has_selection) {
            const auto band_s = std::to_string(base_.getSelectedBand());
            dynamic_on_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PDynamicON::kID + band_s);
            
            right_control_panel_.updateBand();
            
            soothe_attach_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                soothe_slider_.getSlider(), p_ref_.parameters_, zlp::PDynamicSmooth::kID + band_s, updater_);
            soothe_slider_.repaint();
            right_control_panel_.setVisible(true);
            soothe_slider_.setVisible(false);
            soothe_label_.setVisible(false);
        } else {
            dynamic_on_ptr_ = nullptr;
            soothe_attach_.reset();
            
            right_control_panel_.updateBand();
            right_control_panel_.setVisible(true);
            soothe_slider_.setVisible(false);
            soothe_label_.setVisible(false);
        }
        if (isVisible() != has_selection) {
            setVisible(has_selection);
        }
        if (isEnabled() != has_selection) {
            setEnabled(has_selection);
        }
        repaintCallBackSlow();
    }

    void ControlPanel::updateSampleRate(const double sample_rate) {
        const auto freq_max = freq_helper::getSliderMax(sample_rate);
        right_control_panel_.updateFreqMax(freq_max);
    }


}