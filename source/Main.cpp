#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainAudioSource.h"
#include "ArrangerHttpServer.h"
#include "AudioSettingsHelper.h"
#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
#endif

class ServerStatusComponent : public juce::Component
{
public:
    ServerStatusComponent(ArrangerHttpServer& srv, std::function<void()> onOpenUi)
        : server(srv), openUiCallback(std::move(onOpenUi))
    {
        titleLabel.setText("Yamaha Virtual Arranger Workstation", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        statusLabel.setText("Audio Engine & Web Server active at:\n" + server.getBaseUrl(), juce::dontSendNotification);
        statusLabel.setFont(juce::FontOptions(13.0f));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d2ff));
        statusLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusLabel);

        openUiButton.setButtonText("Open Modern Workstation GUI");
        openUiButton.onClick = [this]() {
            if (openUiCallback) openUiCallback();
        };
        addAndMakeVisible(openUiButton);

        infoLabel.setText("You can also open " + server.getBaseUrl() + " in any browser or tablet.", juce::dontSendNotification);
        infoLabel.setFont(juce::FontOptions(11.0f));
        infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        infoLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(infoLabel);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff12141a));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        statusLabel.setBounds(area.removeFromTop(42));
        area.removeFromTop(12);
        openUiButton.setBounds(area.removeFromTop(38).reduced(30, 0));
        area.removeFromTop(8);
        infoLabel.setBounds(area.removeFromTop(24));
    }

private:
    ArrangerHttpServer& server;
    std::function<void()> openUiCallback;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextButton openUiButton;
    juce::Label infoLabel;
};

class VirtualArrangerApplication : public juce::JUCEApplication
{
public:
    VirtualArrangerApplication() = default;

    const juce::String getApplicationName() override       { return "Virtual Arranger Workstation"; }
    const juce::String getApplicationVersion() override    { return "2.0.0"; }
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

        juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        juce::File webDir = exeDir.getChildFile("web");
        if (!webDir.isDirectory())
            webDir = juce::File::getCurrentWorkingDirectory().getChildFile("web");
        if (!webDir.isDirectory())
            webDir = juce::File("C:/VirtualArranger/web");

        httpServer = std::make_unique<ArrangerHttpServer>(mainAudioSource, deviceManager, webDir);
        httpServer->startServer(8088);

        // Auto-detect and load default SoundFont if present
        juce::Array<juce::File> candidateSf2s = {
            juce::File("C:/VirtualArranger/yamaha tyros 4_just_t4_fixed.sf2"),
            exeDir.getChildFile("yamaha tyros 4_just_t4_fixed.sf2"),
            juce::File("C:/VirtualArranger/yamaha.sf2"),
            exeDir.getChildFile("yamaha.sf2"),
            juce::File("C:/VirtualArranger/default.sf2"),
            exeDir.getChildFile("default.sf2")
        };
        for (const auto& sf2 : candidateSf2s)
        {
            if (sf2.existsAsFile())
            {
                httpServer->loadSoundFontFile(sf2);
                break;
            }
        }

        // Auto launch Modern Web GUI
        launchWebGui();

        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *httpServer, [this]() {
            launchWebGui();
        });
    }

    void launchWebGui()
    {
        if (httpServer == nullptr) return;

        juce::String url = httpServer->getBaseUrl();

#if JUCE_WINDOWS
        // Check for Edge
        juce::File edgeExe("C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe");
        if (!edgeExe.existsAsFile())
            edgeExe = juce::File("C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe");

        if (edgeExe.existsAsFile())
        {
            // Use ShellExecuteW to launch Edge as an independent GUI process in app mode
            juce::String params = "--app=" + url + " --window-size=1260,820";
            HINSTANCE res = ShellExecuteW(nullptr, L"open",
                                          edgeExe.getFullPathName().toWideCharPointer(),
                                          params.toWideCharPointer(),
                                          nullptr,
                                          SW_SHOWNORMAL);

            if ((INT_PTR)res > 32)
                return;
        }
#endif

        // Fallback to user default browser
        juce::URL(url).launchInDefaultBrowser();
    }

    void shutdown() override
    {
        for (const auto& midiInput : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(midiInput.identifier, &mainAudioSource);

        deviceManager.removeAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(nullptr);

        if (httpServer != nullptr)
            httpServer->stopServer();
        httpServer = nullptr;

        mainWindow = nullptr;
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, ArrangerHttpServer& srv, std::function<void()> onOpenUi)
            : DocumentWindow(name,
                             juce::Colour(0xff12141a),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new ServerStatusComponent(srv, std::move(onOpenUi)), true);
            setSize(480, 220);
            setResizable(false, false);
            centreWithSize(480, 220);
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
    std::unique_ptr<ArrangerHttpServer> httpServer;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VirtualArrangerApplication)
