// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "controller.hpp"
#include <numbers>

namespace zlp {
    static constexpr size_t kCoefUpdateInterval = 8;

    Controller::Controller(juce::AudioProcessor& processor) :
        p_ref_(processor) {
        not_off_total_.reserve(kBandNum);
        for (auto& v : not_off_indices_) {
            v.reserve(kBandNum);
        }

        for (auto& f : side_emptys_) {
            f.setFilterType(zldsp::filter::kBandPass);
        }

        for (size_t i = 0; i < kBandNum; ++i) {
            pitch_track_[i].store(false, std::memory_order::relaxed);
            pitch_track_harmonic_[i].store(0, std::memory_order::relaxed);
            c_pitch_track_[i] = false;
            c_pitch_track_harmonic_[i] = 0;
            c_pitch_track_active_[i].store(false, std::memory_order::relaxed);
            current_freqs_[i].store(1000.0, std::memory_order::relaxed);

            precise_on_[i].store(0, std::memory_order::relaxed);
            c_precise_on_[i] = 0;
            c_active_sub_band_count_[i].store(0, std::memory_order::relaxed);

            for (size_t j = 0; j < kMaxPreciseSubBands; ++j) {
                sub_follower_states_[i][j] = 0.0;
                sub_freqs_[i][j].store(1000.0, std::memory_order::relaxed);
                sub_gains_[i][j].store(0.0, std::memory_order::relaxed);
            }
        }
    }

    void Controller::prepare(const double sample_rate, const size_t max_num_samples) {
        c_filter_structure_ = kMinimum;
        sample_rate_ = sample_rate;
        pitch_detector_.prepare(sample_rate);

        side_buffers[0].resize(max_num_samples);
        side_buffers[1].resize(max_num_samples);

        for (size_t i = 0; i < kBandNum; ++i) {
            filter_paras_[i] = emptys_[i].getParas();
            side_filter_paras_[i] = side_emptys_[i].getParas();
            current_freqs_[i].store(filter_paras_[i].freq, std::memory_order::relaxed);
        }



        for (size_t i = 0; i < kBandNum; ++i) {
            dynamic_side_handlers_[i].prepare(sample_rate, 41. / 1000.);
            tdf_filters_[i].prepare(sample_rate, 2, max_num_samples);
            tdf_filters_[i].getFilter().updateParas(filter_paras_[i]);
            side_filters_[i].prepare(sample_rate, 2, max_num_samples);
            side_filters_[i].updateParas(side_filter_paras_[i]);

            zldsp::filter::FilterParameters precise_para;
            precise_para.filter_type = zldsp::filter::kPeak;
            precise_para.order = 2;
            precise_para.freq = 1000.0;
            precise_para.q = 4.0;
            precise_para.gain = 0.0;

            for (size_t j = 0; j < kMaxPreciseSubBands; ++j) {
                sub_side_filters_[i][j].prepare(sample_rate, 2, max_num_samples);
                zldsp::filter::FilterParameters bp_para;
                bp_para.filter_type = zldsp::filter::kBandPass;
                bp_para.order = 2;
                bp_para.freq = 1000.0;
                bp_para.q = 0.707;
                bp_para.gain = 0.0;
                sub_side_filters_[i][j].forceUpdate(bp_para);

                sub_precise_filters_[i][j].prepare(sample_rate, 2, max_num_samples);
                sub_precise_filters_[i][j].forceUpdate(precise_para);
            }
        }

        hist_unit_decay_ = std::pow(0.9, 1.0 / sample_rate);
        slow_hist_unit_decay_ = std::pow(0.99, 1.0 / sample_rate);

        for (size_t chan = 0; chan < 2; ++chan) {
            pre_main_buffers_[chan].resize(max_num_samples);
            pre_main_pointers_[chan] = pre_main_buffers_[chan].data();
        }
        analyzer_sender_.prepare(sample_rate, max_num_samples, {2, 2, 2}, 0.1);
        analyzer_sender_.setON(0, true);
        analyzer_sender_.setON(1, true);
        analyzer_sender_.setON(2, true);

        for (size_t chan = 0; chan < 2; ++chan) {
            solo_buffers_[chan].resize(max_num_samples);
            solo_pointers_[chan] = solo_buffers_[chan].data();
        }
        solo_filter_.prepare(sample_rate, 2, max_num_samples);
        if (c_solo_side_) {
            updateSoloFilter<true>(side_filter_paras_[c_solo_idx_]);
        } else {
            updateSoloFilter<true>(filter_paras_[c_solo_idx_]);
        }

        loudness_matcher_.prepare(sample_rate, 2);
        sgc_gain_.prepare(sample_rate, max_num_samples, 0.5);
        output_gain_.prepare(sample_rate, max_num_samples, 0.5);

        delay_.prepare(sample_rate, max_num_samples, 2, 0.021);
        delay_.prepare(sample_rate, max_num_samples, 2, 0.021);

        to_update_delay_.signal();
        to_update_output_.signal();
        to_update_.signal();
    }

    void Controller::prepareBuffer() {
        if (!to_update_.check()) {
            return;
        }
        prepareStatus();
        if (to_update_dynamic_.check()) {
            prepareDynamics();
        }
        if (to_update_lrms_.check()) {
            prepareLRMS();
            if (c_sgc_on_) {
                updateSGC();
            }
        }
        prepareFilters();
        prepareDynamicParameters();

        if (to_update_output_.check()) {
            prepareOutput();
        }
    }

    void Controller::prepareStatus() {
        if (c_filter_structure_ != filter_structure_.load(std::memory_order::relaxed)) {
            // cache filter structure
            c_filter_structure_ = filter_structure_.load(std::memory_order::relaxed);
            // force update all filters
            for (size_t i = 0; i < kBandNum; ++i) {
                empty_update_flags_[i].signal();
            }
            // force reset all filters
            for (size_t i = 0; i < kBandNum; ++i) {
                tdf_filters_[i].reset();
            }
            // update lr/ms on flags
            is_lr_on_ = !not_off_indices_[1].empty() || !not_off_indices_[2].empty();
            is_ms_on_ = !not_off_indices_[3].empty() || !not_off_indices_[4].empty();
        }
        if (to_update_status_.check()) {
            // cache total not off indices
            not_off_total_.clear();
            for (size_t i = 0; i < kBandNum; ++i) {
                c_filter_status_[i] = filter_status_[i].load(std::memory_order::relaxed);
                if (c_filter_status_[i] != FilterStatus::kOff) {
                    not_off_total_.emplace_back(i);
                }
            }
            to_update_lrms_.signal();
        }
        if (to_update_solo_.check()) {
            // update solo status
            const auto solo_whole_idx = solo_whole_idx_.load(std::memory_order::relaxed);
            if (solo_whole_idx == 2 * kBandNum) {
                c_solo_on_ = false;
            } else {
                c_solo_on_ = true;
                solo_filter_.reset();
                if (solo_whole_idx < kBandNum) {
                    c_solo_idx_ = solo_whole_idx;
                    c_solo_side_ = false;
                    updateSoloFilter<true>(filter_paras_[c_solo_idx_]);
                } else {
                    c_solo_idx_ = solo_whole_idx - kBandNum;
                    c_solo_side_ = true;
                    updateSoloFilter<true>(side_filter_paras_[c_solo_idx_]);
                }
            }
        }
    }

    void Controller::prepareFilters() {
        for (size_t i = 0; i < kBandNum; ++i) {
            const auto val = precise_on_[i].load(std::memory_order::relaxed);
            c_precise_on_[i] = val == 2 ? 1 : val;
        }
        // update not-off filters
        bool to_update_sgc{false};
        for (const size_t& i : not_off_total_) {
            if (empty_update_flags_[i].check()) {
                const auto filter_type = filter_paras_[i].filter_type;
                filter_paras_[i] = emptys_[i].getParas();
                if (c_sgc_on_) {
                    to_update_sgc = true;
                    sgc_values_[i] = zldsp::filter::getGainCompensation(filter_paras_[i]);
                }
                if (c_solo_on_ && !c_solo_side_ && c_solo_idx_ == i) {
                    updateSoloFilter<false>(filter_paras_[i]);
                }
                // if the filter type changes, check whether dynamic should be on
                if (filter_type != filter_paras_[i].filter_type) {
                    prepareOneBandDynamics(i);
                }
                dynamic_side_handlers_[i].setBaseGain(filter_paras_[i].gain);
                if (!c_dynamic_on_[i]) {
                    current_gains_[i].store(filter_paras_[i].gain, std::memory_order::relaxed);
                }
                if (c_pitch_track_active_[i].load(std::memory_order::relaxed)) {
                    const auto current_freq = tdf_filters_[i].getFilter().getFreq();
                    tdf_filters_[i].getFilter().updateParas(filter_paras_[i]);
                    tdf_filters_[i].getFilter().setFreq<true>(current_freq);
                } else {
                    tdf_filters_[i].getFilter().updateParas(filter_paras_[i]);
                    current_freqs_[i].store(filter_paras_[i].freq, std::memory_order::relaxed);
                }
                if (c_dynamic_on_[i]) {
                    tdf_filters_[i].getFilter().cacheDynPara();
                }
            }
        }
        if (to_update_sgc) {
            updateSGC();
        }
    }

    void Controller::prepareLRMS() {
        // cache not-off indices and on indices
        for (auto& v : not_off_indices_) {
            v.clear();
        }
        for (const size_t& i : not_off_total_) {
            const auto lr = lrms_[i].load(std::memory_order::relaxed);
            c_lrms_[i] = lr;
            not_off_indices_[static_cast<size_t>(lr)].emplace_back(i);
        }
        is_lr_on_ = !not_off_indices_[1].empty() || !not_off_indices_[2].empty();
        is_ms_on_ = !not_off_indices_[3].empty() || !not_off_indices_[4].empty();
    }

    void Controller::prepareDynamics() {
        // cache dynamic on flags
        for (size_t i = 0; i < kBandNum; ++i) {
            prepareOneBandDynamics(i);
        }
    }

    void Controller::prepareOneBandDynamics(const size_t i) {
        if (dynamic_on_[i].load(std::memory_order::relaxed) && (
            filter_paras_[i].filter_type == zldsp::filter::kPeak ||
            filter_paras_[i].filter_type == zldsp::filter::kLowShelf ||
            filter_paras_[i].filter_type == zldsp::filter::kHighShelf ||
            filter_paras_[i].filter_type == zldsp::filter::kTiltShelf ||
            filter_paras_[i].filter_type == zldsp::filter::kFlatTilt)) {
            // turn on dynamic
            if (c_dynamic_on_[i] != true) {
                c_dynamic_on_[i] = true;
                tdf_filters_[i].resetDynamic();
                tdf_filters_[i].getFilter().cacheDynPara();
                side_filters_[i].reset();

                side_empty_update_flags_[i].signal();
                dynamic_th_update_[i].signal();
                dynamic_ar_update_[i].signal();
                dynamic_extra_update_[i].signal();
            }
        } else {
            // turn dynamic off and reset filter gain
            if (c_dynamic_on_[i] != false) {
                c_dynamic_on_[i] = false;
                tdf_filters_[i].resetDynamic();
                current_gains_[i].store(dynamic_side_handlers_[i].getBaseGain(), std::memory_order::relaxed);
            }
        }
    }



    void Controller::prepareDynamicParameters() {
        for (const size_t& i : not_off_total_) {
            if (!dynamic_on_[i]) {
                continue;
            }
            if (side_empty_update_flags_[i].check()) {
                auto side_para = side_emptys_[i].getParas();
                if (side_para.filter_type == zldsp::filter::kLowPass
                    || side_para.filter_type == zldsp::filter::kHighPass) {
                    side_para.q = std::sqrt(2.0) * .5;
                }
                side_filter_paras_[i] = side_para;
                if (c_solo_on_ && c_solo_side_ && c_solo_idx_ == i) {
                    updateSoloFilter<false>(side_para);
                }
                dynamic_side_handlers_[i].setTargetGain(side_para.gain);
                side_para.gain = 0.0;
                side_filters_[i].updateParas(side_para);
            }
            if (dynamic_th_update_[i].check()) {
                if (c_dynamic_th_relative_[i] != dynamic_th_relative_[i].load(std::memory_order::relaxed)) {
                    c_dynamic_th_relative_[i] = dynamic_th_relative_[i].load(std::memory_order::relaxed);
                    if (c_dynamic_th_learn_[i]) {
                        slow_histograms_[i].reset();
                        learned_thresholds_[i].store(-40.0, std::memory_order::relaxed);
                    }
                }
                if (c_dynamic_th_learn_[i] != dynamic_th_learn_[i].load(std::memory_order::relaxed)) {
                    c_dynamic_th_learn_[i] = dynamic_th_learn_[i].load(std::memory_order::relaxed);
                    if (c_dynamic_th_learn_[i]) {
                        histograms_[i].reset();
                        slow_histograms_[i].reset();
                        learned_thresholds_[i].store(-40.0, std::memory_order::relaxed);
                    }
                    hist_results_[1] = 0.0;
                }
                if (c_dynamic_th_learn_[i]) {
                    c_dynamic_threshold_[i] = dynamic_threshold_[i].load(std::memory_order::relaxed) + 40.0;
                } else {
                    c_dynamic_threshold_[i] = dynamic_threshold_[i].load(std::memory_order::relaxed);
                    dynamic_side_handlers_[i].setThreshold<false>(c_dynamic_threshold_[i]);
                }
                dynamic_side_handlers_[i].setKnee(dynamic_knee_[i].load(std::memory_order::relaxed));
            }
            if (dynamic_ar_update_[i].check()) {
                auto& follower{dynamic_side_handlers_[i].getFollower()};
                follower.setAttack<false>(dynamic_attack_[i].load(std::memory_order::relaxed));
                follower.setRelease<true>(dynamic_release_[i].load(std::memory_order::relaxed));
            }
            if (dynamic_extra_update_[i].check()) {
                if (dynamic_smooth_update_[i].check()) {
                    auto& follower{dynamic_side_handlers_[i].getFollower()};
                    follower.setSmooth<true>(dynamic_smooth_[i].load(std::memory_order::relaxed));
                }
                if (dynamic_rms_length_update_[i].check()) {
                    auto& handler{dynamic_side_handlers_[i]};
                    handler.setRMSLength(dynamic_rms_length_[i].load(std::memory_order::relaxed));
                }
                if (dynamic_rms_mix_update_[i].check()) {
                    auto& handler{dynamic_side_handlers_[i]};
                    handler.setRMSMix(dynamic_rms_mix_[i].load(std::memory_order::relaxed));
                }
            }
        }
    }

    void Controller::prepareOutput() {
        const auto sgc_on = sgc_on_.load(std::memory_order::relaxed);
        if (c_sgc_on_ != sgc_on) {
            c_sgc_on_ = sgc_on;
            if (c_sgc_on_) {
                for (const size_t& band : not_off_total_) {
                    if (c_filter_status_[band] == kOn) {
                        sgc_values_[band] = zldsp::filter::getGainCompensation(filter_paras_[band]);
                    } else {
                        sgc_values_[band] = 0.0;
                    }
                }
                updateSGC();
            }
        }
        const auto loudness_matcher_on = loudness_matcher_on_.load(std::memory_order_relaxed);
        if (loudness_matcher_on != c_loudness_matcher_on_) {
            c_loudness_matcher_on_ = loudness_matcher_on;
            if (c_loudness_matcher_on_) {
                loudness_matcher_.reset();
            }
        }
        const auto agc_on = agc_on_.load(std::memory_order::relaxed);
        if (c_agc_on_ != agc_on) {
            c_agc_on_ = agc_on;
            updateOutputGain();
        }
        c_phase_flip_on_ = phase_flip_on_.load(std::memory_order_relaxed);
        if (to_update_makeup_.check()) {
            c_makeup_gain_linear_ = makeup_gain_linear_.load(std::memory_order::relaxed);
            updateOutputGain();
        }
        if (to_update_delay_.check()) {
            const auto delay_second = delay_second_.load(std::memory_order::relaxed);
            c_delay_on_ = std::abs(delay_second) > 1e-6;
            delay_.setDelay(c_delay_on_ ? delay_second : 0.);
            delay_latency_.store(delay_.getDelayInSamples(), std::memory_order::relaxed);
            triggerAsyncUpdate();
        }
    }

    template <bool bypass>
    void Controller::process(std::array<double*, 2> main_pointers, std::array<double*, 2> side_pointers,
                             const size_t num_samples) {
        prepareBuffer();

        if constexpr (bypass) {
            c_editor_on_ = editor_on_.load(std::memory_order::relaxed);
            if (c_editor_on_) {
                zldsp::vector::copy(pre_main_pointers_[0], main_pointers[0], num_samples);
                zldsp::vector::copy(pre_main_pointers_[1], main_pointers[1], num_samples);
                analyzer_sender_.process({pre_main_pointers_, main_pointers, side_pointers}, num_samples);
                
                if (c_sgc_on_) {
                    displayed_gain_.store(sgc_gain_.getCurrentGainLinear() * output_gain_.getCurrentGainLinear());
                } else {
                    displayed_gain_.store(output_gain_.getCurrentGainLinear());
                }
            }
            return;
        }

        c_editor_on_ = editor_on_.load(std::memory_order::relaxed);

        // Global pitch tracking
        double f0 = 0.0;
        bool any_pitch_tracking = false;
        for (size_t i = 0; i < kBandNum; ++i) {
            c_pitch_track_[i] = pitch_track_[i].load(std::memory_order::relaxed);
            c_pitch_track_harmonic_[i] = pitch_track_harmonic_[i].load(std::memory_order::relaxed);
            if (c_dynamic_on_[i] && c_pitch_track_[i]) {
                any_pitch_tracking = true;
            }
        }
        
        if (any_pitch_tracking) {
            for (size_t sample_idx = 0; sample_idx < num_samples; ++sample_idx) {
                double mono = 0.5 * (main_pointers[0][sample_idx] + main_pointers[1][sample_idx]);
                pitch_detector_.processSample(mono);
            }
            f0 = pitch_detector_.getPitch();
        }

        // Apply pitch tracking frequency offsets
        for (size_t i = 0; i < kBandNum; ++i) {
            if (c_dynamic_on_[i] && c_pitch_track_[i]) {
                if (f0 > 40.0) { // Valid voiced range
                    double multiplier = static_cast<double>(c_pitch_track_harmonic_[i] + 1);
                    double f = f0 * multiplier;
                    f = std::clamp(f, 10.0, sample_rate_ * 0.49);
                    tdf_filters_[i].getFilter().setFreq<true>(f);
                    tdf_filters_[i].getFilter().cacheDynPara();
                    side_filters_[i].setFreq<true>(f);
                    c_pitch_track_active_[i].store(true, std::memory_order::relaxed);
                }
            } else if (c_pitch_track_active_[i].load(std::memory_order::relaxed)) {
                // Restore manual frequency
                tdf_filters_[i].getFilter().setFreq<true>(filter_paras_[i].freq);
                tdf_filters_[i].getFilter().cacheDynPara();
                side_filters_[i].setFreq<true>(side_filter_paras_[i].freq);
                c_pitch_track_active_[i].store(false, std::memory_order::relaxed);
            }
            
            // Store current active frequency
            current_freqs_[i].store(tdf_filters_[i].getFilter().getFreq(), std::memory_order::relaxed);
        }

        if (c_delay_on_) {
            delay_.process(main_pointers, num_samples);
        }
        // copy pre buffer for FFT processing
        if (bypass || c_editor_on_) {
            zldsp::vector::copy(pre_main_pointers_[0], main_pointers[0], num_samples);
            zldsp::vector::copy(pre_main_pointers_[1], main_pointers[1], num_samples);
        }
        // copy solo buffer
        if (c_solo_on_) {
            if (c_solo_side_ || !c_dynamic_on_[c_solo_idx_]) {
                std::memset(solo_pointers_[0], 0, num_samples * sizeof(double));
                std::memset(solo_pointers_[1], 0, num_samples * sizeof(double));
            }
            switch (c_lrms_[c_solo_idx_]) {
            case FilterStereo::kStereo: {
                if (!c_solo_side_) {
                    zldsp::vector::copy(solo_pointers_[0], main_pointers[0], num_samples);
                    zldsp::vector::copy(solo_pointers_[1], main_pointers[1], num_samples);
                    solo_filter_.process(solo_pointers_, num_samples);
                }
                break;
            }
            case FilterStereo::kLeft: {
                std::memset(solo_pointers_[1], 0, num_samples * sizeof(double));
                if (!c_solo_side_) {
                    zldsp::vector::copy(solo_pointers_[0], main_pointers[0], num_samples);
                    solo_filter_.process({&solo_pointers_[0], 1}, num_samples);
                }
                break;
            }
            case FilterStereo::kRight: {
                std::memset(solo_pointers_[0], 0, num_samples * sizeof(double));
                if (!c_solo_side_) {
                    zldsp::vector::copy(solo_pointers_[1], main_pointers[1], num_samples);
                    solo_filter_.process({&solo_pointers_[1], 1}, num_samples);
                }
                break;
            }
            case FilterStereo::kMid: {
                if (!c_solo_side_) {
                    zldsp::vector::copy(solo_pointers_[0], main_pointers[0], num_samples);
                    zldsp::vector::copy(solo_pointers_[1], main_pointers[1], num_samples);
                    zldsp::splitter::InplaceMSSplitter<double>::split(
                        solo_pointers_[0], solo_pointers_[1], num_samples);
                    solo_filter_.process({&solo_pointers_[0], 1}, num_samples);
                }
                std::memset(solo_pointers_[1], 0, num_samples * sizeof(double));
                break;
            }
            case FilterStereo::kSide: {
                if (!c_solo_side_) {
                    zldsp::vector::copy(solo_pointers_[0], main_pointers[0], num_samples);
                    zldsp::vector::copy(solo_pointers_[1], main_pointers[1], num_samples);
                    zldsp::splitter::InplaceMSSplitter<double>::split(
                        solo_pointers_[0], solo_pointers_[1], num_samples);
                    solo_filter_.process({&solo_pointers_[1], 1}, num_samples);
                }
                std::memset(solo_pointers_[0], 0, num_samples * sizeof(double));
                break;
            }
            }
        }
        if (c_loudness_matcher_on_) {
            loudness_matcher_.processPre(main_pointers, num_samples);
        }
        if (c_agc_on_) {
            pre_square_sum_ = 0.0;
            for (size_t chan = 0; chan < 2; chan++) {
                pre_square_sum_ += zldsp::vector::sum_sqr(main_pointers[chan], num_samples);
            }
            pre_square_sum_ = std::clamp(pre_square_sum_ / static_cast<double>(num_samples), 1e-3, 1e3);
        }
        processDynamic(tdf_filters_, main_pointers, side_pointers, num_samples);

        if (c_sgc_on_) {
            sgc_gain_.process(main_pointers, num_samples);
        }
        if (c_loudness_matcher_on_) {
            loudness_matcher_.processPost(main_pointers, num_samples);
        }
        if (c_agc_on_) {
            double post_square_sum{0.0};
            for (size_t chan = 0; chan < 2; chan++) {
                post_square_sum += zldsp::vector::sum_sqr(main_pointers[chan], num_samples);
            }
            post_square_sum = std::clamp(post_square_sum / static_cast<double>(num_samples), 1e-3, 1e3);
            c_agc_gain_linear_ = std::sqrt(pre_square_sum_ / post_square_sum) / c_makeup_gain_linear_;
            updateOutputGain();
        }

        output_gain_.process(main_pointers, num_samples);

        if (c_agc_on_) {
            for (size_t chan = 0; chan < 2; chan++) {
                zldsp::vector::clamp(main_pointers[chan], -1.0, 1.0, num_samples);
            }
        }

        if constexpr (bypass) {
            zldsp::vector::copy(main_pointers[0], pre_main_pointers_[0], num_samples);
            zldsp::vector::copy(main_pointers[1], pre_main_pointers_[1], num_samples);
        }

        if (c_editor_on_) {
            analyzer_sender_.process({pre_main_pointers_, main_pointers, side_pointers}, num_samples);
            if (c_sgc_on_) {
                displayed_gain_.store(sgc_gain_.getCurrentGainLinear() * output_gain_.getCurrentGainLinear());
            } else {
                displayed_gain_.store(output_gain_.getCurrentGainLinear());
            }
        }

        if (c_solo_on_) {
            switch (c_lrms_[c_solo_idx_]) {
            case FilterStereo::kStereo:
            case FilterStereo::kLeft:
            case FilterStereo::kRight: {
                break;
            }
            case FilterStereo::kMid:
            case FilterStereo::kSide: {
                zldsp::splitter::InplaceMSSplitter<double>::combine(
                    solo_pointers_[0], solo_pointers_[1], num_samples);
                break;
            }
            }
            if constexpr (!bypass) {
                zldsp::vector::copy(main_pointers[0], solo_pointers_[0], num_samples);
                zldsp::vector::copy(main_pointers[1], solo_pointers_[1], num_samples);
            }
        }


        if (c_phase_flip_on_) {
            for (size_t chan = 0; chan < 2; chan++) {
                zldsp::vector::flip(main_pointers[chan], num_samples);
            }
        }
    }

    template void Controller::process<true>(std::array<double*, 2>, std::array<double*, 2>, size_t);

    template void Controller::process<false>(std::array<double*, 2>, std::array<double*, 2>, size_t);

    void Controller::handleAsyncUpdate() {
        p_ref_.setLatencySamples(delay_latency_.load(std::memory_order::relaxed));
    }

    template <typename DynamicFilterArrayType, bool should_check_parallel, bool should_be_parallel>
    void Controller::processDynamic(DynamicFilterArrayType& dynamic_filters, std::array<double*, 2> main_pointers,
                                    std::array<double*, 2> side_pointers, const size_t num_samples) {
        processOneChannelDynamic<DynamicFilterArrayType, should_check_parallel, should_be_parallel>(
            dynamic_filters, 0, main_pointers, side_pointers, side_pointers, num_samples);
        if (is_lr_on_) {
            processOneChannelDynamic<DynamicFilterArrayType, should_check_parallel, should_be_parallel>(
                dynamic_filters, 1, {&main_pointers[0], 1}, {&side_pointers[0], 1}, {&side_pointers[1], 1},
                num_samples);
            processOneChannelDynamic<DynamicFilterArrayType, should_check_parallel, should_be_parallel>(
                dynamic_filters, 2, {&main_pointers[1], 1}, {&side_pointers[1], 1}, {&side_pointers[0], 1},
                num_samples);
        }
        if (is_ms_on_) {
            zldsp::splitter::InplaceMSSplitter<double>::split(main_pointers[0], main_pointers[1], num_samples);
            zldsp::splitter::InplaceMSSplitter<double>::split(side_pointers[0], side_pointers[1], num_samples);

            processOneChannelDynamic<DynamicFilterArrayType, should_check_parallel, should_be_parallel>(
                dynamic_filters, 3, {&main_pointers[0], 1}, {&side_pointers[0], 1}, {&side_pointers[1], 1},
                num_samples);
            processOneChannelDynamic<DynamicFilterArrayType, should_check_parallel, should_be_parallel>(
                dynamic_filters, 4, {&main_pointers[1], 1}, {&side_pointers[1], 1}, {&side_pointers[0], 1},
                num_samples);

            zldsp::splitter::InplaceMSSplitter<double>::combine(main_pointers[0], main_pointers[1], num_samples);
            zldsp::splitter::InplaceMSSplitter<double>::combine(side_pointers[0], side_pointers[1], num_samples);
        }
    }

    template <typename DynamicFilterArrayType, bool should_check_parallel, bool should_be_parallel>
    void Controller::processOneChannelDynamic(DynamicFilterArrayType& dynamic_filters, const size_t lrms_idx,
                                              const std::span<double*> main_pointers,
                                              const std::span<double*> side_pointers1,
                                              const std::span<double*> side_pointers2, const size_t num_samples) {
        for (const size_t& i : not_off_indices_[lrms_idx]) {
            if constexpr (should_check_parallel) {
                if (dynamic_filters[i].getShouldBeParallel() != should_be_parallel) {
                    continue;
                }
            }
            const bool precise_on = c_precise_on_[i];
            if (c_filter_status_[i] == kBypass) {
                if (c_dynamic_on_[i]) {
                    const auto swap = dynamic_swap_[i].load(std::memory_order::relaxed);
                    if (precise_on || dynamic_bypass_[i].load(std::memory_order::relaxed)) {
                        processOneBandDynamic<true, true, true>(dynamic_filters, i, main_pointers,
                                                                swap ? side_pointers2 : side_pointers1, num_samples);
                    } else {
                        processOneBandDynamic<true, true, false>(dynamic_filters, i, main_pointers,
                                                                 swap ? side_pointers2 : side_pointers1, num_samples);
                    }
                } else {
                    processOneBandDynamic<true, false, false>(dynamic_filters, i, main_pointers, side_pointers1,
                                                              num_samples);
                }
            } else {
                if (c_dynamic_on_[i]) {
                    const auto swap = dynamic_swap_[i].load(std::memory_order::relaxed);
                    if (precise_on || dynamic_bypass_[i].load(std::memory_order::relaxed)) {
                        processOneBandDynamic<false, true, true>(dynamic_filters, i, main_pointers,
                                                                 swap ? side_pointers2 : side_pointers1, num_samples);
                    } else {
                        processOneBandDynamic<false, true, false>(dynamic_filters, i, main_pointers,
                                                                  swap ? side_pointers2 : side_pointers1, num_samples);
                    }
                } else {
                    processOneBandDynamic<false, false, false>(dynamic_filters, i, main_pointers, side_pointers1,
                                                               num_samples);
                }
            }
        }
    }

    template <bool bypass, bool dynamic_on, bool dynamic_bypass, typename DynamicFilterArrayType>
    void Controller::processOneBandDynamic(DynamicFilterArrayType& dynamic_filters, const size_t i,
                                           const std::span<double*> main_pointers,
                                           const std::span<double*> side_pointers,
                                           const size_t num_samples) {
        if constexpr (dynamic_on) {
            // copy side buffer
            side_copy_count_ = side_pointers.size();
            for (size_t chan = 0; chan < side_copy_count_; ++chan) {
                zldsp::vector::copy(side_buffers[chan].data(), side_pointers[chan], num_samples);
                side_copy_pointers_[chan] = side_buffers[chan].data();
            }
            const std::span<double*> side_copy_span{side_copy_pointers_.data(), side_copy_count_};
            // calculate side total loudness if dynamic relative is on
            double side_total_loudness = 0.0;
            if (c_dynamic_th_relative_[i]) {
                for (size_t chan = 0; chan < side_copy_count_; ++chan) {
                    const auto side_sum_sqr = zldsp::vector::sum_sqr(side_copy_pointers_[chan], num_samples);
                    side_total_loudness += side_sum_sqr / static_cast<double>(num_samples);
                }
                side_total_loudness = zldsp::chore::squareGainToDecibels(side_total_loudness);
            }
            // process side filter
            side_filters_[i].process(side_copy_span, num_samples);
            // copy side buffer to solo buffer if needed
            if (c_solo_on_ && c_solo_side_ && c_solo_idx_ == i) {
                switch (c_lrms_[i]) {
                case FilterStereo::kStereo: {
                    zldsp::vector::copy(solo_pointers_[0], side_copy_pointers_[0], num_samples);
                    zldsp::vector::copy(solo_pointers_[1], side_copy_pointers_[1], num_samples);
                    break;
                }
                case FilterStereo::kLeft:
                case FilterStereo::kMid: {
                    zldsp::vector::copy(solo_pointers_[0], side_copy_pointers_[0], num_samples);
                    break;
                }
                case FilterStereo::kRight:
                case FilterStereo::kSide: {
                    zldsp::vector::copy(solo_pointers_[1], side_copy_pointers_[0], num_samples);
                    break;
                }
                }
            }
            // calculate side histogram loudness if dynamic learn is on
            if (c_dynamic_th_learn_[i]) {
                double side_current_loudness = 0.0;
                for (size_t chan = 0; chan < side_copy_count_; ++chan) {
                    const auto side_sum_sqr = zldsp::vector::sum_sqr(side_copy_pointers_[chan], num_samples);
                    side_current_loudness += side_sum_sqr / static_cast<double>(num_samples);
                }
                side_current_loudness = zldsp::chore::squareGainToDecibels(side_current_loudness);
                // update histograms only if signal is above -60 dB gate
                if (side_current_loudness > -60.0) {
                    slow_histograms_[i].setDecay(std::pow(slow_hist_unit_decay_, static_cast<double>(num_samples)));
                    slow_histograms_[i].push(side_current_loudness - side_total_loudness);
                    slow_histograms_[i].getPercentiles(hist_percentiles_, hist_target_temp_, hist_results_);
                    learned_thresholds_[i].store(hist_results_[1],
                                                 std::memory_order::relaxed);
                    learned_knees_[i].store(std::max(0.5 * (hist_results_[2] - hist_results_[0]), 2.0),
                                            std::memory_order::relaxed);

                    histograms_[i].setDecay(std::pow(hist_unit_decay_, static_cast<double>(num_samples)));
                    histograms_[i].push(side_current_loudness);
                    histograms_[i].getPercentiles(hist_percentiles_, hist_target_temp_, hist_results_);
                    dynamic_side_handlers_[i].setKnee<false>(
                        std::max(0.5 * (hist_results_[2] - hist_results_[0]), 5.0));
                }
            } else if (c_editor_on_) {
                double side_current_loudness = 0.0;
                for (size_t chan = 0; chan < side_copy_count_; ++chan) {
                    const auto side_sum_sqr = zldsp::vector::sum_sqr(side_copy_pointers_[chan], num_samples);
                    side_current_loudness += side_sum_sqr / static_cast<double>(num_samples);
                }
                side_current_loudness = zldsp::chore::squareGainToDecibels(side_current_loudness);
                dynamic_side_loudness_display_[i].store(side_current_loudness, std::memory_order::relaxed);
            }
            // update actual threshold if required
        if (c_dynamic_th_relative_[i] || c_dynamic_th_learn_[i]) {
                dynamic_side_handlers_[i].setThreshold(
                    learned_thresholds_[i].load(std::memory_order::relaxed) - side_total_loudness + c_dynamic_threshold_[i]);
            }

            if (c_precise_on_[i] == 1) {
                // Precise B: design bandpass filters dynamically matching spacing width
                const double fc = filter_paras_[i].freq;
                const double q = filter_paras_[i].q;
                const double clamp_q = std::max(0.1, q * 0.707);
                const double y = 0.5 / clamp_q;
                const double scale = y + std::sqrt(y * y + 1.0);
                const double f_low = fc / scale;
                const double f_high = fc * scale;

                const double log_low = std::log(f_low);
                const double log_high = std::log(f_high);

                // Auto-adaptive sub-band count: wider range -> more bands (10~64)
                const double ratio = f_high / f_low;
                static constexpr double kMaxRatio = 200.0;
                const int num_bands = std::clamp(
                    static_cast<int>(std::round(10.0 + 54.0 * std::log(ratio) / std::log(kMaxRatio))),
                    10, 64);
                c_active_sub_band_count_[i].store(num_bands, std::memory_order::relaxed);

                const double step = (log_high - log_low) / static_cast<double>(num_bands - 1);
                const double factor = std::exp(step);
                const double q_sub = std::clamp(3.0 * std::sqrt(factor) / (factor - 1.0), 0.1, 50.0);

                for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                    double f_j = std::exp(log_low + j * step);
                    f_j = std::clamp(f_j, 20.0, sample_rate_ * 0.49);
                    sub_freqs_[i][j].store(f_j, std::memory_order::relaxed);

                    zldsp::filter::FilterParameters bp_para;
                    bp_para.filter_type = zldsp::filter::kBandPass;
                    bp_para.order = 2;
                    bp_para.freq = f_j;
                    bp_para.q = q_sub;
                    bp_para.gain = 0.0;
                    sub_side_filters_[i][j].forceUpdate(bp_para);
                    sub_side_filters_[i][j].cacheDynPara();

                    // Frequency-adaptive Q for peak filters:
                    // Low freq (< ~200Hz): Q ≈ 6~12 to avoid narrow-band resonance
                    // High freq: Q ≈ 18~30 for precision
                    const double log_ratio_freq = std::log(std::max(f_j, 20.0) / 20.0) / std::log(1000.0);
                    const double sub_peak_q = std::clamp(6.0 + 18.0 * log_ratio_freq, 6.0, 30.0);
                    sub_q_values_[i][j].store(sub_peak_q, std::memory_order::relaxed);

                    zldsp::filter::FilterParameters precise_para;
                    precise_para.filter_type = zldsp::filter::kPeak;
                    precise_para.order = 2;
                    precise_para.freq = f_j;
                    precise_para.q = sub_peak_q;
                    precise_para.gain = 0.0;
                    sub_precise_filters_[i][j].forceUpdate(precise_para);
                    sub_precise_filters_[i][j].cacheDynPara();
                }

                for (size_t j = static_cast<size_t>(num_bands); j < kMaxPreciseSubBands; ++j) {
                    sub_gains_[i][j].store(0.0, std::memory_order::relaxed);
                }
            }
        }
        // process the actual filter
        {
            const std::span<double*> side_span{side_copy_pointers_.data(), side_copy_count_};
            dynamic_filters[i].template processDynamic<bypass, dynamic_on, dynamic_bypass>(
                main_pointers, side_span,
                num_samples);
        }
        if constexpr (dynamic_on) {
            if (c_precise_on_[i] > 0 && !dynamic_bypass_[i].load(std::memory_order::relaxed)) {
                const auto side_p = side_copy_pointers_[0];
                if (c_precise_on_[i] == 1) {
                    // Precise B mode: variable-band zero-latency dynamic dynamics
                    // with frequency-adaptive envelope tracking and gentle gain distribution
                    const int num_bands = c_active_sub_band_count_[i].load(std::memory_order::relaxed);
                    const double inv_side_ch = (side_copy_count_ > 0) ? 1.0 / static_cast<double>(side_copy_count_) : 0.0;
                    // precompute log2(10)/40 for fast dB-to-linear conversion
                    static constexpr double kLog2_10_div40 = 0.08304820237218407;

                    // Pre-compute per-sub-band envelope coefficients (frequency-adaptive)
                    // Low freq: longer time constant (~100-200ms) to avoid fast tracking that excites room modes
                    // High freq: shorter time constant (~30-50ms) for precision
                    std::array<double, kMaxPreciseSubBands> sub_betas{};
                    for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                        const double f_j = sub_freqs_[i][j].load(std::memory_order::relaxed);
                        const double log_ratio = std::log(std::max(f_j, 20.0) / 20.0) / std::log(1000.0);
                        const double tau = std::clamp(0.03 + 0.17 * (1.0 - log_ratio), 0.03, 0.2);
                        sub_betas[j] = std::exp(-1.0 / (sample_rate_ * tau));
                    }

                    for (size_t k = 0; k < num_samples; k += kCoefUpdateInterval) {
                        const size_t limit = std::min(k + kCoefUpdateInterval, num_samples);

                        // 1. Filter sidechain sample and track short-term sub-band energy
                        for (size_t sub_k = k; sub_k < limit; ++sub_k) {
                            double sub_x = 0.0;
                            for (size_t chan = 0; chan < side_copy_count_; ++chan) {
                                sub_x += side_copy_pointers_[chan][sub_k];
                            }
                            sub_x *= inv_side_ch;

                            for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                                double y_j = sub_side_filters_[i][j].processSample(0, sub_x);
                                const double beta_j = sub_betas[j];
                                double energy = beta_j * sub_follower_states_[i][j] + (1.0 - beta_j) * y_j * y_j;
                                if (energy < 1e-15) energy = 0.0;
                                sub_follower_states_[i][j] = energy;
                            }
                        }

                        // 2. Compute relative weights and distribute overall gain reduction
                        double sum_energy_sq = 0.0;
                        for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                            double energy = sub_follower_states_[i][j];
                            sum_energy_sq += energy * energy;
                        }
                        // denormal protection
                        if (sum_energy_sq < 1e-24) sum_energy_sq = 0.0;

                        // 20*log10(x) = 20/(ln10) * ln(x) = 20*log2(x)/log2(10)
                        static constexpr double k20_div_log2_10 = 6.020599913279624; // 20/log2(10)
                        const double side_clamped = std::clamp(side_p[k], 1e-5, 1.0);
                        double g_total_db = k20_div_log2_10 * std::log2(side_clamped);
                        g_total_db = std::min(0.0, g_total_db);

                        for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                            double energy = sub_follower_states_[i][j];
                            double w_j = (sum_energy_sq > 1e-24) ? ((energy * energy) / sum_energy_sq) : (1.0 / static_cast<double>(num_bands));

                            // Gain distribution with moderate anti-resonance protection
                            double sub_gain_db = std::max(g_total_db * 1.4, g_total_db * w_j * 2.5);

                            // Sub-bass protection: only limit extreme attenuation below 80Hz
                            // to avoid reinforcing room modes / standing waves
                            const double f_j = sub_freqs_[i][j].load(std::memory_order::relaxed);
                            if (f_j < 80.0) {
                                const double lf_factor = std::clamp(f_j / 80.0, 0.4, 1.0);
                                sub_gain_db = std::max(sub_gain_db, g_total_db * lf_factor * 0.7);
                            }

                            sub_gains_[i][j].store(sub_gain_db, std::memory_order::relaxed);

                            // Update peak filter coefficients (once per sub-block)
                            sub_precise_filters_[i][j].updateGainLinear(std::exp2(sub_gain_db * kLog2_10_div40));
                        }

                        // 3. Process main signal through the peak cascade
                        for (size_t sub_k = k; sub_k < limit; ++sub_k) {
                            for (size_t chan = 0; chan < main_pointers.size(); ++chan) {
                                for (size_t j = 0; j < static_cast<size_t>(num_bands); ++j) {
                                    if constexpr (bypass) {
                                        sub_precise_filters_[i][j].processSample(chan, main_pointers[chan][sub_k]);
                                    } else {
                                        main_pointers[chan][sub_k] = sub_precise_filters_[i][j].processSample(chan, main_pointers[chan][sub_k]);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (c_precise_on_[i] != 1) {
                for (size_t j = 0; j < kMaxPreciseSubBands; ++j) {
                    sub_gains_[i][j].store(0.0, std::memory_order::relaxed);
                }
            }

            if (c_precise_on_[i] > 0 && !dynamic_bypass_[i].load(std::memory_order::relaxed)) {
                current_gains_[i].store(dynamic_side_handlers_[i].getCurrentGain(), std::memory_order::relaxed);
            } else if constexpr (dynamic_bypass) {
                current_gains_[i].store(dynamic_side_handlers_[i].getBaseGain(), std::memory_order::relaxed);
            } else {
                current_gains_[i].store(dynamic_side_handlers_[i].getCurrentGain(), std::memory_order::relaxed);
            }
        } else {
            // dynamic is off, so precise mode B is definitely off
            for (size_t j = 0; j < kMaxPreciseSubBands; ++j) {
                sub_gains_[i][j].store(0.0, std::memory_order::relaxed);
            }
        }
    }

    template <bool force>
    void Controller::updateSoloFilter(zldsp::filter::FilterParameters paras) {
        switch (paras.filter_type) {
        case zldsp::filter::kPeak:
        case zldsp::filter::kNotch:
        case zldsp::filter::kBandPass: {
            paras.filter_type = zldsp::filter::kBandPass;
            break;
        }
        case zldsp::filter::kTiltShelf:
        case zldsp::filter::kFlatTilt: {
            paras.order = 2;
            paras.q = std::sqrt(2.0) * 0.03125;
            paras.filter_type = zldsp::filter::kBandPass;
            break;
        }
        case zldsp::filter::kLowShelf:
        case zldsp::filter::kHighPass: {
            paras.q = std::sqrt(2.0) * 0.5;
            paras.filter_type = c_solo_side_ ? zldsp::filter::kHighPass : zldsp::filter::kLowPass;
            break;
        }
        case zldsp::filter::kHighShelf:
        case zldsp::filter::kLowPass: {
            paras.q = std::sqrt(2.0) * 0.5;
            paras.filter_type = c_solo_side_ ? zldsp::filter::kLowPass : zldsp::filter::kHighPass;
            break;
        }
        }
        if constexpr (force) {
            solo_filter_.forceUpdate(paras);
        } else {
            solo_filter_.updateParas(paras);
        }
    }

    void Controller::updateSGC() {
        double sgc_gain_db{0.0};
        for (const size_t& band : not_off_total_) {
            if (c_filter_status_[band] == kOn) {
                switch (c_lrms_[band]) {
                case kStereo: {
                    sgc_gain_db += sgc_values_[band];
                    break;
                }
                case kLeft:
                case kRight: {
                    sgc_gain_db += 0.5 * sgc_values_[band];
                }
                case kMid: {
                    sgc_gain_db += 0.9 * sgc_values_[band];
                }
                case kSide: {
                    sgc_gain_db += 0.1 * sgc_values_[band];
                }
                }
            }
        }
        c_sgc_gain_linear_ = zldsp::chore::decibelsToGain(sgc_gain_db);
        sgc_gain_.setGainLinear(c_sgc_gain_linear_);
    }

    void Controller::updateOutputGain() {
        double total = c_makeup_gain_linear_;
        if (c_agc_on_) {
            total *= c_agc_gain_linear_;
        }
        output_gain_.setGainLinear(std::clamp(total, 1e-3, 1e3));
    }
}
