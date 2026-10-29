#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <fluidsynth.h>
#include <atomic>

// Forward declaration
struct OneTouchSetting;

class SynthManager
{
public:
    SynthManager();
    ~SynthManager();

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
    void releaseResources();

    bool loadSoundFont(const juce::File& sf2File);
    bool isSoundFontLoaded() const noexcept { return soundFontLoaded.load(std::memory_order_relaxed); }

    // Master Volume Control
    void setMasterVolume(float newVolume) noexcept;
    float getMasterVolume() const noexcept { return masterVolume.load(std::memory_order_relaxed); }

    // Lead Sound (Right Hand) Selection & Volume
    void setLeadPreset(int programNumber) noexcept;
    void setLeadVolume(float volumeMultiplier) noexcept;

    // Apply One Touch Settings (OTS) from Style
    void applyOTSSettings(const OneTouchSetting& ots) noexcept;
    bool isRight2Active() const noexcept { return right2Active.load(std::memory_order_relaxed); }
    bool isRight3Active() const noexcept { return right3Active.load(std::memory_order_relaxed); }
    bool isLeftActive() const noexcept { return leftActive.load(std::memory_order_relaxed); }
    // Real-Time Audio Rendering
    void renderAudio(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) noexcept;

    // Real-Time MIDI Control
    void noteOn(int channel, int noteNumber, int velocity) noexcept;
    void noteOff(int channel, int noteNumber) noexcept;
    void controlChange(int channel, int controller, int value) noexcept;
    void programChange(int channel, int program) noexcept;
    void pitchBend(int channel, int value) noexcept;
    void allNotesOff() noexcept;

    void resetMixer() noexcept;

    // Style Accompaniment Volume & Channel Mixer
    void setStyleVolume(float volumeMultiplier) noexcept;
    float getStyleVolume() const noexcept { return styleVolume.load(std::memory_order_relaxed); }

    void setChannelVolume(int channel, int volume0To127) noexcept;
    int getChannelVolume(int channel) const noexcept;
    

private:
    void initSynth(double sampleRate);
    void destroySynth();

    fluid_settings_t* settings { nullptr };
    fluid_synth_t* synth { nullptr };
    int currentSoundFontId { -1 };

    double currentSampleRate { 44100.0 };
    int maxBlockSize { 512 };

    std::atomic<bool> soundFontLoaded { false };
    std::atomic<bool> isBusyLoading { false };
    std::atomic<float> masterVolume { 1.0f };
    std::atomic<int> currentLeadProgram { 29 }; // OTS 4 default
    std::atomic<float> styleVolume { 0.85f }; // Default 85% style balance
    std::array<std::atomic<int>, 16> channelVolumes;

    // OTS Active Part Flags
    std::atomic<bool> right2Active { false };
    std::atomic<bool> right3Active { false };
    std::atomic<bool> leftActive { false };

    juce::HeapBlock<float> monoScratchBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthManager)
};