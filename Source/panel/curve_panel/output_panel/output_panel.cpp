// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "output_panel.hpp"
#include "BinaryData.h"

namespace zlpanel {
    OutputPanel::OutputPanel(PluginProcessor& p, zlgui::UIBase& base,
                             const multilingual::TooltipHelper& tooltip_helper) :
        p_ref_(p), base_(base), updater_(),
        control_background_(base),
        level_meter_(p, base),
        level_meter_label_("", "电平表"),
        name_laf_(base),
        gain_label_("", "音量"),
        scale_label_("", "SCALE"),
        scale_slider_("", base,
                      tooltip_helper.getToolTipText(multilingual::kGainScale), 1.25f),
        scale_attach_(scale_slider_.getSlider1(), p.parameters_,
                      zlp::PGainScale::kID, updater_),
        sgc_drawable_(juce::Drawable::createFromImageData(BinaryData::dline_s_svg,
                                                          BinaryData::dline_s_svgSize)),
        sgc_button_(base, sgc_drawable_.get(), sgc_drawable_.get(),
                    tooltip_helper.getToolTipText(multilingual::kStaticGC)),
        sgc_attach_(sgc_button_.getButton(), p.parameters_,
                    zlp::PStaticGain::kID, updater_),
        lm_drawable_(juce::Drawable::createFromImageData(BinaryData::dline_l_svg,
                                                         BinaryData::dline_l_svgSize)),
        lm_button_(base, lm_drawable_.get(), lm_drawable_.get(),
                   tooltip_helper.getToolTipText(multilingual::kLoudnessGC)),
        agc_drawable_(juce::Drawable::createFromImageData(BinaryData::dline_a_svg,
                                                          BinaryData::dline_a_svgSize)),
        agc_button_(base, agc_drawable_.get(), agc_drawable_.get(),
                    tooltip_helper.getToolTipText(multilingual::kAutoGC)),
        agc_attach_(agc_button_.getButton(), p.parameters_,
                    zlp::PAutoGain::kID, updater_),
        phase_drawable_(juce::Drawable::createFromImageData(BinaryData::phase_svg,
                                                            BinaryData::phase_svgSize)),
        phase_button_(base, phase_drawable_.get(), phase_drawable_.get(),
                      tooltip_helper.getToolTipText(multilingual::kPhaseFlip)),
        phase_attach_(phase_button_.getButton(), p.parameters_,
                      zlp::PPhaseFlip::kID, updater_),
        lookahead_label_("", "Lookahead"),
        lookahead_slider_("", base,
                          tooltip_helper.getToolTipText(multilingual::kLookahead)),
        lookahead_attach_(lookahead_slider_.getSlider(), p.parameters_,
                          zlp::PLookahead::kID, updater_) {
        juce::ignoreUnused(p_ref_, base_, tooltip_helper);

        control_background_.setVisible(false);
        addAndMakeVisible(control_background_);

        name_laf_.setFontScale(1.4f);

        gain_label_.setLookAndFeel(&name_laf_);
        gain_label_.setJustificationType(juce::Justification::centred);
        gain_label_.setBufferedToImage(true);
        addAndMakeVisible(gain_label_);

        scale_label_.setLookAndFeel(&name_laf_);
        scale_label_.setJustificationType(juce::Justification::centred);
        scale_label_.setBufferedToImage(true);
        addAndMakeVisible(scale_label_);



        scale_slider_.setComponentID(zlp::PGainScale::kID);
        scale_slider_.setBufferedToImage(true);
        addAndMakeVisible(scale_slider_);

        level_meter_label_.setLookAndFeel(&name_laf_);
        level_meter_label_.setJustificationType(juce::Justification::centred);
        level_meter_label_.setBufferedToImage(true);
        addAndMakeVisible(level_meter_label_);

        addAndMakeVisible(level_meter_);

        lm_button_.getButton().onClick = [this]() {
            if (lm_button_.getToggleState()) {
                p_ref_.getController().setLoudnessMatchON(true);
            } else {
                p_ref_.getController().setLoudnessMatchON(false);
                const auto c_diff = static_cast<float>(p_ref_.getController().getLUFSMatcherDiff());
                auto* output_gain_para = p_ref_.parameters_.getParameter(zlp::POutputGain::kID);
                updateValue(output_gain_para, output_gain_para->convertTo0to1(-c_diff));
                auto* agc_para = p_ref_.parameters_.getParameter(zlp::PAutoGain::kID);
                updateValue(agc_para, 0.f);
            }
        };

        for (auto& b : {&sgc_button_, &lm_button_, &agc_button_, &phase_button_}) {
            b->setImageAlpha(.5f, .75f, 1.f, 1.f);
            b->setBufferedToImage(true);
            addAndMakeVisible(b);
        }

        lookahead_label_.setLookAndFeel(&name_laf_);
        lookahead_label_.setJustificationType(juce::Justification::centredRight);
        lookahead_label_.setBufferedToImage(true);
        addAndMakeVisible(lookahead_label_);

        lookahead_slider_.getSlider().setSliderSnapsToMousePosition(false);
        lookahead_slider_.setBufferedToImage(true);
        addAndMakeVisible(lookahead_slider_);

        base_.setPanelProperty(zlgui::PanelSettingIdx::kOutputPanel, 1.);
        base_.getPanelValueTree().addListener(this);
    }

    OutputPanel::~OutputPanel() {
        base_.getPanelValueTree().removeListener(this);
        p_ref_.getController().setLoudnessMatchON(false);
    }

    int OutputPanel::getIdealWidth() const {
        return 45;
    }

    int OutputPanel::getIdealHeight() const {
        return 400;
    }

    void OutputPanel::paint(juce::Graphics& g) {
        auto bounds = getLocalBounds().toFloat();
        
        // Dark glassmorphism gradient
        juce::ColourGradient grad(juce::Colour(12, 22, 28).withAlpha(0.85f), bounds.getX(), bounds.getY(),
                                  juce::Colour(6, 10, 14).withAlpha(0.92f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 0.0f); // sidebar is flush to screen
        
        // Draw left glowing teal border
        g.setColour(juce::Colour(23, 198, 197).withAlpha(0.6f));
        g.drawVerticalLine(0, 0.0f, bounds.getHeight());
        
        // Subtle highlight outline
        g.setColour(juce::Colour(255, 255, 255).withAlpha(0.04f));
        g.drawRect(bounds, 1.0f);
    }

    void OutputPanel::resized() {
        const auto font_size = base_.getFontSize();
        const auto padding = getPaddingSize(font_size);
        auto bounds = getLocalBounds();

        // Hide unwanted components
        scale_label_.setVisible(false);
        scale_slider_.setVisible(false);
        sgc_button_.setVisible(false);
        lm_button_.setVisible(false);
        agc_button_.setVisible(false);
        phase_button_.setVisible(false);
        lookahead_label_.setVisible(false);
        lookahead_slider_.setVisible(false);

        // Hide redundant labels for narrow layout
        level_meter_label_.setVisible(false);
        gain_label_.setVisible(false);

        // Make wanted components visible
        level_meter_.setVisible(true);

        // Middle level meter takes the entire height and full width (minus left border)
        level_meter_.setBounds(2, 0, getWidth() - 2, getHeight());
    }

    void OutputPanel::repaintCallBackSlow() {
        updater_.updateComponents();
    }

    void OutputPanel::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
        juce::ignoreUnused(property);
        setVisible(true);
    }
}
