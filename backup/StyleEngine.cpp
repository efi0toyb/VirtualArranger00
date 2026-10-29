#include "StyleEngine.h"
#include "SynthManager.h"

StyleEngine::StyleEngine()
{
    for (auto& ch : soundingNotes)
        ch.fill(-1);
}

void StyleEngine::prepareToPlay(double sampleRate) noexcept
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void StyleEngine::sendSetupMixerEvents(SynthManager& synth)
{
    if (currentParser == nullptr) return;

    synth.resetMixer();

    for (const auto& msg : currentParser->getInitSetupEvents())
    {
        const int ch = juce::jlimit(0, 15, msg.getChannel() - 1);
        if (ch == 0) continue; // Protect Right Hand

        if (msg.isProgramChange())
        {
            synth.programChange(ch, msg.getProgramChangeNumber());
        }
        else if (msg.isController())
        {
            const int ctrl = msg.getControllerNumber();
            const int val  = msg.getControllerValue();

            if (ctrl == 7) // Channel Volume
            {
                synth.setChannelVolume(ch, val);
            }
            else
            {
                synth.controlChange(ch, ctrl, val);
            }
        }
    }
}

void StyleEngine::setStyle(std::shared_ptr<StyleParser> parser, SynthManager& synth)
{
    allNotesOff(synth);
    currentParser = parser;

    if (currentParser != nullptr && currentParser->hasStyleLoaded())
    {
        setBpm(currentParser->getInitialBpm());

        // 1. Select Main A (Main 1) FIRST so section rules are active
        transitionToMain(1, false);

        // 2. Dispatch Measure 1 mixer events to match the active style
        sendSetupMixerEvents(synth);
    }
}

juce::String StyleEngine::findMatchingSection(const juce::StringArray& candidates) const
{
    if (currentParser == nullptr) return {};

    for (const auto& candidate : candidates)
    {
        for (const auto& sec : currentParser->getSections())
        {
            if (sec.name.replace(" ", "").equalsIgnoreCase(candidate.replace(" ", "")))
                return sec.name;
        }
    }

    for (const auto& candidate : candidates)
    {
        for (const auto& sec : currentParser->getSections())
        {
            if (sec.name.containsIgnoreCase(candidate))
                return sec.name;
        }
    }
    return {};
}

void StyleEngine::selectSection(const juce::String& sectionName)
{
    if (currentParser == nullptr) return;

    const auto* sec = currentParser->getSection(sectionName);
    if (sec != nullptr)
    {
        currentSection = *sec;
        updateSectionEvents();
        currentTick = 0.0;
    }
}

void StyleEngine::transitionToMain(int mainIndex1To4, bool triggerFill)
{
    if (currentParser == nullptr) return;

    const auto* targetSec = currentParser->findMainSection(mainIndex1To4);
    if (targetSec == nullptr) return;

    if (triggerFill && isPlaying() && autoFillEnabled.load(std::memory_order_relaxed))
    {
        // Find current main variation index
        int currentVarIdx = 1;
        if (currentSection.role == SectionRole::Main)
            currentVarIdx = currentSection.variationIndex;

        // Guaranteed to return ONLY a Fill-in (never Intro or Ending)
        const auto* fillSec = currentParser->findFillSection(currentVarIdx, mainIndex1To4);
        if (fillSec != nullptr)
        {
            queuedSectionAfterFill = targetSec->name;
            selectSection(fillSec->name);
            return;
        }
    }

    queuedSectionAfterFill.clear();
    selectSection(targetSec->name);
}

void StyleEngine::triggerBreakOrFill()
{
    if (currentParser == nullptr) return;

    // Use current main variation index for fill
    int currentVarIdx = 1;
    if (currentSection.role == SectionRole::Main)
        currentVarIdx = currentSection.variationIndex;

    const auto* fillSec = currentParser->findFillSection(currentVarIdx, currentVarIdx);
    if (fillSec != nullptr)
    {
        if (currentSection.role != SectionRole::Fill)
            queuedSectionAfterFill = currentSection.name;

        selectSection(fillSec->name);
    }
}

void StyleEngine::triggerIntro(int introIndex1To3)
{
    if (currentParser == nullptr) return;

    const auto* introSec = currentParser->findIntroSection(introIndex1To3);
    if (introSec != nullptr)
    {
        const auto* mainSec = currentParser->findMainSection(1);
        queuedSectionAfterFill = (mainSec != nullptr) ? mainSec->name : juce::String();
        selectSection(introSec->name);
        play();
    }
}

void StyleEngine::triggerEnding(int endingIndex1To3)
{
    if (currentParser == nullptr) return;

    const auto* endingSec = currentParser->findEndingSection(endingIndex1To3);
    if (endingSec != nullptr)
    {
        queuedSectionAfterFill = "__STOP__";
        selectSection(endingSec->name);
    }
}

void StyleEngine::updateSectionEvents()
{
    activeEvents.clear();
    if (currentParser == nullptr) return;

    const auto* seq = currentParser->getMidiSequence();
    if (seq == nullptr) return;

    const int64_t start = currentSection.startTick;
    const int64_t end   = currentSection.endTick;

    for (int i = 0; i < seq->getNumEvents(); ++i)
    {
        const auto* ev = seq->getEventPointer(i);
        const int64_t t = static_cast<int64_t>(ev->message.getTimeStamp());

        if (t >= start && t < end)
        {
            ScheduledMidiEvent sched;
            sched.tickOffset = t - start;
            sched.message = ev->message;
            activeEvents.push_back(sched);
        }
    }

    std::sort(activeEvents.begin(), activeEvents.end(), [](const ScheduledMidiEvent& a, const ScheduledMidiEvent& b) {
        return a.tickOffset < b.tickOffset;
    });
}

void StyleEngine::play() noexcept
{
    if (currentSection.lengthTicks <= 0 || currentSection.name.isEmpty())
        transitionToMain(1, false);

    currentTick = 0.0;
    playing.store(true, std::memory_order_release);
}

void StyleEngine::stop(SynthManager& synth) noexcept
{
    playing.store(false, std::memory_order_release);
    allNotesOff(synth);
    currentTick = 0.0;
    queuedSectionAfterFill.clear();
}

void StyleEngine::setBpm(double newBpm) noexcept
{
    bpm.store(juce::jlimit(30.0, 300.0, newBpm), std::memory_order_relaxed);
}

void StyleEngine::allNotesOff(SynthManager& synth) noexcept
{
    for (int ch = 0; ch < 16; ++ch)
    {
        for (int n = 0; n < 128; ++n)
        {
            if (soundingNotes[ch][n] >= 0)
            {
                synth.noteOff(ch, soundingNotes[ch][n]);
                soundingNotes[ch][n] = -1;
            }
        }
    }
    synth.allNotesOff();
}

void StyleEngine::processBlock(int numSamples, SynthManager& synth, const ChordDetector& chordDetector) noexcept
{
    if (!playing.load(std::memory_order_relaxed) ||
        currentParser == nullptr ||
        currentSection.lengthTicks <= 0 ||
        activeEvents.empty())
    {
        return;
    }

    const double currentBpm = bpm.load(std::memory_order_relaxed);
    const short ppq = currentParser->getTimeDivision();

    const double ticksPerSec = (currentBpm / 60.0) * ppq;
    const double ticksPerBlock = (ticksPerSec / currentSampleRate) * numSamples;

    const double startTick = currentTick;
    double endTick = startTick + ticksPerBlock;
    const int64_t loopLength = currentSection.lengthTicks;
    const ChordData currentChord = chordDetector.getCurrentChord();

    auto dispatchEventsInRange = [&](double from, double to)
    {
        for (const auto& ev : activeEvents)
        {
            if (ev.tickOffset >= from && ev.tickOffset < to)
            {
                const auto& msg = ev.message;
                const int ch = juce::jlimit(0, 15, msg.getChannel() - 1);

                // Channel 0 is reserved for live player's right hand
                if (ch == 0) continue;

                if (msg.isNoteOn())
                {
                    const int origPitch = msg.getNoteNumber();
                    const int transposedPitch = transposeNote(ch, origPitch, currentChord);

                    if (soundingNotes[ch][origPitch] >= 0)
                        synth.noteOff(ch, soundingNotes[ch][origPitch]);

                    soundingNotes[ch][origPitch] = static_cast<int8_t>(transposedPitch);
                    synth.noteOn(ch, transposedPitch, msg.getVelocity());
                }
                else if (msg.isNoteOff())
                {
                    const int origPitch = msg.getNoteNumber();
                    const int8_t soundingPitch = soundingNotes[ch][origPitch];

                    if (soundingPitch >= 0)
                    {
                        synth.noteOff(ch, soundingPitch);
                        soundingNotes[ch][origPitch] = -1;
                    }
                    else
                    {
                        synth.noteOff(ch, origPitch);
                    }
                }
                else if (msg.isController())
                {
                    if (msg.getControllerNumber() == 7) // Channel Volume
                    {
                        synth.setChannelVolume(ch, msg.getControllerValue());
                    }
                    else
                    {
                        synth.controlChange(ch, msg.getControllerNumber(), msg.getControllerValue());
                    }
                }
                else if (msg.isProgramChange())
                {
                    synth.programChange(ch, msg.getProgramChangeNumber());
                }
                else if (msg.isPitchWheel())
                {
                    synth.pitchBend(ch, msg.getPitchWheelValue());
                }
            }
        }
    };

    if (endTick < loopLength)
    {
        dispatchEventsInRange(startTick, endTick);
        currentTick = endTick;
    }
    else
    {
        dispatchEventsInRange(startTick, loopLength);

        if (queuedSectionAfterFill.isNotEmpty())
        {
            if (queuedSectionAfterFill == "__STOP__")
            {
                stop(synth);
                return;
            }

            juce::String nextSec = queuedSectionAfterFill;
            queuedSectionAfterFill.clear();
            selectSection(nextSec);
            currentTick = 0.0;
            return;
        }

        endTick = std::fmod(endTick, static_cast<double>(loopLength));
        dispatchEventsInRange(0.0, endTick);
        currentTick = endTick;
    }
}

int StyleEngine::transposeNote(int channel, int sourcePitch, const ChordData& chord) const noexcept
{
    // 1. DRUMS AND PERCUSSION (Ch 9 & 10 / index 8 & 9) NEVER TRANSPOSE
    if (channel == 8 || channel == 9)
        return sourcePitch;

    if (currentParser == nullptr)
        return sourcePitch;

    const auto& rule = currentSection.channelRules[channel];

    // Explicit CASM Bypass Check
    if (rule.ntt == NTTRule::Bypass || currentParser->isChannelDrumOrPercussion(channel))
        return sourcePitch;

    const int targetRoot = (chord.rootNote >= 0) ? chord.rootNote : 0;
    const ChordType targetType = (chord.type != ChordType::None) ? chord.type : ChordType::Major;
    const int sourceRoot = (rule.sourceRoot >= 0 && rule.sourceRoot <= 11) ? rule.sourceRoot : 0;

    auto isMinorType = [](ChordType t) noexcept {
        return t == ChordType::Minor ||
               t == ChordType::MinorSeventh ||
               t == ChordType::MinorMajorSeventh ||
               t == ChordType::Diminished ||
               t == ChordType::DiminishedSeventh ||
               t == ChordType::HalfDiminished ||
               t == ChordType::MinorSixth;
    };

    auto hasSeventhType = [](ChordType t) noexcept {
        return t == ChordType::Seventh ||
               t == ChordType::MajorSeventh ||
               t == ChordType::MinorSeventh ||
               t == ChordType::MinorMajorSeventh ||
               t == ChordType::DiminishedSeventh ||
               t == ChordType::HalfDiminished ||
               t == ChordType::SevenSus4;
    };

    // Calculate base root shift
    int rootDelta = targetRoot - sourceRoot;
    if (rootDelta < 0) rootDelta += 12;

    const int highKey = (rule.highKey >= 0 && rule.highKey <= 11) ? rule.highKey : 6;
    if (targetRoot > highKey)
        rootDelta -= 12;

    int transposed = sourcePitch + rootDelta;
    int degree = (sourcePitch - sourceRoot) % 12;
    if (degree < 0) degree += 12;

    // 2. Transposition Tables
    if (rule.ntt == NTTRule::Chord)
    {
        // CHORD TRACKS (Pianos, Guitars, Strings pads)
        if (degree == 4) // Major 3rd
        {
            if (isMinorType(targetType))
                transposed -= 1; // Major 3rd -> Minor 3rd
            else if (targetType == ChordType::Sus4 || targetType == ChordType::SevenSus4)
                transposed += 1; // 4th
            else if (targetType == ChordType::Sus2)
                transposed -= 2; // 2nd
        }
        else if (degree == 7) // 5th
        {
            if (targetType == ChordType::Diminished || targetType == ChordType::DiminishedSeventh || targetType == ChordType::HalfDiminished)
                transposed -= 1; // b5
            else if (targetType == ChordType::Augmented)
                transposed += 1; // #5
        }
        else if (degree == 10 || degree == 11) // 7th
        {
            if (hasSeventhType(targetType))
            {
                if (targetType == ChordType::MajorSeventh || targetType == ChordType::MinorMajorSeventh)
                {
                    if (degree == 10) transposed += 1;
                }
                else if (targetType == ChordType::DiminishedSeventh)
                {
                    if (degree == 11) transposed -= 2;
                    else if (degree == 10) transposed -= 1;
                }
                else
                {
                    if (degree == 11) transposed -= 1; // Maj7 -> b7
                }
            }
            else
            {
                // Collapse 7th to octave Root in simple triads
                if (degree == 11) transposed += 1;
                else if (degree == 10) transposed += 2;
            }
        }
        else if (degree == 9 && isMinorType(targetType) && targetType != ChordType::MinorSixth)
        {
            transposed -= 1;
        }
    }
    else if (rule.ntt == NTTRule::Melodic)
    {
        // MELODIC TRACKS (Phrase 1 & 2: Brass, Sax, Flute, Leads)
        // Natural scale transposition without destroying melodic lines!
        if (degree == 4 && isMinorType(targetType))
            transposed -= 1; // 3rd shifts to minor 3rd
        else if (degree == 7 && (targetType == ChordType::Diminished || targetType == ChordType::DiminishedSeventh))
            transposed -= 1;
        else if (degree == 9 && isMinorType(targetType))
            transposed -= 1; // 6th shifts to b6
        // Notice: 7th notes are PRESERVED in melodies, never collapsed into the root!
    }
    else if (rule.ntt == NTTRule::Bass)
    {
        // BASS TRACK: Follows Bass note + natural scale intervals
        const int effectiveBass = (chord.bassNote >= 0) ? chord.bassNote : targetRoot;
        int bassDelta = effectiveBass - sourceRoot;
        if (bassDelta < 0) bassDelta += 12;
        if (effectiveBass > highKey) bassDelta -= 12;

        transposed = sourcePitch + bassDelta;

        if (degree == 4) // 3rd in bass line
        {
            if (isMinorType(targetType))
                transposed -= 1;
            else if (targetType == ChordType::Sus4)
                transposed += 1;
            else if (targetType == ChordType::Sus2)
                transposed -= 2;
        }
        else if (degree == 7) // 5th in bass line
        {
            if (targetType == ChordType::Diminished || targetType == ChordType::DiminishedSeventh)
                transposed -= 1;
            else if (targetType == ChordType::Augmented)
                transposed += 1;
        }
        else if (degree == 9 && isMinorType(targetType))
        {
            transposed -= 1;
        }
        // Walking bass lines and b7 octave riffs are PRESERVED!
    }

    if (rule.ntr == NTRRule::RootFixed)
    {
        while (transposed > sourcePitch + 6)  transposed -= 12;
        while (transposed < sourcePitch - 6)  transposed += 12;
    }

    int lowLimit = rule.noteLimitLow;
    int highLimit = rule.noteLimitHigh;
    if (highLimit <= lowLimit || highLimit == 0)
    {
        lowLimit = 0;
        highLimit = 127;
    }

    while (transposed < lowLimit)  transposed += 12;
    while (transposed > highLimit) transposed -= 12;

    return juce::jlimit(0, 127, transposed);
}