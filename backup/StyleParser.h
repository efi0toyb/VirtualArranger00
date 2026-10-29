#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

enum class NTRRule : uint8_t
{
    RootTranspose = 0,
    RootFixed     = 1
};

enum class NTTRule : uint8_t
{
    Bypass        = 0,
    Melodic       = 1,
    Chord         = 2,
    Bass          = 3
};

enum class SectionRole : uint8_t
{
    Unknown = 0,
    Intro,
    Main,
    Fill,
    Ending
};

struct ChannelCASM
{
    int sourceChannel { 0 };
    int destChannel { 0 };
    int sourceRoot { 0 };
    int sourceChord { 0 };
    NTRRule ntr { NTRRule::RootTranspose };
    NTTRule ntt { NTTRule::Chord };
    int highKey { 6 };
    int noteLimitLow { 0 };
    int noteLimitHigh { 127 };
};

struct StyleSection
{
    juce::String name;
    SectionRole role { SectionRole::Unknown };
    int variationIndex { 1 }; // 1 for A, 2 for B, 3 for C, 4 for D
    int64_t startTick { 0 };
    int64_t endTick { 0 };
    int64_t lengthTicks { 0 };
    std::array<ChannelCASM, 16> channelRules;
};

struct OTSPart
{
    int program { 0 };
    int bankMsb { 0 };
    int bankLsb { 0 };
    int volume { 100 };
    bool isActive { true };
    juce::String voiceName;
};

struct OneTouchSetting
{
    OTSPart right1;
    OTSPart right2;
    OTSPart right3;
    OTSPart left;
    bool isLoadedFromFile { false };
};

class StyleParser
{
public:
    StyleParser();

    bool loadStyleFile(const juce::File& file);

    bool hasStyleLoaded() const noexcept { return isLoaded; }
    short getTimeDivision() const noexcept { return timeDivision; }
    double getInitialBpm() const noexcept { return initialBpm; }

    const std::vector<StyleSection>& getSections() const noexcept { return sections; }
    const StyleSection* getSection(const juce::String& sectionName) const noexcept;

    // Direct Role-Guaranteed Section Resolvers (Never cross-matches!)
    const StyleSection* findMainSection(int variation1To4) const noexcept;
    const StyleSection* findFillSection(int fromVariation1To4, int toVariation1To4) const noexcept;
    const StyleSection* findIntroSection(int intro1To3) const noexcept;
    const StyleSection* findEndingSection(int ending1To3) const noexcept;

    const std::vector<juce::MidiMessage>& getInitSetupEvents() const noexcept { return initSetupEvents; }
    const juce::MidiMessageSequence* getMidiSequence() const noexcept { return &flatMidiSequence; }

    const OneTouchSetting& getOTS(int index0To3) const noexcept
    {
        return otsSettings[juce::jlimit(0, 3, index0To3)];
    }

    bool isChannelDrumOrPercussion(int channel) const noexcept
    {
        return drumChannels[juce::jlimit(0, 15, channel)];
    }

    static juce::String getGMVoiceName(int program) noexcept;

    juce::String getDebugSummary() const;
    juce::String getErrorMessage() const noexcept { return errorMessage; }

private:
    bool parseMidiStream(const uint8_t* data, size_t size);
    void extractSetupAndSections();
    void parseCASM(const uint8_t* data, size_t size);
    void parseOTS(const uint8_t* data, size_t size);
    void deriveOTSFromStyleTracks();
    void setDefaultOTS();
    void applyDefaultCASM(std::array<ChannelCASM, 16>& rules);

    static juce::MidiMessage createGenericMetaEvent(uint8_t metaType, const uint8_t* data, size_t length, double timeStamp);

    bool isLoaded { false };
    short timeDivision { 480 };
    double initialBpm { 120.0 };
    juce::String errorMessage;
    juce::String otsSourceDescription;

    juce::MidiMessageSequence flatMidiSequence;
    std::vector<juce::MidiMessage> initSetupEvents;
    std::vector<StyleSection> sections;
    std::array<OneTouchSetting, 4> otsSettings;
    std::array<bool, 16> drumChannels {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyleParser)
};