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
#include "../helper/freq_note.hpp"
#include "../helper/helper.hpp"
#include "../multilingual/tooltip_helper.hpp"

#include "control_background.hpp"

namespace zlpanel {
    class ClickableLabel : public juce::Label {
    public:
        ClickableLabel(const juce::String& labelName = {}, const juce::String& labelText = {})
            : juce::Label(labelName, labelText) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        std::function<void()> onClick;

        void mouseDown(const juce::MouseEvent& event) override {
            juce::Label::mouseDown(event);
            if (onClick && isEnabled()) {
                onClick();
            }
        }

        void mouseEnter(const juce::MouseEvent& event) override {
            juce::Label::mouseEnter(event);
            is_hovered_ = true;
            if (getParentComponent() != nullptr) {
                getParentComponent()->repaint();
            }
        }

        void mouseExit(const juce::MouseEvent& event) override {
            juce::Label::mouseExit(event);
            is_hovered_ = false;
            if (getParentComponent() != nullptr) {
                getParentComponent()->repaint();
            }
        }

        bool isHovered() const noexcept { return is_hovered_; }

    private:
        bool is_hovered_ = false;
    };

    class RightControlPanel final : public juce::Component {
    public:
        explicit RightControlPanel(PluginProcessor& p, zlgui::UIBase& base,
                                   const multilingual::TooltipHelper& tooltip_helper);

        ~RightControlPanel() override;

        int getIdealWidth() const;

        void paint(juce::Graphics& g) override;

        void resized() override;

        void repaintCallBack();

        void repaintCallBackSlow();

        void updateBand();

        void updateFreqMax(double freq_max);

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        zlgui::attachment::ComponentUpdater updater_;

        ControlBackground control_background_;

        zlgui::button::ClickTextButton dynamic_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> dynamic_attachment_;

        zlgui::button::ClickTextButton delete_button_;

        zlgui::button::ClickTextButton band_bypass_button_;
        zlgui::button::ClickTextButton band_solo_button_;
        ClickableLabel active_band_label_;

        zlgui::combobox::CompactCombobox filter_type_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> filter_type_attachment_;

        zlgui::combobox::CompactCombobox lr_mode_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> lr_mode_attachment_;

        zlgui::combobox::CompactCombobox order_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> order_attachment_;

        const std::unique_ptr<juce::Drawable> bypass_drawable_;
        zlgui::button::ClickButton bypass_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> bypass_attachment_;

        zlgui::button::ClickTextButton auto_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> auto_attachment_;

        zlgui::button::ClickTextButton link_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> link_attachment_;

        zlgui::button::ClickTextButton track_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> track_attachment_;

        zlgui::button::ClickTextButton precise_button_;
        std::unique_ptr<zlgui::attachment::ButtonAttachment<true>> precise_attachment_;

        zlgui::combobox::CompactCombobox harmonic_box_;
        std::unique_ptr<zlgui::attachment::ComboBoxAttachment<true>> harmonic_attachment_;

        zlgui::slider::TwoValueRotarySlider<false, false, false> static_freq_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> static_freq_attachment_;

        zlgui::slider::TwoValueRotarySlider<false, false, false> static_gain_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> static_gain_attachment_;

        zlgui::slider::TwoValueRotarySlider<false, false, false> static_q_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> static_q_attachment_;

        zlgui::slider::TwoValueRotarySlider<false, false, false> th_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> th_attachment_;
 
        zlgui::slider::TwoValueRotarySlider<false, false, false> attack_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> attack_attachment_;
 
        zlgui::slider::TwoValueRotarySlider<false, false, false> release_slider_;
        std::unique_ptr<zlgui::attachment::SliderAttachment<true>> release_attachment_;
 
        zlgui::slider::TwoValueRotarySlider<false, false, false> range_slider_;
 
        zlgui::label::NameLookAndFeel label_laf_;
        juce::Label dynamic_label_;
 
        zlgui::label::NameLookAndFeel knob_label_laf_;
        juce::Label static_freq_label_;
        juce::Label static_gain_label_;
        juce::Label static_q_label_;
        juce::Label th_label_;
        juce::Label attack_label_;
        juce::Label release_label_;
        juce::Label range_label_;
 
        double freq_max_{20000.0};

        std::atomic<float>* dynamic_on_ptr_{nullptr};
        std::atomic<float>* precise_on_ptr_{nullptr};
        std::atomic<float>* gain_param_ptr_{nullptr};
        std::atomic<float>* target_gain_param_ptr_{nullptr};
        bool c_dynamic_btn_enabled_{false};
        bool c_dynamic_enabled_{false};
        int c_precise_mode_{-1};
        float divider_x_{0.f};
        float left_divider_x_{0.f};
        bool is_syncing_range_{false};
        std::atomic<float>* filter_status_ptr_{nullptr};
        std::atomic<float>* ftype_param_ptr_{nullptr};
        int c_ftype_{-1};

        void enableSlope6(bool f);
        void enableSlope(bool f);

        void turnOnOffAuto();

        void mouseDown(const juce::MouseEvent& event) override;
    };
}
