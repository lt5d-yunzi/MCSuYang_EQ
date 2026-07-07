// Copyright (C) 2026 - zsliu98
// This file is part of ZLEqualizer
//
// ZLEqualizer is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License Version 3 as published by the Free Software Foundation.
//
// ZLEqualizer is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along with ZLEqualizer. If not, see <https://www.gnu.org/licenses/>.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "panel/helper/juce_parameter_value.hpp"

//==============================================================================
PluginProcessor::PluginProcessor() :
    AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        ),
    dummy_processor_(),
    parameters_(*this, nullptr,
                juce::Identifier("ZLEqualizerParameters"),
                zlp::getParameterLayout()),
    parameters_NA_(dummy_processor_, nullptr,
                   juce::Identifier("ZLEqualizerNAParameters"),
                   zlstate::getNAParameterLayout()),
    controller_(*this),
    chore_attachment_(*this, parameters_, controller_),
    ext_side_(*parameters_.getRawParameterValue(zlp::PExtSide::kID)),
    bypass_(*parameters_.getRawParameterValue(zlp::PBypass::kID)) {
    for (size_t i = 0; i < zlp::kBandNum; ++i) {
        filter_attachments_[i] = std::make_unique<zlp::FilterAttach>(*this, parameters_, controller_, i);
        filter_dynamic_attachments_[i] = std::make_unique<zlp::FilterDynamicAttach>(*this, parameters_, controller_, i);
        filter_side_attachments_[i] = std::make_unique<zlp::FilterSideAttach>(*this, parameters_, controller_, i);
    }
    state_A_ = parameters_.copyState();
    state_B_ = parameters_.copyState();
}

PluginProcessor::~PluginProcessor() = default;

//==============================================================================
const juce::String PluginProcessor::getName() const {
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int PluginProcessor::getNumPrograms() {
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram() {
    return 0;
}

void PluginProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String PluginProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void PluginProcessor::changeProgramName(int, const juce::String&) {
}

//==============================================================================
void PluginProcessor::prepareToPlay(const double sample_rate, const int samples_per_block) {
    for (size_t i = 0; i < 2; ++i) {
        main_buffer_[i].resize(static_cast<size_t>(samples_per_block));
        main_pointers_[i] = main_buffer_[i].data();
        side_buffer_[i].resize(static_cast<size_t>(samples_per_block));
        side_pointers_[i] = side_buffer_[i].data();
    }
    updateChannelLayout();
    const juce::PluginHostType hostType;
    update_channel_layout_per_call_ = hostType.isMaschine();
    controller_.prepare(sample_rate, static_cast<size_t>(samples_per_block));
    sample_rate_.store(sample_rate, std::memory_order::relaxed);
}

void PluginProcessor::releaseResources() {
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()) {
        return true;
    }
    if (layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()) {
        return true;
    }
    return false;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    if (bypass_.load(std::memory_order::relaxed) > .5f) {
        processBlockInternal<true>(buffer);
    } else {
        processBlockInternal<false>(buffer);
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) {
    if (bypass_.load(std::memory_order::relaxed) > .5f) {
        processBlockInternal<true>(buffer);
    } else {
        processBlockInternal<false>(buffer);
    }
}

void PluginProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    processBlockInternal<true>(buffer);
}

void PluginProcessor::processBlockBypassed(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) {
    processBlockInternal<true>(buffer);
}

template <bool bypass>
void PluginProcessor::processBlockInternal(juce::AudioBuffer<float>& buffer) {
    juce::ScopedNoDenormals no_denormals;
    if (buffer.getNumSamples() == 0) {
        return; // ignore empty blocks
    }
    if (update_channel_layout_per_call_) {
        updateChannelLayout();
    }
    const auto num_samples = static_cast<size_t>(buffer.getNumSamples());

    if constexpr (bypass) {
        if (channel_layout_ == kMain1Aux0) {
            zldsp::vector::copy(main_pointers_[0], buffer.getReadPointer(0), num_samples);
            zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
        } else {
            zldsp::vector::copy(main_pointers_[0], buffer.getReadPointer(0), num_samples);
            if (buffer.getNumChannels() > 1) {
                zldsp::vector::copy(main_pointers_[1], buffer.getReadPointer(1), num_samples);
            } else {
                zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
            }
        }
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], main_pointers_[1], num_samples);
        controller_.template process<true>(main_pointers_, side_pointers_, num_samples);
        return;
    }

    switch (channel_layout_) {
    case kMain1Aux0: {
        zldsp::vector::copy(main_pointers_[0], buffer.getReadPointer(0), num_samples);
        zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], side_pointers_[0], num_samples);
        controller_.template process<bypass>(main_pointers_, side_pointers_, num_samples);
        zldsp::vector::copy(buffer.getWritePointer(0), main_pointers_[0], num_samples);
        break;
    }
    case kMain2Aux0: {
        zldsp::vector::copy(main_pointers_[0], buffer.getReadPointer(0), num_samples);
        zldsp::vector::copy(main_pointers_[1], buffer.getReadPointer(1), num_samples);
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], main_pointers_[1], num_samples);
        controller_.template process<bypass>(main_pointers_, side_pointers_, num_samples);
        zldsp::vector::copy(buffer.getWritePointer(0), main_pointers_[0], num_samples);
        zldsp::vector::copy(buffer.getWritePointer(1), main_pointers_[1], num_samples);
        break;
    }
    default: {
        return;
    }
    }

    const auto num_channels = buffer.getNumChannels();
    float peak_l = 0.0f;
    float peak_r = 0.0f;
    if (num_channels > 0) {
        peak_l = static_cast<float>(buffer.getMagnitude(0, 0, static_cast<int>(num_samples)));
    }
    if (num_channels > 1) {
        peak_r = static_cast<float>(buffer.getMagnitude(1, 0, static_cast<int>(num_samples)));
    } else {
        peak_r = peak_l;
    }
    output_peak_l_.store(peak_l, std::memory_order::relaxed);
    output_peak_r_.store(peak_r, std::memory_order::relaxed);
}

template <bool bypass>
void PluginProcessor::processBlockInternal(juce::AudioBuffer<double>& buffer) {
    juce::ScopedNoDenormals no_denormals;
    if (buffer.getNumSamples() == 0) {
        return; // ignore empty blocks
    }
    const auto num_samples = static_cast<size_t>(buffer.getNumSamples());

    if constexpr (bypass) {
        if (channel_layout_ == kMain1Aux0) {
            main_pointers_[0] = buffer.getWritePointer(0);
            zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
        } else {
            main_pointers_[0] = buffer.getWritePointer(0);
            if (buffer.getNumChannels() > 1) {
                main_pointers_[1] = buffer.getWritePointer(1);
            } else {
                zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
            }
        }
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], main_pointers_[1], num_samples);
        controller_.template process<true>(main_pointers_, side_pointers_, num_samples);
        return;
    }

    switch (channel_layout_) {
    case kMain1Aux0: {
        main_pointers_[0] = buffer.getWritePointer(0);
        zldsp::vector::copy(main_pointers_[1], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], side_pointers_[0], num_samples);
        controller_.template process<bypass>(main_pointers_, side_pointers_, num_samples);
        break;
    }
    case kMain2Aux0: {
        main_pointers_[0] = buffer.getWritePointer(0);
        main_pointers_[1] = buffer.getWritePointer(1);
        zldsp::vector::copy(side_pointers_[0], main_pointers_[0], num_samples);
        zldsp::vector::copy(side_pointers_[1], main_pointers_[1], num_samples);
        controller_.template process<bypass>(main_pointers_, side_pointers_, num_samples);
        break;
    }
    default: {
        return;
    }
    }

    const auto num_channels = buffer.getNumChannels();
    float peak_l = 0.0f;
    float peak_r = 0.0f;
    if (num_channels > 0) {
        peak_l = static_cast<float>(buffer.getMagnitude(0, 0, static_cast<int>(num_samples)));
    }
    if (num_channels > 1) {
        peak_r = static_cast<float>(buffer.getMagnitude(1, 0, static_cast<int>(num_samples)));
    } else {
        peak_r = peak_l;
    }
    output_peak_l_.store(peak_l, std::memory_order::relaxed);
    output_peak_r_.store(peak_r, std::memory_order::relaxed);
}

bool PluginProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
    // return new juce::GenericAudioProcessorEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& dest_data) {
    auto temp_tree = juce::ValueTree("ZLCompressorParaState");
    temp_tree.appendChild(parameters_.copyState(), nullptr);
    temp_tree.appendChild(parameters_NA_.copyState(), nullptr);
    const std::unique_ptr<juce::XmlElement> xml(temp_tree.createXml());
    copyXmlToBinary(*xml, dest_data);
}

void PluginProcessor::setStateInformation(const void* data, const int size_in_bytes) {
    juce::ScopedValueSetter<bool> svs(is_restoring_or_loading_, true);
    std::unique_ptr<juce::XmlElement> xml_state(getXmlFromBinary(data, size_in_bytes));
    if (xml_state != nullptr && xml_state->hasTagName("ZLCompressorParaState")) {
        auto temp_tree = juce::ValueTree::fromXml(*xml_state);
        auto params_tree = temp_tree.getChildWithName(parameters_.state.getType());
        if (params_tree.isValid()) {
            for (int i = 0; i < params_tree.getNumChildren(); ++i) {
                auto child = params_tree.getChild(i);
                if (child.hasType("PARAM")) {
                    juce::String id = child.getProperty("id").toString();
                    if (id.startsWith(zlp::PFilterType::kID) && !id.startsWith(zlp::PSideFilterType::kID)) {
                        float val = static_cast<float>(child.getProperty("value"));
                        int val_int = std::round(val);
                        int new_val_int = val_int;
                        if (val_int == 5 || val_int == 6) {
                            new_val_int = 0; // Peak
                        } else if (val_int == 7) {
                            new_val_int = 5; // Tilt Shelf
                        } else if (val_int == 8) {
                            new_val_int = 6; // Flat Tilt
                        }
                        if (new_val_int != val_int) {
                            child.setProperty("value", static_cast<float>(new_val_int), nullptr);
                        }
                    }
                }
            }
        }
        parameters_.replaceState(params_tree);
        parameters_NA_.replaceState(temp_tree.getChildWithName(parameters_NA_.state.getType()));
        state_A_ = parameters_.copyState();
        state_B_ = parameters_.copyState();
        is_state_B_active_ = false;
    }
}

void PluginProcessor::updateChannelLayout() {
    // determine current channel layout
    const auto* main_bus = getBus(true, 0);
    channel_layout_ = ChannelLayout::kInvalid;
    if (main_bus == nullptr) {
        return;
    }
    if (main_bus->getCurrentLayout() == juce::AudioChannelSet::mono()) {
        channel_layout_ = ChannelLayout::kMain1Aux0;
    } else if (main_bus->getCurrentLayout() == juce::AudioChannelSet::stereo()) {
        channel_layout_ = ChannelLayout::kMain2Aux0;
    }
}

void PluginProcessor::toggleABState() {
    juce::ScopedValueSetter<bool> svs(is_restoring_or_loading_, true);
    if (is_state_B_active_) {
        state_B_ = parameters_.copyState();
        parameters_.replaceState(state_A_);
        is_state_B_active_ = false;
    } else {
        state_A_ = parameters_.copyState();
        parameters_.replaceState(state_B_);
        is_state_B_active_ = true;
    }
}

void PluginProcessor::copyActiveStateToInactive() {
    if (is_state_B_active_) {
        state_A_ = parameters_.copyState();
    } else {
        state_B_ = parameters_.copyState();
    }
}

void PluginProcessor::loadPreset(int preset_index) {
    juce::ScopedValueSetter<bool> svs(is_restoring_or_loading_, true);
    // 1. Reset all bands to OFF first
    for (size_t i = 0; i < zlp::kBandNum; ++i) {
        const auto suffix = std::to_string(i);
        if (auto* status_param = parameters_.getParameter(zlp::PFilterStatus::kID + suffix)) {
            zlpanel::updateValue(status_param, status_param->convertTo0to1(0.f));
        }
    }
    
    // Helper lambda to configure a specific band
    auto set_band = [this](size_t band_idx, int type, float freq, float gain, float q) {
        const auto suffix = std::to_string(band_idx);
        
        if (auto* type_param = parameters_.getParameter(zlp::PFilterType::kID + suffix))
            zlpanel::updateValue(type_param, type_param->convertTo0to1(static_cast<float>(type)));
            
        if (auto* freq_param = parameters_.getParameter(zlp::PFreq::kID + suffix))
            zlpanel::updateValue(freq_param, freq_param->convertTo0to1(freq));
            
        if (auto* gain_param = parameters_.getParameter(zlp::PGain::kID + suffix))
            zlpanel::updateValue(gain_param, gain_param->convertTo0to1(gain));
            
        if (auto* target_gain_param = parameters_.getParameter(zlp::PTargetGain::kID + suffix))
            zlpanel::updateValue(target_gain_param, target_gain_param->convertTo0to1(gain));
            
        if (auto* q_param = parameters_.getParameter(zlp::PQ::kID + suffix))
            zlpanel::updateValue(q_param, q_param->convertTo0to1(q));
            
        if (auto* status_param = parameters_.getParameter(zlp::PFilterStatus::kID + suffix))
            zlpanel::updateValue(status_param, status_param->convertTo0to1(2.f)); // 2.f is "On"
    };

    // 2. Load the specific preset
    if (preset_index == 1) { // Vocal Warmth (Male Vocal)
        set_band(0, 2, 100.f, -3.f, 0.707f);
        set_band(1, 0, 200.f, 2.f, 1.0f);
        set_band(2, 0, 3000.f, 1.5f, 0.8f);
        set_band(3, 3, 10000.f, 1.f, 0.707f);
    } else if (preset_index == 2) { // Vocal Bright (Female Vocal)
        set_band(0, 1, 80.f, 0.f, 0.707f);
        set_band(1, 0, 250.f, -1.5f, 1.0f);
        set_band(2, 0, 5000.f, 2.5f, 0.8f);
        set_band(3, 3, 12000.f, 3.f, 0.707f);
    } else if (preset_index == 3) { // Bass Boost
        set_band(0, 2, 80.f, 4.f, 0.707f);
        set_band(1, 0, 150.f, 1.5f, 1.0f);
    } else if (preset_index == 4) { // Acoustic Guitar
        set_band(0, 1, 80.f, 0.f, 0.707f);
        set_band(1, 0, 240.f, -2.f, 1.2f);
        set_band(2, 0, 2000.f, 2.f, 0.9f);
        set_band(3, 3, 8000.f, 2.f, 0.707f);
    } else if (preset_index == 5) { // Telephone
        set_band(0, 1, 400.f, 0.f, 0.707f);
        set_band(1, 4, 4000.f, 0.f, 0.707f);
        set_band(2, 0, 1500.f, 3.f, 1.0f);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE

createPluginFilter() {
    return new PluginProcessor();
}
