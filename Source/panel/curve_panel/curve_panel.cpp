// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "curve_panel.hpp"

namespace zlpanel {
    CurvePanel::CurvePanel(PluginProcessor& p,
                           zlgui::UIBase& base,
                           multilingual::TooltipHelper& tooltip_helper) :
        Thread("curve_panel"),
        base_(base),
        background_panel_(p, base, tooltip_helper),
        fft_panel_(p, base),
        response_panel_(p, base, tooltip_helper),
        output_panel_(p, base, tooltip_helper) {
        background_panel_.setBufferedToImage(true);
        addAndMakeVisible(background_panel_);
        addAndMakeVisible(fft_panel_);
        addAndMakeVisible(response_panel_);
        response_panel_.addMouseListener(this, true);
        addAndMakeVisible(output_panel_);
        setInterceptsMouseClicks(false, true);
    }

    CurvePanel::~CurvePanel() {
        stopThreads();
    }

    void CurvePanel::paintOverChildren(juce::Graphics& g) {
        juce::ignoreUnused(g);
        notify();
        response_panel_.notify();
    }

    void CurvePanel::run() {
        while (!threadShouldExit()) {
            const auto flag = wait(-1);
            juce::ignoreUnused(flag);
            fft_panel_.run(*this);
        }
    }

    void CurvePanel::resized() {
        auto bound = getLocalBounds();

        // Pinned to the right: Output panel (fixed width output_panel_.getIdealWidth() px, full height)
        const auto output_width = output_panel_.getIdealWidth();
        auto output_bound = bound.removeFromRight(output_width);
        output_panel_.setBounds(output_bound);
        output_panel_.setVisible(true);

        // Center / Left: the response curve graph & FFT take the remaining bounds
        background_panel_.setBounds(bound);
        fft_panel_.setBounds(bound);
        response_panel_.setBounds(bound);
    }

    void CurvePanel::mouseDown(const juce::MouseEvent&) {
    }

    void CurvePanel::repaintCallBack() {
        repaint();
        response_panel_.repaintCallBack();
    }

    void CurvePanel::repaintCallBackSlow() {
        response_panel_.repaintCallBackSlow();
        output_panel_.repaintCallBackSlow();
    }

    void CurvePanel::updateBand() {
        response_panel_.updateBand();
    }

    void CurvePanel::updateSampleRate(const double sample_rate) {
        sample_rate_ = sample_rate;
        background_panel_.updateSampleRate(sample_rate);
        response_panel_.updateSampleRate(sample_rate);
    }

    void CurvePanel::startThreads() {
        startThread(juce::Thread::Priority::low);
        response_panel_.startThread(juce::Thread::Priority::low);
    }

    void CurvePanel::stopThreads() {
        if (isThreadRunning()) {
            stopThread(-1);
        }
        if (response_panel_.isThreadRunning()) {
            response_panel_.stopThread(-1);
        }
    }


}
