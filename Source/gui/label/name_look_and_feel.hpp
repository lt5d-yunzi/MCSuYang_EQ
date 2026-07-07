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

#include "../interface_definitions.hpp"

namespace zlgui::label {
    class NameLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        explicit NameLookAndFeel(UIBase& base) : base_(base) {
            setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black);
            setColour(juce::PopupMenu::textColourId, base_.getTextColour());
            setColour(juce::PopupMenu::highlightedTextColourId, base_.getTextColour());
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(255, 255, 255).withAlpha(0.08f));
        }

        void drawLabel(juce::Graphics& g, juce::Label& label) override {
            if (label.isBeingEdited()) {
                return;
            }
            g.setColour(label.findColour(juce::Label::textColourId));
            g.setFont(base_.getFontSize() * font_scale_);
            g.drawText(label.getText(), label.getLocalBounds().toFloat(), label.getJustificationType());
        }

        void drawPopupMenuBackground(juce::Graphics& g, const int width, const int height) override {
            const auto box_bound = juce::Rectangle<float>(0, 0, static_cast<float>(width),
                                                           static_cast<float>(height));
            g.setColour(juce::Colours::black);
            g.fillRect(box_bound);
            
            // Subtle premium border
            g.setColour(base_.getTextColour().withAlpha(0.12f));
            g.drawRect(box_bound, 1.0f);
        }

        int getPopupMenuBorderSize() override {
            return 0;
        }

        inline void setFontScale(const float x) { font_scale_ = x; }

    private:
        UIBase& base_;

        float font_scale_{kFontNormal};
    };
}
