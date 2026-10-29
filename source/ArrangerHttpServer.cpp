#include "ArrangerHttpServer.h"
#include <sstream>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shlobj.h>
 #include <shobjidl.h>
#endif

ArrangerHttpServer::ArrangerHttpServer(MainAudioSource& src, juce::AudioDeviceManager& dm, const juce::File& webDir)
    : juce::Thread("ArrangerHttpServer"),
      audioSource(src),
      deviceManager(dm),
      webDirectory(webDir)
{
}

ArrangerHttpServer::~ArrangerHttpServer()
{
    stopServer();
}

bool ArrangerHttpServer::startServer(int port)
{
    stopServer();

    for (int p = port; p < port + 20; ++p)
    {
        serverSocket = std::make_unique<juce::StreamingSocket>();
        if (serverSocket->createListener(p, juce::String()))
        {
            serverPort = p;
            isRunning.store(true);
            startThread();
            return true;
        }
        serverSocket = nullptr;
    }

    return false;
}

void ArrangerHttpServer::stopServer()
{
    if (isRunning.load())
    {
        isRunning.store(false);
        if (serverSocket != nullptr)
            serverSocket->close();

        signalThreadShouldExit();
        stopThread(2000);
        serverSocket = nullptr;
    }
}

void ArrangerHttpServer::loadStyleFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    auto parser = std::make_shared<StyleParser>();
    if (parser->loadStyleFile(file))
    {
        const juce::ScopedLock sl(stateLock);
        activeStyleParser = parser;
        currentStyleFile = file;
        audioSource.getStyleEngine().setStyle(parser, audioSource.getSynthManager());

        // Apply OTS 1 by default
        audioSource.getSynthManager().applyOTSSettings(parser->getOTS(0));
        currentActiveOTS = 0;
    }
}

void ArrangerHttpServer::loadSoundFontFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    if (audioSource.getSynthManager().loadSoundFont(file))
    {
        const juce::ScopedLock sl(stateLock);
        currentSoundFontFile = file;

        if (activeStyleParser != nullptr)
            audioSource.getSynthManager().applyOTSSettings(activeStyleParser->getOTS(currentActiveOTS));
    }
}

void ArrangerHttpServer::run()
{
    while (!threadShouldExit() && isRunning.load())
    {
        if (serverSocket == nullptr) break;

        auto* rawClient = serverSocket->waitForNextConnection();
        if (rawClient != nullptr)
        {
            std::unique_ptr<juce::StreamingSocket> client(rawClient);
            handleClientConnection(std::move(client));
        }
    }
}

void ArrangerHttpServer::handleClientConnection(std::unique_ptr<juce::StreamingSocket> client)
{
    if (client == nullptr) return;

    // Read HTTP Request Header with timeout
    std::string requestData;
    char buffer[2048];

    if (client->waitUntilReady(true, 2000) <= 0)
        return;

    while (requestData.find("\r\n\r\n") == std::string::npos)
    {
        int bytesRead = client->read(buffer, sizeof(buffer) - 1, false);
        if (bytesRead <= 0)
        {
            if (client->waitUntilReady(true, 300) <= 0)
                break;
            bytesRead = client->read(buffer, sizeof(buffer) - 1, false);
            if (bytesRead <= 0) break;
        }
        buffer[bytesRead] = '\0';
        requestData.append(buffer, static_cast<size_t>(bytesRead));
        if (requestData.length() > 65536) break;
    }

    if (requestData.empty()) return;

    // Parse Request Line
    std::istringstream reqStream(requestData);
    std::string method, fullUrl, protocol;
    reqStream >> method >> fullUrl >> protocol;

    juce::String jMethod = juce::String(method).toUpperCase();
    juce::String jUrl = juce::String(fullUrl);

    juce::String path = jUrl.upToFirstOccurrenceOf("?", false, false);
    juce::String query = jUrl.fromFirstOccurrenceOf("?", false, false);

    // Read Body if POST
    std::string body;
    auto headerEnd = requestData.find("\r\n\r\n");
    if (headerEnd != std::string::npos)
    {
        body = requestData.substr(headerEnd + 4);

        // Find Content-Length if needed
        auto clPos = requestData.find("Content-Length: ");
        if (clPos == std::string::npos) clPos = requestData.find("content-length: ");
        if (clPos != std::string::npos)
        {
            size_t valStart = clPos + 16;
            size_t valEnd = requestData.find("\r\n", valStart);
            if (valEnd != std::string::npos)
            {
                int contentLen = std::atoi(requestData.substr(valStart, valEnd - valStart).c_str());
                while (static_cast<int>(body.length()) < contentLen)
                {
                    if (client->waitUntilReady(true, 500) <= 0) break;
                    int bytesRead = client->read(buffer, sizeof(buffer) - 1, false);
                    if (bytesRead <= 0) break;
                    buffer[bytesRead] = '\0';
                    body.append(buffer, static_cast<size_t>(bytesRead));
                }
            }
        }
    }

    // CORS preflight OPTIONS
    if (jMethod == "OPTIONS")
    {
        juce::String response = "HTTP/1.1 204 No Content\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                "Access-Control-Allow-Headers: Content-Type\r\n"
                                "Connection: close\r\n\r\n";
        client->write(response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        return;
    }

    // 1. Handle API Requests
    if (path.startsWith("/api/"))
    {
        juce::String jsonResponse = handleApiRequest(jMethod, path, query, juce::String::fromUTF8(body.c_str(), static_cast<int>(body.length())));
        juce::String response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json; charset=utf-8\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Content-Length: " + juce::String(jsonResponse.getNumBytesAsUTF8()) + "\r\n"
                                "Connection: close\r\n\r\n" + jsonResponse;

        client->write(response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        return;
    }

    // 2. Handle Static Files from webDirectory with fallback
    juce::String relPath = path.substring(1);
    if (relPath.isEmpty() || relPath == "/")
        relPath = "index.html";

    while (relPath.startsWithChar('/') || relPath.startsWithChar('\\'))
        relPath = relPath.substring(1);

    juce::File targetFile = webDirectory.getChildFile(relPath);

    if (!targetFile.existsAsFile())
    {
        juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        targetFile = exeDir.getChildFile("web").getChildFile(relPath);
    }
    if (!targetFile.existsAsFile())
    {
        juce::File projectWeb("C:/VirtualArranger/web");
        targetFile = projectWeb.getChildFile(relPath);
    }

    if (targetFile.existsAsFile())
    {
        juce::String mime = "application/octet-stream";
        if (relPath.endsWithIgnoreCase(".html") || relPath.endsWithIgnoreCase(".htm")) mime = "text/html; charset=utf-8";
        else if (relPath.endsWithIgnoreCase(".css")) mime = "text/css; charset=utf-8";
        else if (relPath.endsWithIgnoreCase(".js"))  mime = "application/javascript; charset=utf-8";
        else if (relPath.endsWithIgnoreCase(".json")) mime = "application/json; charset=utf-8";
        else if (relPath.endsWithIgnoreCase(".svg"))  mime = "image/svg+xml";
        else if (relPath.endsWithIgnoreCase(".png"))  mime = "image/png";
        else if (relPath.endsWithIgnoreCase(".ico"))  mime = "image/x-icon";
        else if (relPath.endsWithIgnoreCase(".woff2")) mime = "font/woff2";
        else if (relPath.endsWithIgnoreCase(".woff")) mime = "font/woff";
        else if (relPath.endsWithIgnoreCase(".ttf"))  mime = "font/ttf";

        juce::MemoryBlock fileData;
        targetFile.loadFileAsData(fileData);

        juce::String header = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: " + mime + "\r\n"
                              "Content-Length: " + juce::String(fileData.getSize()) + "\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Connection: close\r\n\r\n";

        client->write(header.toRawUTF8(), static_cast<int>(header.getNumBytesAsUTF8()));
        client->write(fileData.getData(), static_cast<int>(fileData.getSize()));
    }
    else
    {
        juce::String errorHtml = "<!DOCTYPE html><html><head><title>File Not Found</title></head>"
                                 "<body style='background:#12141a;color:#fff;font-family:sans-serif;padding:40px;text-align:center'>"
                                 "<h1>Workstation UI File Not Found</h1>"
                                 "<p>Requested: " + relPath + "</p>"
                                 "<p>Expected in: " + webDirectory.getFullPathName() + "</p>"
                                 "</body></html>";
        juce::String notFound = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=utf-8\r\n"
                                "Content-Length: " + juce::String(errorHtml.getNumBytesAsUTF8()) + "\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Connection: close\r\n\r\n" + errorHtml;
        client->write(notFound.toRawUTF8(), static_cast<int>(notFound.getNumBytesAsUTF8()));
    }
}

juce::String ArrangerHttpServer::handleApiRequest(const juce::String& method, const juce::String& path, const juce::String& query, const juce::String& body)
{
    if (path == "/api/status" && method == "GET")
        return getStatusJson();

    if (path == "/api/choose-folder")
    {
        juce::String chosen = chooseFolderDialog();
        if (chosen.isNotEmpty())
            return handleBrowseStylesGet(chosen);
        else
            return "{\"cancelled\":true}";
    }

    if (path == "/api/choose-soundfont")
    {
        juce::String chosen = chooseFileDialog("Select SoundFont (.sf2) File", "sf2");
        if (chosen.isNotEmpty())
        {
            juce::File f(chosen);
            if (f.existsAsFile())
            {
                loadSoundFontFile(f);
                juce::String cleanPath = chosen.replace("\\", "/");
                return "{\"success\":true,\"name\":\"" + f.getFileName() + "\",\"path\":\"" + cleanPath + "\"}";
            }
        }
        return "{\"cancelled\":true}";
    }

    if (path == "/api/choose-style")
    {
        juce::String chosen = chooseFileDialog("Select Yamaha Style File", "sty");
        if (chosen.isNotEmpty())
        {
            juce::File f(chosen);
            if (f.existsAsFile())
            {
                loadStyleFile(f);
                juce::String cleanPath = chosen.replace("\\", "/");
                return "{\"success\":true,\"name\":\"" + f.getFileName() + "\",\"path\":\"" + cleanPath + "\"}";
            }
        }
        return "{\"cancelled\":true}";
    }

    if (path == "/api/audio-settings")
    {
        if (method == "GET")
            return getAudioSettingsJson();
        else if (method == "POST")
            return handleAudioSettingsPost(juce::JSON::parse(body));
    }

    if (path == "/api/asio-control-panel")
    {
        bool opened = AudioSettingsHelper::showControlPanel(deviceManager);
        return "{\"success\":" + juce::String(opened ? "true" : "false") + "}";
    }

    if (method == "POST")
    {
        auto json = juce::JSON::parse(body);

        if (path == "/api/transport")
            return handleTransportPost(json);
        if (path == "/api/section")
            return handleSectionPost(json);
        if (path == "/api/ots")
            return handleOTSPost(json);
        if (path == "/api/mixer")
            return handleMixerPost(json);
        if (path == "/api/lead-voice")
            return handleLeadVoicePost(json);
        if (path == "/api/note")
            return handleNotePost(json);
        if (path == "/api/load-style")
            return handleLoadStylePost(json);
        if (path == "/api/load-soundfont")
            return handleLoadSoundFontPost(json);
    }

    if (path == "/api/browse-styles" && method == "GET")
    {
        juce::String dirParam;
        if (query.contains("dir="))
            dirParam = juce::URL::removeEscapeChars(query.fromFirstOccurrenceOf("dir=", false, false).upToFirstOccurrenceOf("&", false, false));
        return handleBrowseStylesGet(dirParam);
    }

    if (path == "/api/browse-soundfonts" && method == "GET")
    {
        return handleBrowseSoundFontsGet();
    }

    if (path == "/api/audio-devices" && method == "GET")
    {
        juce::String driver = juce::URL::removeEscapeChars(query.fromFirstOccurrenceOf("driver=", false, false).upToFirstOccurrenceOf("&", false, false));
        if (driver.isEmpty()) driver = AudioSettingsHelper::getCurrentDriverId(deviceManager);
        auto devs = AudioSettingsHelper::getAvailableOutputDevices(deviceManager, driver);
        auto obj = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> arr;
        for (const auto& d : devs) arr.add(d);
        obj->setProperty("driver", driver);
        obj->setProperty("devices", arr);
        return juce::JSON::toString(juce::var(obj.release()));
    }

    if (path == "/api/voices" && method == "GET")
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        auto presets = audioSource.getSynthManager().getAvailablePresets();
        juce::Array<juce::var> voiceList;
        for (const auto& p : presets)
        {
            auto vObj = std::make_unique<juce::DynamicObject>();
            vObj->setProperty("name", p.name);
            vObj->setProperty("bank", p.bank);
            vObj->setProperty("prog", p.program);
            vObj->setProperty("category", p.category);
            voiceList.add(juce::var(vObj.release()));
        }
        obj->setProperty("soundFontName", currentSoundFontFile.getFileNameWithoutExtension());
        obj->setProperty("count", voiceList.size());
        obj->setProperty("voices", voiceList);
        return juce::JSON::toString(juce::var(obj.release()));
    }

    return "{\"status\":\"ok\"}";
}

juce::String ArrangerHttpServer::getStatusJson()
{
    auto& engine = audioSource.getStyleEngine();
    auto& chordDet = audioSource.getChordDetector();
    auto& synth = audioSource.getSynthManager();

    auto rootObj = std::make_unique<juce::DynamicObject>();

    rootObj->setProperty("isPlaying", engine.isPlaying());
    rootObj->setProperty("bpm", engine.getBpm());

    auto chord = chordDet.getCurrentChord();
    rootObj->setProperty("chordName", ChordDetector::getChordName(chord));
    rootObj->setProperty("chordRoot", static_cast<int>(chord.rootNote));
    rootObj->setProperty("chordType", ChordDetector::getChordTypeName(chord.type));
    rootObj->setProperty("bassNote", static_cast<int>(chord.bassNote));

    rootObj->setProperty("sectionName", engine.getCurrentSectionName());
    rootObj->setProperty("variationIndex", engine.getCurrentVariationIndex());

    rootObj->setProperty("autoFill", engine.isAutoFillEnabled());
    rootObj->setProperty("syncStart", engine.isSyncStartEnabled());
    rootObj->setProperty("syncStop", engine.isSyncStopEnabled());
    rootObj->setProperty("otsLink", engine.isOtsLinkEnabled());

    rootObj->setProperty("singleFinger", chordDet.isSingleFingerMode());
    rootObj->setProperty("splitPoint", chordDet.getSplitPoint());

    rootObj->setProperty("masterVolume", synth.getMasterVolume());
    rootObj->setProperty("styleVolume", synth.getStyleVolume());
    rootObj->setProperty("soundFontLoaded", synth.isSoundFontLoaded());
    rootObj->setProperty("soundFontName", currentSoundFontFile.getFileName());

    rootObj->setProperty("styleLoaded", activeStyleParser != nullptr && activeStyleParser->hasStyleLoaded());
    rootObj->setProperty("styleName", currentStyleFile.getFileNameWithoutExtension());

    // Active Keyboard Notes
    juce::Array<juce::var> activeNotesLeft;
    juce::Array<juce::var> activeNotesRight;
    for (int n = 0; n < 128; ++n)
    {
        if (chordDet.isNoteActiveLeft(n)) activeNotesLeft.add(n);
        if (chordDet.isNoteActiveRight(n)) activeNotesRight.add(n);
    }
    rootObj->setProperty("activeNotesLeft", activeNotesLeft);
    rootObj->setProperty("activeNotesRight", activeNotesRight);

    // Channel Volumes, Mutes, Solos
    juce::Array<juce::var> chVols;
    juce::Array<juce::var> chMutes;
    juce::Array<juce::var> chSolos;
    for (int ch = 0; ch < 16; ++ch)
    {
        chVols.add(synth.getChannelVolume(ch));
        chMutes.add(synth.isChannelMuted(ch));
        chSolos.add(synth.isChannelSoloed(ch));
    }
    rootObj->setProperty("channelVolumes", chVols);
    rootObj->setProperty("channelMutes", chMutes);
    rootObj->setProperty("channelSolos", chSolos);

    // Sections
    juce::Array<juce::var> secArray;
    if (activeStyleParser != nullptr)
    {
        for (const auto& sec : activeStyleParser->getSections())
        {
            auto secObj = std::make_unique<juce::DynamicObject>();
            secObj->setProperty("name", sec.name);
            secObj->setProperty("role", static_cast<int>(sec.role));
            secObj->setProperty("variation", sec.variationIndex);
            secObj->setProperty("lengthTicks", sec.lengthTicks);
            secArray.add(juce::var(secObj.release()));
        }
    }
    rootObj->setProperty("sections", secArray);

    // OTS
    juce::Array<juce::var> otsArray;
    for (int i = 0; i < 4; ++i)
    {
        auto otsObj = std::make_unique<juce::DynamicObject>();
        if (activeStyleParser != nullptr)
        {
            const auto& ots = activeStyleParser->getOTS(i);
            otsObj->setProperty("r1Name", ots.right1.voiceName.isNotEmpty() ? ots.right1.voiceName : StyleParser::getGMVoiceName(ots.right1.program));
            otsObj->setProperty("r1Prog", ots.right1.program);
            otsObj->setProperty("r1Vol", ots.right1.volume);
        }
        else
        {
            otsObj->setProperty("r1Name", "Voice " + juce::String(i + 1));
            otsObj->setProperty("r1Prog", 0);
            otsObj->setProperty("r1Vol", 100);
        }
        otsArray.add(juce::var(otsObj.release()));
    }
    rootObj->setProperty("ots", otsArray);
    rootObj->setProperty("activeOTS", currentActiveOTS);

    return juce::JSON::toString(juce::var(rootObj.release()));
}

juce::String ArrangerHttpServer::getAudioSettingsJson()
{
    auto rootObj = std::make_unique<juce::DynamicObject>();

    // Available Drivers (Flattened with distinct ASIO drivers)
    juce::Array<juce::var> drivers;
    auto driverInfos = AudioSettingsHelper::getAvailableDriverInfos(deviceManager);
    for (const auto& info : driverInfos)
    {
        auto dObj = std::make_unique<juce::DynamicObject>();
        dObj->setProperty("id", info.id);
        dObj->setProperty("name", info.displayName);
        dObj->setProperty("isAsio", info.juceType.equalsIgnoreCase("ASIO"));
        drivers.add(juce::var(dObj.release()));
    }
    rootObj->setProperty("drivers", drivers);

    juce::String currentDriver = AudioSettingsHelper::getCurrentDriverId(deviceManager);
    rootObj->setProperty("currentDriver", currentDriver);

    // Devices for current driver
    juce::Array<juce::var> devices;
    for (const auto& dev : AudioSettingsHelper::getAvailableOutputDevices(deviceManager, currentDriver))
        devices.add(dev);
    rootObj->setProperty("devices", devices);
    rootObj->setProperty("currentDevice", AudioSettingsHelper::getCurrentOutputDevice(deviceManager));

    // Sample Rates
    juce::Array<juce::var> rates;
    for (double r : AudioSettingsHelper::getAvailableSampleRates(deviceManager))
        rates.add(r);
    rootObj->setProperty("sampleRates", rates);
    rootObj->setProperty("currentSampleRate", AudioSettingsHelper::getCurrentSampleRate(deviceManager));

    // Buffer Sizes
    juce::Array<juce::var> buffers;
    for (int b : AudioSettingsHelper::getAvailableBufferSizes(deviceManager))
        buffers.add(b);
    rootObj->setProperty("bufferSizes", buffers);
    rootObj->setProperty("currentBufferSize", AudioSettingsHelper::getCurrentBufferSize(deviceManager));

    // MIDI Inputs
    juce::Array<juce::var> midiList;
    for (const auto& m : AudioSettingsHelper::getMidiInputs(deviceManager))
    {
        auto mObj = std::make_unique<juce::DynamicObject>();
        mObj->setProperty("id", m.identifier);
        mObj->setProperty("name", m.name);
        mObj->setProperty("enabled", m.enabled);
        midiList.add(juce::var(mObj.release()));
    }
    rootObj->setProperty("midiInputs", midiList);

    return juce::JSON::toString(juce::var(rootObj.release()));
}

juce::String ArrangerHttpServer::handleAudioSettingsPost(const juce::var& json)
{
    juce::String driver = json["driver"].toString();
    juce::String device = json["device"].toString();
    double sampleRate = json["sampleRate"];
    int bufferSize = json["bufferSize"];

    bool ok = AudioSettingsHelper::applyDeviceSetup(deviceManager, driver, device, sampleRate, bufferSize);

    // MIDI Devices if provided
    if (json.hasProperty("midiDevices") && json["midiDevices"].isArray())
    {
        for (int i = 0; i < json["midiDevices"].size(); ++i)
        {
            auto item = json["midiDevices"][i];
            juce::String mId = item["id"].toString();
            bool enabled = item["enabled"];
            AudioSettingsHelper::setMidiInputEnabled(deviceManager, mId, enabled, &audioSource);
        }
    }

    return "{\"success\":" + juce::String(ok ? "true" : "false") + "}";
}

juce::String ArrangerHttpServer::handleTransportPost(const juce::var& json)
{
    juce::String action = json["action"].toString();
    auto& engine = audioSource.getStyleEngine();
    auto& chordDet = audioSource.getChordDetector();

    if (action == "play")
    {
        engine.play();
    }
    else if (action == "stop")
    {
        engine.stop(audioSource.getSynthManager());
    }
    else if (action == "set_bpm")
    {
        engine.setBpm(static_cast<double>(json["value"]));
    }
    else if (action == "set_autofill")
    {
        engine.setAutoFillEnabled(static_cast<bool>(json["value"]));
    }
    else if (action == "set_sync_start")
    {
        engine.setSyncStartEnabled(static_cast<bool>(json["value"]));
    }
    else if (action == "set_sync_stop")
    {
        engine.setSyncStopEnabled(static_cast<bool>(json["value"]));
    }
    else if (action == "set_ots_link")
    {
        engine.setOtsLinkEnabled(static_cast<bool>(json["value"]));
    }
    else if (action == "set_single_finger")
    {
        chordDet.setSingleFingerMode(static_cast<bool>(json["value"]));
    }
    else if (action == "set_split_point")
    {
        chordDet.setSplitPoint(static_cast<int>(json["value"]));
    }

    return "{\"success\":true}";
}

juce::String ArrangerHttpServer::handleSectionPost(const juce::var& json)
{
    juce::String type = json["type"].toString().toLowerCase();
    int index = json["index"];
    auto& engine = audioSource.getStyleEngine();

    if (type == "main")
    {
        engine.transitionToMain(index, true);
        if (engine.isOtsLinkEnabled() && activeStyleParser != nullptr)
        {
            currentActiveOTS = juce::jlimit(0, 3, index - 1);
            audioSource.getSynthManager().applyOTSSettings(activeStyleParser->getOTS(currentActiveOTS));
        }
    }
    else if (type == "fill")
    {
        engine.triggerBreakOrFill();
    }
    else if (type == "break")
    {
        engine.triggerBreak();
    }
    else if (type == "intro")
    {
        engine.triggerIntro(index);
    }
    else if (type == "ending")
    {
        engine.triggerEnding(index);
    }

    return "{\"success\":true}";
}

juce::String ArrangerHttpServer::handleOTSPost(const juce::var& json)
{
    int index = juce::jlimit(0, 3, static_cast<int>(json["index"]));
    currentActiveOTS = index;

    if (activeStyleParser != nullptr)
    {
        audioSource.getSynthManager().applyOTSSettings(activeStyleParser->getOTS(index));
    }

    return "{\"success\":true}";
}

juce::String ArrangerHttpServer::handleMixerPost(const juce::var& json)
{
    auto& synth = audioSource.getSynthManager();

    if (json.hasProperty("masterVolume"))
        synth.setMasterVolume(static_cast<float>(json["masterVolume"]));
    if (json.hasProperty("styleVolume"))
        synth.setStyleVolume(static_cast<float>(json["styleVolume"]));
    if (json.hasProperty("leadVolume"))
        synth.setLeadVolume(static_cast<float>(json["leadVolume"]));

    if (json.hasProperty("channel"))
    {
        int ch = json["channel"];
        if (json.hasProperty("volume"))
            synth.setChannelVolume(ch, static_cast<int>(json["volume"]));
        if (json.hasProperty("mute"))
            synth.setChannelMute(ch, static_cast<bool>(json["mute"]));
        if (json.hasProperty("solo"))
            synth.setChannelSolo(ch, static_cast<bool>(json["solo"]));
    }

    return "{\"success\":true}";
}

juce::String ArrangerHttpServer::handleNotePost(const juce::var& json)
{
    int note = json["note"];
    int vel = json.hasProperty("velocity") ? static_cast<int>(json["velocity"]) : 100;
    bool isOn = json["on"];

    auto msg = isOn ? juce::MidiMessage::noteOn(1, note, static_cast<uint8_t>(vel))
                    : juce::MidiMessage::noteOff(1, note);

    audioSource.handleIncomingMidiMessage(nullptr, msg);

    return "{\"success\":true}";
}

juce::String ArrangerHttpServer::handleLoadStylePost(const juce::var& json)
{
    juce::String path = json["path"].toString();
    juce::File f(path);
    if (f.existsAsFile())
    {
        loadStyleFile(f);
        return "{\"success\":true,\"name\":\"" + f.getFileName() + "\"}";
    }
    return "{\"success\":false,\"error\":\"File not found\"}";
}

juce::String ArrangerHttpServer::handleLoadSoundFontPost(const juce::var& json)
{
    juce::String path = json["path"].toString();
    juce::File f(path);
    if (f.existsAsFile())
    {
        loadSoundFontFile(f);
        return "{\"success\":true,\"name\":\"" + f.getFileName() + "\"}";
    }
    return "{\"success\":false,\"error\":\"File not found\"}";
}

juce::String ArrangerHttpServer::handleBrowseStylesGet(const juce::String& queryParam)
{
    juce::File searchDir;
    if (queryParam.isNotEmpty())
    {
        searchDir = juce::File(queryParam);
    }

    if (!searchDir.isDirectory())
    {
        if (currentStyleFile.existsAsFile() && currentStyleFile.getParentDirectory().isDirectory())
            searchDir = currentStyleFile.getParentDirectory();
        else if (juce::File("C:/VirtualArranger/set 2019").isDirectory())
            searchDir = juce::File("C:/VirtualArranger/set 2019");
        else if (juce::File("C:/VirtualArranger/styles").isDirectory())
            searchDir = juce::File("C:/VirtualArranger/styles");
        else if (juce::File("C:/VirtualArranger").isDirectory())
            searchDir = juce::File("C:/VirtualArranger");
        else
            searchDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    auto rootObj = std::make_unique<juce::DynamicObject>();
    rootObj->setProperty("currentDir", searchDir.getFullPathName().replace("\\", "/"));

    auto parent = searchDir.getParentDirectory();
    if (parent.isDirectory() && parent != searchDir)
        rootObj->setProperty("parentDir", parent.getFullPathName().replace("\\", "/"));
    else
        rootObj->setProperty("parentDir", "");

    // 1. Direct Subfolders
    juce::Array<juce::var> folderArray;
    juce::Array<juce::File> subDirs;
    searchDir.findChildFiles(subDirs, juce::File::findDirectories, false, "*");
    for (const auto& d : subDirs)
    {
        auto dObj = std::make_unique<juce::DynamicObject>();
        dObj->setProperty("name", d.getFileName());
        dObj->setProperty("path", d.getFullPathName().replace("\\", "/"));
        folderArray.add(juce::var(dObj.release()));
    }
    rootObj->setProperty("folders", folderArray);

    // 2. All Style Files in this folder and its child subfolders
    juce::Array<juce::var> fileArray;
    juce::Array<juce::File> found;
    searchDir.findChildFiles(found, juce::File::findFiles, true, "*");

    static const char* const validExts[] = { ".sty", ".prs", ".bcs", ".sst", ".mid" };

    for (const auto& f : found)
    {
        juce::String ext = f.getFileExtension().toLowerCase();
        bool valid = false;
        for (const auto* e : validExts)
        {
            if (ext == e) { valid = true; break; }
        }
        if (!valid) continue;

        auto fObj = std::make_unique<juce::DynamicObject>();
        fObj->setProperty("name", f.getFileName());
        fObj->setProperty("path", f.getFullPathName().replace("\\", "/"));
        fObj->setProperty("size", static_cast<int>(f.getSize()));

        // Relative subfolder path from searchDir
        juce::String relFolder = f.getParentDirectory().getFullPathName().replace("\\", "/");
        juce::String rootPath = searchDir.getFullPathName().replace("\\", "/");
        if (relFolder.startsWith(rootPath))
        {
            relFolder = relFolder.substring(rootPath.length());
            while (relFolder.startsWithChar('/'))
                relFolder = relFolder.substring(1);
        }
        fObj->setProperty("subfolder", relFolder);

        fileArray.add(juce::var(fObj.release()));
    }

    rootObj->setProperty("files", fileArray);
    return juce::JSON::toString(juce::var(rootObj.release()));
}

juce::String ArrangerHttpServer::chooseFolderDialog()
{
    juce::String result;

    juce::MessageManager::getInstance()->callFunctionOnMessageThread(
        [](void* param) -> void*
        {
            auto* pRes = static_cast<juce::String*>(param);

#if JUCE_WINDOWS
            IFileOpenDialog* pFileOpen = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                          IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(hr))
            {
                DWORD dwOptions;
                if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
                    pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

                pFileOpen->SetTitle(L"Select Yamaha Styles Directory");

                HWND activeHwnd = GetForegroundWindow();
                if (SUCCEEDED(pFileOpen->Show(activeHwnd)))
                {
                    IShellItem* pItem = nullptr;
                    if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
                    {
                        PWSTR pszFilePath = nullptr;
                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                        {
                            *pRes = juce::String(pszFilePath);
                            CoTaskMemFree(pszFilePath);
                        }
                        pItem->Release();
                    }
                }
                pFileOpen->Release();
            }
            else
            {
                BROWSEINFOW bi = { 0 };
                bi.lpszTitle = L"Select Yamaha Styles Directory";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl != 0)
                {
                    wchar_t path[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, path))
                        *pRes = juce::String(path);
                    CoTaskMemFree(pidl);
                }
            }
#endif
            return nullptr;
        },
        &result);

    return result;
}

juce::String ArrangerHttpServer::chooseFileDialog(const juce::String& title, const juce::String& filterSpec)
{
    struct DialogParams {
        juce::String title;
        juce::String filterSpec;
        juce::String result;
    } params { title, filterSpec, {} };

    juce::MessageManager::getInstance()->callFunctionOnMessageThread(
        [](void* p) -> void*
        {
            auto* dp = static_cast<DialogParams*>(p);
#if JUCE_WINDOWS
            IFileOpenDialog* pFileOpen = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                          IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(hr))
            {
                DWORD dwOptions;
                if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
                    pFileOpen->SetOptions(dwOptions | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);

                pFileOpen->SetTitle(dp->title.toWideCharPointer());

                COMDLG_FILTERSPEC filter[1];
                if (dp->filterSpec.containsIgnoreCase("sf2"))
                {
                    filter[0].pszName = L"SoundFont 2 Files (*.sf2)";
                    filter[0].pszSpec = L"*.sf2;*.SF2";
                    pFileOpen->SetFileTypes(1, filter);
                }
                else if (dp->filterSpec.containsIgnoreCase("sty"))
                {
                    filter[0].pszName = L"Yamaha Style Files (*.sty;*.prs;*.bcs;*.sst)";
                    filter[0].pszSpec = L"*.sty;*.prs;*.bcs;*.sst;*.STY;*.PRS;*.BCS;*.SST";
                    pFileOpen->SetFileTypes(1, filter);
                }

                HWND activeHwnd = GetForegroundWindow();
                if (SUCCEEDED(pFileOpen->Show(activeHwnd)))
                {
                    IShellItem* pItem = nullptr;
                    if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
                    {
                        PWSTR pszFilePath = nullptr;
                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                        {
                            dp->result = juce::String(pszFilePath);
                            CoTaskMemFree(pszFilePath);
                        }
                        pItem->Release();
                    }
                }
                pFileOpen->Release();
            }
#endif
            return nullptr;
        },
        &params);

    return params.result;
}

juce::String ArrangerHttpServer::handleLeadVoicePost(const juce::var& json)
{
    if (json.hasProperty("program"))
    {
        int prog = static_cast<int>(json["program"]);
        int bank = json.hasProperty("bank") ? static_cast<int>(json["bank"]) : 0;
        audioSource.getSynthManager().setLeadPresetWithBank(bank, prog);
        return "{\"success\":true}";
    }
    return "{\"success\":false,\"error\":\"No program specified\"}";
}

juce::String ArrangerHttpServer::handleBrowseSoundFontsGet()
{
    auto rootObj = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> sfList;

    juce::Array<juce::File> searchDirs;
    searchDirs.add(juce::File::getCurrentWorkingDirectory());
    searchDirs.add(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory());
    searchDirs.add(juce::File("C:/VirtualArranger"));
    searchDirs.add(juce::File("C:/fluidsynth"));
    searchDirs.add(juce::File("C:/Program Files/Image-Line/FL Studio 2024/Data/Patches/Soundfonts"));
    searchDirs.add(juce::File("C:/Program Files/Image-Line/FL Studio 2024/Plugins/Fruity/Generators/Soundfont Player/Data"));
    searchDirs.add(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    searchDirs.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads"));

    juce::StringArray seenPaths;

    for (const auto& dir : searchDirs)
    {
        if (!dir.isDirectory()) continue;
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, false, "*.sf2;*.SF2");
        for (const auto& f : files)
        {
            if (!seenPaths.contains(f.getFullPathName()))
            {
                seenPaths.add(f.getFullPathName());
                auto fObj = std::make_unique<juce::DynamicObject>();
                fObj->setProperty("name", f.getFileName());
                fObj->setProperty("path", f.getFullPathName().replace("\\", "/"));
                fObj->setProperty("size", static_cast<int>(f.getSize()));
                sfList.add(juce::var(fObj.release()));
            }
        }
    }

    rootObj->setProperty("soundfonts", sfList);
    return juce::JSON::toString(juce::var(rootObj.release()));
}
