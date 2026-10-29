#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "MainAudioSource.h"
#include "StyleParser.h"

class ArrangerKeyboardComponent : public juce::Component
{
public:
    explicit ArrangerKeyboardComponent(ChordDetector& detector);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

    static constexpr int startMidiNote = 36; // C2
    static constexpr int numKeys = 61;       // 61 Keys (C2 to C7)

private:
    ChordDetector& chordDetector;
    bool isBlack(int midiNote) const noexcept;
    int getWhiteKeyIndex(int midiNote) const noexcept;
    int getMidiNoteFromX(float x, float y) const noexcept;
};

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    explicit MainComponent(MainAudioSource& source);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // Persistence
    void loadPersistentSettings();
    void savePersistentSettings();

    // Style Directory & File Handling
    void chooseStyleDirectory();
    void scanStyleDirectory(const juce::File& dir);
    void loadStyleFile(const juce::File& file);

    // Voice Categories
    void openVoiceBrowser();
    void selectVoice(int programNumber);

    void updateOtsButtonLabels(const StyleParser& parser);

    MainAudioSource& audioSource;
    std::shared_ptr<StyleParser> currentStyleParser;

    // Persistent Settings
    std::unique_ptr<juce::PropertiesFile> appProperties;
    juce::File currentStyleDirectory;
    juce::Array<juce::File> availableStyles;

    // Touchscreen Tabs: 0 = Dashboard/Home, 1 = Style Browser, 2 = Voice Browser
    int activeScreenTab { 0 };

    // --- Screen Components ---
    juce::TextButton tabHomeBtn { "HOME" };
    juce::TextButton tabStylesBtn { "STYLES" };
    juce::TextButton tabVoicesBtn { "VOICES" };
    juce::TextButton loadSf2Btn { "🎵 Load SoundFont (.sf2)" };
    juce::TextButton chooseStyleDirBtn { "📂 Select Styles Folder..." };
    void chooseSoundFont();

    // Home Screen Displays
    juce::Label screenChordLabel;
    juce::Label screenStyleNameLabel;
    juce::Label screenSectionLabel;
    juce::Label screenTempoLabel;
    juce::Label screenLeadVoiceLabel;
    juce::Label screenStatusLabel;

    // Screen Style Browser
    juce::ListBox styleListBox;
    struct StyleListModel : public juce::ListBoxModel
    {
        MainComponent& owner;
        explicit StyleListModel(MainComponent& o) : owner(o) {}
        int getNumRows() override { return owner.availableStyles.size(); }
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override;
    } styleListModel { *this };

    // Screen Voice Browser Category Tabs
    juce::TabbedComponent voiceCategoryTabs { juce::TabbedButtonBar::TabsAtTop };

    // --- Left/Right Dials & Knobs (Flanking the screen) ---
    juce::Slider masterVolKnob;
    juce::Label masterVolLabel;
    juce::Slider leadVolKnob;
    juce::Label leadVolLabel;
    juce::Slider tempoKnob;
    juce::Label tempoLabel;

    // OTS Quick Sound Buttons
    juce::TextButton ots1Btn { "OTS 1" };
    juce::TextButton ots2Btn { "OTS 2" };
    juce::TextButton ots3Btn { "OTS 3" };
    juce::TextButton ots4Btn { "OTS 4" };

    // --- Horizontal Section Strip (Directly Above the Keys) ---
    juce::TextButton intro1Btn { "INTRO 1" };
    juce::TextButton intro2Btn { "INTRO 2" };
    juce::TextButton intro3Btn { "INTRO 3" };

    juce::TextButton main1Btn { "MAIN 1" };
    juce::TextButton main2Btn { "MAIN 2" };
    juce::TextButton main3Btn { "MAIN 3" };
    juce::TextButton main4Btn { "MAIN 4" };

    juce::TextButton fillBreakBtn { "FILL / BREAK" };
    juce::ToggleButton autoFillToggle { "AUTO FILL" };

    juce::TextButton ending1Btn { "ENDING 1" };
    juce::TextButton ending2Btn { "ENDING 2" };
    juce::TextButton ending3Btn { "ENDING 3" };

    juce::TextButton startStopBtn { "START / STOP" };
    juce::ToggleButton singleFingerToggle { "SINGLE FINGER" };

    // --- Bottom 61-Key Keyboard ---
    ArrangerKeyboardComponent keyboardComponent;

    std::unique_ptr<juce::FileChooser> fileChooser;
    // Touchscreen Tabs: 0 = Home, 1 = Styles, 2 = Voices, 3 = Mixer
    juce::TextButton tabMixerBtn { "MIXER" };

    // Hardware Style Volume Knob
    juce::Slider styleVolKnob;
    juce::Label styleVolLabel;

    // Touchscreen 8-Track Mixer Component
    struct MixerComponent : public juce::Component
    {
        SynthManager& synth;
        struct TrackStrip {
            juce::Slider slider;
            juce::Label label;
            juce::Label valLabel;
        };
        std::array<TrackStrip, 8> strips;

        explicit MixerComponent(SynthManager& sm);
        void resized() override;
        void updateSlidersFromSynth();
    } mixerComp;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};