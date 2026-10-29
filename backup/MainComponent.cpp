#include "MainComponent.h"

// -----------------------------------------------------------------------------
// ArrangerKeyboardComponent Implementation
// -----------------------------------------------------------------------------
ArrangerKeyboardComponent::ArrangerKeyboardComponent(ChordDetector& detector)
    : chordDetector(detector)
{
}

bool ArrangerKeyboardComponent::isBlack(int midiNote) const noexcept
{
    const int pc = midiNote % 12;
    return (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
}

int ArrangerKeyboardComponent::getWhiteKeyIndex(int midiNote) const noexcept
{
    int count = 0;
    for (int n = startMidiNote; n < midiNote; ++n)
        if (!isBlack(n)) count++;
    return count;
}

void ArrangerKeyboardComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const int split = chordDetector.getSplitPoint();

    int totalWhiteKeys = 0;
    for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
        if (!isBlack(n)) totalWhiteKeys++;

    const float whiteKeyWidth = bounds.getWidth() / static_cast<float>(totalWhiteKeys);
    const float blackKeyWidth = whiteKeyWidth * 0.62f;
    const float blackKeyHeight = bounds.getHeight() * 0.64f;

    // 1. Draw White Keys
    for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
    {
        if (isBlack(n)) continue;

        const int whiteIdx = getWhiteKeyIndex(n);
        const float x = static_cast<float>(whiteIdx) * whiteKeyWidth;
        const auto keyRect = juce::Rectangle<float>(x, 0.0f, whiteKeyWidth, bounds.getHeight());

        const bool isLeftZone = (n < split);
        const bool isPressed = isLeftZone ? chordDetector.isNoteActiveLeft(n) : chordDetector.isNoteActiveRight(n);

        juce::Colour keyCol = isLeftZone ? juce::Colour(0xffe1f5fe) : juce::Colour(0xfffafafa);

        if (isPressed)
            keyCol = isLeftZone ? juce::Colour(0xff00e5ff) : juce::Colour(0xffff9100);

        g.setColour(keyCol);
        g.fillRect(keyRect);

        g.setColour(juce::Colour(0xff2b2b2b));
        g.drawRect(keyRect, 1.0f);
    }

    // 2. Draw Black Keys
    for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
    {
        if (!isBlack(n)) continue;

        const int whiteIdx = getWhiteKeyIndex(n);
        const float x = (static_cast<float>(whiteIdx) * whiteKeyWidth) - (blackKeyWidth * 0.5f);
        const auto keyRect = juce::Rectangle<float>(x, 0.0f, blackKeyWidth, blackKeyHeight);

        const bool isLeftZone = (n < split);
        const bool isPressed = isLeftZone ? chordDetector.isNoteActiveLeft(n) : chordDetector.isNoteActiveRight(n);

        juce::Colour keyCol = isLeftZone ? juce::Colour(0xff0d232e) : juce::Colour(0xff121212);

        if (isPressed)
            keyCol = isLeftZone ? juce::Colour(0xff00b0ff) : juce::Colour(0xffff6d00);

        g.setColour(keyCol);
        g.fillRect(keyRect);

        g.setColour(juce::Colours::black);
        g.drawRect(keyRect, 1.0f);
    }

    // 3. Draw Split Marker
    if (split >= startMidiNote && split <= startMidiNote + numKeys)
    {
        const int splitWhiteIdx = getWhiteKeyIndex(split);
        const float splitX = static_cast<float>(splitWhiteIdx) * whiteKeyWidth;

        g.setColour(juce::Colour(0xffff1744));
        g.drawLine(splitX, 0.0f, splitX, bounds.getHeight(), 3.0f);

        auto badgeRect = juce::Rectangle<float>(splitX - 32.0f, 2.0f, 64.0f, 18.0f);
        g.fillRoundedRectangle(badgeRect, 3.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("▼ SPLIT", badgeRect, juce::Justification::centred);
    }
}

int ArrangerKeyboardComponent::getMidiNoteFromX(float x, float y) const noexcept
{
    int totalWhiteKeys = 0;
    for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
        if (!isBlack(n)) totalWhiteKeys++;

    const float whiteKeyWidth = getWidth() / static_cast<float>(totalWhiteKeys);
    const float blackKeyWidth = whiteKeyWidth * 0.62f;
    const float blackKeyHeight = getHeight() * 0.64f;

    if (y < blackKeyHeight)
    {
        for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
        {
            if (!isBlack(n)) continue;
            const int whiteIdx = getWhiteKeyIndex(n);
            const float kx = (static_cast<float>(whiteIdx) * whiteKeyWidth) - (blackKeyWidth * 0.5f);
            if (x >= kx && x <= kx + blackKeyWidth)
                return n;
        }
    }

    const int wIdx = juce::jlimit(0, totalWhiteKeys - 1, static_cast<int>(x / whiteKeyWidth));
    int curWhite = 0;
    for (int n = startMidiNote; n < startMidiNote + numKeys; ++n)
    {
        if (!isBlack(n))
        {
            if (curWhite == wIdx) return n;
            curWhite++;
        }
    }
    return startMidiNote;
}

void ArrangerKeyboardComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < 26)
    {
        const int note = getMidiNoteFromX(e.position.x, e.position.y);
        chordDetector.setSplitPoint(note);
        repaint();
    }
}

// -----------------------------------------------------------------------------
// StyleListModel Implementation
// -----------------------------------------------------------------------------
void MainComponent::StyleListModel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (row < 0 || row >= owner.availableStyles.size()) return;

    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff00acc1).withAlpha(0.35f));

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffcfd8dc));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(owner.availableStyles[row].getFileNameWithoutExtension(),
               10, 0, width - 20, height, juce::Justification::centredLeft);
}

void MainComponent::StyleListModel::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < owner.availableStyles.size())
        owner.loadStyleFile(owner.availableStyles[lastRowSelected]);
}

// -----------------------------------------------------------------------------
// MixerComponent Implementation
// -----------------------------------------------------------------------------
MainComponent::MixerComponent::MixerComponent(SynthManager& sm) : synth(sm)
{
    static const char* const trackNames[8] = {
        "RHY 1", "RHY 2", "BASS", "CHD 1", "CHD 2", "PAD", "PHR 1", "PHR 2"
    };

    for (int i = 0; i < 8; ++i)
    {
        const int midiCh = i + 8;
        auto& strip = strips[i];

        strip.slider.setSliderStyle(juce::Slider::LinearVertical);
        strip.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        strip.slider.setRange(0, 127, 1);
        strip.slider.setValue(synth.getChannelVolume(midiCh), juce::dontSendNotification);
        strip.slider.onValueChange = [this, midiCh, i] {
            const int val = static_cast<int>(strips[i].slider.getValue());
            synth.setChannelVolume(midiCh, val);
            strips[i].valLabel.setText(juce::String(val), juce::dontSendNotification);
        };
        addAndMakeVisible(strip.slider);

        strip.label.setText(trackNames[i], juce::dontSendNotification);
        strip.label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        strip.label.setJustificationType(juce::Justification::centred);
        strip.label.setColour(juce::Label::textColourId, juce::Colour(0xff80deea));
        addAndMakeVisible(strip.label);

        strip.valLabel.setText(juce::String(synth.getChannelVolume(midiCh)), juce::dontSendNotification);
        strip.valLabel.setFont(juce::FontOptions(10.0f));
        strip.valLabel.setJustificationType(juce::Justification::centred);
        strip.valLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(strip.valLabel);
    }
}

void MainComponent::MixerComponent::updateSlidersFromSynth()
{
    for (int i = 0; i < 8; ++i)
    {
        const int vol = synth.getChannelVolume(i + 8);
        strips[i].slider.setValue(vol, juce::dontSendNotification);
        strips[i].valLabel.setText(juce::String(vol), juce::dontSendNotification);
    }
}

void MainComponent::MixerComponent::resized()
{
    auto area = getLocalBounds().reduced(5, 4);
    const int stripW = area.getWidth() / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto col = area.removeFromLeft(stripW);
        strips[i].label.setBounds(col.removeFromTop(18));
        strips[i].valLabel.setBounds(col.removeFromBottom(16));
        strips[i].slider.setBounds(col);
    }
}

// -----------------------------------------------------------------------------
// MainComponent Implementation
// -----------------------------------------------------------------------------
MainComponent::MainComponent(MainAudioSource& source)
    : audioSource(source),
      keyboardComponent(source.getChordDetector()),
      mixerComp(source.getSynthManager())
{
    loadPersistentSettings();

    // 1. Touchscreen Tabs
    tabHomeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b262c));
    tabHomeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff80deea));
    tabHomeBtn.onClick = [this] { activeScreenTab = 0; resized(); };
    addAndMakeVisible(tabHomeBtn);

    tabStylesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b262c));
    tabStylesBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff80deea));
    tabStylesBtn.onClick = [this] { activeScreenTab = 1; resized(); };
    addAndMakeVisible(tabStylesBtn);

    tabVoicesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b262c));
    tabVoicesBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff80deea));
    tabVoicesBtn.onClick = [this] { activeScreenTab = 2; resized(); };
    addAndMakeVisible(tabVoicesBtn);

    tabMixerBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b262c));
    tabMixerBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff80deea));
    tabMixerBtn.onClick = [this] { activeScreenTab = 3; mixerComp.updateSlidersFromSynth(); resized(); };
    addAndMakeVisible(tabMixerBtn);

    // 2. Load SoundFont and Style Folder Buttons
    loadSf2Btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00695c));
    loadSf2Btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadSf2Btn.onClick = [this] { chooseSoundFont(); };
    addAndMakeVisible(loadSf2Btn);

    chooseStyleDirBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00838f));
    chooseStyleDirBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    chooseStyleDirBtn.onClick = [this] { chooseStyleDirectory(); };
    addChildComponent(chooseStyleDirBtn);

    // 3. Central LCD Screen Labels (Home Tab)
    screenStyleNameLabel.setText("STYLE: None Selected", juce::dontSendNotification);
    screenStyleNameLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    screenStyleNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(screenStyleNameLabel);

    screenChordLabel.setText("--", juce::dontSendNotification);
    screenChordLabel.setFont(juce::FontOptions(46.0f, juce::Font::bold));
    screenChordLabel.setJustificationType(juce::Justification::centred);
    screenChordLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e676));
    addAndMakeVisible(screenChordLabel);

    screenSectionLabel.setText("SECTION: --", juce::dontSendNotification);
    screenSectionLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    screenSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffd54f));
    addAndMakeVisible(screenSectionLabel);

    screenTempoLabel.setText("TEMPO: 120", juce::dontSendNotification);
    screenTempoLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    screenTempoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff80deea));
    addAndMakeVisible(screenTempoLabel);

    screenLeadVoiceLabel.setText("RIGHT 1: Grand Piano", juce::dontSendNotification);
    screenLeadVoiceLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    screenLeadVoiceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff80ab));
    addAndMakeVisible(screenLeadVoiceLabel);

    screenStatusLabel.setText("Ready", juce::dontSendNotification);
    screenStatusLabel.setFont(juce::FontOptions(11.0f));
    screenStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(screenStatusLabel);

    // 4. Style List Box & Mixer Component
    styleListBox.setModel(&styleListModel);
    styleListBox.setRowHeight(24);
    styleListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d1b2a));
    addChildComponent(styleListBox);
    addChildComponent(mixerComp);

    // 5. Voice Category Browser (Voices Tab)
    openVoiceBrowser();
    addChildComponent(voiceCategoryTabs);

    // 6. Hardware Rotary Knobs
    auto setupKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& text, double min, double max, double init)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        s.setRange(min, max, 1);
        s.setValue(init, juce::dontSendNotification);
        addAndMakeVisible(s);

        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(l);
    };

    setupKnob(masterVolKnob, masterVolLabel, "MASTER", 0, 150, 100);
    masterVolKnob.onValueChange = [this] {
        audioSource.getSynthManager().setMasterVolume(static_cast<float>(masterVolKnob.getValue() / 100.0));
    };

    setupKnob(styleVolKnob, styleVolLabel, "STYLE", 0, 150, 85);
    styleVolKnob.onValueChange = [this] {
        audioSource.getSynthManager().setStyleVolume(static_cast<float>(styleVolKnob.getValue() / 100.0));
    };

    setupKnob(leadVolKnob, leadVolLabel, "LEAD", 0, 150, 115);
    leadVolKnob.onValueChange = [this] {
        audioSource.getSynthManager().setLeadVolume(static_cast<float>(leadVolKnob.getValue() / 100.0));
    };

    setupKnob(tempoKnob, tempoLabel, "TEMPO", 40, 240, 120);
    tempoKnob.onValueChange = [this] {
        audioSource.getStyleEngine().setBpm(tempoKnob.getValue());
    };

    // OTS Buttons
    auto setupOts = [this](juce::TextButton& btn, int index)
    {
        btn.setRadioGroupId(201);
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
        btn.onClick = [this, index]
        {
            if (currentStyleParser != nullptr)
                audioSource.getSynthManager().applyOTSSettings(currentStyleParser->getOTS(index));
        };
        addAndMakeVisible(btn);
    };

    setupOts(ots1Btn, 0);
    setupOts(ots2Btn, 1);
    setupOts(ots3Btn, 2);
    setupOts(ots4Btn, 3);
    ots1Btn.setToggleState(true, juce::dontSendNotification);

    // 7. Horizontal Section Strip (Directly above keyboard)
    auto setupStrip = [this](juce::TextButton& btn, const juce::Colour& col)
    {
        btn.setColour(juce::TextButton::buttonColourId, col);
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(btn);
    };

    setupStrip(intro1Btn, juce::Colour(0xff37474f));
    intro1Btn.onClick = [this] { audioSource.getStyleEngine().triggerIntro(1); };
    setupStrip(intro2Btn, juce::Colour(0xff37474f));
    intro2Btn.onClick = [this] { audioSource.getStyleEngine().triggerIntro(2); };
    setupStrip(intro3Btn, juce::Colour(0xff37474f));
    intro3Btn.onClick = [this] { audioSource.getStyleEngine().triggerIntro(3); };

    setupStrip(main1Btn, juce::Colour(0xff1565c0));
    main1Btn.onClick = [this] { audioSource.getStyleEngine().transitionToMain(1, true); };
    setupStrip(main2Btn, juce::Colour(0xff1565c0));
    main2Btn.onClick = [this] { audioSource.getStyleEngine().transitionToMain(2, true); };
    setupStrip(main3Btn, juce::Colour(0xff1565c0));
    main3Btn.onClick = [this] { audioSource.getStyleEngine().transitionToMain(3, true); };
    setupStrip(main4Btn, juce::Colour(0xff1565c0));
    main4Btn.onClick = [this] { audioSource.getStyleEngine().transitionToMain(4, true); };

    setupStrip(fillBreakBtn, juce::Colour(0xffe65100));
    fillBreakBtn.onClick = [this] { audioSource.getStyleEngine().triggerBreakOrFill(); };

    autoFillToggle.setToggleState(true, juce::dontSendNotification);
    autoFillToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    autoFillToggle.onClick = [this] {
        audioSource.getStyleEngine().setAutoFillEnabled(autoFillToggle.getToggleState());
    };
    addAndMakeVisible(autoFillToggle);

    setupStrip(ending1Btn, juce::Colour(0xff455a64));
    ending1Btn.onClick = [this] { audioSource.getStyleEngine().triggerEnding(1); };
    setupStrip(ending2Btn, juce::Colour(0xff455a64));
    ending2Btn.onClick = [this] { audioSource.getStyleEngine().triggerEnding(2); };
    setupStrip(ending3Btn, juce::Colour(0xff455a64));
    ending3Btn.onClick = [this] { audioSource.getStyleEngine().triggerEnding(3); };

    setupStrip(startStopBtn, juce::Colour(0xff1b5e20));
    startStopBtn.onClick = [this]
    {
        auto& engine = audioSource.getStyleEngine();
        if (engine.isPlaying())
        {
            engine.stop(audioSource.getSynthManager());
            startStopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b5e20));
        }
        else
        {
            engine.play();
            startStopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb71c1c));
        }
    };

    singleFingerToggle.setToggleState(audioSource.getChordDetector().isSingleFingerMode(), juce::dontSendNotification);
    singleFingerToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    singleFingerToggle.onClick = [this] {
        audioSource.getChordDetector().setSingleFingerMode(singleFingerToggle.getToggleState());
    };
    addAndMakeVisible(singleFingerToggle);

    // 8. 61-Key Interactive Keyboard
    addAndMakeVisible(keyboardComponent);

    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    savePersistentSettings();
}

void MainComponent::loadPersistentSettings()
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VirtualArranger");
    if (!configDir.exists()) configDir.createDirectory();

    juce::PropertiesFile::Options opts;
    opts.applicationName     = "VirtualArranger";
    opts.filenameSuffix      = "settings";
    opts.folderName          = "VirtualArranger";
    opts.osxLibrarySubFolder = "Application Support";

    appProperties = std::make_unique<juce::PropertiesFile>(opts);

    juce::String sf2Path = appProperties->getValue("soundfont_path");
    if (sf2Path.isNotEmpty() && juce::File(sf2Path).existsAsFile())
    {
        if (audioSource.getSynthManager().loadSoundFont(juce::File(sf2Path)))
        {
            loadSf2Btn.setButtonText("SF2: " + juce::File(sf2Path).getFileNameWithoutExtension());
            screenStatusLabel.setText("SoundFont Ready: " + juce::File(sf2Path).getFileName(), juce::dontSendNotification);
        }
    }

    juce::String styDir = appProperties->getValue("style_dir_path");
    if (styDir.isNotEmpty() && juce::File(styDir).isDirectory())
    {
        scanStyleDirectory(juce::File(styDir));
    }
}

void MainComponent::savePersistentSettings()
{
    if (appProperties != nullptr)
        appProperties->saveIfNeeded();
}

void MainComponent::chooseSoundFont()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select SoundFont (.sf2)",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.sf2");

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            if (audioSource.getSynthManager().loadSoundFont(file))
            {
                if (appProperties != nullptr)
                {
                    appProperties->setValue("soundfont_path", file.getFullPathName());
                    appProperties->saveIfNeeded();
                }

                loadSf2Btn.setButtonText("SF2: " + file.getFileNameWithoutExtension());
                screenStatusLabel.setText("SoundFont Ready: " + file.getFileName(), juce::dontSendNotification);
            }
        }
    });
}

void MainComponent::chooseStyleDirectory()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Yamaha Style Directory",
        currentStyleDirectory.exists() ? currentStyleDirectory : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
                scanStyleDirectory(dir);
        });
}

void MainComponent::scanStyleDirectory(const juce::File& dir)
{
    currentStyleDirectory = dir;
    availableStyles.clear();

    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.sty;*.prs;*.sst;*.mid");
    availableStyles.addArray(files);

    if (appProperties != nullptr)
    {
        appProperties->setValue("style_dir_path", dir.getFullPathName());
        appProperties->saveIfNeeded();
    }

    styleListBox.updateContent();
    styleListBox.repaint();

    if (!availableStyles.isEmpty())
        loadStyleFile(availableStyles.getFirst());
}

void MainComponent::loadStyleFile(const juce::File& file)
{
    currentStyleParser = std::make_shared<StyleParser>();
    if (currentStyleParser->loadStyleFile(file))
    {
        audioSource.getStyleEngine().setStyle(currentStyleParser, audioSource.getSynthManager());
        screenStyleNameLabel.setText("STYLE: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);

        updateOtsButtonLabels(*currentStyleParser);
        audioSource.getSynthManager().applyOTSSettings(currentStyleParser->getOTS(0));
        ots1Btn.setToggleState(true, juce::dontSendNotification);

        // ADD THIS LINE TO REFRESH THE MIXER FADERS FOR THE NEW STYLE:
        mixerComp.updateSlidersFromSynth();

        tempoKnob.setValue(currentStyleParser->getInitialBpm(), juce::dontSendNotification);
        screenStatusLabel.setText("Style Ready: " + file.getFileName(), juce::dontSendNotification);
    }
}

void MainComponent::openVoiceBrowser()
{
    voiceCategoryTabs.clearTabs();

    struct CategoryDef {
        const char* name;
        int startProg;
        int endProg;
    };

    static const CategoryDef categories[] = {
        { "Piano & Keys", 0, 7 },
        { "Organ & Acc.", 16, 23 },
        { "Guitars", 24, 31 },
        { "Strings/Choir", 40, 55 },
        { "Brass & Winds", 56, 79 },
        { "Synths & Leads", 80, 103 }
    };

    for (const auto& cat : categories)
    {
        auto* comp = new juce::Component();
        int y = 5;

        for (int p = cat.startProg; p <= cat.endProg; ++p)
        {
            auto* btn = new juce::TextButton(StyleParser::getGMVoiceName(p));
            btn->setBounds(5, y, 220, 24);
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e272e));
            btn->onClick = [this, p] { selectVoice(p); };
            comp->addAndMakeVisible(btn);
            y += 28;
        }

        voiceCategoryTabs.addTab(cat.name, juce::Colour(0xff12181b), comp, true);
    }
}

void MainComponent::selectVoice(int programNumber)
{
    audioSource.getSynthManager().setLeadPreset(programNumber);
    screenLeadVoiceLabel.setText("RIGHT 1: " + StyleParser::getGMVoiceName(programNumber), juce::dontSendNotification);
}

void MainComponent::updateOtsButtonLabels(const StyleParser& parser)
{
    auto formatBtn = [](juce::TextButton& btn, const juce::String& prefix, const OTSPart& p)
    {
        juce::String name = p.voiceName;
        if (name.isEmpty()) name = StyleParser::getGMVoiceName(p.program);
        btn.setButtonText(prefix + ": " + name);
    };

    formatBtn(ots1Btn, "OTS 1", parser.getOTS(0).right1);
    formatBtn(ots2Btn, "OTS 2", parser.getOTS(1).right1);
    formatBtn(ots3Btn, "OTS 3", parser.getOTS(2).right1);
    formatBtn(ots4Btn, "OTS 4", parser.getOTS(3).right1);
}

void MainComponent::timerCallback()
{
    const auto chord = audioSource.getChordDetector().getCurrentChord();
    screenChordLabel.setText(ChordDetector::getChordName(chord), juce::dontSendNotification);

    const int bpm = static_cast<int>(audioSource.getStyleEngine().getBpm());
    screenTempoLabel.setText("TEMPO: " + juce::String(bpm) + " BPM", juce::dontSendNotification);

    const juce::String curSec = audioSource.getStyleEngine().getCurrentSectionName();
    screenSectionLabel.setText("SECTION: " + (curSec.isNotEmpty() ? curSec : "--"), juce::dontSendNotification);

    keyboardComponent.repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    // 1. Medeli AKX10 Matte Gunmetal Chassis
    juce::ColourGradient chassisGrad(juce::Colour(0xff1e2227), 0.0f, 0.0f,
                                     juce::Colour(0xff111316), 0.0f, area.getHeight(), false);
    g.setGradientFill(chassisGrad);
    g.fillRect(area);

    // 2. Speaker Mesh Panels
    auto drawSpeakerMesh = [&g](const juce::Rectangle<float>& r)
    {
        g.setColour(juce::Colour(0xff0d0f12));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(juce::Colour(0xff252930).withAlpha(0.6f));
        g.drawRoundedRectangle(r, 6.0f, 1.5f);

        for (float y = r.getY() + 8.0f; y < r.getBottom() - 8.0f; y += 8.0f)
            g.drawHorizontalLine(static_cast<int>(y), r.getX() + 8.0f, r.getRight() - 8.0f);
    };

    drawSpeakerMesh(juce::Rectangle<float>(15.0f, 15.0f, 150.0f, 210.0f));
    drawSpeakerMesh(juce::Rectangle<float>(area.getWidth() - 165.0f, 15.0f, 150.0f, 210.0f));

    // 3. Central Color Touchscreen Glass
    const auto screenBounds = juce::Rectangle<float>(area.getWidth() * 0.28f, 15.0f, area.getWidth() * 0.44f, 210.0f);

    g.setColour(juce::Colour(0xff050709));
    g.fillRoundedRectangle(screenBounds.expanded(4.0f), 8.0f);

    juce::ColourGradient glassGrad(juce::Colour(0xff0d1b2a), screenBounds.getX(), screenBounds.getY(),
                                   juce::Colour(0xff060c14), screenBounds.getX(), screenBounds.getBottom(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(screenBounds, 6.0f);

    g.setColour(juce::Colour(0xff00acc1).withAlpha(0.5f));
    g.drawRoundedRectangle(screenBounds, 6.0f, 1.5f);

    // 4. Pitch-Bend Joystick Pod
    const auto joyRect = juce::Rectangle<float>(20.0f, area.getHeight() - 170.0f, 50.0f, 120.0f);
    g.setColour(juce::Colour(0xff121417));
    g.fillRoundedRectangle(joyRect, 5.0f);
    g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.7f));
    g.fillEllipse(joyRect.getCentreX() - 15.0f, joyRect.getCentreY() - 15.0f, 30.0f, 30.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Bottom Piano Keyboard
    auto bottomArea = area.removeFromBottom(175).reduced(10, 5);
    bottomArea.removeFromLeft(65);
    keyboardComponent.setBounds(bottomArea);

    // 2. Horizontal Section Control Strip (Directly above keyboard)
    auto stripArea = area.removeFromBottom(38).reduced(15, 2);

    intro1Btn.setBounds(stripArea.removeFromLeft(65));
    stripArea.removeFromLeft(4);
    intro2Btn.setBounds(stripArea.removeFromLeft(65));
    stripArea.removeFromLeft(4);
    intro3Btn.setBounds(stripArea.removeFromLeft(65));
    stripArea.removeFromLeft(12);

    main1Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(4);
    main2Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(4);
    main3Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(4);
    main4Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(12);

    fillBreakBtn.setBounds(stripArea.removeFromLeft(95));
    stripArea.removeFromLeft(6);
    autoFillToggle.setBounds(stripArea.removeFromLeft(90));
    stripArea.removeFromLeft(12);

    ending1Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(4);
    ending2Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(4);
    ending3Btn.setBounds(stripArea.removeFromLeft(68));
    stripArea.removeFromLeft(12);

    startStopBtn.setBounds(stripArea.removeFromLeft(110));
    stripArea.removeFromLeft(8);
    singleFingerToggle.setBounds(stripArea);

    // 3. Top Workstation Area
    auto topArea = area.reduced(15, 10);

    // Left Panel: Knobs
    auto leftControls = topArea.removeFromLeft(static_cast<int>(topArea.getWidth() * 0.28f));
    leftControls.removeFromLeft(160);

    auto masterArea = leftControls.removeFromTop(55);
    masterVolLabel.setBounds(masterArea.removeFromTop(16));
    masterVolKnob.setBounds(masterArea);

    auto styleArea = leftControls.removeFromTop(55);
    styleVolLabel.setBounds(styleArea.removeFromTop(16));
    styleVolKnob.setBounds(styleArea);

    auto leadArea = leftControls.removeFromTop(55);
    leadVolLabel.setBounds(leadArea.removeFromTop(16));
    leadVolKnob.setBounds(leadArea);

    // Right Panel: Knobs & OTS
    auto rightControls = topArea.removeFromRight(static_cast<int>(topArea.getWidth() * 0.35f));
    rightControls.removeFromRight(160);

    auto tempoArea = rightControls.removeFromTop(65);
    tempoLabel.setBounds(tempoArea.removeFromTop(16));
    tempoKnob.setBounds(tempoArea);

    auto otsRow1 = rightControls.removeFromTop(32);
    ots1Btn.setBounds(otsRow1.removeFromLeft(otsRow1.getWidth() / 2 - 2));
    otsRow1.removeFromLeft(4);
    ots2Btn.setBounds(otsRow1);

    rightControls.removeFromTop(4);

    auto otsRow2 = rightControls.removeFromTop(32);
    ots3Btn.setBounds(otsRow2.removeFromLeft(otsRow2.getWidth() / 2 - 2));
    otsRow2.removeFromLeft(4);
    ots4Btn.setBounds(otsRow2);

    // 4. Central Screen Area
    auto screenArea = topArea.reduced(15, 5);

    auto tabRow = screenArea.removeFromTop(28);
    tabHomeBtn.setBounds(tabRow.removeFromLeft(65));
    tabRow.removeFromLeft(4);
    tabStylesBtn.setBounds(tabRow.removeFromLeft(65));
    tabRow.removeFromLeft(4);
    tabVoicesBtn.setBounds(tabRow.removeFromLeft(65));
    tabRow.removeFromLeft(4);
    tabMixerBtn.setBounds(tabRow.removeFromLeft(65));

    screenArea.removeFromTop(6);

    // Visibility toggling
    screenStyleNameLabel.setVisible(activeScreenTab == 0);
    screenChordLabel.setVisible(activeScreenTab == 0);
    screenSectionLabel.setVisible(activeScreenTab == 0);
    screenTempoLabel.setVisible(activeScreenTab == 0);
    screenLeadVoiceLabel.setVisible(activeScreenTab == 0);
    screenStatusLabel.setVisible(activeScreenTab == 0);
    loadSf2Btn.setVisible(activeScreenTab == 0);

    chooseStyleDirBtn.setVisible(activeScreenTab == 1);
    styleListBox.setVisible(activeScreenTab == 1);

    voiceCategoryTabs.setVisible(activeScreenTab == 2);
    mixerComp.setVisible(activeScreenTab == 3);

    if (activeScreenTab == 0) // HOME DASHBOARD
    {
        screenStyleNameLabel.setBounds(screenArea.removeFromTop(20));
        screenChordLabel.setBounds(screenArea.removeFromTop(55));
        auto r = screenArea.removeFromTop(20);
        screenSectionLabel.setBounds(r.removeFromLeft(r.getWidth() / 2));
        screenTempoLabel.setBounds(r);
        screenLeadVoiceLabel.setBounds(screenArea.removeFromTop(20));

        auto bottomRow = screenArea.removeFromBottom(24);
        loadSf2Btn.setBounds(bottomRow.removeFromLeft(180));
        screenStatusLabel.setBounds(bottomRow);
    }
    else if (activeScreenTab == 1) // STYLES BROWSER
    {
        chooseStyleDirBtn.setBounds(screenArea.removeFromTop(28));
        screenArea.removeFromTop(4);
        styleListBox.setBounds(screenArea);
    }
    else if (activeScreenTab == 2) // VOICES BROWSER
    {
        voiceCategoryTabs.setBounds(screenArea);
    }
    else if (activeScreenTab == 3) // 8-TRACK LIVE MIXER
    {
        mixerComp.setBounds(screenArea);
    }
}