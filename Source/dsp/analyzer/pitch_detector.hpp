// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace zldsp::analyzer {

    class PitchDetector {
    public:
        PitchDetector() = default;
        ~PitchDetector() = default;

        void prepare(double sample_rate) {
            sample_rate_ = sample_rate;
            current_pitch_ = 0.0;
            
            // Calculate decimation factor to target ~12 kHz processing rate
            decimation_factor_ = std::max(1, static_cast<int>(std::round(sample_rate / 12000.0)));
            decimated_sample_rate_ = sample_rate_ / decimation_factor_;
            
            // Search range: 50 Hz to 1000 Hz
            tau_min_ = std::max(2, static_cast<int>(std::round(decimated_sample_rate_ / 1000.0)));
            tau_max_ = std::min(400, static_cast<int>(std::round(decimated_sample_rate_ / 50.0)));
            
            // Window size for autocorrelation
            window_size_ = 256; 
            
            // Resize buffer: circular buffer needs to hold at least window_size_ + tau_max_
            circ_buf_.assign(1024, 0.0);
            write_ptr_ = 0;
            sample_count_ = 0;
            yin_trigger_count_ = 0;
            
            // Design biquad low-pass filter (cutoff 800 Hz) to prevent aliasing and filter out harmonics
            double wc = 2.0 * juce::MathConstants<double>::pi * 800.0 / sample_rate_;
            double c = 1.0 / std::tan(wc / 2.0);
            double c2 = c * c;
            double sqrt2 = std::sqrt(2.0);
            double a0 = c2 + sqrt2 * c + 1.0;
            
            b0_ = 1.0 / a0;
            b1_ = 2.0 / a0;
            b2_ = 1.0 / a0;
            a1_ = 2.0 * (1.0 - c2) / a0;
            a2_ = (c2 - sqrt2 * c + 1.0) / a0;
            
            x1_ = 0.0;
            x2_ = 0.0;
            y1_ = 0.0;
            y2_ = 0.0;
        }

        // Returns the currently detected pitch (in Hz)
        double getPitch() const {
            return current_pitch_;
        }

        // Processes an input sample (mono)
        void processSample(double x) {
            // Apply 2nd order low pass filter
            double y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
            x2_ = x1_;
            x1_ = x;
            y2_ = y1_;
            y1_ = y;
            
            // Decimate signal
            if (++sample_count_ >= decimation_factor_) {
                sample_count_ = 0;
                
                circ_buf_[write_ptr_] = y;
                write_ptr_ = (write_ptr_ + 1) % 1024;
                
                // Run YIN every 32 decimated samples (approx. 2.6 ms at 12kHz)
                if (++yin_trigger_count_ >= 32) {
                    yin_trigger_count_ = 0;
                    detectPitch();
                }
            }
        }

    private:
        void detectPitch() {
            int count_needed = window_size_ + tau_max_;
            std::vector<double> linear_buf(count_needed, 0.0);
            
            int start_idx = (write_ptr_ - count_needed + 1024) % 1024;
            for (int i = 0; i < count_needed; ++i) {
                linear_buf[i] = circ_buf_[(start_idx + i) % 1024];
            }
            
            // Check signal energy in the analysis window
            double energy = 0.0;
            for (int n = 0; n < window_size_; ++n) {
                energy += linear_buf[n] * linear_buf[n];
            }
            
            // If energy is below threshold (-60 dB roughly), consider it silent/unvoiced
            if (energy < 0.0002) {
                current_pitch_ *= 0.95;
                if (current_pitch_ < 10.0) {
                    current_pitch_ = 0.0;
                }
                return;
            }
            
            // YIN Step 1 & 2: Difference function
            std::vector<double> d(tau_max_ + 1, 0.0);
            for (int tau = 1; tau <= tau_max_; ++tau) {
                double diff_sum = 0.0;
                for (int n = 0; n < window_size_; ++n) {
                    double diff = linear_buf[n] - linear_buf[n + tau];
                    diff_sum += diff * diff;
                }
                d[tau] = diff_sum;
            }
            
            // YIN Step 3: Cumulative mean normalized difference
            std::vector<double> d_prime(tau_max_ + 1, 1.0);
            double running_sum = 0.0;
            for (int tau = 1; tau <= tau_max_; ++tau) {
                running_sum += d[tau];
                if (running_sum > 1e-9) {
                    d_prime[tau] = d[tau] / (running_sum / tau);
                } else {
                    d_prime[tau] = 1.0;
                }
            }
            
            // YIN Step 4: Absolute thresholding
            int period = -1;
            const double threshold = 0.15;
            for (int tau = tau_min_; tau <= tau_max_; ++tau) {
                if (d_prime[tau] < threshold) {
                    // Look for local minimum
                    if (tau > tau_min_ && tau < tau_max_) {
                        if (d_prime[tau] < d_prime[tau - 1] && d_prime[tau] < d_prime[tau + 1]) {
                            period = tau;
                            break;
                        }
                    }
                }
            }
            
            // YIN Step 5: Global minimum fallback
            if (period == -1) {
                double min_val = 1.0;
                int min_tau = -1;
                for (int tau = tau_min_; tau <= tau_max_; ++tau) {
                    if (d_prime[tau] < min_val) {
                        min_val = d_prime[tau];
                        min_tau = tau;
                    }
                }
                if (min_val < 0.35) { // Voiced threshold
                    period = min_tau;
                }
            }
            
            // Calculate frequency and apply smoothing
            if (period != -1) {
                // Parabolic interpolation for sub-sample accuracy
                double alpha = d_prime[period - 1];
                double beta = d_prime[period];
                double gamma = d_prime[period + 1];
                double denominator = 2.0 * (alpha - 2.0 * beta + gamma);
                double delta = 0.0;
                if (std::abs(denominator) > 1e-9) {
                    delta = (alpha - gamma) / denominator;
                }
                
                double interpolated_period = period + delta;
                double freq = decimated_sample_rate_ / interpolated_period;
                
                // Validate frequency limits
                if (freq >= 40.0 && freq <= 1200.0) {
                    if (current_pitch_ > 0.0) {
                        current_pitch_ = 0.85 * current_pitch_ + 0.15 * freq;
                    } else {
                        current_pitch_ = freq;
                    }
                }
            } else {
                current_pitch_ *= 0.95;
                if (current_pitch_ < 10.0) {
                    current_pitch_ = 0.0;
                }
            }
        }

        double sample_rate_ = 48000.0;
        int decimation_factor_ = 4;
        double decimated_sample_rate_ = 12000.0;
        
        int tau_min_ = 12;
        int tau_max_ = 240;
        int window_size_ = 256;
        
        std::vector<double> circ_buf_;
        int write_ptr_ = 0;
        int sample_count_ = 0;
        int yin_trigger_count_ = 0;
        
        double current_pitch_ = 0.0;
        
        // Lowpass filter coefficients & state
        double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;
        double a1_ = 0.0, a2_ = 0.0;
        double x1_ = 0.0, x2_ = 0.0;
        double y1_ = 0.0, y2_ = 0.0;
    };
}
