// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "float_pop_panel.hpp"
#include "BinaryData.h"

namespace {
    std::unique_ptr<juce::Drawable> createTextDrawable(const juce::String& text) {
        auto composite = std::make_unique<juce::DrawableComposite>();

        // 1. Add transparent background to define the bounds (24x24)
        auto bg = std::make_unique<juce::DrawableRectangle>();
        bg->setRectangle(juce::Rectangle<float>(0.0f, 0.0f, 24.0f, 24.0f));
        bg->setFill(juce::Colours::transparentBlack);
        bg->setStrokeFill(juce::Colours::transparentBlack);
        bg->setStrokeThickness(0.0f);
        bg->setBounds(0, 0, 24, 24);
        composite->addChildComponent(bg.release());

        // 2. Create the character path, centered and shrunk (using 2, 2, 20, 20 to scale)
        juce::Path path;
        juce::GlyphArrangement ga;
        ga.addFittedText(juce::Font(juce::FontOptions("Microsoft YaHei", 20.0f, juce::Font::bold)), text, 2.0f, 2.0f, 20.0f, 20.0f, juce::Justification::centred, 1);
        ga.createPath(path);

        auto drawablePath = std::make_unique<juce::DrawablePath>();
        drawablePath->setPath(path);
        drawablePath->setFill(juce::Colours::black);
        drawablePath->setBounds(0, 0, 24, 24);
        composite->addChildComponent(drawablePath.release());

        // Set the content area and bounding box of the composite
        composite->setContentArea(juce::Rectangle<float>(0.0f, 0.0f, 24.0f, 24.0f));
        composite->setBoundingBox(juce::Rectangle<float>(0.0f, 0.0f, 24.0f, 24.0f));

        return composite;
    }

    std::unique_ptr<juce::Drawable> createFilterIconDrawable(const juce::String& pathData, bool isStroke) {
        juce::String svg;
        if (isStroke) {
            svg = "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\">"
                  "<rect x=\"0\" y=\"0\" width=\"24\" height=\"24\" fill=\"none\"/>"
                  "<g transform=\"translate(4, 4) scale(0.0625)\">"
                  "<path d=\"" + pathData + "\" stroke=\"#000000\" stroke-width=\"32\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"
                  "</g>"
                  "</svg>";
        } else {
            svg = "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\">"
                  "<rect x=\"0\" y=\"0\" width=\"24\" height=\"24\" fill=\"none\"/>"
                  "<g transform=\"translate(4, 4) scale(0.0625)\">"
                  "<path d=\"" + pathData + "\" fill=\"#000000\"/>"
                  "</g>"
                  "</svg>";
        }
        auto xml = juce::XmlDocument::parse(svg);
        if (xml != nullptr) {
            return juce::Drawable::createFromSVG(*xml);
        }
        return nullptr;
    }
}

namespace zlpanel {
    FloatPopPanel::FloatPopPanel(PluginProcessor& p, zlgui::UIBase& base,
                                 const multilingual::TooltipHelper& tooltip_helper) :
        p_ref_(p), base_(base), updater_(),
        control_background_(base, .25f),
        bypass_drawable_(createTextDrawable(juce::String::fromUTF8("\xE5\x85\xB3"))),
        bypass_on_drawable_(createTextDrawable(juce::String::fromUTF8("\xE5\xBC\x80"))),
        bypass_button_(base, bypass_drawable_.get(), bypass_on_drawable_.get(),
                       tooltip_helper.getToolTipText(multilingual::kBandBypass)),
        solo_drawable_(createTextDrawable(juce::String::fromUTF8("\xE7\x8B\xAC"))),
        solo_button_(base, solo_drawable_.get(), solo_drawable_.get(),
                     tooltip_helper.getToolTipText(multilingual::kBandSolo)),
        dynamic_drawable_(createTextDrawable(juce::String::fromUTF8("\xE5\x8A\xA8"))),
        dynamic_button_(base, dynamic_drawable_.get(), dynamic_drawable_.get(),
                        tooltip_helper.getToolTipText(multilingual::kBandDynamic)),
        close_drawable_(createTextDrawable(juce::String::fromUTF8("\xE5\x88\xA0"))),
        close_button_(base, close_drawable_.get(), nullptr),
        ftype_box_([]() -> std::vector<std::unique_ptr<juce::Drawable>> {
            std::vector<std::unique_ptr<juce::Drawable>> icons;
            icons.emplace_back(createFilterIconDrawable("M201.077 113H245V143H201.077L127.544 215.669L127.5 215.624L127.456 215.669L53.9229 143H10V113H53.9229L127.456 40.3311L127.5 40.375L127.544 40.3311L201.077 113ZM81.4229 127.999L127.5 173.534L173.576 127.999L127.5 82.4648L81.4229 127.999Z", false));
            icons.emplace_back(createFilterIconDrawable("M16 184.5L126.5 74H245", true));
            icons.emplace_back(createFilterIconDrawable("M97.2471 49L101.469 52.6406L170.294 112H245V142H170.36L101.557 202.282L97.3135 206H10V176H86.0303L141.854 127.089L86.0967 79H10V49H97.2471Z", false));
            icons.emplace_back(createFilterIconDrawable("M245 79H168.903L113.145 127.089L168.97 176H245V206H157.687L153.443 202.282L84.6396 142H10V112H84.7061L153.531 52.6406L157.753 49H245V79Z", false));
            icons.emplace_back(createFilterIconDrawable("M245 184.5L134.5 74H16", true));
            icons.emplace_back(createFilterIconDrawable("M97.8818 76L102.275 80.3896L128.371 106.469L154.044 80.4619L158.448 76H245V106H170.989L149.592 127.676L170.93 149H245V179H158.509L154.115 174.61L128.516 149.027L102.347 175.538L97.9424 180H10V150H85.4014L107.295 127.82L85.4609 106H10V76H97.8818Z", false));
            icons.emplace_back(createFilterIconDrawable("M245 100L168.488 128L245 156V186L127.5 143L10 186V156L86.5107 128L10 100V70L127.5 113L245 70V100Z", false));
            return icons;
        }(), base, "", {}),
        lr_box_([]() -> std::vector<std::unique_ptr<juce::Drawable>> {
            std::vector<std::unique_ptr<juce::Drawable>> icons;
            icons.emplace_back(createTextDrawable(juce::String::fromUTF8("\xE5\x8F\x8C")));
            icons.emplace_back(createTextDrawable(juce::String::fromUTF8("\xE5\xB7\xA6")));
            icons.emplace_back(createTextDrawable(juce::String::fromUTF8("\xE5\x8F\xB3")));
            icons.emplace_back(createTextDrawable(juce::String::fromUTF8("\xE4\xB8\xAD")));
            icons.emplace_back(createTextDrawable(juce::String::fromUTF8("\xE4\xBE\xA7")));
            return icons;
        }(), base, "", {}),
        freq_slider_("", base),
        gain_slider_("", base),
        q_slider_("", base) {
        control_background_.setBufferedToImage(true);
        addAndMakeVisible(control_background_);

        close_button_.setBufferedToImage(true);
        addAndMakeVisible(close_button_);
        close_button_.getButton().onClick = [this]() {
            juce::Component::SafePointer<FloatPopPanel> safeThis(this);
            juce::MessageManager::callAsync([safeThis]() {
                if (safeThis == nullptr) return;
                auto* self = safeThis.getComponent();
                if (const auto c_band = self->base_.getSelectedBand(); c_band < zlp::kBandNum) {
                    band_helper::turnOffBand(self->p_ref_, c_band, self->base_.getSelectedBandSet());
                    const auto band1 = band_helper::findClosestBand<true>(self->p_ref_, c_band);
                    const auto band2 = band_helper::findClosestBand<false>(self->p_ref_, c_band);
                    if (band1 < zlp::kBandNum) {
                        self->base_.setSelectedBand(band1);
                    } else if (band2 < zlp::kBandNum) {
                        self->base_.setSelectedBand(band2);
                    } else {
                        self->base_.setSelectedBand(zlp::kBandNum);
                    }
                }
            });
        };

        solo_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        solo_button_.setBufferedToImage(true);
        addAndMakeVisible(solo_button_);
        solo_button_.getButton().onClick = [this]() {
            if (solo_button_.getButton().getToggleState()) {
                if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                    base_.setSoloWholeIdx(c_band);
                }
            } else {
                base_.setSoloWholeIdx(2 * zlp::kBandNum);
            }
        };

        dynamic_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        dynamic_button_.setBufferedToImage(true);
        addAndMakeVisible(dynamic_button_);
        dynamic_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                band_helper::turnOnOffDynamic(p_ref_, c_band, dynamic_button_.getToggleState());
            }
            if (!dynamic_button_.getToggleState() && base_.getSoloWholeIdx() < 2 * zlp::kBandNum) {
                base_.setSoloWholeIdx(2 * zlp::kBandNum);
            }
        };

        bypass_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        bypass_button_.setBufferedToImage(true);
        addAndMakeVisible(bypass_button_);
        bypass_button_.getButton().onClick = [this]() {
            if (const auto c_band = base_.getSelectedBand(); c_band < zlp::kBandNum) {
                updateValue(p_ref_.parameters_.getParameter(zlp::PFilterStatus::kID + std::to_string(c_band)),
                            bypass_button_.getToggleState() ? 1.f : .5f);
            }
        };

        const auto popup_option1 = juce::PopupMenu::Options().withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::upwards).withMinimumNumColumns(7);
        ftype_box_.getLAF().setOption(popup_option1);
        ftype_box_.setBufferedToImage(true);
        addAndMakeVisible(ftype_box_);

        const auto popup_option2 = juce::PopupMenu::Options().withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::upwards).withMinimumNumColumns(5);
        lr_box_.getLAF().setOption(popup_option2);
        lr_box_.setBufferedToImage(true);
        addAndMakeVisible(lr_box_);

        freq_slider_.setFontScale(1.05f);
        freq_slider_.getSlider().setDraggingEnabled(false);
        freq_slider_.getSlider().setSliderSnapsToMousePosition(false);
        freq_slider_.getSlider().setScrollWheelEnabled(false);
        freq_slider_.permitted_characters_ = "-0123456789.kK#ABCDEFGHzhZ ";
        freq_slider_.value_formatter_ = [](double v) {
            char buf[64];
            if (v < 1000.0) {
                snprintf(buf, sizeof(buf), "%.1f Hz", v);
            } else {
                snprintf(buf, sizeof(buf), "%.2f kHz", v / 1000.0);
            }
            return std::string(buf);
        };
        freq_slider_.string_formatter_ = [](const std::string& s) -> std::optional<double> {
            auto note_freq = freq_note::getFrequencyFromNote(s);
            if (note_freq != std::nullopt) {
                return note_freq;
            }
            std::string tmp = s;
            auto pos = tmp.find("kHz");
            if (pos != std::string::npos) tmp.erase(pos);
            pos = tmp.find("khz");
            if (pos != std::string::npos) tmp.erase(pos);
            pos = tmp.find("Hz");
            if (pos != std::string::npos) tmp.erase(pos);
            pos = tmp.find("hz");
            if (pos != std::string::npos) tmp.erase(pos);
            const double k = (tmp.find("k") != std::string::npos || tmp.find("K") != std::string::npos) ? 1000.0 : 1.0;
            pos = tmp.find("K");
            if (pos != std::string::npos) tmp.erase(pos);
            pos = tmp.find("k");
            if (pos != std::string::npos) tmp.erase(pos);
            try {
                return std::stod(tmp) * k;
            } catch (...) {
                return std::nullopt;
            }
        };
        freq_slider_.setBufferedToImage(true);
        addAndMakeVisible(freq_slider_);

        gain_slider_.setFontScale(1.05f);
        gain_slider_.getSlider().setDraggingEnabled(false);
        gain_slider_.getSlider().setSliderSnapsToMousePosition(false);
        gain_slider_.getSlider().setScrollWheelEnabled(false);
        gain_slider_.permitted_characters_ = "-0123456789.dDbB ";
        gain_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f dB", v);
            return std::string(buf);
        };
        gain_slider_.string_formatter_ = [](const std::string& s) -> std::optional<double> {
            std::string tmp = s;
            auto pos = tmp.find("dB");
            if (pos != std::string::npos) tmp.erase(pos);
            pos = tmp.find("db");
            if (pos != std::string::npos) tmp.erase(pos);
            try {
                return std::stod(tmp);
            } catch (...) {
                return std::nullopt;
            }
        };
        gain_slider_.setBufferedToImage(true);
        addAndMakeVisible(gain_slider_);

        q_slider_.setFontScale(1.05f);
        q_slider_.getSlider().setDraggingEnabled(false);
        q_slider_.getSlider().setSliderSnapsToMousePosition(false);
        q_slider_.getSlider().setScrollWheelEnabled(false);
        q_slider_.permitted_characters_ = "-0123456789.qQ: ";
        q_slider_.value_formatter_ = [](double v) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Q: %.3f", v);
            return std::string(buf);
        };
        q_slider_.string_formatter_ = [](const std::string& s) -> std::optional<double> {
            std::string tmp = s;
            auto pos = tmp.find("Q:");
            if (pos != std::string::npos) tmp.erase(pos, 2);
            pos = tmp.find("q:");
            if (pos != std::string::npos) tmp.erase(pos, 2);
            pos = tmp.find("Q");
            if (pos != std::string::npos) tmp.erase(pos, 1);
            pos = tmp.find("q");
            if (pos != std::string::npos) tmp.erase(pos, 1);
            try {
                return std::stod(tmp);
            } catch (...) {
                return std::nullopt;
            }
        };
        q_slider_.setBufferedToImage(true);
        addAndMakeVisible(q_slider_);

        base_.getSoloWholeIdxTree().addListener(this);
    }

    FloatPopPanel::~FloatPopPanel() {
        base_.getSoloWholeIdxTree().removeListener(this);
    }

    void FloatPopPanel::resized() {
        control_background_.setBounds(getLocalBounds());

        const auto font_size = base_.getFontSize();
        const auto button_size = getButtonSize(font_size);
        const auto h_gap = static_cast<int>(std::round(font_size * 0.12f));
        const auto outer_h_padding = static_cast<int>(std::round(font_size * 0.25f));
        const auto row_padding = static_cast<int>(std::round(font_size * 0.05f));
        const auto outer_v_padding = static_cast<int>(std::round(font_size * 0.2f));

        auto bound = getLocalBounds().reduced(outer_h_padding, outer_v_padding);

        // Row 1
        auto row1_bound = bound.removeFromTop(button_size);
        bypass_button_.setBounds(row1_bound.removeFromLeft(button_size));
        row1_bound.removeFromLeft(h_gap);
        close_button_.setBounds(row1_bound.removeFromRight(button_size));
        row1_bound.removeFromRight(h_gap);
        freq_slider_.setBounds(row1_bound);

        bound.removeFromTop(row_padding);

        // Row 2
        auto row2_bound = bound.removeFromTop(button_size);
        solo_button_.setBounds(row2_bound.removeFromLeft(button_size));
        row2_bound.removeFromLeft(h_gap);
        dynamic_button_.setBounds(row2_bound.removeFromRight(button_size));
        row2_bound.removeFromRight(h_gap);
        gain_slider_.setBounds(row2_bound);

        bound.removeFromTop(row_padding);

        // Row 3
        auto row3_bound = bound.removeFromTop(button_size);
        ftype_box_.setBounds(row3_bound.removeFromLeft(button_size).reduced(solo_button_.getButton().getEdgeIndent() / 2));
        row3_bound.removeFromLeft(h_gap);
        lr_box_.setBounds(row3_bound.removeFromRight(button_size).reduced(solo_button_.getButton().getEdgeIndent() / 2));
        row3_bound.removeFromRight(h_gap);
        q_slider_.setBounds(row3_bound);

        ideal_height_ = static_cast<float>(getIdealHeight());
        ideal_width_ = static_cast<float>(getIdealWidth());
    }

    void FloatPopPanel::updateBand() {
        if (base_.getSelectedBand() < zlp::kBandNum) {
            const auto band_s = std::to_string(base_.getSelectedBand());
            ftype_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                ftype_box_.getBox(), p_ref_.parameters_, zlp::PFilterType::kID + band_s, updater_);
            lr_attachment_ = std::make_unique<zlgui::attachment::ComboBoxAttachment<true>>(
                lr_box_.getBox(), p_ref_.parameters_, zlp::PLRMode::kID + band_s, updater_);
            freq_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                freq_slider_.getSlider(), p_ref_.parameters_, zlp::PFreq::kID + band_s, updater_);
            freq_attachment_->updateComponent();
            freq_slider_.updateDisplayValue();

            gain_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                gain_slider_.getSlider(), p_ref_.parameters_, zlp::PGain::kID + band_s, updater_);
            gain_attachment_->updateComponent();
            gain_slider_.updateDisplayValue();

            q_attachment_ = std::make_unique<zlgui::attachment::SliderAttachment<true>>(
                q_slider_.getSlider(), p_ref_.parameters_, zlp::PQ::kID + band_s, updater_);
            q_attachment_->updateComponent();
            q_slider_.updateDisplayValue();

            dynamic_attachment_ = std::make_unique<zlgui::attachment::ButtonAttachment<true>>(
                dynamic_button_.getButton(), p_ref_.parameters_, zlp::PDynamicON::kID + band_s, updater_,
                juce::dontSendNotification);

            filter_status_ptr_ = p_ref_.parameters_.getRawParameterValue(zlp::PFilterStatus::kID + band_s);
            repaintCallBackSlow();
        } else {
            ftype_attachment_.reset();
            lr_attachment_.reset();
            freq_attachment_.reset();
            gain_attachment_.reset();
            q_attachment_.reset();
            dynamic_attachment_.reset();
            filter_status_ptr_ = nullptr;
        }
        setVisible(base_.getSelectedBand() < zlp::kBandNum);
    }

    void FloatPopPanel::repaintCallBack() {
        updater_.updateComponents();
    }

    void FloatPopPanel::repaintCallBackSlow() {
        const auto band = base_.getSelectedBand();
        if (filter_status_ptr_ != nullptr && band < zlp::kBandNum) {
            const auto filter_on = filter_status_ptr_->load(std::memory_order::relaxed) > 1.5f;
            if (filter_on != bypass_button_.getToggleState()) {
                bypass_button_.getButton().setToggleState(filter_on, juce::dontSendNotification);
            }

            const auto band_s = std::to_string(band);
            if (auto* ftype_param = p_ref_.parameters_.getRawParameterValue(zlp::PFilterType::kID + band_s)) {
                const auto ftype = zlp::choiceIndexToFilterType(static_cast<int>(std::round(ftype_param->load(std::memory_order::relaxed))));
                const bool gain_enabled = (ftype == zldsp::filter::kPeak)
                    || (ftype == zldsp::filter::kLowShelf)
                    || (ftype == zldsp::filter::kHighShelf)
                    || (ftype == zldsp::filter::kTiltShelf);
                if (gain_slider_.isVisible() != gain_enabled) {
                    gain_slider_.setVisible(gain_enabled);
                }
                if (dynamic_button_.isVisible() != gain_enabled) {
                    dynamic_button_.setVisible(gain_enabled);
                }
            }
        }
    }

    int FloatPopPanel::getIdealWidth() const {
        const auto font_size = base_.getFontSize();
        const auto button_size = getButtonSize(font_size);
        const auto h_gap = static_cast<int>(std::round(font_size * 0.12f));
        const auto outer_h_padding = static_cast<int>(std::round(font_size * 0.25f));
        const auto middle_slider_width = static_cast<int>(std::round(font_size * 4.6f));
        return 2 * button_size + 2 * h_gap + middle_slider_width + 2 * outer_h_padding;
    }

    int FloatPopPanel::getIdealHeight() const {
        const auto font_size = base_.getFontSize();
        const auto button_size = getButtonSize(font_size);
        const auto row_padding = static_cast<int>(std::round(font_size * 0.05f));
        const auto outer_v_padding = static_cast<int>(std::round(font_size * 0.2f));
        return 3 * button_size + 2 * outer_v_padding + 2 * row_padding;
    }

    void FloatPopPanel::setTargetVisible(const bool is_target_visible) {
        if (is_target_visible != is_target_visible_) {
            is_target_visible_ = is_target_visible;
            updateTransformation();
        }
    }

    void FloatPopPanel::updatePosition(const juce::Point<float> position) {
        if (std::abs(position.x - position_.x) > .01f
            || std::abs(position.y - position_.y) > .01f) {
            position_ = position;
            updateTransformation();
        }
    }

    void FloatPopPanel::updateFloatingBound(const juce::Rectangle<float> bound) {
        const auto width = ideal_width_;
        const auto height = ideal_height_;
        const auto padding = base_.getFontSize() * kPaddingScale * 1.5f;

        upper_center_ = {width * .5f, -base_.getFontSize() - 20.f};
        lower_center_ = {width * .5f, height + base_.getFontSize() + 20.f};
        left_center_ = {-padding - 20.f, ideal_height_ * .5f};
        right_center_ = {width + padding + 20.f, ideal_height_ * .5f};

        x_min_ = width * .5f;
        x_mid_ = bound.getWidth() * .5f;
        x_max_ = bound.getWidth() - width * .5f;
        y_min_ = ideal_height_ * .5f;
        y_max_ = bound.getHeight() - height * .5f;

        y1_ = bound.getHeight() * .25f;
        y2_ = bound.getHeight() * .5f + .5f;
        y3_ = bound.getHeight() * .75f;

        updateTransformation();
    }

    void FloatPopPanel::updateTransformation() {
        if (is_target_visible_) {
            if (position_.x < x_mid_) {
                setTransform(juce::AffineTransform::translation(
                    position_.x - left_center_.x,
                    std::clamp(position_.y, y_min_, y_max_) - left_center_.y));
            } else {
                setTransform(juce::AffineTransform::translation(
                    position_.x - right_center_.x,
                    std::clamp(position_.y, y_min_, y_max_) - right_center_.y));
            }
        } else if (position_.y < y1_ || (position_.y > y2_ && position_.y < y3_)) {
            setTransform(juce::AffineTransform::translation(
                std::clamp(position_.x, x_min_, x_max_) - upper_center_.x,
                position_.y - upper_center_.y));
        } else {
            setTransform(juce::AffineTransform::translation(
                std::clamp(position_.x, x_min_, x_max_) - lower_center_.x,
                position_.y - lower_center_.y));
        }
    }

    void FloatPopPanel::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) {
        const auto solo_whole_idx = base_.getSoloWholeIdx();
        solo_button_.getButton().setToggleState(solo_whole_idx < zlp::kBandNum, juce::dontSendNotification);
    }

    void FloatPopPanel::showGainEditor() {
        if (gain_slider_.isVisible()) {
            gain_slider_.showEditor();
        }
    }
}
