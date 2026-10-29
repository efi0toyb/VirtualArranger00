#include "StyleParser.h"
#include <cstring>
#include <algorithm>

StyleParser::StyleParser()
{
    setDefaultOTS();
}

juce::String StyleParser::getGMVoiceName(int program) noexcept
{
    static const char* const gmNames[128] = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
        "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Choir", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"
    };

    if (program >= 0 && program < 128)
        return gmNames[program];
    return "Voice " + juce::String(program);
}

void StyleParser::setDefaultOTS()
{
    const int progs[4] = { 0, 21, 66, 29 };

    for (int i = 0; i < 4; ++i)
    {
        otsSettings[i].isLoadedFromFile = false;

        otsSettings[i].right1.program = progs[i];
        otsSettings[i].right1.bankMsb = 0;
        otsSettings[i].right1.bankLsb = 0;
        otsSettings[i].right1.volume = 120;
        otsSettings[i].right1.isActive = true;
        otsSettings[i].right1.voiceName = getGMVoiceName(progs[i]);

        otsSettings[i].right2.program = 48;
        otsSettings[i].right2.volume = 0;
        otsSettings[i].right2.isActive = false;

        otsSettings[i].right3.program = 0;
        otsSettings[i].right3.volume = 0;
        otsSettings[i].right3.isActive = false;

        otsSettings[i].left.program = 33;
        otsSettings[i].left.volume = 0;
        otsSettings[i].left.isActive = false;
    }
}

void StyleParser::applyDefaultCASM(std::array<ChannelCASM, 16>& rules)
{
    for (int ch = 0; ch < 16; ++ch)
    {
        rules[ch].sourceChannel = ch;
        rules[ch].destChannel = ch;
        rules[ch].sourceRoot = 0;
        rules[ch].sourceChord = 0;
        rules[ch].highKey = 6;
        rules[ch].noteLimitLow = 0;
        rules[ch].noteLimitHigh = 127;

        if (ch == 8 || ch == 9)
        {
            rules[ch].ntr = NTRRule::RootFixed;
            rules[ch].ntt = NTTRule::Bypass;
        }
        else if (ch == 10)
        {
            rules[ch].ntr = NTRRule::RootTranspose;
            rules[ch].ntt = NTTRule::Bass;
        }
        else if (ch == 14 || ch == 15)
        {
            rules[ch].ntr = NTRRule::RootTranspose;
            rules[ch].ntt = NTTRule::Melodic;
        }
        else
        {
            rules[ch].ntr = NTRRule::RootTranspose;
            rules[ch].ntt = NTTRule::Chord;
        }
    }
}

const StyleSection* StyleParser::getSection(const juce::String& sectionName) const noexcept
{
    for (const auto& sec : sections)
    {
        if (sec.name.equalsIgnoreCase(sectionName))
            return &sec;
    }
    return sections.empty() ? nullptr : &sections.front();
}

const StyleSection* StyleParser::findMainSection(int variation1To4) const noexcept
{
    const int targetVar = juce::jlimit(1, 4, variation1To4);
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Main && sec.variationIndex == targetVar)
            return &sec;
    }
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Main)
            return &sec;
    }
    return nullptr;
}

const StyleSection* StyleParser::findFillSection(int fromVar1To4, int toVar1To4) const noexcept
{
    const int fromVar = juce::jlimit(1, 4, fromVar1To4);
    const int toVar   = juce::jlimit(1, 4, toVar1To4);

    char fromChar = 'A' + (fromVar - 1);
    char toChar   = 'A' + (toVar - 1);

    juce::String directFillName = "Fill In " + juce::String::charToString(fromChar) + juce::String::charToString(toChar);
    juce::String singleFillName = "Fill In " + juce::String::charToString(toChar) + juce::String::charToString(toChar);

    // 1. Try directional fill (e.g. Fill In AB)
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Fill && sec.name.replace(" ", "").equalsIgnoreCase(directFillName.replace(" ", "")))
            return &sec;
    }

    // 2. Try target fill (e.g. Fill In BB)
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Fill && sec.name.replace(" ", "").equalsIgnoreCase(singleFillName.replace(" ", "")))
            return &sec;
    }

    // 3. Try variation index match
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Fill && sec.variationIndex == toVar)
            return &sec;
    }

    // 4. Return ANY Fill or Break (STRICTLY SectionRole::Fill only!)
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Fill)
            return &sec;
    }
    return nullptr;
}

const StyleSection* StyleParser::findIntroSection(int intro1To3) const noexcept
{
    const int targetIdx = juce::jlimit(1, 3, intro1To3);
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Intro && sec.variationIndex == targetIdx)
            return &sec;
    }
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Intro)
            return &sec;
    }
    return nullptr;
}

const StyleSection* StyleParser::findEndingSection(int ending1To3) const noexcept
{
    const int targetIdx = juce::jlimit(1, 3, ending1To3);
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Ending && sec.variationIndex == targetIdx)
            return &sec;
    }
    for (const auto& sec : sections)
    {
        if (sec.role == SectionRole::Ending)
            return &sec;
    }
    return nullptr;
}

static inline uint32_t readVLQ(const uint8_t*& ptr, const uint8_t* end)
{
    uint32_t value = 0;
    while (ptr < end)
    {
        uint8_t byte = *ptr++;
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80))
            break;
    }
    return value;
}

juce::MidiMessage StyleParser::createGenericMetaEvent(uint8_t metaType, const uint8_t* data, size_t length, double timeStamp)
{
    if (metaType >= 0x01 && metaType <= 0x07)
    {
        juce::String str = juce::String::fromUTF8(reinterpret_cast<const char*>(data), static_cast<int>(length));
        auto msg = juce::MidiMessage::textMetaEvent(static_cast<int>(metaType), str);
        msg.setTimeStamp(timeStamp);
        return msg;
    }
    else if (metaType == 0x51 && length == 3)
    {
        int us = (data[0] << 16) | (data[1] << 8) | data[2];
        auto msg = juce::MidiMessage::tempoMetaEvent(us);
        msg.setTimeStamp(timeStamp);
        return msg;
    }
    else if (metaType == 0x58 && length >= 2)
    {
        int num = data[0];
        int den = 1 << data[1];
        auto msg = juce::MidiMessage::timeSignatureMetaEvent(num, den);
        msg.setTimeStamp(timeStamp);
        return msg;
    }
    else if (metaType == 0x2F)
    {
        auto msg = juce::MidiMessage::endOfTrack();
        msg.setTimeStamp(timeStamp);
        return msg;
    }
    else
    {
        juce::MemoryOutputStream mos;
        mos.writeByte(static_cast<char>(0xFF));
        mos.writeByte(static_cast<char>(metaType));

        uint8_t vlqBuf[4];
        int vlqLen = 0;
        uint32_t val = static_cast<uint32_t>(length);
        vlqBuf[vlqLen++] = static_cast<uint8_t>(val & 0x7F);
        while (val >>= 7)
            vlqBuf[vlqLen++] = static_cast<uint8_t>((val & 0x7F) | 0x80);

        for (int i = vlqLen - 1; i >= 0; --i)
            mos.writeByte(static_cast<char>(vlqBuf[i]));

        if (length > 0 && data != nullptr)
            mos.write(data, length);

        return juce::MidiMessage(mos.getData(), static_cast<int>(mos.getDataSize()), timeStamp);
    }
}

bool StyleParser::loadStyleFile(const juce::File& file)
{
    isLoaded = false;
    errorMessage.clear();
    sections.clear();
    initSetupEvents.clear();
    flatMidiSequence.clear();
    otsSourceDescription.clear();

    drumChannels.fill(false);
    drumChannels[8] = true;
    drumChannels[9] = true;

    setDefaultOTS();

    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData) || fileData.getSize() < 22)
    {
        errorMessage = "Could not open file or file is too small: " + file.getFullPathName();
        return false;
    }

    const uint8_t* rawBytes = static_cast<const uint8_t*>(fileData.getData());
    const size_t totalSize  = fileData.getSize();

    size_t mthdOffset = 0;
    bool foundMThd = false;

    for (size_t i = 0; i + 4 <= totalSize; ++i)
    {
        if (rawBytes[i] == 'M' && rawBytes[i+1] == 'T' && rawBytes[i+2] == 'h' && rawBytes[i+3] == 'd')
        {
            mthdOffset = i;
            foundMThd = true;
            break;
        }
    }

    if (!foundMThd)
    {
        errorMessage = "Invalid style file: missing standard MIDI 'MThd' header.";
        return false;
    }

    if (!parseMidiStream(rawBytes + mthdOffset, totalSize - mthdOffset))
    {
        if (errorMessage.isEmpty())
            errorMessage = "Failed to parse MIDI track data.";
        return false;
    }

    extractSetupAndSections();
    parseCASM(rawBytes, totalSize);
    parseOTS(rawBytes, totalSize);

    if (!otsSettings[0].isLoadedFromFile)
    {
        deriveOTSFromStyleTracks();
    }

    isLoaded = !sections.empty() && flatMidiSequence.getNumEvents() > 0;
    return isLoaded;
}

bool StyleParser::parseMidiStream(const uint8_t* data, size_t size)
{
    flatMidiSequence.clear();

    if (size < 14) return false;
    const uint8_t* ptr = data;
    const uint8_t* end = data + size;

    if (std::memcmp(ptr, "MThd", 4) != 0) return false;
    ptr += 4;

    uint32_t headerLen = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
    ptr += 4;
    if (ptr + headerLen > end || headerLen < 6) return false;

    ptr += 2;
    uint16_t numTracks = (ptr[0] << 8) | ptr[1];
    ptr += 2;
    timeDivision = static_cast<short>((ptr[0] << 8) | ptr[1]);
    if (timeDivision < 0) timeDivision = 480;
    ptr += (headerLen - 4);

    for (int t = 0; t < numTracks && ptr + 8 <= end; ++t)
    {
        while (ptr + 8 <= end && std::memcmp(ptr, "MTrk", 4) != 0)
            ++ptr;

        if (ptr + 8 > end) break;
        ptr += 4;

        uint32_t trackLen = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        ptr += 4;

        const uint8_t* trackEnd = (ptr + trackLen <= end) ? (ptr + trackLen) : end;
        uint32_t currentTickAcc = 0;
        uint8_t runningStatus = 0;

        while (ptr < trackEnd)
        {
            uint32_t delta = readVLQ(ptr, trackEnd);
            currentTickAcc += delta;

            if (ptr >= trackEnd) break;

            uint8_t status = *ptr;

            if (status == 0xFF)
            {
                ptr++;
                if (ptr >= trackEnd) break;
                uint8_t metaType = *ptr++;
                uint32_t metaLen = readVLQ(ptr, trackEnd);

                if (ptr + metaLen > trackEnd) break;

                if (metaType == 0x51 && metaLen == 3)
                {
                    uint32_t usPerBeat = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
                    if (usPerBeat > 0)
                        initialBpm = 60000000.0 / usPerBeat;
                }

                auto msg = createGenericMetaEvent(metaType, ptr, metaLen, static_cast<double>(currentTickAcc));
                flatMidiSequence.addEvent(msg);

                ptr += metaLen;
            }
            else if (status == 0xF0 || status == 0xF7)
            {
                ptr++;
                uint32_t sysexLen = readVLQ(ptr, trackEnd);
                if (ptr + sysexLen > trackEnd) break;
                ptr += sysexLen;
                runningStatus = 0;
            }
            else
            {
                if (status & 0x80)
                {
                    runningStatus = status;
                    ptr++;
                }
                else
                {
                    status = runningStatus;
                }

                if ((status & 0x80) == 0) break;

                uint8_t type = status & 0xF0;
                uint8_t data1 = (ptr < trackEnd) ? *ptr++ : 0;
                uint8_t data2 = 0;

                if (type != 0xC0 && type != 0xD0)
                {
                    data2 = (ptr < trackEnd) ? *ptr++ : 0;
                }

                juce::MidiMessage msg(status, data1, data2, static_cast<double>(currentTickAcc));
                flatMidiSequence.addEvent(msg);
            }
        }
    }

    flatMidiSequence.updateMatchedPairs();
    return flatMidiSequence.getNumEvents() > 0;
}

void StyleParser::extractSetupAndSections()
{
    struct MarkerInfo
    {
        juce::String name;
        int64_t tick;
        SectionRole role;
        int varIndex;
    };
    std::vector<MarkerInfo> markers;

    int64_t firstMusicTick = -1;

    auto classifySection = [](const juce::String& text, int64_t tick) noexcept -> std::pair<SectionRole, int>
    {
        if (tick == 0) return { SectionRole::Unknown, 0 };

        juce::String c = text.replace(" ", "").toLowerCase();
        if (c == "sff1" || c == "sff2" || c == "sint")
            return { SectionRole::Unknown, 0 };

        if (c.startsWith("intro"))
        {
            int idx = 1;
            if (c.contains("b") || c.contains("2")) idx = 2;
            else if (c.contains("c") || c.contains("3")) idx = 3;
            return { SectionRole::Intro, idx };
        }

        if (c.startsWith("ending") || c.startsWith("end"))
        {
            int idx = 1;
            if (c.contains("b") || c.contains("2")) idx = 2;
            else if (c.contains("c") || c.contains("3")) idx = 3;
            return { SectionRole::Ending, idx };
        }

        if (c.startsWith("fill") || c.startsWith("break"))
        {
            int idx = 1;
            if (c.contains("bb") || c.contains("b") || c.contains("2")) idx = 2;
            else if (c.contains("cc") || c.contains("c") || c.contains("3")) idx = 3;
            else if (c.contains("dd") || c.contains("d") || c.contains("4")) idx = 4;
            return { SectionRole::Fill, idx };
        }

        if (c.startsWith("main") || c.startsWith("var") || c.startsWith("basic"))
        {
            int idx = 1;
            if (c.contains("b") || c.contains("2")) idx = 2;
            else if (c.contains("c") || c.contains("3")) idx = 3;
            else if (c.contains("d") || c.contains("4")) idx = 4;
            return { SectionRole::Main, idx };
        }

        return { SectionRole::Unknown, 0 };
    };

    for (int i = 0; i < flatMidiSequence.getNumEvents(); ++i)
    {
        const auto& msg = flatMidiSequence.getEventPointer(i)->message;

        if (msg.isMetaEvent())
        {
            const int metaType = msg.getMetaEventType();
            // ONLY accept 0x06 (Marker), 0x07 (Cue), or 0x01 (Text) - NEVER 0x03 (Track Name)!
            if (metaType == 0x06 || metaType == 0x07 || metaType == 0x01)
            {
                const auto* d = reinterpret_cast<const char*>(msg.getMetaEventData());
                int len = msg.getMetaEventLength();
                juce::String markerText = juce::String::fromUTF8(d, len).trim();

                const int64_t tick = static_cast<int64_t>(msg.getTimeStamp());
                auto [role, varIdx] = classifySection(markerText, tick);

                if (role != SectionRole::Unknown)
                {
                    bool exists = false;
                    for (const auto& m : markers)
                    {
                        if (m.tick == tick) { exists = true; break; }
                    }

                    if (!exists)
                    {
                        markers.push_back({ markerText, tick, role, varIdx });
                        if (firstMusicTick < 0)
                            firstMusicTick = tick;
                    }
                }
            }
        }
    }

    if (firstMusicTick < 0)
        firstMusicTick = timeDivision * 4;

    std::array<bool, 16> channelHasVolumeInSetup {};

    for (int i = 0; i < flatMidiSequence.getNumEvents(); ++i)
    {
        const auto& msg = flatMidiSequence.getEventPointer(i)->message;
        const int64_t t = static_cast<int64_t>(msg.getTimeStamp());

        if (t <= firstMusicTick)
        {
            if (msg.isProgramChange() || msg.isController())
            {
                initSetupEvents.push_back(msg);

                const int ch = juce::jlimit(0, 15, msg.getChannel() - 1);
                if (msg.isController())
                {
                    if (msg.getControllerNumber() == 7)
                        channelHasVolumeInSetup[ch] = true;
                    else if (msg.getControllerNumber() == 0 && (msg.getControllerValue() == 127 || msg.getControllerValue() == 126))
                        drumChannels[ch] = true;
                }
            }
            else if (msg.isSysEx())
            {
                const auto* raw = msg.getRawData();
                const int rawSz = msg.getRawDataSize();

                if (rawSz >= 9 && raw[0] == 0xF0 && raw[1] == 0x43 && (raw[2] & 0xF0) == 0x10 && raw[3] == 0x4C && raw[4] == 0x08)
                {
                    const int ch    = raw[5] & 0x0F;
                    const int param = raw[6];
                    const int val   = raw[7];

                    if (param == 0x0B)
                    {
                        auto cc7 = juce::MidiMessage::controllerEvent(ch + 1, 7, val);
                        cc7.setTimeStamp(msg.getTimeStamp());
                        initSetupEvents.push_back(cc7);
                        channelHasVolumeInSetup[ch] = true;
                    }
                    else if (param == 0x0E)
                    {
                        auto cc10 = juce::MidiMessage::controllerEvent(ch + 1, 10, val);
                        cc10.setTimeStamp(msg.getTimeStamp());
                        initSetupEvents.push_back(cc10);
                    }
                    else if (param == 0x01)
                    {
                        auto cc0 = juce::MidiMessage::controllerEvent(ch + 1, 0, val);
                        cc0.setTimeStamp(msg.getTimeStamp());
                        initSetupEvents.push_back(cc0);
                        if (val == 127 || val == 126) drumChannels[ch] = true;
                    }
                    else if (param == 0x02)
                    {
                        auto cc32 = juce::MidiMessage::controllerEvent(ch + 1, 32, val);
                        cc32.setTimeStamp(msg.getTimeStamp());
                        initSetupEvents.push_back(cc32);
                    }
                    else if (param == 0x03)
                    {
                        auto pc = juce::MidiMessage::programChange(ch + 1, val);
                        pc.setTimeStamp(msg.getTimeStamp());
                        initSetupEvents.push_back(pc);
                    }
                }
            }
        }
    }

    for (int ch = 8; ch < 16; ++ch)
    {
        if (!channelHasVolumeInSetup[ch])
        {
            for (int i = 0; i < flatMidiSequence.getNumEvents(); ++i)
            {
                const auto& msg = flatMidiSequence.getEventPointer(i)->message;
                if (msg.isController() && (msg.getChannel() - 1 == ch) && msg.getControllerNumber() == 7)
                {
                    initSetupEvents.push_back(msg);
                    channelHasVolumeInSetup[ch] = true;
                    break;
                }
            }
        }
    }

    std::sort(markers.begin(), markers.end(), [](const MarkerInfo& a, const MarkerInfo& b) {
        return a.tick < b.tick;
    });

    const int64_t totalEndTick = static_cast<int64_t>(flatMidiSequence.getEndTime());

    for (size_t i = 0; i < markers.size(); ++i)
    {
        StyleSection sec;
        sec.name = markers[i].name;
        sec.role = markers[i].role;
        sec.variationIndex = markers[i].varIndex;
        sec.startTick = markers[i].tick;
        sec.endTick = (i + 1 < markers.size()) ? markers[i + 1].tick : totalEndTick;
        sec.lengthTicks = sec.endTick - sec.startTick;

        applyDefaultCASM(sec.channelRules);

        if (sec.lengthTicks > 0)
            sections.push_back(sec);
    }

    if (sections.empty() && totalEndTick > firstMusicTick)
    {
        StyleSection sec;
        sec.name = "Main A";
        sec.role = SectionRole::Main;
        sec.variationIndex = 1;
        sec.startTick = firstMusicTick;
        sec.endTick = totalEndTick;
        sec.lengthTicks = sec.endTick - sec.startTick;
        applyDefaultCASM(sec.channelRules);
        sections.push_back(sec);
    }
}

void StyleParser::parseCASM(const uint8_t* data, size_t totalSize)
{
    const char* bytes = reinterpret_cast<const char*>(data);

    size_t casmPos = 0;
    bool found = false;

    for (size_t i = 0; i + 4 <= totalSize; ++i)
    {
        if (bytes[i] == 'C' && bytes[i+1] == 'A' && bytes[i+2] == 'S' && bytes[i+3] == 'M')
        {
            casmPos = i;
            found = true;
            break;
        }
    }

    if (!found || casmPos + 8 > totalSize)
        return;

    const uint32_t casmLen = (static_cast<uint8_t>(bytes[casmPos+4]) << 24) |
                             (static_cast<uint8_t>(bytes[casmPos+5]) << 16) |
                             (static_cast<uint8_t>(bytes[casmPos+6]) << 8)  |
                              static_cast<uint8_t>(bytes[casmPos+7]);

    const size_t casmEnd = std::min(totalSize, casmPos + 8 + casmLen);
    size_t csegPos = casmPos + 8;

    while (csegPos + 8 <= casmEnd)
    {
        if (std::memcmp(bytes + csegPos, "CSEG", 4) == 0)
        {
            const uint32_t csegLen = (static_cast<uint8_t>(bytes[csegPos+4]) << 24) |
                                     (static_cast<uint8_t>(bytes[csegPos+5]) << 16) |
                                     (static_cast<uint8_t>(bytes[csegPos+6]) << 8)  |
                                      static_cast<uint8_t>(bytes[csegPos+7]);

            const size_t csegEnd = std::min(casmEnd, csegPos + 8 + csegLen);
            size_t subPos = csegPos + 8;

            juce::StringArray targetSectionNames;

            if (subPos + 8 <= csegEnd && std::memcmp(bytes + subPos, "Sdec", 4) == 0)
            {
                const uint32_t sdecLen = (static_cast<uint8_t>(bytes[subPos+4]) << 24) |
                                         (static_cast<uint8_t>(bytes[subPos+5]) << 16) |
                                         (static_cast<uint8_t>(bytes[subPos+6]) << 8)  |
                                          static_cast<uint8_t>(bytes[subPos+7]);

                if (subPos + 8 + sdecLen <= csegEnd)
                {
                    juce::String sdecStr = juce::String::fromUTF8(bytes + subPos + 8, static_cast<int>(sdecLen));
                    targetSectionNames.addTokens(sdecStr, ",", "\"");
                    targetSectionNames.trim();
                }
                subPos += 8 + sdecLen;
            }

            while (subPos + 8 <= csegEnd)
            {
                bool isCtab = (std::memcmp(bytes + subPos, "Ctab", 4) == 0);
                bool isCtb2 = (std::memcmp(bytes + subPos, "Ctb2", 4) == 0);

                if (!isCtab && !isCtb2)
                {
                    subPos++;
                    continue;
                }

                const uint32_t ctabLen = (static_cast<uint8_t>(bytes[subPos+4]) << 24) |
                                         (static_cast<uint8_t>(bytes[subPos+5]) << 16) |
                                         (static_cast<uint8_t>(bytes[subPos+6]) << 8)  |
                                          static_cast<uint8_t>(bytes[subPos+7]);

                if (subPos + 8 + ctabLen > csegEnd || ctabLen < 20)
                {
                    subPos += 8;
                    continue;
                }

                const size_t p = subPos + 8;
                const int srcCh  = static_cast<uint8_t>(bytes[p]) & 0x0F;
                const int destCh = static_cast<uint8_t>(bytes[p + 9]) & 0x0F;
                juce::String partName = juce::String::fromUTF8(bytes + p + 1, 8).trim();

                NTRRule ntr = NTRRule::RootTranspose;
                NTTRule ntt = NTTRule::Chord;
                int srcRoot = 0;
                int highKey = 6;
                int lowLim  = 0;
                int highLim = 127;

                if (isCtb2)
                {
                    srcRoot = static_cast<uint8_t>(bytes[p + 18]) % 12;
                    ntr = (bytes[p + 20] == 1) ? NTRRule::RootFixed : NTRRule::RootTranspose;
                    ntt = NTTRule::Chord;
                }
                else
                {
                    srcRoot = static_cast<uint8_t>(bytes[p + 18]) % 12;
                    ntr     = (bytes[p + 20] == 1) ? NTRRule::RootFixed : NTRRule::RootTranspose;

                    const uint8_t nttVal = static_cast<uint8_t>(bytes[p + 21]);
                    if      (nttVal == 0) ntt = NTTRule::Bypass;
                    else if (nttVal == 1 || nttVal == 4 || nttVal == 5) ntt = NTTRule::Melodic;
                    else if (nttVal == 3) ntt = NTTRule::Bass;
                    else                  ntt = NTTRule::Chord;

                    highKey = static_cast<uint8_t>(bytes[p + 22]);
                    lowLim  = static_cast<uint8_t>(bytes[p + 23]);
                    highLim = static_cast<uint8_t>(bytes[p + 24]);
                }

                if (destCh == 8 || destCh == 9 ||
                    partName.containsIgnoreCase("Rhythm") ||
                    partName.containsIgnoreCase("Drum") ||
                    partName.containsIgnoreCase("Perc"))
                {
                    ntr = NTRRule::RootFixed;
                    ntt = NTTRule::Bypass;
                    drumChannels[srcCh] = true;
                }

                for (auto& sec : sections)
                {
                    bool matches = targetSectionNames.isEmpty();
                    if (!matches)
                    {
                        for (const auto& target : targetSectionNames)
                        {
                            if (sec.name.replace(" ", "").equalsIgnoreCase(target.replace(" ", "")))
                            {
                                matches = true;
                                break;
                            }
                        }
                    }

                    if (matches)
                    {
                        auto& rule = sec.channelRules[srcCh];
                        rule.sourceChannel = srcCh;
                        rule.destChannel   = destCh;
                        rule.sourceRoot    = srcRoot;
                        rule.ntr           = ntr;
                        rule.ntt           = ntt;
                        rule.highKey       = highKey;
                        rule.noteLimitLow  = lowLim;
                        rule.noteLimitHigh = highLim;
                    }
                }

                subPos += 8 + ctabLen;
            }

            csegPos = csegEnd;
        }
        else
        {
            csegPos++;
        }
    }
}

void StyleParser::parseOTS(const uint8_t* data, size_t totalSize)
{
    const char* bytes = reinterpret_cast<const char*>(data);

    size_t otsOffset = 0;
    bool foundOTS = false;

    for (size_t i = 0; i + 4 <= totalSize; ++i)
    {
        if ((bytes[i] == 'O' && bytes[i+1] == 'T' && bytes[i+2] == 'S' && bytes[i+3] == 'c') ||
            (bytes[i] == 'O' && bytes[i+1] == 'T' && bytes[i+2] == 'S' && bytes[i+3] == ' '))
        {
            otsOffset = i;
            foundOTS = true;
            break;
        }
    }

    if (!foundOTS || otsOffset + 8 > totalSize)
        return;

    const uint32_t otsLen = (static_cast<uint8_t>(bytes[otsOffset+4]) << 24) |
                            (static_cast<uint8_t>(bytes[otsOffset+5]) << 16) |
                            (static_cast<uint8_t>(bytes[otsOffset+6]) << 8)  |
                             static_cast<uint8_t>(bytes[otsOffset+7]);

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(bytes + otsOffset + 8);
    const uint8_t* end = (ptr + otsLen <= data + totalSize) ? (ptr + otsLen) : (data + totalSize);

    int otsIndex = 0;

    while (ptr + 8 <= end && otsIndex < 4)
    {
        if (std::memcmp(ptr, "MTrk", 4) == 0)
        {
            ptr += 4;
            uint32_t trackLen = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            ptr += 4;

            const uint8_t* trackEnd = (ptr + trackLen <= end) ? (ptr + trackLen) : end;
            auto& ots = otsSettings[otsIndex];
            ots.isLoadedFromFile = true;
            otsSourceDescription = "Loaded from Style OTSc";

            auto getPart = [&](int ch) -> OTSPart*
            {
                if      (ch == 0) return &ots.right1;
                else if (ch == 1) return &ots.right2;
                else if (ch == 2) return &ots.right3;
                else if (ch == 3) return &ots.left;
                return nullptr;
            };

            uint8_t runningStatus = 0;

            while (ptr < trackEnd)
            {
                readVLQ(ptr, trackEnd);
                if (ptr >= trackEnd) break;

                uint8_t status = *ptr;

                if (status == 0xFF)
                {
                    ptr++;
                    if (ptr >= trackEnd) break;
                    uint8_t metaType = *ptr++;
                    uint32_t metaLen = readVLQ(ptr, trackEnd);
                    if (ptr + metaLen > trackEnd) break;

                    if ((metaType == 0x03 || metaType == 0x04 || metaType == 0x01) && metaLen > 0)
                    {
                        juce::String name = juce::String::fromUTF8(reinterpret_cast<const char*>(ptr), static_cast<int>(metaLen)).trim();
                        if (name.isNotEmpty() && ots.right1.voiceName.isEmpty())
                            ots.right1.voiceName = name;
                    }
                    ptr += metaLen;
                }
                else if (status == 0xF0 || status == 0xF7)
                {
                    ptr++;
                    uint32_t sysexLen = readVLQ(ptr, trackEnd);
                    if (ptr + sysexLen > trackEnd) break;
                    ptr += sysexLen;
                    runningStatus = 0;
                }
                else
                {
                    if (status & 0x80)
                    {
                        runningStatus = status;
                        ptr++;
                    }
                    else
                    {
                        status = runningStatus;
                    }

                    if ((status & 0x80) == 0) break;

                    int ch = status & 0x0F;
                    int type = status & 0xF0;
                    auto* part = getPart(ch);

                    if (type == 0xC0)
                    {
                        uint8_t pc = (ptr < trackEnd) ? *ptr++ : 0;
                        if (part != nullptr)
                        {
                            part->program = pc;
                            part->isActive = true;
                            if (part->voiceName.isEmpty())
                                part->voiceName = getGMVoiceName(pc);
                        }
                    }
                    else if (type == 0xB0)
                    {
                        uint8_t cc  = (ptr < trackEnd) ? *ptr++ : 0;
                        uint8_t val = (ptr < trackEnd) ? *ptr++ : 0;

                        if (part != nullptr)
                        {
                            if      (cc == 0)  part->bankMsb = val;
                            else if (cc == 32) part->bankLsb = val;
                            else if (cc == 7)
                            {
                                part->volume = val;
                                if (val == 0) part->isActive = false;
                            }
                        }
                    }
                    else
                    {
                        if (ptr < trackEnd) ptr++;
                        if (type != 0xD0 && ptr < trackEnd) ptr++;
                    }
                }
            }

            ptr = trackEnd;
            otsIndex++;
        }
        else
        {
            ptr++;
        }
    }
}

void StyleParser::deriveOTSFromStyleTracks()
{
    struct TrackVoice {
        int channel { -1 };
        int program { 0 };
        int bankMsb { 0 };
        int bankLsb { 0 };
        int volume { 100 };
    };
    std::vector<TrackVoice> melodicVoices;

    for (int ch = 10; ch < 16; ++ch)
    {
        TrackVoice tv;
        tv.channel = ch;
        bool foundProg = false;

        for (const auto& msg : initSetupEvents)
        {
            if (msg.getChannel() - 1 == ch)
            {
                if (msg.isProgramChange())
                {
                    tv.program = msg.getProgramChangeNumber();
                    foundProg = true;
                }
                else if (msg.isController())
                {
                    if      (msg.getControllerNumber() == 0)  tv.bankMsb = msg.getControllerValue();
                    else if (msg.getControllerNumber() == 32) tv.bankLsb = msg.getControllerValue();
                    else if (msg.getControllerNumber() == 7)  tv.volume  = msg.getControllerValue();
                }
            }
        }

        if (foundProg && ch != 10)
            melodicVoices.push_back(tv);
    }

    if (!melodicVoices.empty())
    {
        otsSourceDescription = "Extracted from Style Accompaniment Tracks";

        for (int i = 0; i < 4; ++i)
        {
            const auto& tv = melodicVoices[i % melodicVoices.size()];
            otsSettings[i].isLoadedFromFile = true;

            otsSettings[i].right1.program   = tv.program;
            otsSettings[i].right1.bankMsb   = tv.bankMsb;
            otsSettings[i].right1.bankLsb   = tv.bankLsb;
            otsSettings[i].right1.volume    = 120;
            otsSettings[i].right1.isActive  = true;
            otsSettings[i].right1.voiceName = getGMVoiceName(tv.program);

            otsSettings[i].right2.isActive  = false;
            otsSettings[i].right3.isActive  = false;
            otsSettings[i].left.isActive    = false;
        }
    }
}

juce::String StyleParser::getDebugSummary() const
{
    if (!errorMessage.isEmpty())
        return errorMessage;

    juce::String s;
    s << "Loaded Style: " << (int)sections.size() << " sections found.\n";
    s << "Tempo: " << initialBpm << " BPM | PPQ: " << timeDivision << " | Total Events: " << flatMidiSequence.getNumEvents() << "\n";
    s << "Sections:\n";
    for (const auto& sec : sections)
        s << " • " << sec.name << " (" << sec.lengthTicks << " ticks)\n";

    s << "\nOne Touch Settings (OTS) [" << (otsSourceDescription.isNotEmpty() ? otsSourceDescription : "Default Presets") << "]:\n";
    for (int i = 0; i < 4; ++i)
    {
        const auto& ots = otsSettings[i];
        s << " OTS " << (i + 1) << ": "
          << (ots.right1.voiceName.isNotEmpty() ? ots.right1.voiceName : getGMVoiceName(ots.right1.program))
          << " (Prog: " << ots.right1.program << ", Bank: " << ots.right1.bankMsb << ", Vol: " << ots.right1.volume << ")"
          << (ots.right2.isActive && ots.right2.volume > 0 ? " + R2" : "")
          << (ots.left.isActive && ots.left.volume > 0 ? " + Left" : "")
          << "\n";
    }
    return s;
}