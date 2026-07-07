// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../interface_definitions.hpp"

namespace zlgui {
    class ClickTextButtonLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        explicit ClickTextButtonLookAndFeel(UIBase& base) : base_(base) {
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour&, bool highlight, bool down) override {
            auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
            auto cornerSize = 4.0f;

            if (!button.isEnabled()) {
                g.setColour(juce::Colour(0, 0, 0).withAlpha(0.05f));
                g.fillRoundedRectangle(bounds, cornerSize);
                g.setColour(base_.getTextColour().withAlpha(0.1f));
                g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
                return;
            }

            if (button.getToggleState()) {
                // Toggled / Active state: semi-transparent active color background and border matching current band
                const auto active_color = (use_band_colour_ && base_.getSelectedBand() < zlstate::kBandNum)
                    ? base_.getColourMap1(base_.getSelectedBand()) 
                    : juce::Colour(23, 198, 197);

                g.setColour(active_color.withAlpha(0.2f));
                g.fillRoundedRectangle(bounds, cornerSize);
                
                g.setColour(active_color.withAlpha(0.8f));
                g.drawRoundedRectangle(bounds, cornerSize, 1.2f);
            } else if (down) {
                // Pressed state: darker background, medium grey border
                g.setColour(juce::Colour(255, 255, 255).withAlpha(0.08f));
                g.fillRoundedRectangle(bounds, cornerSize);
                
                g.setColour(base_.getTextColour().withAlpha(0.5f));
                g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            } else if (highlight) {
                // Hover state: subtle white highlight background and soft white border
                g.setColour(juce::Colour(255, 255, 255).withAlpha(0.05f));
                g.fillRoundedRectangle(bounds, cornerSize);
                
                g.setColour(base_.getTextColour().withAlpha(0.35f));
                g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            } else {
                // Normal state: dark glass background and very thin, subtle grey/white border
                g.setColour(juce::Colour(0, 0, 0).withAlpha(0.15f));
                g.fillRoundedRectangle(bounds, cornerSize);

                g.setColour(juce::Colour(255, 255, 255).withAlpha(0.15f));
                g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            }
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                            const bool highlight, const bool down) override {
            if (!button.isEnabled()) {
                g.setColour(base_.getTextColour().withAlpha(0.2f));
            } else if (down || button.getToggleState()) {
                const auto active_color = (button.getToggleState() && use_band_colour_ && base_.getSelectedBand() < zlstate::kBandNum)
                    ? base_.getColourMap1(base_.getSelectedBand())
                    : (button.getToggleState() ? juce::Colour(23, 198, 197) : base_.getTextColour());
                g.setColour(active_color);
            } else if (highlight) {
                g.setColour(base_.getTextColour().withAlpha(.75f));
            } else {
                g.setColour(base_.getTextColour().withAlpha(.5f));
            }
            g.setFont(base_.getFontSize() * font_scale_);
            g.drawText(button.getButtonText(), button.getLocalBounds(), justification_);
        }

        void setJustification(const juce::Justification justification) {
            justification_ = justification;
        }

        void setFontScale(const float font_scale) {
            font_scale_ = font_scale;
        }

        void setUseBandColour(const bool use) {
            use_band_colour_ = use;
        }

    private:
        UIBase& base_;

        float font_scale_{1.f};
        juce::Justification justification_{juce::Justification::centredLeft};
        bool use_band_colour_{false};
    };
}
