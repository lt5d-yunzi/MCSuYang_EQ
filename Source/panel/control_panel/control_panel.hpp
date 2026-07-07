// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "left_control_panel.hpp"
#include "right_control_panel.hpp"

namespace zlpanel {
    class ControlPanel final : public juce::Component {
    public:
        explicit ControlPanel(PluginProcessor& p, zlgui::UIBase& base,
                              const multilingual::TooltipHelper& tooltip_helper);

        ~ControlPanel() override;

        int getIdealWidth() const;

        int getIdealHeight() const;

        void resized() override;

        void paint(juce::Graphics& g) override;

        void repaintCallBack();

        void repaintCallBackSlow();

        void updateBand();

        void updateSampleRate(double sample_rate);

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        zlgui::attachment::ComponentUpdater updater_;

        juce::Component mouse_event_eater_;
        RightControlPanel right_control_panel_;

        zlgui::slider::CompactLinearSlider<false, false, false> soothe_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> soothe_attach_;
        juce::Label soothe_label_;
        zlgui::label::NameLookAndFeel soothe_laf_;


        std::atomic<float>* dynamic_on_ptr_{nullptr};
        bool c_dynamic_on_{false};
    };
}
