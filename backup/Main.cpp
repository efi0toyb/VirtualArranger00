#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainAudioSource.h"
#include "MainComponent.h"

class VirtualArrangerApplication : public juce::JUCEApplication
{
public:
    VirtualArrangerApplication() = default;

    const juce::String getApplicationName() override       { return "Virtual Arranger Workstation"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String&) override
    {
        audioSourcePlayer.setSource(&mainAudioSource);

        deviceManager.initialise(0, 2, nullptr, true);
        deviceManager.addAudioCallback(&audioSourcePlayer);

        for (const auto& midiInput : juce::MidiInput::getAvailableDevices())
        {
            deviceManager.setMidiInputDeviceEnabled(midiInput.identifier, true);
            deviceManager.addMidiInputDeviceCallback(midiInput.identifier, &mainAudioSource);
        }

        mainWindow = std::make_unique<MainWindow>(getApplicationName(), mainAudioSource);
    }

    void shutdown() override
    {
        for (const auto& midiInput : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(midiInput.identifier, &mainAudioSource);

        deviceManager.removeAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(nullptr);
        mainWindow = nullptr;
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, MainAudioSource& source)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(source), true);
            setSize(1180, 680);
            setResizable(true, true);
            setResizeLimits(1020, 600, 1600, 950);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;
    MainAudioSource mainAudioSource;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VirtualArrangerApplication)