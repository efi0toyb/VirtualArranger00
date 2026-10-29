#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <cstdint>

class SynthManager;

enum class ChordType : uint8_t
{
    None = 0,
    Major,
    Minor,
    Seventh,
    MajorSeventh,
    MinorSeventh,
    MinorMajorSeventh,
    Diminished,
    DiminishedSeventh,
    HalfDiminished,
    Augmented,
    Sus4,
    Sus2,
    SevenSus4,
    MajorSixth,
    MinorSixth
};

#pragma pack(push, 1)
struct ChordData
{
    int8_t rootNote { -1 };
    ChordType type { ChordType::None };
    int8_t bassNote { -1 };
    uint8_t notesCount { 0 };

    bool operator==(const ChordData& other) const noexcept
    {
        return rootNote == other.rootNote &&
               type == other.type &&
               bassNote == other.bassNote &&
               notesCount == other.notesCount;
    }
};
#pragma pack(pop)

static_assert(sizeof(ChordData) == 4, "ChordData must be exactly 4 bytes for lock-free atomic access");

class ChordDetector
{
public:
    ChordDetector() noexcept;

    void setSplitPoint(int noteNumber) noexcept;
    int getSplitPoint() const noexcept { return splitPoint.load(std::memory_order_relaxed); }

    void setSingleFingerMode(bool enabled) noexcept;
    bool isSingleFingerMode() const noexcept { return singleFingerMode.load(std::memory_order_relaxed); }

    void processMidiMessage(const juce::MidiMessage& msg, SynthManager& synth) noexcept;

    ChordData getCurrentChord() const noexcept
    {
        return currentChord.load(std::memory_order_acquire);
    }

    // Lock-Free Key State queries for the UI
    bool isNoteActiveLeft(int note) const noexcept
    {
        if (note < 0 || note > 127) return false;
        if (note < 64)
            return (activeLeftLo.load(std::memory_order_relaxed) & (1ULL << note)) != 0;
        else
            return (activeLeftHi.load(std::memory_order_relaxed) & (1ULL << (note - 64))) != 0;
    }

    bool isNoteActiveRight(int note) const noexcept
    {
        if (note < 0 || note > 127) return false;
        if (note < 64)
            return (activeRightLo.load(std::memory_order_relaxed) & (1ULL << note)) != 0;
        else
            return (activeRightHi.load(std::memory_order_relaxed) & (1ULL << (note - 64))) != 0;
    }

    static juce::String getRootName(int root) noexcept;
    static juce::String getChordTypeName(ChordType type) noexcept;
    static juce::String getChordName(const ChordData& chord) noexcept;

    void reset() noexcept;

private:
    void analyzeChordOnNoteOn() noexcept;
    bool analyzeSingleFinger(const std::vector<int>& notes) noexcept;
    void analyzeFullFingered(const std::vector<int>& notes) noexcept;

    static inline bool isBlackKey(int midiNote) noexcept
    {
        const int pc = midiNote % 12;
        return (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
    }

    std::atomic<int> splitPoint { 60 };
    std::atomic<bool> singleFingerMode { true };
    std::atomic<ChordData> currentChord;

    // 64-bit atomic bitmasks for 100% lock-free UI key visualization
    std::atomic<uint64_t> activeLeftLo  { 0 };
    std::atomic<uint64_t> activeLeftHi  { 0 };
    std::atomic<uint64_t> activeRightLo { 0 };
    std::atomic<uint64_t> activeRightHi { 0 };

    std::array<bool, 128> leftHandActiveNotes {};
    std::array<bool, 128> rightHandActiveNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordDetector)
};