// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "control_panel/control_panel.hpp"

#include "curve_panel/curve_panel.hpp"

namespace zlpanel {
    class FaderLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        explicit FaderLookAndFeel(zlgui::UIBase& base) : base_(base) {}

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider) override {
            juce::ignoreUnused(minSliderPos, maxSliderPos, slider);

            auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
            auto centreX = bounds.getCentreX();
            auto centreY = bounds.getCentreY();

            if (style == juce::Slider::LinearHorizontal) {
                // 1. Draw track
                float track_h = 3.0f;
                auto track_rect = juce::Rectangle<float>(bounds.getX() + 4.0f, centreY - track_h / 2.0f,
                                                         bounds.getWidth() - 8.0f, track_h);
                
                g.setColour(juce::Colour(8, 12, 16).withAlpha(0.8f));
                g.fillRoundedRectangle(track_rect, 1.5f);

                if (sliderPos > track_rect.getX()) {
                    auto active_track = track_rect.withRight(sliderPos);
                    g.setColour(juce::Colour(23, 198, 197));
                    g.fillRoundedRectangle(active_track, 1.5f);
                }

                // 2. Draw thumb
                float thumb_w = 8.0f;
                float thumb_h = bounds.getHeight() - 10.0f;
                auto thumb_rect = juce::Rectangle<float>(sliderPos - thumb_w / 2.0f, centreY - thumb_h / 2.0f,
                                                         thumb_w, thumb_h);

                g.setColour(juce::Colour(0, 0, 0).withAlpha(0.35f));
                g.fillRoundedRectangle(thumb_rect.translated(1.0f, 0.0f), 2.0f);

                juce::ColourGradient thumb_grad(juce::Colour(45, 55, 62), thumb_rect.getX(), thumb_rect.getY(),
                                                juce::Colour(20, 25, 30), thumb_rect.getRight(), thumb_rect.getY(), false);
                g.setGradientFill(thumb_grad);
                g.fillRoundedRectangle(thumb_rect, 2.0f);

                g.setColour(juce::Colour(10, 12, 14));
                g.drawRoundedRectangle(thumb_rect, 2.0f, 1.0f);

                g.setColour(juce::Colour(30, 255, 240));
                g.drawVerticalLine(static_cast<int>(sliderPos), thumb_rect.getY() + 2.0f, thumb_rect.getBottom() - 2.0f);
            } else {
                // 1. Draw track
                float track_w = 3.0f;
                auto track_rect = juce::Rectangle<float>(centreX - track_w / 2.0f, bounds.getY() + 4.0f,
                                                         track_w, bounds.getHeight() - 8.0f);
                
                g.setColour(juce::Colour(8, 12, 16).withAlpha(0.8f));
                g.fillRoundedRectangle(track_rect, 1.5f);

                if (sliderPos < track_rect.getBottom()) {
                    auto active_track = track_rect.withTop(sliderPos).withBottom(track_rect.getBottom());
                    g.setColour(juce::Colour(23, 198, 197));
                    g.fillRoundedRectangle(active_track, 1.5f);
                }

                // 2. Draw thumb
                float thumb_w = bounds.getWidth() - 10.0f;
                float thumb_h = 8.0f;
                auto thumb_rect = juce::Rectangle<float>(centreX - thumb_w / 2.0f, sliderPos - thumb_h / 2.0f,
                                                         thumb_w, thumb_h);

                g.setColour(juce::Colour(0, 0, 0).withAlpha(0.35f));
                g.fillRoundedRectangle(thumb_rect.translated(0.0f, 1.0f), 2.0f);

                juce::ColourGradient thumb_grad(juce::Colour(45, 55, 62), thumb_rect.getX(), thumb_rect.getY(),
                                                juce::Colour(20, 25, 30), thumb_rect.getX(), thumb_rect.getBottom(), false);
                g.setGradientFill(thumb_grad);
                g.fillRoundedRectangle(thumb_rect, 2.0f);

                g.setColour(juce::Colour(10, 12, 14));
                g.drawRoundedRectangle(thumb_rect, 2.0f, 1.0f);

                g.setColour(juce::Colour(30, 255, 240));
                g.drawHorizontalLine(static_cast<int>(sliderPos), thumb_rect.getX() + 2.0f, thumb_rect.getRight() - 2.0f);
            }
        }

    private:
        zlgui::UIBase& base_;
    };

    class MainPanel final : public juce::Component,
                            private juce::ValueTree::Listener,
                            private juce::Timer,
                            private juce::Label::Listener,
                            private juce::ComboBox::Listener {
    public:
        explicit MainPanel(PluginProcessor& p, zlgui::UIBase& base, multilingual::TooltipLanguage language);

        ~MainPanel() override;

        void paint(juce::Graphics& g) override;

        void resized() override;

        void repaintCallBack(double time_stamp);

        void startThreads();

        void stopThreads();

        ControlPanel& getControlPanel() {
            return control_panel_;
        }

        OutputPanel& getOutputPanel() {
            return curve_panel_.getOutputPanel();
        }

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        multilingual::TooltipHelper tooltip_helper_;

        RefreshHandler refresh_handler_;
        double previous_time_stamp_{-1.0};
        double refresh_rate_{-1.0};

        CurvePanel curve_panel_;
        ControlPanel control_panel_;

        zlgui::tooltip::TooltipLookAndFeel tooltip_laf_;
        std::unique_ptr<zlgui::tooltip::TooltipWindow> tooltip_window_;

        size_t c_band_{zlp::kBandNum};
        double c_sample_rate_{0.};
        bool first_frame_{true};
        int startup_frames_{0};

        void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) override;

        void timerCallback() override;

        void repaintCallBackSlow();
        void updateABButtonStates();

        void labelTextChanged(juce::Label* label) override;
        void editorShown(juce::Label* label, juce::TextEditor& editor) override;
        void editorHidden(juce::Label* label, juce::TextEditor& editor) override;

        void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

        void refreshPresetsList();
        void saveUserPreset(const juce::String& name);
        void deleteActiveUserPreset();
        void importUserPreset();
        void exportUserPreset();
        void loadSelectedPreset(int id);

        zlgui::slider::SnappingSlider gain_slider_;
        FaderLookAndFeel fader_laf_;
        zlgui::attachment::ComponentUpdater updater_;
        zlgui::attachment::SliderAttachment<true> gain_attach_;
        juce::Label gain_title_label_;
        juce::Label gain_value_label_;
        std::unique_ptr<juce::Drawable> phase_drawable_;
        zlgui::button::ClickButton phase_button_;
        zlgui::attachment::ButtonAttachment<true> phase_attach_;
        zlgui::button::ClickTextButton auto_gain_btn_;
        zlgui::attachment::ButtonAttachment<true> auto_gain_attach_;
        zlgui::combobox::CompactCombobox preset_box_;
        int active_preset_id_{10};
        std::unique_ptr<juce::FileChooser> file_chooser_;
        zlgui::button::ClickTextButton a_btn_;
        zlgui::button::ClickTextButton ab_copy_btn_;
        zlgui::button::ClickTextButton b_btn_;
        juce::Label fft_title_label_;
        zlgui::button::ClickTextButton fft_collision_btn_;
        zlgui::attachment::ButtonAttachment<true> fft_collision_attach_;
        zlgui::button::ClickTextButton fft_pre_btn_;
        zlgui::attachment::ButtonAttachment<true> fft_pre_attach_;
        zlgui::button::ClickTextButton fft_post_btn_;
        zlgui::attachment::ButtonAttachment<true> fft_post_attach_;
        juce::Label bottom_left_label_;

        void mouseUp(const juce::MouseEvent& event) override;
    };
} // zlpanel
