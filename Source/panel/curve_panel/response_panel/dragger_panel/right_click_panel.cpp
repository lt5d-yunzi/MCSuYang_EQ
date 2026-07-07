// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "right_click_panel.hpp"

namespace zlpanel {
    RightClickPanel::RightClickPanel(PluginProcessor& p, zlgui::UIBase& base,
                                     const multilingual::TooltipHelper&) :
        p_ref_(p), base_(base),
        items_set_(base.getSelectedBandSet()),
        control_background_(base, .25f),
        lr_split_button_(base, juce::String::fromUTF8("L/R \xe6\x8b\x86\xe5\x88\x86")),
        ms_split_button_(base, juce::String::fromUTF8("M/S \xe6\x8b\x86\xe5\x88\x86")),
        slope_box_(juce::StringArray{
                       "6 dB/oct", "12 dB/oct", "24 dB/oct", "36 dB/oct",
                       "48 dB/oct", "72 dB/oct", "96 dB/oct"
                   }, base),
        menu_laf_(base) {
        control_background_.setBufferedToImage(true);
        addAndMakeVisible(control_background_);
        addAndMakeVisible(mouse_event_eater_);

        lr_split_button_.getButton().onClick = [this]() {
            splitBand(zlp::FilterStereo::kLeft, zlp::FilterStereo::kRight);
        };

        ms_split_button_.getButton().onClick = [this]() {
            splitBand(zlp::FilterStereo::kMid, zlp::FilterStereo::kSide);
        };

        for (auto& b : {&lr_split_button_, &ms_split_button_}) {
            addAndMakeVisible(b);
        }

        addAndMakeVisible(slope_box_);

        lr_split_button_.getButton().setLookAndFeel(&menu_laf_);
        ms_split_button_.getButton().setLookAndFeel(&menu_laf_);
        slope_box_.getBox().setLookAndFeel(&menu_laf_);

        setInterceptsMouseClicks(false, true);
    }

    int RightClickPanel::getIdealWidth() const {
        const auto font_size = base_.getFontSize();
        const auto padding = getPaddingSize(font_size);
        const auto slider_width = getSliderWidth(font_size);

        return 6 * padding + slider_width;
    }

    int RightClickPanel::getIdealHeight() const {
        const auto font_size = base_.getFontSize();
        const auto padding = getPaddingSize(font_size);
        const auto button_height = getButtonSize(font_size);
        const auto gap = static_cast<int>(std::round(button_height * 0.25f));

        return 6 * padding + 3 * button_height + 2 * gap;
    }

    void RightClickPanel::paint(juce::Graphics& g) {
        juce::ignoreUnused(g);
        updater_.updateComponents();
    }

    void RightClickPanel::resized() {
        const auto font_size = base_.getFontSize();
        const auto padding = getPaddingSize(font_size);
        const auto button_height = getButtonSize(font_size);
        const auto gap = static_cast<int>(std::round(button_height * 0.25f));

        auto bound = getLocalBounds();
        control_background_.setBounds(bound);
        bound.reduce(3 * padding, 3 * padding);
        mouse_event_eater_.setBounds(bound);

        lr_split_button_.setBounds(bound.removeFromTop(button_height));
        bound.removeFromTop(gap);
        
        ms_split_button_.setBounds(bound.removeFromTop(button_height));
        bound.removeFromTop(gap);

        auto row3 = bound.removeFromTop(button_height);
        slope_box_.setBounds(row3.toNearestInt());
        menu_laf_.setItemSize(row3.getWidth(), row3.getHeight());
    }

    void RightClickPanel::setPosition(juce::Point<float> pos) {
        const auto parent_width = safe_area_.getWidth();
        const auto parent_height = safe_area_.getHeight();

        const auto width = static_cast<float>(getWidth());
        const auto height = static_cast<float>(getHeight());

        if (pos.x + width > parent_width || pos.y + height > parent_height) {
            setTransform(juce::AffineTransform::translation(pos.x - width, pos.y - height));
        } else {
            setTransform(juce::AffineTransform::translation(pos.x, pos.y));
        }
    }

    void RightClickPanel::setSafeArea(juce::Rectangle<float> area) {
        safe_area_ = area;
    }

    void RightClickPanel::visibilityChanged() {
        if (isVisible()) {
            const auto band = base_.getSelectedBand();
            const auto f = band < zlp::kBandNum;
            for (auto& b : {&lr_split_button_, &ms_split_button_}) {
                b->setInterceptsMouseClicks(f, f);
                b->setAlpha(f ? 1.f : .25f);
            }
            if (f) {
                const auto band_s = std::to_string(band);
                slope_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                    slope_box_.getBox(), p_ref_.parameters_, zlp::POrder::kID + band_s, updater_);
                slope_attachment_->updateComponent();

                const auto ftype_idx = static_cast<int>(std::round(
                    p_ref_.parameters_.getRawParameterValue(zlp::PFilterType::kID + band_s)->load(std::memory_order::relaxed)));
                const auto ftype = zlp::choiceIndexToFilterType(ftype_idx);
                const auto slope_6_disabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kBandPass)
                    || (ftype == zldsp::filter::kNotch);

                if (slope_6_disabled && slope_box_.getBox().getSelectedId() == 1) {
                    slope_box_.getBox().setSelectedId(2, juce::sendNotificationSync);
                }
                slope_box_.getBox().setItemEnabled(1, !slope_6_disabled);

                const auto is_flat_tilt = (ftype == static_cast<int>(zldsp::filter::kFlatTilt));
                slope_box_.setEditable(!is_flat_tilt);
                slope_box_.setVisible(true);
            } else {
                slope_attachment_.reset();
                slope_box_.setVisible(false);
            }
        } else {
            slope_attachment_.reset();
        }
    }

    void RightClickPanel::splitBand(zlp::FilterStereo stereo1, zlp::FilterStereo stereo2) {
        const auto band1 = base_.getSelectedBand();
        if (band1 == zlp::kBandNum) {
            return;
        }
        const auto band2 = band_helper::findOffBand(p_ref_);
        if (band2 == zlp::kBandNum) {
            return;
        }
        const auto band1_s = std::to_string(band1);
        const auto band2_s = std::to_string(band2);
        for (auto& ID : kIDs) {
            if (ID != zlp::PLRMode::kID) {
                auto* para1 = p_ref_.parameters_.getParameter(ID + band1_s);
                auto* para2 = p_ref_.parameters_.getParameter(ID + band2_s);
                updateValue(para2, para1->getValue());
            }
        }
        auto* para1 = p_ref_.parameters_.getParameter(zlp::PLRMode::kID + band1_s);
        auto* para2 = p_ref_.parameters_.getParameter(zlp::PLRMode::kID + band2_s);
        if (std::abs(para1->convertFrom0to1(para1->getValue()) - static_cast<float>(stereo1)) < .1f) {
            updateValue(para2, para2->convertTo0to1(static_cast<float>(stereo2)));
        } else if (std::abs(para1->convertFrom0to1(para1->getValue()) - static_cast<float>(stereo2)) < .1f) {
            updateValue(para2, para2->convertTo0to1(static_cast<float>(stereo1)));
        } else {
            updateValue(para1, para1->convertTo0to1(static_cast<float>(stereo1)));
            updateValue(para2, para2->convertTo0to1(static_cast<float>(stereo2)));
        }
        base_.setSelectedBand(band2);
        setVisible(false);
    }

    void RightClickPanel::RightClickMenuLAF::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                                 const juce::Colour&, bool highlight, bool down) {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto cornerSize = 4.0f;

        if (!button.isEnabled()) {
            g.setColour(juce::Colour(0, 0, 0).withAlpha(0.05f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.1f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            return;
        }

        if (down) {
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.08f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.65f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        } else if (highlight) {
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.05f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.50f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        } else {
            g.setColour(juce::Colour(0, 0, 0).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.35f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }

    void RightClickPanel::RightClickMenuLAF::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                                           const bool highlight, const bool down) {
        if (!button.isEnabled()) {
            g.setColour(base_ref_.getTextColour().withAlpha(0.2f));
        } else if (down) {
            g.setColour(base_ref_.getTextColour());
        } else if (highlight) {
            g.setColour(base_ref_.getTextColour().withAlpha(.85f));
        } else {
            g.setColour(base_ref_.getTextColour().withAlpha(.7f));
        }
        g.setFont(base_ref_.getFontSize() * 1.5f);
        
        auto bounds = button.getLocalBounds().toFloat();
        bounds.removeFromLeft(base_ref_.getFontSize() * 0.5f);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centredLeft);
    }

    void RightClickPanel::RightClickMenuLAF::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int,
                                                         juce::ComboBox& box) {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
        auto cornerSize = 4.0f;
        
        bool highlight = box.isMouseOver() || box.isPopupActive();
        bool down = isButtonDown || box.isPopupActive();

        if (!box.isEnabled()) {
            g.setColour(juce::Colour(0, 0, 0).withAlpha(0.05f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.1f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            return;
        }

        if (down) {
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.08f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.65f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        } else if (highlight) {
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.05f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(base_ref_.getTextColour().withAlpha(0.50f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        } else {
            g.setColour(juce::Colour(0, 0, 0).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(juce::Colour(255, 255, 255).withAlpha(0.35f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }

    void RightClickPanel::RightClickMenuLAF::drawLabel(juce::Graphics& g, juce::Label& label) {
        g.setColour(base_ref_.getTextColour());
        g.setFont(base_ref_.getFontSize() * getFontScale());
        
        auto bounds = label.getLocalBounds().toFloat();
        bounds.removeFromLeft(base_ref_.getFontSize() * 0.5f);
        g.drawText(label.getText(), bounds, juce::Justification::centredLeft);
    }
}
