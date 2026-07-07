// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "main_panel.hpp"
#include "BinaryData.h"

namespace zlpanel {
    MainPanel::MainPanel(PluginProcessor& p, zlgui::UIBase& base, const multilingual::TooltipLanguage language) :
        p_ref_(p), base_(base),
        tooltip_helper_(language),
        refresh_handler_(zlstate::PTargetRefreshSpeed::kRates[base_.getRefreshRateID()]),
        curve_panel_(p, base, tooltip_helper_),
        control_panel_(p, base, tooltip_helper_),
        tooltip_laf_(base_),
        gain_slider_(base),
        fader_laf_(base),
        updater_(),
        gain_attach_(gain_slider_, p.parameters_, zlp::POutputGain::kID, updater_),
        phase_drawable_(juce::Drawable::createFromImageData(BinaryData::phase_svg, BinaryData::phase_svgSize)),
        phase_button_(base, phase_drawable_.get(), phase_drawable_.get(), tooltip_helper_.getToolTipText(multilingual::kPhaseFlip)),
        phase_attach_(phase_button_.getButton(), p.parameters_, zlp::PPhaseFlip::kID, updater_),
        auto_gain_btn_(base, "A", tooltip_helper_.getToolTipText(multilingual::kAutoGC)),
        auto_gain_attach_(auto_gain_btn_.getButton(), p.parameters_, zlp::PAutoGain::kID, updater_),
        preset_box_(juce::StringArray(), base, juce::String::fromUTF8("管理并选择预置参数")),
        a_btn_(base, "A", juce::String::fromUTF8("切换到 A 状态")),
        ab_copy_btn_(base, juce::String::fromUTF8("\xe2\x86\x92"), juce::String::fromUTF8("复制当前参数到另一状态")),
        b_btn_(base, "B", juce::String::fromUTF8("切换到 B 状态")),
        fft_collision_btn_(base, juce::String::fromUTF8("\xe9\x87\x8d\xe7\x82\xb9"), tooltip_helper_.getToolTipText(multilingual::kFFTCollision)),
        fft_collision_attach_(fft_collision_btn_.getButton(), p.parameters_NA_, zlstate::PCollisionON::kID, updater_),
        fft_pre_btn_(base, juce::String::fromUTF8("\xe5\x89\x8d"), tooltip_helper_.getToolTipText(multilingual::kFFTPre)),
        fft_pre_attach_(fft_pre_btn_.getButton(), p.parameters_NA_, zlstate::PFFTPreON::kID, updater_),
        fft_post_btn_(base, juce::String::fromUTF8("\xe5\x90\x8e"), tooltip_helper_.getToolTipText(multilingual::kFFTPost)),
        fft_post_attach_(fft_post_btn_.getButton(), p.parameters_NA_, zlstate::PFFTPostON::kID, updater_) {
        juce::ignoreUnused(base_);

        base_.getPanelValueTree().addListener(this);

        startTimerHz(1);

        gain_slider_.setLookAndFeel(&fader_laf_);
        gain_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        gain_slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        gain_slider_.setTooltip(tooltip_helper_.getToolTipText(multilingual::kOutputGain));
        gain_slider_.setBufferedToImage(true);
        gain_slider_.setTrackClickEnabled(false);
        gain_slider_.setSliderSnapsToMousePosition(false);
        auto* output_gain_param = p.parameters_.getParameter(zlp::POutputGain::kID);
        gain_slider_.setDoubleClickReturnValue(true, output_gain_param->convertFrom0to1(output_gain_param->getDefaultValue()));
        addAndMakeVisible(gain_slider_);

        gain_title_label_.setText(juce::String::fromUTF8("\xe9\x9f\xb3\xe9\x87\x8f"), juce::dontSendNotification);
        gain_title_label_.setJustificationType(juce::Justification::centredRight);
        gain_title_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
        addAndMakeVisible(gain_title_label_);

        gain_value_label_.setText("+0.0 dB", juce::dontSendNotification);
        gain_value_label_.setJustificationType(juce::Justification::centredLeft);
        gain_value_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
        gain_value_label_.setEditable(false, true, false);
        gain_value_label_.addListener(this);
        addAndMakeVisible(gain_value_label_);

        gain_slider_.onValueChange = [this]() {
            if (gain_value_label_.getCurrentTextEditor() == nullptr) {
                gain_value_label_.setText(gain_slider_.getTextFromValue(gain_slider_.getValue()), juce::dontSendNotification);
            }
        };
        gain_value_label_.setText(gain_slider_.getTextFromValue(gain_slider_.getValue()), juce::dontSendNotification);

        phase_button_.setImageAlpha(.5f, .75f, 1.f, 1.f);
        phase_button_.setBufferedToImage(true);
        addAndMakeVisible(phase_button_);

        auto_gain_btn_.getLAF().setFontScale(1.05f);
        auto_gain_btn_.getLAF().setJustification(juce::Justification::centred);
        auto_gain_btn_.getButton().setToggleable(true);
        auto_gain_btn_.getButton().setClickingTogglesState(true);
        auto_gain_btn_.setBufferedToImage(true);
        addAndMakeVisible(auto_gain_btn_);

        preset_box_.getLAF().setFontScale(0.95f);
        preset_box_.getLAF().setLabelJustification(juce::Justification::centred);
        preset_box_.getBox().setScrollWheelEnabled(false);
        preset_box_.setBufferedToImage(true);
        addAndMakeVisible(preset_box_);

        refreshPresetsList();
        preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
        preset_box_.getBox().addListener(this);

        a_btn_.getLAF().setFontScale(0.95f);
        a_btn_.getLAF().setJustification(juce::Justification::centred);
        a_btn_.getButton().setToggleable(true);
        a_btn_.getButton().setClickingTogglesState(false);
        a_btn_.setBufferedToImage(true);
        addAndMakeVisible(a_btn_);

        b_btn_.getLAF().setFontScale(0.95f);
        b_btn_.getLAF().setJustification(juce::Justification::centred);
        b_btn_.getButton().setToggleable(true);
        b_btn_.getButton().setClickingTogglesState(false);
        b_btn_.setBufferedToImage(true);
        addAndMakeVisible(b_btn_);

        ab_copy_btn_.getLAF().setFontScale(0.95f);
        ab_copy_btn_.getLAF().setJustification(juce::Justification::centred);
        ab_copy_btn_.getButton().setToggleable(false);
        ab_copy_btn_.setBufferedToImage(true);
        addAndMakeVisible(ab_copy_btn_);

        a_btn_.getButton().onClick = [this]() {
            if (p_ref_.isStateBActive()) {
                p_ref_.toggleABState();
                updateABButtonStates();
            }
        };

        b_btn_.getButton().onClick = [this]() {
            if (!p_ref_.isStateBActive()) {
                p_ref_.toggleABState();
                updateABButtonStates();
            }
        };

        ab_copy_btn_.getButton().onClick = [this]() {
            p_ref_.copyActiveStateToInactive();
        };

        updateABButtonStates();

        fft_title_label_.setText(juce::String::fromUTF8("\xe9\xa2\x91\xe8\xb0\xb1"), juce::dontSendNotification);
        fft_title_label_.setJustificationType(juce::Justification::centredRight);
        fft_title_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
        addAndMakeVisible(fft_title_label_);

        for (auto* b : {&fft_collision_btn_, &fft_pre_btn_, &fft_post_btn_}) {
            b->getLAF().setFontScale(0.95f);
            b->getLAF().setJustification(juce::Justification::centred);
            b->getButton().setToggleable(true);
            b->getButton().setClickingTogglesState(true);
            b->setBufferedToImage(true);
            addAndMakeVisible(b);
        }

        addAndMakeVisible(curve_panel_);
        addAndMakeVisible(control_panel_);
        control_panel_.updateBand();

        bottom_left_label_.setText(juce::String::fromUTF8("\xe6\xa2\xb5\xe8\x95\x8a\xe5\xa3\xb0\xe5\xad\xa6"), juce::dontSendNotification);
        bottom_left_label_.setJustificationType(juce::Justification::centredLeft);
        bottom_left_label_.setColour(juce::Label::textColourId, base_.getTextColour().withAlpha(0.85f));
        bottom_left_label_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        bottom_left_label_.addMouseListener(this, false);
        addAndMakeVisible(bottom_left_label_);
    }

    MainPanel::~MainPanel() {
        base_.getPanelValueTree().removeListener(this);
        stopTimer();
        gain_slider_.setLookAndFeel(nullptr);
        preset_box_.getBox().removeListener(this);
    }

    void MainPanel::paint(juce::Graphics& g) {
        auto bounds = getLocalBounds();
        auto bottom_bar = bounds.removeFromBottom(40);

        // Fill bottom bar background
        g.setColour(juce::Colour(10, 18, 22).withAlpha(0.95f));
        g.fillRect(bottom_bar);

        // Draw top divider line of the bottom bar
        g.setColour(juce::Colour(23, 198, 197).withAlpha(0.4f)); // glowing teal/cyan line
        g.drawHorizontalLine(bottom_bar.getY(), 0.0f, static_cast<float>(bounds.getWidth()));
    }

    void MainPanel::resized() {
        auto bound = getLocalBounds();

        // set actual width/height
        {
            const auto height = static_cast<float>(bound.getHeight());
            const auto width = static_cast<float>(bound.getWidth());
            if (height < width * 0.575f) {
                bound.setHeight(static_cast<int>(std::ceil(width * .575f)));
            } else if (height > width * 1.f) {
                bound.setWidth(static_cast<int>(std::ceil(height * 1.f)));
            }
        }

        const auto max_font_size = static_cast<float>(bound.getWidth()) * kFontSizeOverWidth;
        const auto min_font_size = max_font_size * .25f;
        const auto font_size = base_.getFontMode() == 0
            ? max_font_size * base_.getFontScale()
            : std::clamp(base_.getStaticFontSize(), min_font_size, max_font_size);
        base_.setFontSize(font_size);

        // 40px bottom bar bounds
        auto bottom_bar = bound.removeFromBottom(40);

        // CurvePanel takes the remaining area
        curve_panel_.setBounds(bound);

        // Place horizontal fader, its labels, and the phase switch in the bottom-right corner of the bottom bar
        const int title_w = static_cast<int>(std::round(font_size * 2.8f));
        const int fader_w = static_cast<int>(std::round(font_size * 6.5f));
        const int value_w = static_cast<int>(std::round(font_size * 4.5f));
        const int phase_w = 24;
        const int gap = static_cast<int>(std::round(font_size * 0.3f));
        const int margin_right = 10;
        const int fader_h = 24;
        
        auto fader_y = bottom_bar.getY() + (bottom_bar.getHeight() - fader_h) / 2;
        auto phase_y = bottom_bar.getY() + (bottom_bar.getHeight() - phase_w) / 2;

        const auto label_font = juce::Font("Microsoft YaHei", font_size * 0.95f, juce::Font::plain);
        gain_title_label_.setFont(label_font);
        gain_value_label_.setFont(label_font);
        fft_title_label_.setFont(label_font);

        const auto title_font = juce::Font("Microsoft YaHei", font_size * 1.35f, juce::Font::bold);
        bottom_left_label_.setFont(title_font);

        const int text_width = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(title_font, bottom_left_label_.getText()))) + 4;
        bottom_left_label_.setBounds(15, fader_y, text_width, fader_h);

        auto right_edge = bottom_bar.getRight() - margin_right;

        const int auto_gain_w = 24;
        const int shift_w = auto_gain_w + gap;

        phase_button_.setBounds(right_edge - phase_w, phase_y, phase_w, phase_w);
        auto_gain_btn_.setBounds(right_edge - phase_w - shift_w, phase_y, auto_gain_w, phase_w);
        gain_value_label_.setBounds(right_edge - phase_w - shift_w - 6 - value_w, fader_y, value_w, fader_h);
        gain_slider_.setBounds(right_edge - phase_w - shift_w - 6 - value_w - gap - fader_w, fader_y, fader_w, fader_h);

        const int gain_title_x = right_edge - phase_w - shift_w - 6 - value_w - gap - fader_w - gap - title_w;
        gain_title_label_.setBounds(gain_title_x, fader_y, title_w, fader_h);

        // Spectrum control group: 频谱 label, 重点 button, 前 button, 后 button
        const int fft_title_w = static_cast<int>(std::round(font_size * 2.8f));
        const int collision_w = static_cast<int>(std::round(font_size * 2.8f));
        const int pre_w = static_cast<int>(std::round(font_size * 1.8f));
        const int post_w = static_cast<int>(std::round(font_size * 1.8f));
        const int btn_gap = static_cast<int>(std::round(font_size * 0.15f));
        const int group_gap = static_cast<int>(std::round(font_size * 1.2f));

        const int new_group_right = gain_title_x - group_gap;
        const int post_x = new_group_right - post_w;
        const int pre_x = post_x - btn_gap - pre_w;
        const int collision_x = pre_x - btn_gap - collision_w;
        const int fft_title_x = collision_x - gap - fft_title_w;

        const int ab_btn_w = static_cast<int>(std::round(font_size * 1.8f));
        const int ab_copy_w = static_cast<int>(std::round(font_size * 1.5f));
        const int ab_group_gap = static_cast<int>(std::round(font_size * 0.8f));

        const int b_x = fft_title_x - ab_group_gap - ab_btn_w;
        const int copy_x = b_x - btn_gap - ab_copy_w;
        const int a_x = copy_x - btn_gap - ab_btn_w;

        const int preset_w = static_cast<int>(std::round(font_size * 15.0f));
        const int preset_x = a_x - ab_group_gap - preset_w;

        preset_box_.setBounds(preset_x, fader_y, preset_w, fader_h);
        a_btn_.setBounds(a_x, fader_y, ab_btn_w, fader_h);
        ab_copy_btn_.setBounds(copy_x, fader_y, ab_copy_w, fader_h);
        b_btn_.setBounds(b_x, fader_y, ab_btn_w, fader_h);

        fft_post_btn_.setBounds(post_x, fader_y, post_w, fader_h);
        fft_pre_btn_.setBounds(pre_x, fader_y, pre_w, fader_h);
        fft_collision_btn_.setBounds(collision_x, fader_y, collision_w, fader_h);
        fft_title_label_.setBounds(fft_title_x, fader_y, fft_title_w, fader_h);

        // ControlPanel floats at the bottom of the CurvePanel area, centered on the selected band node if active
        const auto control_w = control_panel_.getIdealWidth();
        const auto control_h = control_panel_.getIdealHeight();
        const auto output_w = curve_panel_.getOutputPanel().getIdealWidth();
        const auto control_x = bound.getX() + (bound.getWidth() - output_w - control_w) / 2;
        const auto control_y = bound.getBottom() - control_h - 35;
        control_panel_.setBounds(control_x, control_y, control_w, control_h);
    }

    void MainPanel::repaintCallBack(const double time_stamp) {
        if (refresh_handler_.tick(time_stamp)) {
            if (time_stamp - previous_time_stamp_ > 0.1) {
                previous_time_stamp_ = time_stamp;
                repaintCallBackSlow();
            }
            if (first_frame_ && c_sample_rate_ > 0.0) {
                startup_frames_++;
                if (startup_frames_ >= 15) {
                    first_frame_ = false;
                    base_.setSelectedBand(0);
                }
            }
            // update selected band
            if (c_band_ != base_.getSelectedBand()) {
                c_band_ = base_.getSelectedBand();

                control_panel_.updateBand();
                curve_panel_.updateBand();
            }
            curve_panel_.repaintCallBack();
            control_panel_.repaintCallBack();

            // Keep ControlPanel horizontally centered at all times
            const auto control_w = control_panel_.getIdealWidth();
            const auto control_h = control_panel_.getIdealHeight();
            const auto output_w = curve_panel_.getOutputPanel().getIdealWidth();
            const auto control_x = (getWidth() - output_w - control_w) / 2;
            const auto control_y = getHeight() - 40 - control_h - 35;
            control_panel_.setBounds(control_x, control_y, control_w, control_h);

            const auto c_refresh_rate = refresh_handler_.getActualRefreshRate();
            if (std::abs(c_refresh_rate - refresh_rate_) > 0.1) {
                refresh_rate_ = c_refresh_rate;
                curve_panel_.getFFTPanel().setRefreshRate(static_cast<float>(refresh_rate_));
            }
        }
    }

    void MainPanel::updateABButtonStates() {
        const bool is_b_active = p_ref_.isStateBActive();
        a_btn_.getButton().setToggleState(!is_b_active, juce::dontSendNotification);
        b_btn_.getButton().setToggleState(is_b_active, juce::dontSendNotification);
        if (is_b_active) {
            ab_copy_btn_.getButton().setButtonText(juce::String::fromUTF8("\xe2\x86\x90"));
            ab_copy_btn_.getButton().setTooltip(juce::String::fromUTF8("复制 B 状态参数到 A"));
        } else {
            ab_copy_btn_.getButton().setButtonText(juce::String::fromUTF8("\xe2\x86\x92"));
            ab_copy_btn_.getButton().setTooltip(juce::String::fromUTF8("复制 A 状态参数到 B"));
        }
    }

    void MainPanel::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
        juce::ignoreUnused(property);
    }

    void MainPanel::timerCallback() {
        if (juce::Process::isForegroundProcess()) {
            if (getCurrentlyFocusedComponent() != this) {
                grabKeyboardFocus();
            }
            stopTimer();
        }
    }

    void MainPanel::repaintCallBackSlow() {
        // update sample rate
        const auto sample_rate = p_ref_.getAtomicSampleRate();
        if (std::abs(sample_rate - c_sample_rate_) > 1.0) {
            c_sample_rate_ = sample_rate;
            curve_panel_.updateSampleRate(sample_rate);
            control_panel_.updateSampleRate(sample_rate);
        }
        // sub slow callbacks

        updater_.updateComponents();

        control_panel_.repaintCallBackSlow();
        curve_panel_.repaintCallBackSlow();

        updateABButtonStates();
    }

    void MainPanel::startThreads() {
        curve_panel_.startThreads();
    }

    void MainPanel::stopThreads() {
        curve_panel_.stopThreads();
    }

    void MainPanel::labelTextChanged(juce::Label* label) {
        juce::ignoreUnused(label);
    }

    void MainPanel::editorShown(juce::Label* label, juce::TextEditor& editor) {
        juce::ignoreUnused(label);
        editor.setJustification(juce::Justification::centredLeft);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(8, 12, 16).withAlpha(0.9f));
        editor.setColour(juce::TextEditor::highlightedTextColourId, base_.getTextColour());
        editor.applyFontToAllText(juce::Font("Microsoft YaHei", base_.getFontSize() * 0.95f, juce::Font::plain));
        editor.applyColourToAllText(base_.getTextColour(), true);
    }

    void MainPanel::editorHidden(juce::Label* label, juce::TextEditor& editor) {
        juce::ignoreUnused(label);
        const auto ctext = editor.getText();
        if (gain_slider_.valueFromTextFunction) {
            double actual_value = gain_slider_.valueFromTextFunction(ctext);
            gain_slider_.setValue(actual_value, juce::sendNotificationSync);
        } else {
            gain_slider_.setValue(ctext.getDoubleValue(), juce::sendNotificationSync);
        }
        gain_value_label_.setText(gain_slider_.getTextFromValue(gain_slider_.getValue()), juce::dontSendNotification);
    }

    void MainPanel::mouseUp(const juce::MouseEvent& event) {
        if (event.originalComponent == &bottom_left_label_ && event.mouseWasClicked()) {
            juce::URL("http://www.kx2222.com").launchInDefaultBrowser();
        }
    }

    void MainPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
        if (comboBoxThatHasChanged == &preset_box_.getBox()) {
            int id = preset_box_.getBox().getSelectedId();
            if (id <= 0) return;

            if (id == 1) { // Save
                preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
                
                auto* alertWindow = new juce::AlertWindow(
                    juce::String::fromUTF8("保存预置"),
                    juce::String::fromUTF8("请输入预置名称:"),
                    juce::AlertWindow::NoIcon
                );
                alertWindow->addTextEditor("presetName", "", "");
                alertWindow->addButton(juce::String::fromUTF8("保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
                alertWindow->addButton(juce::String::fromUTF8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

                alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([this, alertWindow](int result) {
                    if (result == 1) {
                        auto text = alertWindow->getTextEditorContents("presetName");
                        if (text.trim().isNotEmpty()) {
                            saveUserPreset(text.trim());
                        }
                    }
                    delete alertWindow;
                }), true);
            } else if (id == 2) { // Delete
                preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
                
                if (active_preset_id_ < 100) {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        juce::String::fromUTF8("提示"),
                        juce::String::fromUTF8("不能删除系统内置预置！")
                    );
                } else {
                    juce::AlertWindow::showOkCancelBox(
                        juce::AlertWindow::QuestionIcon,
                        juce::String::fromUTF8("删除预置"),
                        juce::String::fromUTF8("确定要删除当前预置吗？"),
                        juce::String::fromUTF8("确定"),
                        juce::String::fromUTF8("取消"),
                        nullptr,
                        juce::ModalCallbackFunction::create([this](int result) {
                            if (result != 0) {
                                deleteActiveUserPreset();
                            }
                        })
                    );
                }
            } else if (id == 3) { // Import
                preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
                importUserPreset();
            } else if (id == 4) { // Export
                preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
                exportUserPreset();
            } else {
                loadSelectedPreset(id);
            }
        }
    }

    void MainPanel::refreshPresetsList() {
        auto& box = preset_box_.getBox();
        box.clear(juce::dontSendNotification);

        box.addItem(juce::String::fromUTF8("保存当前预置"), 1);
        box.addItem(juce::String::fromUTF8("删除当前预置"), 2);
        box.addItem(juce::String::fromUTF8("导入外部预置"), 3);
        box.addItem(juce::String::fromUTF8("导出当前预置"), 4);
        box.addSeparator();

        box.addItem(juce::String::fromUTF8("默认 (Reset to Default)"), 10);
        box.addItem(juce::String::fromUTF8("男声温暖 (Male Vocal)"), 11);
        box.addItem(juce::String::fromUTF8("女声亮丽 (Female Vocal)"), 12);
        box.addItem(juce::String::fromUTF8("低音增强 (Bass Boost)"), 13);
        box.addItem(juce::String::fromUTF8("吉他原声 (Acoustic Guitar)"), 14);
        box.addItem(juce::String::fromUTF8("电话效果 (Telephone)"), 15);

        auto userFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("FERRY")
                            .getChildFile("ZLEqualizer")
                            .getChildFile("Presets");
        if (userFolder.exists() && userFolder.isDirectory()) {
            auto files = userFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
            if (files.size() > 0) {
                box.addSeparator();
                int userPresetId = 100;
                for (auto& file : files) {
                    auto name = file.getFileNameWithoutExtension();
                    box.addItem(name, userPresetId++);
                }
            }
        }
    }

    void MainPanel::saveUserPreset(const juce::String& name) {
        auto userFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("FERRY")
                            .getChildFile("ZLEqualizer")
                            .getChildFile("Presets");
        if (!userFolder.exists()) {
            userFolder.createDirectory();
        }
        
        auto safeName = juce::File::createLegalFileName(name);
        auto presetFile = userFolder.getChildFile(safeName + ".xml");
        
        auto temp_tree = juce::ValueTree("ZLCompressorParaState");
        temp_tree.appendChild(p_ref_.parameters_.copyState(), nullptr);
        temp_tree.appendChild(p_ref_.parameters_NA_.copyState(), nullptr);
        const std::unique_ptr<juce::XmlElement> xml(temp_tree.createXml());
        
        if (xml != nullptr) {
            if (xml->writeTo(presetFile)) {
                refreshPresetsList();
                
                auto& box = preset_box_.getBox();
                for (int i = 0; i < box.getNumItems(); ++i) {
                    int itemId = box.getItemId(i);
                    if (itemId >= 100 && box.getItemText(i) == safeName) {
                        active_preset_id_ = itemId;
                        box.setSelectedId(active_preset_id_, juce::dontSendNotification);
                        break;
                    }
                }
            }
        }
    }

    void MainPanel::deleteActiveUserPreset() {
        if (active_preset_id_ < 100) return;
        
        auto userFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("FERRY")
                            .getChildFile("ZLEqualizer")
                            .getChildFile("Presets");
        if (userFolder.exists() && userFolder.isDirectory()) {
            auto files = userFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
            int index = active_preset_id_ - 100;
            if (index >= 0 && index < files.size()) {
                auto file = files[index];
                if (file.deleteFile()) {
                    refreshPresetsList();
                    active_preset_id_ = 10;
                    preset_box_.getBox().setSelectedId(active_preset_id_, juce::dontSendNotification);
                    p_ref_.loadPreset(0);
                }
            }
        }
    }

    void MainPanel::importUserPreset() {
        file_chooser_ = std::make_unique<juce::FileChooser> (
            juce::String::fromUTF8("导入预置"),
            juce::File(),
            "*.xml"
        );
        
        file_chooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc) {
                auto resultFile = fc.getResult();
                if (resultFile.existsAsFile()) {
                    auto userFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                        .getChildFile("FERRY")
                                        .getChildFile("ZLEqualizer")
                                        .getChildFile("Presets");
                    if (!userFolder.exists()) {
                        userFolder.createDirectory();
                    }
                    auto destFile = userFolder.getChildFile(resultFile.getFileName());
                    if (resultFile.copyFileTo(destFile)) {
                        refreshPresetsList();
                        auto nameWithoutExt = destFile.getFileNameWithoutExtension();
                        auto& box = preset_box_.getBox();
                        for (int i = 0; i < box.getNumItems(); ++i) {
                            int itemId = box.getItemId(i);
                            if (itemId >= 100 && box.getItemText(i) == nameWithoutExt) {
                                active_preset_id_ = itemId;
                                box.setSelectedId(active_preset_id_, juce::dontSendNotification);
                                loadSelectedPreset(active_preset_id_);
                                break;
                            }
                        }
                    }
                }
            }
        );
    }

    void MainPanel::exportUserPreset() {
        file_chooser_ = std::make_unique<juce::FileChooser> (
            juce::String::fromUTF8("导出预置"),
            juce::File(),
            "*.xml"
        );
        
        file_chooser_->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) {
                auto resultFile = fc.getResult();
                if (resultFile != juce::File()) {
                    auto temp_tree = juce::ValueTree("ZLCompressorParaState");
                    temp_tree.appendChild(p_ref_.parameters_.copyState(), nullptr);
                    temp_tree.appendChild(p_ref_.parameters_NA_.copyState(), nullptr);
                    const std::unique_ptr<juce::XmlElement> xml(temp_tree.createXml());
                    if (xml != nullptr) {
                        xml->writeTo(resultFile);
                    }
                }
            }
        );
    }

    void MainPanel::loadSelectedPreset(int id) {
        if (id >= 10 && id <= 15) {
            p_ref_.loadPreset(id - 10);
            active_preset_id_ = id;
            preset_box_.getBox().setSelectedId(id, juce::dontSendNotification);
        } else if (id >= 100) {
            auto userFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("FERRY")
                                .getChildFile("ZLEqualizer")
                                .getChildFile("Presets");
            if (userFolder.exists() && userFolder.isDirectory()) {
                auto files = userFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
                int index = id - 100;
                if (index >= 0 && index < files.size()) {
                    auto file = files[index];
                    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
                    if (xml != nullptr && xml->hasTagName("ZLCompressorParaState")) {
                        auto temp_tree = juce::ValueTree::fromXml(*xml);
                        auto params_tree = temp_tree.getChildWithName(p_ref_.parameters_.state.getType());
                        if (params_tree.isValid()) {
                            p_ref_.parameters_.replaceState(params_tree);
                        }
                        auto params_na_tree = temp_tree.getChildWithName(p_ref_.parameters_NA_.state.getType());
                        if (params_na_tree.isValid()) {
                            p_ref_.parameters_NA_.replaceState(params_na_tree);
                        }
                        active_preset_id_ = id;
                        preset_box_.getBox().setSelectedId(id, juce::dontSendNotification);
                    }
                }
            }
        }
    }
}
