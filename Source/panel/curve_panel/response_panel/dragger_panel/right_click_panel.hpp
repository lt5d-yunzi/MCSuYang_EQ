// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "../../../../PluginProcessor.h"
#include "../../../../gui/gui.hpp"
#include "../../../helper/helper.hpp"
#include "../../../multilingual/tooltip_helper.hpp"

#include "../../../control_panel/control_background.hpp"

namespace zlpanel {
    class RightClickPanel final : public juce::Component {
    public:
        explicit RightClickPanel(PluginProcessor& p, zlgui::UIBase& base,
                                 const multilingual::TooltipHelper& tooltip_helper);

        int getIdealWidth() const;

        int getIdealHeight() const;

        void paint(juce::Graphics& g) override;

        void resized() override;

        void setPosition(juce::Point<float> pos);

        void setSafeArea(juce::Rectangle<float> area);

    private:
        static constexpr std::array kIDs{
            zlp::PFilterStatus::kID,
            zlp::PFilterType::kID,
            zlp::POrder::kID, zlp::PLRMode::kID,
            zlp::PFreq::kID,
            zlp::PGain::kID, zlp::PTargetGain::kID,
            zlp::PQ::kID,
            zlp::PDynamicON::kID, zlp::PDynamicLearn::kID,
            zlp::PDynamicBypass::kID, zlp::PDynamicRelative::kID,
            zlp::PSideSwap::kID,
            zlp::PSideLink::kID,
            zlp::PThreshold::kID, zlp::PKneeW::kID, zlp::PAttack::kID, zlp::PRelease::kID,
            zlp::PSideFilterType::kID, zlp::PSideOrder::kID,
            zlp::PSideFreq::kID, zlp::PSideQ::kID
        };

        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        juce::SelectedItemSet<size_t>& items_set_;
        ControlBackground control_background_;

        juce::Rectangle<float> safe_area_{};

        juce::Component mouse_event_eater_;
        zlgui::button::ClickTextButton lr_split_button_;
        zlgui::button::ClickTextButton ms_split_button_;
        zlgui::attachment::ComponentUpdater updater_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> slope_attachment_;
        zlgui::combobox::CompactCombobox slope_box_;

        struct RightClickMenuLAF final : public zlgui::combobox::CompactComboboxLookAndFeel {
            explicit RightClickMenuLAF(zlgui::UIBase& base) :
                CompactComboboxLookAndFeel(base), base_ref_(base) {}

            void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                      const juce::Colour&, bool highlight, bool down) override;
            void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                const bool highlight, const bool down) override;
            void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int,
                              juce::ComboBox& box) override;
            void drawLabel(juce::Graphics& g, juce::Label& label) override;

        private:
            zlgui::UIBase& base_ref_;
        };

        RightClickMenuLAF menu_laf_;

        void visibilityChanged() override;

        void splitBand(zlp::FilterStereo stereo1, zlp::FilterStereo stereo2);
    };
}
