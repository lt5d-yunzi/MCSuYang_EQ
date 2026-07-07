// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "../../../PluginProcessor.h"
#include "../../../gui/gui.hpp"
#include "../../helper/helper.hpp"
#include "../../multilingual/tooltip_helper.hpp"

#include "../../control_panel/control_background.hpp"

namespace zlpanel {
    class LevelMeter final : public juce::Component, public juce::Timer {
    public:
        explicit LevelMeter(PluginProcessor& p, zlgui::UIBase& base)
            : p_ref_(p), base_(base) {
            startTimerHz(30);
        }

        ~LevelMeter() override {
            stopTimer();
        }

        void mouseDown(const juce::MouseEvent&) override {
            peak_hold_db_ = -150.0f;
            repaint();
        }

        float mapDBToNormalized(float db) const {
            if (db >= 6.0f) return 1.0f;
            if (db <= -150.0f) return 0.0f;

            struct MapPoint {
                float db;
                float norm;
            };

            static const std::array<MapPoint, 15> points{{
                {   6.0f,  1.0f  },
                {   0.0f,  0.85f },
                {  -3.0f,  0.78f },
                {  -6.0f,  0.70f },
                {  -9.0f,  0.64f },
                { -12.0f,  0.58f },
                { -15.0f,  0.53f },
                { -18.0f,  0.48f },
                { -24.0f,  0.38f },
                { -36.0f,  0.30f },
                { -48.0f,  0.22f },
                { -72.0f,  0.15f },
                { -96.0f,  0.08f },
                {-120.0f,  0.04f },
                {-150.0f,  0.0f  }
            }};

            for (size_t i = 0; i < points.size() - 1; ++i) {
                if (db <= points[i].db && db >= points[i+1].db) {
                    return juce::jmap(db, points[i+1].db, points[i].db, points[i+1].norm, points[i].norm);
                }
            }
            return 0.0f;
        }

        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds().toFloat();
            float w = bounds.getWidth();
            float h = bounds.getHeight();

            // 1. Draw top peak hold DB text box
            auto peak_box = juce::Rectangle<float>(2.0f, 2.0f, w - 4.0f, 16.0f);
            g.setColour(juce::Colour(6, 10, 14));
            g.fillRoundedRectangle(peak_box, 3.0f);
            
            g.setColour(juce::Colour(23, 198, 197).withAlpha(0.6f));
            g.drawRoundedRectangle(peak_box, 3.0f, 1.0f);

            juce::String peak_text;
            juce::Colour peak_text_color = base_.getTextColour();

            if (peak_hold_db_ <= -149.0f) {
                peak_text = "-inf";
            } else {
                if (peak_hold_db_ >= 0.05f) {
                    peak_text = "+" + juce::String(peak_hold_db_, 1);
                    peak_text_color = juce::Colour(255, 50, 50); // Vibrant red for clipping
                } else {
                    peak_text = juce::String(peak_hold_db_, 1);
                    if (peak_hold_db_ > -3.0f) {
                        peak_text_color = juce::Colour(255, 180, 20); // Vibrant orange near clipping
                    } else {
                        peak_text_color = base_.getTextColour(); // Fully white/bright
                    }
                }
            }

            g.setFont(juce::Font("Microsoft YaHei", 10.0f, juce::Font::bold));
            g.setColour(peak_text_color);
            g.drawText(peak_text, peak_box, juce::Justification::centred, false);

            // 2. Setup area for level bars and scale with vertical padding
            float content_y = 21.0f;
            float content_h = h - content_y - 2.0f;
            
            // Add padding to top and bottom to prevent being cramped
            float top_pad = 6.0f;
            float bottom_pad = 6.0f;
            float active_y = content_y + top_pad;
            float active_h = content_h - top_pad - bottom_pad;

            float bars_bg_x = w - 9.0f - 1.0f;
            auto bars_bg = juce::Rectangle<float>(bars_bg_x, active_y, 9.0f, active_h);
            
            g.setColour(juce::Colour(8, 12, 16).withAlpha(0.7f));
            g.fillRoundedRectangle(bars_bg, 1.5f);

            float bar_w = 3.5f;
            auto left_bar = juce::Rectangle<float>(bars_bg_x + 0.5f, active_y + 1.0f, bar_w, active_h - 2.0f);
            auto right_bar = juce::Rectangle<float>(bars_bg_x + 5.0f, active_y + 1.0f, bar_w, active_h - 2.0f);

            // Poll peaks in dB
            float db_l = juce::Decibels::gainToDecibels(current_peak_l_, -150.0f);
            float db_r = juce::Decibels::gainToDecibels(current_peak_r_, -150.0f);

            float norm_l = mapDBToNormalized(db_l);
            float norm_r = mapDBToNormalized(db_r);

            float h_l = left_bar.getHeight() * norm_l;
            auto active_l = left_bar.withTop(left_bar.getBottom() - h_l);

            float h_r = right_bar.getHeight() * norm_r;
            auto active_r = right_bar.withTop(right_bar.getBottom() - h_r);

            // Draw active level bars
            if (h_l > 0.1f) {
                juce::ColourGradient grad_l(juce::Colour(23, 198, 197), left_bar.getX(), left_bar.getBottom(),
                                            juce::Colour(255, 50, 50), left_bar.getX(), left_bar.getY(), false);
                grad_l.addColour(0.70, juce::Colour(30, 255, 240));
                grad_l.addColour(0.85, juce::Colour(255, 200, 0));
                g.setGradientFill(grad_l);
                g.fillRoundedRectangle(active_l, 1.0f);
            }
            if (h_r > 0.1f) {
                juce::ColourGradient grad_r(juce::Colour(23, 198, 197), right_bar.getX(), right_bar.getBottom(),
                                            juce::Colour(255, 50, 50), right_bar.getX(), right_bar.getY(), false);
                grad_r.addColour(0.70, juce::Colour(30, 255, 240));
                grad_r.addColour(0.85, juce::Colour(255, 200, 0));
                g.setGradientFill(grad_r);
                g.fillRoundedRectangle(active_r, 1.0f);
            }

            // 3. Draw ticks and labels on the left of bars
            g.setFont(juce::Font("Microsoft YaHei", 13.0f, juce::Font::plain));

            std::array<float, 15> ticks{6.0f, 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -24.0f, -36.0f, -48.0f, -72.0f, -96.0f, -120.0f, -150.0f};
            for (float db : ticks) {
                float norm_y = mapDBToNormalized(db);
                float y = left_bar.getBottom() - norm_y * left_bar.getHeight();
                
                // Draw horizontal line behind bars
                g.setColour(juce::Colour(255, 255, 255).withAlpha(0.08f));
                g.drawHorizontalLine(static_cast<int>(y), bars_bg_x, bars_bg.getRight());

                // Draw label to the left of the bars
                if (db == 6.0f) {
                    g.setColour(juce::Colour(255, 80, 80));
                } else if (db == 0.0f) {
                    g.setColour(juce::Colour(255, 200, 0));
                } else if (db == -18.0f) {
                    g.setColour(base_.getTextColour().withAlpha(0.85f));
                } else {
                    g.setColour(base_.getTextColour().withAlpha(0.45f));
                }
                
                juce::String labelStr;
                if (db > 0.0f) {
                    labelStr = "+" + juce::String(static_cast<int>(db));
                } else if (db == 0.0f) {
                    labelStr = "0";
                } else {
                    labelStr = juce::String(static_cast<int>(db));
                }

                auto label_rect = juce::Rectangle<float>(0.0f, y - 8.0f, bars_bg_x - 2.0f, 16.0f);
                g.drawText(labelStr, label_rect, juce::Justification::centredRight, false);
            }
        }

        void timerCallback() override {
            float peak_l = p_ref_.output_peak_l_.load(std::memory_order::relaxed);
            float peak_r = p_ref_.output_peak_r_.load(std::memory_order::relaxed);

            if (peak_l > current_peak_l_) {
                current_peak_l_ = peak_l;
            } else {
                current_peak_l_ = current_peak_l_ * 0.90f + peak_l * 0.10f;
            }

            if (peak_r > current_peak_r_) {
                current_peak_r_ = peak_r;
            } else {
                current_peak_r_ = current_peak_r_ * 0.90f + peak_r * 0.10f;
            }

            // Track peak hold
            float max_peak = std::max(peak_l, peak_r);
            float max_db = juce::Decibels::gainToDecibels(max_peak, -150.0f);
            if (max_db > peak_hold_db_) {
                peak_hold_db_ = max_db;
            }

            repaint();
        }

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        float current_peak_l_{0.0f};
        float current_peak_r_{0.0f};
        float peak_hold_db_{-150.0f};
    };

    class OutputPanel final : public juce::Component,
                              private juce::ValueTree::Listener {
    public:
        explicit OutputPanel(PluginProcessor& p, zlgui::UIBase& base,
                             const multilingual::TooltipHelper& tooltip_helper);

        ~OutputPanel() override;

        int getIdealWidth() const;

        int getIdealHeight() const;

        void resized() override;

        void paint(juce::Graphics& g) override;

        void repaintCallBackSlow();

    private:
        PluginProcessor& p_ref_;
        zlgui::UIBase& base_;
        zlgui::attachment::ComponentUpdater updater_;

        ControlBackground control_background_;
        LevelMeter level_meter_;
        juce::Label level_meter_label_;

        zlgui::label::NameLookAndFeel name_laf_;
        juce::Label gain_label_;
        juce::Label scale_label_;



        zlgui::slider::TwoValueRotarySlider<false, false, false> scale_slider_;
        zlgui::attachment::SliderAttachment<true> scale_attach_;

        const std::unique_ptr<juce::Drawable> sgc_drawable_;
        zlgui::button::ClickButton sgc_button_;
        zlgui::attachment::ButtonAttachment<true> sgc_attach_;

        const std::unique_ptr<juce::Drawable> lm_drawable_;
        zlgui::button::ClickButton lm_button_;

        const std::unique_ptr<juce::Drawable> agc_drawable_;
        zlgui::button::ClickButton agc_button_;
        zlgui::attachment::ButtonAttachment<true> agc_attach_;

        const std::unique_ptr<juce::Drawable> phase_drawable_;
        zlgui::button::ClickButton phase_button_;
        zlgui::attachment::ButtonAttachment<true> phase_attach_;

        juce::Label lookahead_label_;
        zlgui::slider::CompactLinearSlider<false, false, false> lookahead_slider_;
        zlgui::attachment::SliderAttachment<true> lookahead_attach_;

        void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) override;
    };
}
