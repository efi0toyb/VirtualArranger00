#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
#include <vector>
#include "MainAudioSource.h"
#include "AudioSettingsHelper.h"

class ArrangerHttpServer : public juce::Thread
{
public:
    ArrangerHttpServer(MainAudioSource& audioSource, juce::AudioDeviceManager& deviceManager, const juce::File& webDir);
    ~ArrangerHttpServer() override;

    bool startServer(int port = 8088);
    void stopServer();

    int getPort() const noexcept { return serverPort; }
    juce::String getBaseUrl() const { return "http://127.0.0.1:" + juce::String(serverPort); }

    void loadStyleFile(const juce::File& file);
    void loadSoundFontFile(const juce::File& file);

    const juce::File& getCurrentStyleFile() const noexcept { return currentStyleFile; }
    const juce::File& getCurrentSoundFontFile() const noexcept { return currentSoundFontFile; }

private:
    void run() override;

    void handleClientConnection(std::unique_ptr<juce::StreamingSocket> client);
    juce::String handleApiRequest(const juce::String& method, const juce::String& path, const juce::String& query, const juce::String& body);

    juce::String chooseFolderDialog();
    juce::String chooseFileDialog(const juce::String& title, const juce::String& filterSpec);

    juce::String getStatusJson();
    juce::String getAudioSettingsJson();
    juce::String handleAudioSettingsPost(const juce::var& json);
    juce::String handleTransportPost(const juce::var& json);
    juce::String handleSectionPost(const juce::var& json);
    juce::String handleOTSPost(const juce::var& json);
    juce::String handleLeadVoicePost(const juce::var& json);
    juce::String handleMixerPost(const juce::var& json);
    juce::String handleNotePost(const juce::var& json);
    juce::String handleLoadStylePost(const juce::var& json);
    juce::String handleLoadSoundFontPost(const juce::var& json);
    juce::String handleBrowseStylesGet(const juce::String& queryParam);
    juce::String handleBrowseSoundFontsGet();

    MainAudioSource& audioSource;
    juce::AudioDeviceManager& deviceManager;
    juce::File webDirectory;
    juce::File currentStyleFile;
    juce::File currentSoundFontFile;

    std::unique_ptr<juce::StreamingSocket> serverSocket;
    std::atomic<bool> isRunning { false };
    int serverPort { 8088 };

    std::shared_ptr<StyleParser> activeStyleParser;
    int currentActiveOTS { 0 };

    juce::CriticalSection stateLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangerHttpServer)
};
