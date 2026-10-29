#include "ChordDetector.h"
#include "SynthManager.h"
#include <vector>

ChordDetector::ChordDetector() noexcept
{
    ChordData empty;
    currentChord.store(empty, std::memory_order_relaxed);
    reset();
}

void ChordDetector::setSplitPoint(int noteNumber) noexcept
{
    splitPoint.store(juce::jlimit(0, 127, noteNumber), std::memory_order_relaxed);
}

void ChordDetector::setSingleFingerMode(bool enabled) noexcept
{
    singleFingerMode.store(enabled, std::memory_order_relaxed);
}

void ChordDetector::reset() noexcept
{
    leftHandActiveNotes.fill(false);
    rightHandActiveNotes.fill(false);
    activeLeftLo.store(0, std::memory_order_relaxed);
    activeLeftHi.store(0, std::memory_order_relaxed);
    activeRightLo.store(0, std::memory_order_relaxed);
    activeRightHi.store(0, std::memory_order_relaxed);

    ChordData empty;
    currentChord.store(empty, std::memory_order_release);
}

void ChordDetector::processMidiMessage(const juce::MidiMessage& msg, SynthManager& synth) noexcept
{
    if (msg.isNoteOn())
    {
        const int note = msg.getNoteNumber();
        const int vel  = msg.getVelocity();
        const int currentSplit = splitPoint.load(std::memory_order_relaxed);

        if (note >= currentSplit)
        {
            const int boostedVel = juce::jlimit(1, 127, static_cast<int>(vel * 1.25f + 15));
            rightHandActiveNotes[note] = true;

            if (note < 64) activeRightLo.fetch_or(1ULL << note, std::memory_order_relaxed);
            else           activeRightHi.fetch_or(1ULL << (note - 64), std::memory_order_relaxed);

            synth.noteOn(0, note, boostedVel);
            if (synth.isRight2Active()) synth.noteOn(1, note, boostedVel);
            if (synth.isRight3Active()) synth.noteOn(2, note, boostedVel);
        }
        else
        {
            leftHandActiveNotes[note] = true;

            if (note < 64) activeLeftLo.fetch_or(1ULL << note, std::memory_order_relaxed);
            else           activeLeftHi.fetch_or(1ULL << (note - 64), std::memory_order_relaxed);

            if (synth.isLeftActive()) synth.noteOn(3, note, vel);
            analyzeChordOnNoteOn();
        }
    }
    else if (msg.isNoteOff())
    {
        const int note = msg.getNoteNumber();

        if (rightHandActiveNotes[note])
        {
            rightHandActiveNotes[note] = false;

            if (note < 64) activeRightLo.fetch_and(~(1ULL << note), std::memory_order_relaxed);
            else           activeRightHi.fetch_and(~(1ULL << (note - 64)), std::memory_order_relaxed);

            synth.noteOff(0, note);
            synth.noteOff(1, note);
            synth.noteOff(2, note);
        }

        if (leftHandActiveNotes[note])
        {
            leftHandActiveNotes[note] = false;

            if (note < 64) activeLeftLo.fetch_and(~(1ULL << note), std::memory_order_relaxed);
            else           activeLeftHi.fetch_and(~(1ULL << (note - 64)), std::memory_order_relaxed);

            if (synth.isLeftActive()) synth.noteOff(3, note);
        }
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        reset();
        synth.allNotesOff();
    }
    else if (msg.isPitchWheel())
    {
        synth.pitchBend(0, msg.getPitchWheelValue());
    }
    else if (msg.isController())
    {
        synth.controlChange(0, msg.getControllerNumber(), msg.getControllerValue());
    }
}

void ChordDetector::analyzeChordOnNoteOn() noexcept
{
    std::vector<int> notes;
    notes.reserve(16);

    for (int i = 0; i < 128; ++i)
    {
        if (leftHandActiveNotes[i])
            notes.push_back(i);
    }

    if (notes.empty())
        return;

    if (notes.size() >= 3)
    {
        analyzeFullFingered(notes);
        if (currentChord.load(std::memory_order_relaxed).type != ChordType::None)
            return;
    }

    if (singleFingerMode.load(std::memory_order_relaxed))
    {
        if (analyzeSingleFinger(notes))
            return;
    }

    if (notes.size() < 3)
    {
        analyzeFullFingered(notes);
    }
}

bool ChordDetector::analyzeSingleFinger(const std::vector<int>& notes) noexcept
{
    const size_t count = notes.size();
    if (count == 0 || count > 3)
        return false;

    const int rootKey = notes.back();
    const int root = rootKey % 12;

    if (count == 1)
    {
        ChordData chord;
        chord.rootNote = static_cast<int8_t>(root);
        chord.type = ChordType::Major;
        chord.bassNote = static_cast<int8_t>(root);
        chord.notesCount = 1;
        currentChord.store(chord, std::memory_order_release);
        return true;
    }
    else if (count == 2)
    {
        const int modKey = notes[0];
        const int semitoneDistance = rootKey - modKey;
        if (semitoneDistance >= 1 && semitoneDistance <= 3)
        {
            ChordData chord;
            chord.rootNote = static_cast<int8_t>(root);
            chord.bassNote = static_cast<int8_t>(root);
            chord.notesCount = 2;

            if (isBlackKey(modKey))
                chord.type = ChordType::Minor;
            else
                chord.type = ChordType::Seventh;

            currentChord.store(chord, std::memory_order_release);
            return true;
        }
    }
    else if (count == 3)
    {
        const int k1 = notes[0];
        const int k2 = notes[1];

        if ((rootKey - k1 <= 3) && (rootKey - k2 <= 3))
        {
            const bool hasBlack = isBlackKey(k1) || isBlackKey(k2);
            const bool hasWhite = (!isBlackKey(k1)) || (!isBlackKey(k2));

            if (hasBlack && hasWhite)
            {
                ChordData chord;
                chord.rootNote = static_cast<int8_t>(root);
                chord.type = ChordType::MinorSeventh;
                chord.bassNote = static_cast<int8_t>(root);
                chord.notesCount = 3;
                currentChord.store(chord, std::memory_order_release);
                return true;
            }
        }
    }

    return false;
}

void ChordDetector::analyzeFullFingered(const std::vector<int>& notes) noexcept
{
    uint16_t pitchClassMask = 0;
    const int lowestNote = notes.front();
    const int bass = lowestNote % 12;

    for (int n : notes)
        pitchClassMask |= static_cast<uint16_t>(1 << (n % 12));

    constexpr uint16_t MASK_MAJOR     = (1 << 0) | (1 << 4) | (1 << 7);
    constexpr uint16_t MASK_MINOR     = (1 << 0) | (1 << 3) | (1 << 7);
    constexpr uint16_t MASK_DOM7      = (1 << 0) | (1 << 4) | (1 << 7) | (1 << 10);
    constexpr uint16_t MASK_MAJ7      = (1 << 0) | (1 << 4) | (1 << 7) | (1 << 11);
    constexpr uint16_t MASK_MIN7      = (1 << 0) | (1 << 3) | (1 << 7) | (1 << 10);
    constexpr uint16_t MASK_MMAJ7     = (1 << 0) | (1 << 3) | (1 << 7) | (1 << 11);
    constexpr uint16_t MASK_DIM       = (1 << 0) | (1 << 3) | (1 << 6);
    constexpr uint16_t MASK_DIM7      = (1 << 0) | (1 << 3) | (1 << 6) | (1 << 9);
    constexpr uint16_t MASK_M7B5      = (1 << 0) | (1 << 3) | (1 << 6) | (1 << 10);
    constexpr uint16_t MASK_AUG       = (1 << 0) | (1 << 4) | (1 << 8);
    constexpr uint16_t MASK_SUS4      = (1 << 0) | (1 << 5) | (1 << 7);
    constexpr uint16_t MASK_SUS2      = (1 << 0) | (1 << 2) | (1 << 7);
    constexpr uint16_t MASK_7SUS4     = (1 << 0) | (1 << 5) | (1 << 7) | (1 << 10);
    constexpr uint16_t MASK_MAJ6      = (1 << 0) | (1 << 4) | (1 << 7) | (1 << 9);
    constexpr uint16_t MASK_MIN6      = (1 << 0) | (1 << 3) | (1 << 7) | (1 << 9);

    constexpr uint16_t MASK_DYAD_MIN  = (1 << 0) | (1 << 3);
    constexpr uint16_t MASK_DYAD_MAJ  = (1 << 0) | (1 << 4);
    constexpr uint16_t MASK_DYAD_7TH  = (1 << 0) | (1 << 10);

    for (int r = 0; r < 12; ++r)
    {
        const uint16_t rotated = static_cast<uint16_t>(
            ((pitchClassMask >> r) | (pitchClassMask << (12 - r))) & 0x0FFF
        );

        ChordType detectedType = ChordType::None;

        if      ((rotated & MASK_DOM7) == MASK_DOM7)   detectedType = ChordType::Seventh;
        else if ((rotated & MASK_MAJ7) == MASK_MAJ7)   detectedType = ChordType::MajorSeventh;
        else if ((rotated & MASK_MIN7) == MASK_MIN7)   detectedType = ChordType::MinorSeventh;
        else if ((rotated & MASK_MMAJ7) == MASK_MMAJ7) detectedType = ChordType::MinorMajorSeventh;
        else if ((rotated & MASK_DIM7) == MASK_DIM7)   detectedType = ChordType::DiminishedSeventh;
        else if ((rotated & MASK_M7B5) == MASK_M7B5)   detectedType = ChordType::HalfDiminished;
        else if ((rotated & MASK_7SUS4) == MASK_7SUS4) detectedType = ChordType::SevenSus4;
        else if ((rotated & MASK_MAJ6) == MASK_MAJ6)   detectedType = ChordType::MajorSixth;
        else if ((rotated & MASK_MIN6) == MASK_MIN6)   detectedType = ChordType::MinorSixth;
        else if ((rotated & MASK_MAJOR) == MASK_MAJOR) detectedType = ChordType::Major;
        else if ((rotated & MASK_MINOR) == MASK_MINOR) detectedType = ChordType::Minor;
        else if ((rotated & MASK_DIM) == MASK_DIM)     detectedType = ChordType::Diminished;
        else if ((rotated & MASK_AUG) == MASK_AUG)     detectedType = ChordType::Augmented;
        else if ((rotated & MASK_SUS4) == MASK_SUS4)   detectedType = ChordType::Sus4;
        else if ((rotated & MASK_SUS2) == MASK_SUS2)   detectedType = ChordType::Sus2;
        else if (notes.size() == 2)
        {
            if      ((rotated & MASK_DYAD_MIN) == MASK_DYAD_MIN) detectedType = ChordType::Minor;
            else if ((rotated & MASK_DYAD_MAJ) == MASK_DYAD_MAJ) detectedType = ChordType::Major;
            else if ((rotated & MASK_DYAD_7TH) == MASK_DYAD_7TH) detectedType = ChordType::Seventh;
        }

        if (detectedType != ChordType::None)
        {
            if (r == bass || currentChord.load(std::memory_order_relaxed).rootNote != r)
            {
                ChordData chord;
                chord.rootNote = static_cast<int8_t>(r);
                chord.type = detectedType;
                chord.bassNote = static_cast<int8_t>(bass);
                chord.notesCount = static_cast<uint8_t>(notes.size());
                currentChord.store(chord, std::memory_order_release);
                return;
            }
        }
    }
}

juce::String ChordDetector::getRootName(int root) noexcept
{
    static const char* const names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    if (root >= 0 && root < 12)
        return names[root];
    return "--";
}

juce::String ChordDetector::getChordTypeName(ChordType type) noexcept
{
    switch (type)
    {
        case ChordType::Major:             return "Maj";
        case ChordType::Minor:             return "min";
        case ChordType::Seventh:           return "7";
        case ChordType::MajorSeventh:      return "Maj7";
        case ChordType::MinorSeventh:      return "m7";
        case ChordType::MinorMajorSeventh: return "mMaj7";
        case ChordType::Diminished:        return "dim";
        case ChordType::DiminishedSeventh: return "dim7";
        case ChordType::HalfDiminished:    return "m7b5";
        case ChordType::Augmented:         return "aug";
        case ChordType::Sus4:              return "sus4";
        case ChordType::Sus2:              return "sus2";
        case ChordType::SevenSus4:         return "7sus4";
        case ChordType::MajorSixth:        return "6";
        case ChordType::MinorSixth:        return "m6";
        case ChordType::None:
        default:                           return "";
    }
}

juce::String ChordDetector::getChordName(const ChordData& chord) noexcept
{
    if (chord.rootNote < 0 || chord.type == ChordType::None)
        return "--";

    juce::String str = getRootName(chord.rootNote) + " " + getChordTypeName(chord.type);

    if (chord.bassNote >= 0 && chord.bassNote != chord.rootNote)
        str += "/" + getRootName(chord.bassNote);

    return str;
}