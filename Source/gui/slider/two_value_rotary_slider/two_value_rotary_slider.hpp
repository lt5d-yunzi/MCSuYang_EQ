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
#include "../../label/name_look_and_feel.hpp"
#include "../extra_slider/snapping_slider.h"

namespace zlgui::slider {
    template <bool kOpaque = true, bool kUseSecondSlider = true, bool kUseName = true>
    class TwoValueRotarySlider final : public juce::Component,
                                       private juce::Label::Listener,
                                       private juce::Slider::Listener,
                                       public juce::SettableTooltipClient {
    public:
        static constexpr float kStartAngle = 2.0943951023931953f, kEndAngle = 7.3303828583761845f;
        std::string permitted_characters_ = "-0123456789.kK";
        std::function<std::string(double)> value_formatter_;
        std::function<std::optional<double>(const std::string&)> string_formatter_;

    public:
        class ValueLookAndFeel final : public juce::LookAndFeel_V4 {
        public:
            explicit ValueLookAndFeel(UIBase& base) : base_(base) {}
            void drawLabel(juce::Graphics& g, juce::Label& label) override {
                if (label.isBeingEdited()) {
                    return;
                }
                g.setColour(juce::Colours::white);
                auto font = juce::Font(base_.getFontSize() * font_scale_);
                font.setBold(true);
                g.setFont(font);
                g.drawText(label.getText(), label.getLocalBounds().toFloat(), label.getJustificationType());
            }
            inline void setFontScale(const float x) { font_scale_ = x; }
        private:
            UIBase& base_;
            float font_scale_{1.1f};
        };

    private:
        class Background final : public juce::Component {
        public:
            explicit Background(UIBase& base, const float thick_scale) :
                base_(base), thick_scale_(thick_scale) {
                setOpaque(kOpaque);
                setInterceptsMouseClicks(false, false);
                setBufferedToImage(true);
            }

            void paint(juce::Graphics& g) override {
                if constexpr (kOpaque) {
                    g.fillAll(base_.getBackgroundColour());
                } else {
                    g.setColour(base_.getBackgroundColour());
                    g.fillEllipse(getLocalBounds().toFloat());
                }
                auto bounds = getLocalBounds().toFloat();
                const auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
                bounds = bounds.withSizeKeepingCentre(diameter, diameter);
                
                const float t_scale = thick_scale_;
                const auto track_bounds = bounds.reduced(4.5f * t_scale);
                const auto new_bounds = bounds.reduced(8.0f * t_scale);
                
                // 1. Draw inactive track (faint arc)
                const float radius = track_bounds.getWidth() * 0.5f;
                juce::Path track_path;
                track_path.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), radius, radius, 0.0f,
                                         kStartAngle + juce::MathConstants<float>::pi * 0.5f,
                                         kEndAngle + juce::MathConstants<float>::pi * 0.5f,
                                         true);
                g.setColour(juce::Colour(20, 25, 30).withAlpha(0.6f));
                g.strokePath(track_path, juce::PathStrokeType(2.5f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                
                // 2. Draw knob cap background & outer bevel/border
                juce::ColourGradient cap_grad(juce::Colour(42, 50, 56), new_bounds.getX(), new_bounds.getY(),
                                              juce::Colour(18, 22, 25), new_bounds.getX(), new_bounds.getBottom(), false);
                g.setGradientFill(cap_grad);
                g.fillEllipse(new_bounds);
                
                // Outer ring outline highlight
                g.setColour(juce::Colour(10, 12, 14));
                g.drawEllipse(new_bounds, 1.2f * t_scale);
                
                // Subtle top highlight
                g.setColour(juce::Colour(255, 255, 255).withAlpha(0.04f));
                g.drawEllipse(new_bounds.reduced(1.0f * t_scale), 1.0f * t_scale);
            }

        private:
            UIBase& base_;
            float thick_scale_{1.f};
        };

        class Display final : public juce::Component {
        public:
            explicit Display(UIBase& base, const float thick_scale) :
                base_(base), thick_scale_(thick_scale) {
                setInterceptsMouseClicks(false, false);
            }

            void paint(juce::Graphics& g) override {
                const auto centre = bound_.getCentre();
                const float t_scale = thick_scale_;
                const float radius = (bound_.getWidth() - 9.0f * t_scale) * 0.5f;
                
                // 1. Draw active track 1
                juce::Path active_track;
                active_track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                                           kStartAngle + juce::MathConstants<float>::pi * 0.5f,
                                           angle1_ + juce::MathConstants<float>::pi * 0.5f,
                                           true);
                
                // Neon glow color dynamically resolved from active band
                const auto neon_color = custom_neon_color_.value_or(base_.getSelectedBand() < zlstate::kBandNum ? base_.getColourMap1(base_.getSelectedBand()) : juce::Colour(140, 175, 210));
                
                // Draw active track 1 glow (outer thick blur)
                g.setColour(neon_color.withAlpha(0.12f));
                g.strokePath(active_track, juce::PathStrokeType(6.0f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                
                // Draw active track 1 mid glow
                g.setColour(neon_color.withAlpha(0.35f));
                g.strokePath(active_track, juce::PathStrokeType(3.5f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                
                // Draw active track 1 core line
                g.setColour(neon_color);
                g.strokePath(active_track, juce::PathStrokeType(1.8f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                
                // 2. Draw active track 2 (if second slider is used)
                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        const float start_a = std::min(angle1_, angle2_) + juce::MathConstants<float>::pi * 0.5f;
                        const float end_a = std::max(angle1_, angle2_) + juce::MathConstants<float>::pi * 0.5f;
                        juce::Path active_track2;
                        active_track2.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                                                   start_a, end_a, true);
                        
                        juce::Colour track2_col;
                        if (value1_ > value2_) {
                            track2_col = base_.getColourMap2(0); // orange/red
                        } else {
                            track2_col = base_.getColourMap2(2); // green/blue
                        }
                        
                        // Draw glow
                        g.setColour(track2_col.withAlpha(0.12f));
                        g.strokePath(active_track2, juce::PathStrokeType(6.0f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        
                        g.setColour(track2_col.withAlpha(0.35f));
                        g.strokePath(active_track2, juce::PathStrokeType(3.5f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        
                        // Draw line
                        g.setColour(track2_col);
                        g.strokePath(active_track2, juce::PathStrokeType(1.8f * t_scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }
                }

                // 3. Draw pointer indicator tick inside the cap
                const auto cap_bounds = bound_.reduced(8.0f * t_scale);
                const float cap_radius = cap_bounds.getWidth() * 0.5f;
                
                const float pointer_angle = angle1_;
                const float cos_a = std::cos(pointer_angle);
                const float sin_a = std::sin(pointer_angle);
                
                const float p_start_r = cap_radius - 6.5f * t_scale;
                const float p_end_r = cap_radius - 1.5f * t_scale;
                
                const juce::Point<float> p1(centre.x + p_start_r * cos_a, centre.y + p_start_r * sin_a);
                const juce::Point<float> p2(centre.x + p_end_r * cos_a, centre.y + p_end_r * sin_a);
                
                g.setColour(neon_color);
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f * t_scale);

                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        const float pointer2_angle = angle2_;
                        const float cos_a2 = std::cos(pointer2_angle);
                        const float sin_a2 = std::sin(pointer2_angle);
                        
                        const juce::Point<float> p1_2(centre.x + p_start_r * cos_a2, centre.y + p_start_r * sin_a2);
                        const juce::Point<float> p2_2(centre.x + p_end_r * cos_a2, centre.y + p_end_r * sin_a2);
                        
                        juce::Colour pointer2_col;
                        if (value1_ > value2_) {
                            pointer2_col = base_.getColourMap2(0); // orange/red
                        } else {
                            pointer2_col = base_.getColourMap2(2); // green/blue
                        }
                        
                        g.setColour(pointer2_col);
                        g.drawLine(p1_2.x, p1_2.y, p2_2.x, p2_2.y, 2.0f * t_scale);
                    }
                }
            }

            void resized() override {
                bound_ = getLocalBounds().toFloat();
                const auto diameter = juce::jmin(bound_.getWidth(), bound_.getHeight());
                bound_ = bound_.withSizeKeepingCentre(diameter, diameter);
                old_bound_ = UIBase::getInnerShadowEllipseArea(bound_, base_.getFontSize() * 0.5f * thick_scale_, {});
                new_bound_ = UIBase::getShadowEllipseArea(old_bound_, base_.getFontSize() * 0.5f * thick_scale_, {});
                arrow_unit_ = (diameter - new_bound_.getWidth()) * 0.5f;

                mask_.clear();
                mask_.addEllipse(bound_);
                mask_.setUsingNonZeroWinding(false);
                mask_.addEllipse(new_bound_);

                setSlider1Value(value1_);
                if constexpr (kUseSecondSlider) {
                    setSlider2Value(value2_);
                }
            }

            void setSlider1Value(const float x) {
                value1_ = x;
                const auto rotationAngle = kStartAngle + x * (kEndAngle - kStartAngle);
                angle1_ = rotationAngle;
                filling1_.clear();
                filling1_.addPieSegment(bound_,
                                        kStartAngle + juce::MathConstants<float>::pi * .5f,
                                        rotationAngle + juce::MathConstants<float>::pi * .5f,
                                        0);
                if (kUseSecondSlider && show_slider2_) {
                    setSlider2Value(value2_);
                } else {
                    repaint();
                }
            }

            void setSlider2Value(const float x) {
                if constexpr (kUseSecondSlider) {
                    value2_ = x;
                    const auto rotationAngle = kStartAngle + x * (kEndAngle - kStartAngle);
                    angle2_ = rotationAngle;
                    filling2_.clear();
                    filling2_.addPieSegment(bound_,
                                            angle1_ + juce::MathConstants<float>::pi * .5f,
                                            rotationAngle + juce::MathConstants<float>::pi * .5f,
                                            0);
                    repaint();
                }
            }

            void setShowSlider2(const bool x) {
                if constexpr (kUseSecondSlider) {
                    show_slider2_ = x;
                }
            }

            void setCustomNeonColor(const std::optional<juce::Colour>& c) {
                custom_neon_color_ = c;
                repaint();
            }

        private:
            UIBase& base_;
            float thick_scale_{1.f};
            juce::Rectangle<float> bound_, old_bound_, new_bound_;
            float value1_{0.f}, value2_{0.f};
            float arrow_unit_{0.f}, angle1_{0.f}, angle2_{0.f};
            bool show_slider2_{false};
            juce::Path mask_;
            juce::Rectangle<float> arrow1_, arrow2_;
            juce::Path filling1_, filling2_;
            std::optional<juce::Colour> custom_neon_color_{std::nullopt};
        };

    public:
        explicit TwoValueRotarySlider(const juce::String& label_text, UIBase& base,
                                      const juce::String& tooltip_text = "",
                                      float thick_scale = 1.f) :
            base_(base), background_(base_, thick_scale), display_(base, thick_scale),
            slider1_(base, label_text), slider2_(base, label_text),
            label_look_and_feel_(base), label_look_and_feel1_(base), label_look_and_feel2_(base),
            text_box_laf_(base) {
            addAndMakeVisible(background_);
            for (auto const s : {&slider1_, &slider2_}) {
                s->setSliderStyle(base_.getRotaryStyle());
                s->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
                s->setDoubleClickReturnValue(true, 0.0);
                s->setScrollWheelEnabled(true);
                s->setInterceptsMouseClicks(false, false);
            }

            slider1_.addListener(this);
            if constexpr (kUseSecondSlider) {
                slider2_.addListener(this);
            }
            addAndMakeVisible(display_);

            if constexpr (kUseName) {
                label_.setText(label_text, juce::dontSendNotification);
                label_.setJustificationType(juce::Justification::centred);
                label_.setBufferedToImage(true);
                label_look_and_feel_.setFontScale(1.75f);
                label_.setLookAndFeel(&label_look_and_feel_);
            }

            label1_.setText(getDisplayValue(slider1_), juce::dontSendNotification);
            label_look_and_feel1_.setFontScale(1.1f);
            label1_.setJustificationType(juce::Justification::centredBottom);
            label1_.setLookAndFeel(&label_look_and_feel1_);

            if constexpr (kUseSecondSlider) {
                label2_.setText(getDisplayValue(slider2_), juce::dontSendNotification);
                label2_.setJustificationType(juce::Justification::centredTop);
                label2_.setLookAndFeel(&label_look_and_feel1_);
            }

            for (auto& l : {&label_, &label1_, &label2_}) {
                l->setInterceptsMouseClicks(false, false);
            }

            if constexpr (kUseName) {
                addAndMakeVisible(label_);
                addChildComponent(label1_);
            } else {
                addAndMakeVisible(label1_);
            }

            setEditable(true);
            label1_.setEditable(false, true);
            label1_.addListener(this);

            if constexpr (kUseSecondSlider) {
                addChildComponent(label2_);
                label2_.setEditable(false, true);
                label2_.addListener(this);
            }

            // set up tooltip
            if (tooltip_text.length() > 0) {
                SettableTooltipClient::setTooltip(tooltip_text);
            }

            setOpaque(kOpaque);
        }

        ~TwoValueRotarySlider() override {
            slider1_.removeListener(this);
            label1_.removeListener(this);
            if constexpr (kUseSecondSlider) {
                slider2_.removeListener(this);
                label2_.removeListener(this);
            }
        }

        void visibilityChanged() override {
            if (isVisible()) {
                sliderValueChanged(&slider1_);
                if constexpr (kUseSecondSlider) {
                    sliderValueChanged(&slider2_);
                }
            }
        }

        void paint(juce::Graphics& g) override {
            juce::ignoreUnused(g);
        }

        void mouseUp(const juce::MouseEvent& event) override {
            if (event.getNumberOfClicks() > 1 || event.mods.isCommandDown()) {
                return;
            }
            if (event.mods.isLeftButtonDown()) {
                slider1_.mouseUp(event);
            } else if (show_slider2_ && event.mods.isRightButtonDown()) {
                slider2_.mouseUp(event);
            }
        }

        void mouseDown(const juce::MouseEvent& event) override {
            if (event.getNumberOfClicks() > 1 || event.mods.isCommandDown()) {
                return;
            }
            if (event.mods.isLeftButtonDown()) {
                slider1_.mouseDown(event);
            } else if (show_slider2_ && event.mods.isRightButtonDown()) {
                slider2_.mouseDown(event);
            }
            const auto current_shift_pressed = event.mods.isShiftDown();
            if (current_shift_pressed != is_shift_pressed_) {
                is_shift_pressed_ = current_shift_pressed;
                updateDragDistance();
            }
        }

        void mouseDrag(const juce::MouseEvent& event) override {
            if (event.mods.isLeftButtonDown()) {
                slider1_.mouseDrag(event);
            } else if (show_slider2_ && event.mods.isRightButtonDown()) {
                slider2_.mouseDrag(event);
            }
        }

        void mouseEnter(const juce::MouseEvent& event) override {
            slider1_.mouseEnter(event);
            slider2_.mouseEnter(event);
            if constexpr (kUseName) {
                label_.setVisible(false);
                label1_.setVisible(true);
                label1_.setText(getDisplayValue(slider1_), juce::dontSendNotification);
                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        label2_.setVisible(true);
                        label2_.setText(getDisplayValue(slider2_), juce::dontSendNotification);
                    }
                }
            }
        }

        void mouseExit(const juce::MouseEvent& event) override {
            slider1_.mouseExit(event);
            slider2_.mouseExit(event);
            if constexpr (kUseName) {
                if (label1_.getCurrentTextEditor() != nullptr || label2_.getCurrentTextEditor() != nullptr) {
                    return;
                }

                label_.setVisible(true);
                label1_.setVisible(false);
                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        label2_.setVisible(false);
                    }
                }
            }
        }

        void mouseMove(const juce::MouseEvent& event) override {
            juce::ignoreUnused(event);
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override {
            if (base_.getIsSliderDoubleClickOpenEditor() != event.mods.isCommandDown()) {
                const auto portion = event.position.y / static_cast<float>(getLocalBounds().getHeight());
                if (portion < .5f || !show_slider2_) {
                    label1_.showEditor();
                    return;
                } else {
                    label2_.showEditor();
                    return;
                }
            }
            if (event.mods.isLeftButtonDown()) {
                slider1_.mouseDoubleClick(event);
            } else if (show_slider2_ && event.mods.isRightButtonDown()) {
                slider2_.mouseDoubleClick(event);
            }
        }

        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override {
            if (!show_slider2_) {
                slider1_.mouseWheelMove(event, wheel);
            } else {
                slider1_.mouseWheelMove(event, wheel);
                slider2_.mouseWheelMove(event, wheel);
            }
        }

        void resized() override {
            const auto bound = getLocalBounds();
            background_.setBounds(bound);
            display_.setBounds(bound);
            slider1_.setBounds(bound);
            if constexpr (kUseSecondSlider) {
                slider2_.setBounds(bound);
            }
            if constexpr (kUseName) {
                const auto bound_size = std::min(bound.getWidth(), bound.getHeight());
                const auto label_bound = bound.withSizeKeepingCentre(
                    static_cast<int>(std::round(static_cast<float>(bound_size * 0.7f))),
                    static_cast<int>(std::round(static_cast<float>(bound_size * 0.6f))));
                label_.setBounds(label_bound);
            }

            setShowSlider2(show_slider2_);
        }

        inline juce::Slider& getSlider1() { return slider1_; }

        inline juce::Slider& getSlider2() { return slider2_; }

        void setShowSlider2(const bool x) {
            show_slider2_ = x && kUseSecondSlider;

            const auto bound = getLocalBounds().toFloat();

            auto label_bound = bound.withSizeKeepingCentre(bound.getWidth() * 0.9f,
                                                           bound.getHeight() * 0.5f);
            if (show_slider2_) {
                const auto valueBound1 = label_bound.removeFromTop(label_bound.getHeight() * 0.5f);
                const auto valueBound2 = label_bound;
                label1_.setBounds(valueBound1.toNearestInt());
                label1_.setJustificationType(juce::Justification::centredBottom);
                label2_.setVisible(true);
                label2_.setBounds(valueBound2.toNearestInt());
                label2_.setJustificationType(juce::Justification::centredTop);
            } else {
                label_bound = label_bound.withSizeKeepingCentre(label_bound.getWidth(), label_bound.getHeight() * .5f);
                label1_.setBounds(label_bound.toNearestInt());
                label2_.setVisible(false);
                label1_.setJustificationType(juce::Justification::centred);
            }
            display_.setShowSlider2(show_slider2_);
            sliderValueChanged(&slider2_);
        }

        inline void setEditable(const bool x) {
            setAlpha(x ? 1.f : .5f);
            setInterceptsMouseClicks(x, false);
        }

        void setMouseDragSensitivity(const int x) {
            drag_distance_ = x;
            updateDragDistance();
        }

        void setPrecision(const int x) {
            precision_ = x;
        }

        void setCustomNeonColour(const std::optional<juce::Colour>& c) {
            display_.setCustomNeonColor(c);
        }

    private:
        UIBase& base_;
        Background background_;
        Display display_;

        SnappingSlider slider1_, slider2_;

        label::NameLookAndFeel label_look_and_feel_;
        ValueLookAndFeel label_look_and_feel1_, label_look_and_feel2_;
        label::NameLookAndFeel text_box_laf_;

        juce::Label label_, label1_, label2_;

        bool show_slider2_{false};

        int drag_distance_{10};
        bool is_shift_pressed_{false};

        int precision_{4};

        juce::String getDisplayValue(const juce::Slider& s) const {
            auto value = s.getValue();
            if (std::abs(value) < 1e-6) {
                value = 0.0;
            }
            if (value_formatter_) {
                return value_formatter_(value);
            }
            const bool append_k = precision_ >= 4 ? std::abs(value) >= 10000.0 : std::abs(value) >= 1000.0;
            const auto display_value = append_k ? value * 0.001 : value;
            auto actual_precision = append_k ? precision_ - 1 : precision_;
            if (std::abs(display_value) >= 100.0) {
                actual_precision = std::max(actual_precision, 3);
            }

            char buffer[32];
            if (std::abs(value) < 1.0) {
                snprintf(buffer, sizeof(buffer), "%.*f", actual_precision - 1, display_value);
            } else {
                snprintf(buffer, sizeof(buffer), "%.*g", actual_precision, display_value);
            }
            std::string str{buffer};
            // remove trailing zeros and decimal point
            const auto last_decimal = str.find_last_of('.');
            if (last_decimal != std::string::npos) {
                const auto last_not_zero = str.find_last_not_of('0');
                if (last_not_zero != std::string::npos) {
                    str.erase(last_not_zero + 1);
                }
                if (str.back() == '.') {
                    str.pop_back();
                }
            }

            return append_k ? juce::String{str + "K"} : juce::String{str};
        }

        void labelTextChanged(juce::Label* label_that_has_changed) override {
            juce::ignoreUnused(label_that_has_changed);
        }

        void editorShown(juce::Label* l, juce::TextEditor& editor) override {
            juce::ignoreUnused(l);
            editor.setInputRestrictions(0, permitted_characters_);

            if constexpr (kUseName) {
                label_.setVisible(false);
                label1_.setVisible(true);
                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        label2_.setVisible(true);
                    }
                }
            }

            editor.setJustification(juce::Justification::centred);
            editor.setColour(juce::TextEditor::outlineColourId, base_.getTextColour());
            editor.setColour(juce::TextEditor::highlightedTextColourId, base_.getTextColour());
            editor.applyFontToAllText(juce::FontOptions{base_.getFontSize() * 1.1f});
            editor.applyColourToAllText(base_.getTextColour(), true);
        }

        void editorHidden(juce::Label* l, juce::TextEditor& editor) override {
            const auto ctext = editor.getText();
            std::optional<double> format_result{std::nullopt};
            if (string_formatter_) {
                format_result = string_formatter_(ctext.toStdString());
            }
            double actual_value;
            if (format_result != std::nullopt) {
                actual_value = format_result.value();
            } else {
                const auto k = ctext.contains("k") || ctext.contains("K") ? 1000.0 : 1.0;
                actual_value = ctext.getDoubleValue() * k;
            }

            if (l == &label1_) {
                slider1_.setValue(actual_value, juce::sendNotificationAsync);
            }
            if constexpr (kUseSecondSlider) {
                if (l == &label2_) {
                    slider2_.setValue(actual_value, juce::sendNotificationAsync);
                }
            }
            if constexpr (kUseName) {
                label_.setVisible(true);
                label1_.setVisible(false);
                if constexpr (kUseSecondSlider) {
                    if (show_slider2_) {
                        label2_.setVisible(false);
                    }
                }
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (slider == &slider1_) {
                label1_.setText(getDisplayValue(slider1_), juce::dontSendNotification);
                display_.setSlider1Value(
                    static_cast<float>(slider1_.getNormalisableRange().convertTo0to1(slider1_.getValue())));
            }
            if constexpr (kUseSecondSlider) {
                if (slider == &slider2_) {
                    label2_.setText(getDisplayValue(slider2_), juce::dontSendNotification);
                    display_.setSlider2Value(
                        static_cast<float>(slider2_.getNormalisableRange().convertTo0to1(slider2_.getValue())));
                }
            }
        }

        void updateDragDistance() {
            int actual_drag_distance;
            if (is_shift_pressed_) {
                actual_drag_distance = juce::roundToInt(
                    static_cast<float>(drag_distance_) / base_.getSensitivity(SensitivityIdx::kMouseDragFine));
            } else {
                actual_drag_distance = juce::roundToInt(
                    static_cast<float>(drag_distance_) / base_.getSensitivity(SensitivityIdx::kMouseDrag));
            }
            actual_drag_distance = std::max(actual_drag_distance, 1);
            slider1_.setMouseDragSensitivity(actual_drag_distance);
            if constexpr (kUseSecondSlider) {
                slider2_.setMouseDragSensitivity(actual_drag_distance);
            }
        }
    };
}
