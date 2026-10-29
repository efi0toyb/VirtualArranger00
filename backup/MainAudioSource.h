#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "SynthManager.h"
#include "ChordDetector.h"
#include "StyleEngine.h"

class MainAudioSource : public juce::AudioSource,
                        public juce::MidiInputCallback
{
public:
    MainAudioSource();
    ~MainAudioSource() override = default;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    SynthManager& getSynthManager() noexcept { return synthManager; }
    ChordDetector& getChordDetector() noexcept { return chordDetector; }
    StyleEngine& getStyleEngine() noexcept { return styleEngine; }

private:
    SynthManager synthManager;
    ChordDetector chordDetector;
    StyleEngine styleEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainAudioSource)
};