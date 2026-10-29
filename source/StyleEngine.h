#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>
#include <array>
#include <memory>
#include "StyleParser.h"
#include "ChordDetector.h"

class SynthManager;

struct ScheduledMidiEvent
{
    int64_t tickOffset { 0 };
    juce::MidiMessage message;
};

class StyleEngine
{
public:
    StyleEngine();
    ~StyleEngine() = default;

    void prepareToPlay(double sampleRate) noexcept;
    void setStyle(std::shared_ptr<StyleParser> parser, SynthManager& synth);

    // Section Selection & Transitions
    void selectSection(const juce::String& sectionName);
    void transitionToMain(int mainIndex1To4, bool triggerFill);
    void triggerBreakOrFill();
    void triggerIntro(int introIndex1To3);
    void triggerEnding(int endingIndex1To3);

    // Playback Controls
    void play() noexcept;
    void stop(SynthManager& synth) noexcept;
    bool isPlaying() const noexcept { return playing.load(std::memory_order_relaxed); }

    void setBpm(double newBpm) noexcept;
    double getBpm() const noexcept { return bpm.load(std::memory_order_relaxed); }

    void setAutoFillEnabled(bool enabled) noexcept { autoFillEnabled.store(enabled, std::memory_order_relaxed); }
    bool isAutoFillEnabled() const noexcept { return autoFillEnabled.load(std::memory_order_relaxed); }

    void setSyncStartEnabled(bool enabled) noexcept { syncStartEnabled.store(enabled, std::memory_order_relaxed); }
    bool isSyncStartEnabled() const noexcept { return syncStartEnabled.load(std::memory_order_relaxed); }

    void setSyncStopEnabled(bool enabled) noexcept { syncStopEnabled.store(enabled, std::memory_order_relaxed); }
    bool isSyncStopEnabled() const noexcept { return syncStopEnabled.load(std::memory_order_relaxed); }

    void setOtsLinkEnabled(bool enabled) noexcept { otsLinkEnabled.store(enabled, std::memory_order_relaxed); }
    bool isOtsLinkEnabled() const noexcept { return otsLinkEnabled.load(std::memory_order_relaxed); }

    int getCurrentVariationIndex() const noexcept { return currentSection.variationIndex; }
    double getCurrentTick() const noexcept { return currentTick; }
    int64_t getCurrentSectionLength() const noexcept { return currentSection.lengthTicks; }

    juce::String getCurrentSectionName() const noexcept { return currentSection.name; }
    void triggerBreak();

    void processBlock(int numSamples, SynthManager& synth, const ChordDetector& chordDetector) noexcept;
    void allNotesOff(SynthManager& synth) noexcept;

private:
    int transposeNote(int channel, int sourcePitch, const ChordData& chord) const noexcept;
    void updateSectionEvents();
    void sendSetupMixerEvents(SynthManager& synth);

    juce::String findMatchingSection(const juce::StringArray& candidates) const;

    std::shared_ptr<StyleParser> currentParser;
    StyleSection currentSection;
    std::vector<ScheduledMidiEvent> activeEvents;

    // Queued transition section after Fill finishes
    juce::String queuedSectionAfterFill;

    double currentSampleRate { 44100.0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> autoFillEnabled { true };
    std::atomic<bool> syncStartEnabled { false };
    std::atomic<bool> syncStopEnabled { false };
    std::atomic<bool> otsLinkEnabled { true };
    double currentTick { 0.0 };

    ChordData lastProcessedChord;
    std::array<std::array<int8_t, 128>, 16> soundingNotes {};
    std::array<std::array<uint8_t, 128>, 16> soundingVelocities {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyleEngine)
};