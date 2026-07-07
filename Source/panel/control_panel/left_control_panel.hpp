// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "../../PluginProcessor.h"
#include "../../gui/gui.hpp"
#include "../helper/helper.hpp"
#include "../multilingual/tooltip_helper.hpp"
#include "../helper/freq_note.hpp"

namespace zlpanel {
    class LeftControlPanel final : public juce::Component {
    public:
        explicit LeftControlPanel(PluginProcessor& p, zlgui::UIBase& base,
                                  const multilingual::TooltipHelper& tooltip_helper);

        ~LeftControlPanel() override;

        int getIdealWidth() const;

        void paint(juce::Graphics& g) override;

        void resized() override;

        void repaintCallBack();

        void repaintCallBackSlow();

        void updateBand();

        void updateFreqMax(double freq_max);

        void turnOnOffDynamic(bool dynamic_on);

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        zlgui::attachment::ComponentUpdater updater_;

        // Navigation & Header
        const std::unique_ptr<juce::Drawable> left_drawable_;
        zlgui::button::ClickButton left_button_;
        juce::Label band_label_;
        const std::unique_ptr<juce::Drawable> right_drawable_;
        zlgui::button::ClickButton right_button_;
        
        const std::unique_ptr<juce::Drawable> close_drawable_;
        zlgui::button::ClickButton close_button_;
        
        const std::unique_ptr<juce::Drawable> bypass_drawable_;
        zlgui::button::ClickButton bypass_button_;
        std::atomic<float>* filter_status_ptr_{nullptr};

        // Comboboxes
        zlgui::combobox::CompactCombobox ftype_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> ftype_attachment_;

        zlgui::combobox::CompactCombobox slope_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> slope_attachment_;

        zlgui::combobox::CompactCombobox stereo_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> stereo_attachment_;

        // Rotary Sliders
        zlgui::slider::TwoValueRotarySlider<false, true, false> gain_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> gain_attachment_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> target_gain_attachment_;

        zlgui::slider::TwoValueRotarySlider<false, false, false> freq_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> freq_attachment_;
        double freq_max_{20000.0};

        zlgui::slider::TwoValueRotarySlider<false, false, false> q_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> q_attachment_;

        zlgui::label::NameLookAndFeel label_laf_;
        juce::Label freq_label_;
        juce::Label gain_label_;
        juce::Label q_label_;

        std::atomic<float>& max_db_idx_ref_;
        float c_max_db_idx_{-1.f};

        std::atomic<float>* ftype_ptr_{nullptr};
        int c_ftype_{-1};

        bool dynamic_on_{false};

        void updateGainSliderDraggingDistance();

        void updateGainSliderComponentID();

        void enableSlope6(bool f);

        void enableSlope(bool f);

        void enableGain(bool f);

        void enableQ(bool f);
    };
}
