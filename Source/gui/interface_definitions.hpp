// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../state/state_definitions.hpp"

namespace zlgui {
    enum ColourIdx {
        kTextColour,
        kBackgroundColour,
        kShadowColour,
        kGlowColour,
        kGridColour,
        kPreColour,
        kPostColour,
        kSideColour,
        kCollisionColour,
        kColourNum
    };

    enum SensitivityIdx {
        kMouseWheel,
        kMouseWheelFine,
        kMouseDrag,
        kMouseDragFine,
        kSensitivityNum
    };

    static constexpr std::array<std::string_view, kColourNum> kColourNames = zlstate::kColourNames;

    enum PanelSettingIdx {
        kUISettingPanel,
        kUISettingChanged,
        kCurveShouldTransparent,
        kOutputPanel,
        kAnalyzerPanel,
        kDynamicExtraPanel,
        kFFTFrozen,
        kPanelSettingNum
    };

    inline std::array kPanelSettingIdentifiers{
        juce::Identifier("ui_setting_panel"),
        juce::Identifier("ui_setting_changed"),
        juce::Identifier("curve_should_transparent"),
        juce::Identifier("output_panel"),
        juce::Identifier("analyzer_panel"),
        juce::Identifier("dynamic_extra_panel"),
        juce::Identifier("fft_frozen")
    };

    inline juce::Identifier kSoloIdentifier("solo_whole_idx");

    static constexpr size_t kColorMap1Size = 10;
    static constexpr size_t kColorMap2Size = 6;

    inline std::array kColourMaps = {
        std::vector<juce::Colour>{ // Default Light
            juce::Colour(90, 150, 210),
            juce::Colour(220, 120, 50),
            juce::Colour(80, 170, 100),
            juce::Colour(205, 80, 80),
            juce::Colour(145, 105, 185),
            juce::Colour(205, 100, 160),
            juce::Colour(200, 165, 45),
            juce::Colour(65, 165, 175),
            juce::Colour(65, 155, 125),
            juce::Colour(120, 130, 185)
        },
        std::vector<juce::Colour>{ // Default Dark
            juce::Colour(100, 165, 220),
            juce::Colour(235, 135, 65),
            juce::Colour(90, 185, 120),
            juce::Colour(220, 95, 95),
            juce::Colour(160, 120, 200),
            juce::Colour(220, 115, 175),
            juce::Colour(215, 180, 60),
            juce::Colour(80, 180, 190),
            juce::Colour(80, 170, 140),
            juce::Colour(135, 145, 200)
        },
        std::vector<juce::Colour>{ // Seaborn Normal Light
            juce::Colour(76, 114, 176),
            juce::Colour(221, 132, 82),
            juce::Colour(85, 168, 104),
            juce::Colour(196, 78, 82),
            juce::Colour(129, 114, 178),
            juce::Colour(147, 120, 96),
            juce::Colour(218, 139, 195),
            juce::Colour(140, 140, 140),
            juce::Colour(204, 185, 116),
            juce::Colour(100, 181, 205)
        },
        std::vector<juce::Colour>{ // Seaborn Normal Dark
            juce::Colour(90, 130, 190),
            juce::Colour(230, 145, 95),
            juce::Colour(100, 185, 120),
            juce::Colour(210, 95, 100),
            juce::Colour(145, 130, 195),
            juce::Colour(160, 135, 110),
            juce::Colour(225, 150, 205),
            juce::Colour(155, 155, 155),
            juce::Colour(215, 195, 130),
            juce::Colour(115, 195, 220)
        },
        std::vector<juce::Colour>{ // Seaborn Bright Light
            juce::Colour(0, 100, 250),
            juce::Colour(245, 120, 0),
            juce::Colour(0, 200, 80),
            juce::Colour(230, 30, 40),
            juce::Colour(130, 50, 220),
            juce::Colour(240, 80, 180),
            juce::Colour(240, 190, 0),
            juce::Colour(0, 210, 240),
            juce::Colour(0, 170, 150),
            juce::Colour(150, 80, 60)
        },
        std::vector<juce::Colour>{ // Seaborn Bright Dark
            juce::Colour(50, 140, 255),
            juce::Colour(255, 140, 30),
            juce::Colour(40, 220, 110),
            juce::Colour(255, 60, 70),
            juce::Colour(160, 80, 255),
            juce::Colour(255, 110, 200),
            juce::Colour(255, 210, 30),
            juce::Colour(30, 230, 255),
            juce::Colour(30, 195, 170),
            juce::Colour(180, 110, 90)
        }
    };

    static constexpr float kFontTiny = 0.5f;
    static constexpr float kFontSmall = 0.75f;
    static constexpr float kFontNormal = 1.0f;
    static constexpr float kFontLarge = 1.25f;
    static constexpr float kFontHuge = 1.5f;
    static constexpr float kFontHuge2 = 3.0f;
    static constexpr float kFontHuge3 = 4.5f;

    struct FillRoundedShadowRectangleArgs {
        float blur_radius = 0.5f;
        bool curve_top_left = true, curve_top_right = true, curve_bottom_left = true, curve_bottom_right = true;
        bool fit = true, flip = false;
        bool draw_bright = true, draw_dark = true, draw_main = true;
        juce::Colour main_colour = juce::Colours::white.withAlpha(0.f);
        juce::Colour dark_shadow_color = juce::Colours::white.withAlpha(0.f);
        juce::Colour bright_shadow_color = juce::Colours::white.withAlpha(0.f);
        bool change_main = false, change_dark = false, change_bright = false;
    };

    struct FillShadowEllipseArgs {
        float blur_radius = 0.5f;
        bool fit = true, flip = false;
        bool draw_bright = true, draw_dark = true;
        juce::Colour main_colour = juce::Colours::white.withAlpha(0.f);
        juce::Colour dark_shadow_color = juce::Colours::white.withAlpha(0.f);
        juce::Colour bright_shadow_color = juce::Colours::white.withAlpha(0.f);
        bool change_main = false, change_dark = false, change_bright = false;
    };

    inline std::string formatFloat(const float x, const int precision) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, precision)) << x;
        return stream.str();
    }

    inline std::string fixFormatFloat(const float x, const int length) {
        auto y = std::abs(x);
        if (y < 10) {
            return formatFloat(x, length - 1);
        } else if (y < 100) {
            return formatFloat(x, length - 2);
        } else if (y < 1000) {
            return formatFloat(x, length - 3);
        } else if (y < 10000) {
            return formatFloat(x, length - 4);
        } else {
            return formatFloat(x, length - 5);
        }
    }

    class UIBase {
    public:
        juce::ReferenceCountedObjectPtr<juce::Typeface> font_;

        explicit UIBase(juce::AudioProcessorValueTreeState& apvts) :
            state(apvts) {
            loadFromAPVTS();
        }

        void setFontMode(const size_t font_mode) {
            font_mode_ = font_mode;
        }

        size_t getFontMode() const {
            return font_mode_;
        }

        void setFontScale(const float font_scale) {
            font_scale_ = font_scale;
        }

        float getFontScale() const {
            return font_scale_;
        }

        void setStaticFontSize(const float static_font_size) {
            static_font_size_ = static_font_size;
        }

        float getStaticFontSize() const {
            return static_font_size_;
        }

        void setFontSize(const float font_size) {
            font_size_ = font_size;
        }

        float getFontSize() const {
            return font_size_;
        }

        juce::Colour getTextColour() const {
            return custom_colours_[static_cast<size_t>(kTextColour)];
        }

        juce::Colour getTextInactiveColour() const {
            return getTextColour().withAlpha(0.5f);
        }

        juce::Colour getTextHideColour() const {
            return getTextColour().withAlpha(0.25f);
        }

        juce::Colour getBackgroundColour() const {
            return custom_colours_[static_cast<size_t>(kBackgroundColour)];
        }

        juce::Colour getBackgroundInactiveColour() const { return getBackgroundColour().withAlpha(0.8f); }

        juce::Colour getBackgroundHideColour() const { return getBackgroundColour().withAlpha(0.5f); }

        juce::Colour getColourBlendedWithBackground(const juce::Colour colour, const float alpha) const {
            const auto background_color = getBackgroundColour();
            return juce::Colour::fromFloatRGBA(
                alpha * colour.getFloatRed() + (1.f - alpha) * background_color.getFloatRed(),
                alpha * colour.getFloatGreen() + (1.f - alpha) * background_color.getFloatGreen(),
                alpha * colour.getFloatBlue() + (1.f - alpha) * background_color.getFloatBlue(),
                1.f);
        }

        juce::Colour getDarkShadowColour() const {
            return custom_colours_[static_cast<size_t>(kShadowColour)];
        }

        juce::Colour getBrightShadowColour() const {
            return custom_colours_[static_cast<size_t>(kGlowColour)];
        }

        juce::Colour getColourMap1(const size_t idx) const {
            return kColourMaps[colour_map1_idx_][idx % kColourMaps[colour_map1_idx_].size()];
        }

        juce::Colour getColourMap2(const size_t idx) const {
            return kColourMaps[colour_map2_idx_][idx % kColourMaps[colour_map2_idx_].size()];
        }

        static juce::Rectangle<float> getRoundedShadowRectangleArea(juce::Rectangle<float> box_bounds,
                                                                    float corner_size,
                                                                    const FillRoundedShadowRectangleArgs& margs);

        juce::Rectangle<float> fillRoundedShadowRectangle(juce::Graphics& g,
                                                          juce::Rectangle<float> box_bounds,
                                                          float corner_size,
                                                          const FillRoundedShadowRectangleArgs& margs) const;

        juce::Rectangle<float> fillRoundedInnerShadowRectangle(juce::Graphics& g,
                                                               juce::Rectangle<float> box_bounds,
                                                               float corner_size,
                                                               const FillRoundedShadowRectangleArgs& margs) const;

        static juce::Rectangle<float> getShadowEllipseArea(juce::Rectangle<float> box_bounds,
                                                           float corner_size,
                                                           const FillShadowEllipseArgs& margs);

        juce::Rectangle<float> drawShadowEllipse(juce::Graphics& g,
                                                 juce::Rectangle<float> box_bounds,
                                                 float corner_size,
                                                 const FillShadowEllipseArgs& margs) const;

        static juce::Rectangle<float> getInnerShadowEllipseArea(juce::Rectangle<float> box_bounds,
                                                                float corner_size,
                                                                const FillShadowEllipseArgs& margs);

        juce::Rectangle<float> drawInnerShadowEllipse(juce::Graphics& g,
                                                      juce::Rectangle<float> box_bounds,
                                                      float corner_size,
                                                      const FillShadowEllipseArgs& margs) const;

        juce::Colour getColourByIdx(ColourIdx idx) const {
            return custom_colours_[static_cast<size_t>(idx)];
        }

        void setColourByIdx(const ColourIdx idx, const juce::Colour colour) {
            custom_colours_[static_cast<size_t>(idx)] = colour;
        }

        float getSensitivity(const SensitivityIdx idx) const {
            return wheel_sensitivity_[static_cast<size_t>(idx)];
        }

        void setSensitivity(const float v, const SensitivityIdx idx) {
            wheel_sensitivity_[static_cast<size_t>(idx)] = v;
        }

        size_t getRotaryStyleID() const {
            return rotary_style_id_;
        }

        juce::Slider::SliderStyle getRotaryStyle() const {
            return zlstate::PRotaryStyle::styles[rotary_style_id_];
        }

        void setRotaryStyleID(const size_t x) {
            rotary_style_id_ = x;
        }

        float getRotaryDragSensitivity() const {
            return rotary_drag_sensitivity_;
        }

        void setRotaryDragSensitivity(const float x) {
            rotary_drag_sensitivity_ = x;
        }

        size_t getRefreshRateID() const {
            return refresh_rate_id_.load(std::memory_order::relaxed);
        }

        void setRefreshRateID(const size_t x) {
            refresh_rate_id_.store(x, std::memory_order::relaxed);
        }

        float getFFTExtraTilt() const {
            return fft_extra_tilt_.load(std::memory_order::relaxed);
        }

        void setFFTExtraTilt(const float x) {
            fft_extra_tilt_.store(x, std::memory_order::relaxed);
        }

        float getFFTExtraSpeed() const {
            return fft_extra_speed_.load(std::memory_order::relaxed);
        }

        void setFFTExtraSpeed(const float x) {
            fft_extra_speed_.store(x, std::memory_order::relaxed);
        }

        float getSingleEQCurveThickness() const {
            return single_eq_curve_thickness_.load(std::memory_order::relaxed);
        }

        void setSingleEQCurveThickness(const float x) {
            single_eq_curve_thickness_.store(x, std::memory_order::relaxed);
        }

        float getSumEQCurveThickness() const {
            return single_eq_curve_thickness_.load(std::memory_order::relaxed);
        }

        void setSumEQCurveThickness(const float x) {
            single_eq_curve_thickness_.store(x, std::memory_order::relaxed);
        }

        size_t getTooltipLangID() const {
            return tooltip_lang_id_;
        }

        void setTooltipLandID(const size_t x) {
            tooltip_lang_id_ = x;
        }

        void loadFromAPVTS();

        void saveToAPVTS() const;

        bool getIsMouseWheelShiftReverse() const { return is_mouse_wheel_shift_reverse_.load(); }

        void setIsMouseWheelShiftReverse(const bool x) { is_mouse_wheel_shift_reverse_.store(x); }

        bool getIsSliderDoubleClickOpenEditor() const { return is_slider_double_click_open_editor_.load(); }

        void setIsSliderDoubleClickOpenEditor(const bool x) { is_slider_double_click_open_editor_.store(x); }

        size_t getCMap1Idx() const { return colour_map1_idx_; }

        void setCMap1Idx(const size_t x) { colour_map1_idx_ = x; }

        size_t getCMap2Idx() const { return colour_map2_idx_; }

        void setCMap2Idx(const size_t x) { colour_map2_idx_ = x; }

        void setIsEditorShowing(const bool x) { is_editor_showing_ = x; }

        bool getIsEditorShowing() const { return is_editor_showing_; }

        juce::ValueTree& getPanelValueTree() { return panel_value_tree_; }

        void setWindowSizeFix(const bool f) { window_size_fix_ = f; }

        bool getWindowSizeFix() const { return window_size_fix_; }

        bool isPanelIdentifier(const PanelSettingIdx idx, const juce::Identifier& identifier) const {
            return identifier == kPanelSettingIdentifiers[static_cast<size_t>(idx)];
        }

        juce::var getPanelProperty(const PanelSettingIdx idx) const {
            return panel_value_tree_.getProperty(kPanelSettingIdentifiers[static_cast<size_t>(idx)]);
        }

        void setPanelProperty(const PanelSettingIdx idx, const juce::var& v) {
            panel_value_tree_.setProperty(kPanelSettingIdentifiers[idx], v, nullptr);
        }

        juce::ValueTree& getSoloWholeIdxTree() { return solo_whole_idx_tree_; }

        size_t getSoloWholeIdx() const {
            return static_cast<size_t>(static_cast<int>(solo_whole_idx_tree_.getProperty(kSoloIdentifier)));
        }

        void setSoloWholeIdx(const size_t idx) {
            solo_whole_idx_tree_.setProperty(kSoloIdentifier, static_cast<int>(idx), nullptr);
        }

        size_t getSelectedBand() const { return selected_band_; }

        void setSelectedBand(const size_t x) { selected_band_ = x; }

        juce::SelectedItemSet<size_t>& getSelectedBandSet() { return selected_band_set_; }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::ValueTree panel_value_tree_{"panel_setting_tree"};
        juce::ValueTree solo_whole_idx_tree_{"solo_whole_idx_tree"};

        float font_size_{0.f};
        size_t font_mode_{0};
        float font_scale_{9.f};
        float static_font_size_{0.f};
        bool window_size_fix_{false};
        std::array<juce::Colour, kColourNum> custom_colours_;
        std::array<float, kSensitivityNum> wheel_sensitivity_{1.f, 0.12f, 1.f, .25f};
        size_t rotary_style_id_{0};
        std::atomic<size_t> refresh_rate_id_{2};
        float rotary_drag_sensitivity_{1.f};
        std::atomic<float> fft_extra_tilt_{0.f}, fft_extra_speed_{1.f};
        std::atomic<float> single_eq_curve_thickness_{1.f}, sum_eq_curve_thickness_{1.f};
        size_t tooltip_lang_id_{1};

        std::atomic<bool> is_mouse_wheel_shift_reverse_{false};
        std::atomic<bool> is_slider_double_click_open_editor_{false};
        bool is_editor_showing_{false};

        size_t colour_map1_idx_{zlstate::PColourMapIdx::kDefaultDark};
        size_t colour_map2_idx_{zlstate::PColourMapIdx::kSeabornBrightDark};

        size_t selected_band_{zlstate::kBandNum};
        juce::SelectedItemSet<size_t> selected_band_set_;

        float loadPara(const std::string& id) const {
            return state.getRawParameterValue(id)->load(std::memory_order::relaxed);
        }

        void savePara(const std::string& id, const float x) const {
            const auto para = state.getParameter(id);
            para->beginChangeGesture();
            para->setValueNotifyingHost(para->convertTo0to1(x));
            para->endChangeGesture();
        }
    };
}
