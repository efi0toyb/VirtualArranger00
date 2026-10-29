#include "SynthManager.h"
#include "StyleParser.h" // Needed for OneTouchSetting definition

SynthManager::SynthManager()
{
    // Initialize user mixer faders to default 100
    for (int i = 0; i < 16; ++i)
    {
        channelVolumes[i].store(100, std::memory_order_relaxed);
        styleSequenceVolumes[i].store(100, std::memory_order_relaxed);
        channelMuted[i].store(false, std::memory_order_relaxed);
        channelSoloed[i].store(false, std::memory_order_relaxed);
    }
    // Lead tracks Ch 0-3 default to 127
    channelVolumes[0].store(127, std::memory_order_relaxed);
    styleSequenceVolumes[0].store(127, std::memory_order_relaxed);

    initSynth(currentSampleRate);
}

SynthManager::~SynthManager()
{
    destroySynth();
}

void SynthManager::initSynth(double sampleRate)
{
    currentSampleRate = sampleRate;

    settings = new_fluid_settings();
    fluid_settings_setnum(settings, "synth.sample-rate", sampleRate);
    fluid_settings_setint(settings, "synth.polyphony", 256);
    fluid_settings_setint(settings, "synth.midi-channels", 16);
    fluid_settings_setnum(settings, "synth.gain", 0.7);
    fluid_settings_setstr(settings, "synth.midi-bank-select", "xg");
    fluid_settings_setstr(settings, "audio.driver", "");
    fluid_settings_setstr(settings, "midi.driver", "");
    fluid_settings_setnum(settings, "synth.gain", 0.45);

    synth = new_fluid_synth(settings);

    if (synth != nullptr)
    {
        fluid_synth_set_channel_type(synth, 8, CHANNEL_TYPE_DRUM);
        fluid_synth_set_channel_type(synth, 9, CHANNEL_TYPE_DRUM);
    }
}

void SynthManager::setStyleVolume(float volumeMultiplier) noexcept
{
    styleVolume.store(juce::jlimit(0.0f, 1.5f, volumeMultiplier), std::memory_order_relaxed);
    for (int ch = 8; ch < 16; ++ch)
    {
        updateEffectiveChannelVolume(ch);
    }
}

void SynthManager::updateEffectiveChannelVolume(int channel) noexcept
{
    if (synth == nullptr) return;
    const int ch = juce::jlimit(0, 15, channel);

    const int userVol = channelVolumes[ch].load(std::memory_order_relaxed);
    const int seqVol = styleSequenceVolumes[ch].load(std::memory_order_relaxed);

    int finalVol = userVol;
    if (ch >= 8 && ch < 16)
    {
        // Combined user fader trim (userVol / 100.0) with style CC7 (seqVol) and master style knob
        const float mult = styleVolume.load(std::memory_order_relaxed);
        const float trim = static_cast<float>(userVol) / 100.0f;
        finalVol = juce::jlimit(0, 127, static_cast<int>(seqVol * trim * mult));
    }

    fluid_synth_cc(synth, ch, 7, finalVol);
}

void SynthManager::setChannelVolume(int channel, int volume0To127) noexcept
{
    const int ch = juce::jlimit(0, 15, channel);
    const int val = juce::jlimit(0, 127, volume0To127);
    channelVolumes[ch].store(val, std::memory_order_relaxed);

    updateEffectiveChannelVolume(ch);
}

void SynthManager::setStyleSequenceVolume(int channel, int volume0To127) noexcept
{
    const int ch = juce::jlimit(0, 15, channel);
    const int val = juce::jlimit(0, 127, volume0To127);
    styleSequenceVolumes[ch].store(val, std::memory_order_relaxed);

    updateEffectiveChannelVolume(ch);
}

int SynthManager::getChannelVolume(int channel) const noexcept
{
    return channelVolumes[juce::jlimit(0, 15, channel)].load(std::memory_order_relaxed);
}

void SynthManager::setChannelMute(int channel, bool muted) noexcept
{
    const int ch = juce::jlimit(0, 15, channel);
    channelMuted[ch].store(muted, std::memory_order_relaxed);
    if (muted && synth != nullptr)
        fluid_synth_all_notes_off(synth, ch);
}

bool SynthManager::isChannelMuted(int channel) const noexcept
{
    return channelMuted[juce::jlimit(0, 15, channel)].load(std::memory_order_relaxed);
}

void SynthManager::setChannelSolo(int channel, bool soloed) noexcept
{
    const int ch = juce::jlimit(0, 15, channel);
    channelSoloed[ch].store(soloed, std::memory_order_relaxed);

    bool anySolo = false;
    for (int i = 0; i < 16; ++i)
    {
        if (channelSoloed[i].load(std::memory_order_relaxed))
        {
            anySolo = true;
            break;
        }
    }
    anySoloActive.store(anySolo, std::memory_order_relaxed);

    if (synth != nullptr && anySolo)
    {
        for (int i = 8; i < 16; ++i)
        {
            if (!channelSoloed[i].load(std::memory_order_relaxed))
                fluid_synth_all_notes_off(synth, i);
        }
    }
}

bool SynthManager::isChannelSoloed(int channel) const noexcept
{
    return channelSoloed[juce::jlimit(0, 15, channel)].load(std::memory_order_relaxed);
}

void SynthManager::destroySynth()
{
    soundFontLoaded.store(false, std::memory_order_release);

    if (synth != nullptr)
    {
        delete_fluid_synth(synth);
        synth = nullptr;
    }

    if (settings != nullptr)
    {
        delete_fluid_settings(settings);
        settings = nullptr;
    }

    currentSoundFontId = -1;
}

void SynthManager::prepareToPlay(double sampleRate, int maxSamplesPerBlock)
{
    maxBlockSize = maxSamplesPerBlock;
    monoScratchBuffer.allocate(static_cast<size_t>(maxBlockSize), true);

    if (std::abs(currentSampleRate - sampleRate) > 1.0)
    {
        isBusyLoading.store(true, std::memory_order_release);
        juce::File sfToReload = lastLoadedSoundFontFile;
        destroySynth();
        initSynth(sampleRate);
        if (sfToReload.existsAsFile())
        {
            loadSoundFont(sfToReload);
        }
        isBusyLoading.store(false, std::memory_order_release);
    }
}

void SynthManager::releaseResources()
{
    allNotesOff();
    monoScratchBuffer.free();
}

void SynthManager::setMasterVolume(float newVolume) noexcept
{
    masterVolume.store(juce::jlimit(0.0f, 2.0f, newVolume), std::memory_order_relaxed);
}

void SynthManager::setLeadPreset(int programNumber) noexcept
{
    currentLeadProgram.store(programNumber, std::memory_order_relaxed);
    if (synth != nullptr)
    {
        fluid_synth_bank_select(synth, 0, 0);
        fluid_synth_program_change(synth, 0, programNumber);
        fluid_synth_cc(synth, 0, 7, 127);
        fluid_synth_cc(synth, 0, 11, 127);
    }
}

void SynthManager::setLeadVolume(float volumeMultiplier) noexcept
{
    if (synth != nullptr)
    {
        const int ccVal = juce::jlimit(0, 127, static_cast<int>(127.0f * volumeMultiplier));
        fluid_synth_cc(synth, 0, 7, ccVal);
    }
}

void SynthManager::applyOTSSettings(const OneTouchSetting& ots) noexcept
{
    if (synth == nullptr) return;

    auto applyPartVoice = [this](int ch, int bankMsb, int program, int volume)
    {
        // 1. Try style's exact bank
        fluid_synth_bank_select(synth, ch, bankMsb);
        int res = fluid_synth_program_change(synth, ch, program);

        // 2. Fallback to GM Bank 0 only if SoundFont lacks that bank
        if (res != FLUID_OK)
        {
            fluid_synth_bank_select(synth, ch, 0);
            fluid_synth_program_change(synth, ch, program % 128);
        }

        fluid_synth_cc(synth, ch, 7, volume);
        fluid_synth_cc(synth, ch, 11, 127);
    };

    // Right 1 (Ch 0)
    currentLeadProgram.store(ots.right1.program, std::memory_order_relaxed);
    applyPartVoice(0, ots.right1.bankMsb, ots.right1.program, ots.right1.volume);

    // Right 2 (Ch 1)
    right2Active.store(ots.right2.isActive && ots.right2.volume > 0, std::memory_order_relaxed);
    if (ots.right2.isActive && ots.right2.volume > 0)
        applyPartVoice(1, ots.right2.bankMsb, ots.right2.program, ots.right2.volume);
    else
        fluid_synth_cc(synth, 1, 7, 0);

    // Right 3 (Ch 2)
    right3Active.store(ots.right3.isActive && ots.right3.volume > 0, std::memory_order_relaxed);
    if (ots.right3.isActive && ots.right3.volume > 0)
        applyPartVoice(2, ots.right3.bankMsb, ots.right3.program, ots.right3.volume);
    else
        fluid_synth_cc(synth, 2, 7, 0);

    // Left Voice (Ch 3)
    leftActive.store(ots.left.isActive && ots.left.volume > 0, std::memory_order_relaxed);
    if (ots.left.isActive && ots.left.volume > 0)
        applyPartVoice(3, ots.left.bankMsb, ots.left.program, ots.left.volume);
    else
        fluid_synth_cc(synth, 3, 7, 0);
}

bool SynthManager::loadSoundFont(const juce::File& sf2File)
{
    if (!sf2File.existsAsFile() || synth == nullptr)
        return false;

    lastLoadedSoundFontFile = sf2File;

    isBusyLoading.store(true, std::memory_order_release);

    if (currentSoundFontId >= 0)
    {
        fluid_synth_sfunload(synth, currentSoundFontId, 1);
        currentSoundFontId = -1;
        soundFontLoaded.store(false, std::memory_order_release);
    }

    int sfontId = fluid_synth_sfload(synth, sf2File.getFullPathName().toRawUTF8(), 1);

    if (sfontId == FLUID_FAILED)
    {
        isBusyLoading.store(false, std::memory_order_release);
        return false;
    }

    currentSoundFontId = sfontId;

    fluid_synth_set_channel_type(synth, 8, CHANNEL_TYPE_DRUM);
    fluid_synth_set_channel_type(synth, 9, CHANNEL_TYPE_DRUM);

    resetMixer();

    soundFontLoaded.store(true, std::memory_order_release);
    isBusyLoading.store(false, std::memory_order_release);

    return true;
}

void SynthManager::resetMixer() noexcept
{
    if (synth == nullptr) return;

    // Yamaha Arranger Default Acoustic Balance Table
    static const int defaultSeqVols[16] = {
        127, 0, 0, 0,       // Ch 1-4: Lead Voices
        100, 100, 100, 100, // Ch 5-8: General
        85, 100,            // Ch 9 (Perc: 85), Ch 10 (Main Drums: 100)
        95,                 // Ch 11 (Bass: 95)
        80, 75, 70,         // Ch 12 (Chd 1: 80), Ch 13 (Chd 2: 75), Ch 14 (Pad: 70)
        85, 85              // Ch 15 (Phr 1: 85), Ch 16 (Phr 2: 85)
    };

    for (int ch = 0; ch < 16; ++ch)
    {
        styleSequenceVolumes[ch].store(defaultSeqVols[ch], std::memory_order_relaxed);
        updateEffectiveChannelVolume(ch);

        fluid_synth_cc(synth, ch, 11, 127); // Full Expression
        fluid_synth_cc(synth, ch, 10, 64);  // Center Pan
        fluid_synth_cc(synth, ch, 64, 0);   // Sustain Off
        fluid_synth_pitch_bend(synth, ch, 8192);
    }

    // Right Hand Lead (Ch 0) at maximum
    const int leadProg = currentLeadProgram.load(std::memory_order_relaxed);
    fluid_synth_bank_select(synth, 0, 0);
    fluid_synth_program_change(synth, 0, leadProg);
    fluid_synth_cc(synth, 0, 7, 127);
}

void SynthManager::renderAudio(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) noexcept
{
    if (!soundFontLoaded.load(std::memory_order_relaxed) ||
        isBusyLoading.load(std::memory_order_relaxed) ||
        synth == nullptr)
    {
        return;
    }

    const int totalChannels = outputBuffer.getNumChannels();
    if (totalChannels == 0 || numSamples <= 0)
        return;

    float* leftChannel = outputBuffer.getWritePointer(0, startSample);

    if (totalChannels >= 2)
    {
        float* rightChannel = outputBuffer.getWritePointer(1, startSample);
        fluid_synth_write_float(synth, numSamples,
                                leftChannel, 0, 1,
                                rightChannel, 0, 1);
    }
    else
    {
        fluid_synth_write_float(synth, numSamples,
                                leftChannel, 0, 1,
                                monoScratchBuffer.get(), 0, 1);
    }

    const float vol = masterVolume.load(std::memory_order_relaxed);
    if (vol != 1.0f)
    {
        outputBuffer.applyGain(startSample, numSamples, vol);
    }
}

void SynthManager::noteOn(int channel, int noteNumber, int velocity) noexcept
{
    if (synth != nullptr && soundFontLoaded.load(std::memory_order_relaxed))
    {
        const int ch = juce::jlimit(0, 15, channel);
        if (channelMuted[ch].load(std::memory_order_relaxed))
            return;
        if (anySoloActive.load(std::memory_order_relaxed) && ch != 0 && !channelSoloed[ch].load(std::memory_order_relaxed))
            return;

        fluid_synth_noteon(synth, ch, noteNumber, velocity);
    }
}

void SynthManager::noteOff(int channel, int noteNumber) noexcept
{
    if (synth != nullptr && soundFontLoaded.load(std::memory_order_relaxed))
    {
        fluid_synth_noteoff(synth, channel, noteNumber);
    }
}

void SynthManager::controlChange(int channel, int controller, int value) noexcept
{
    if (synth != nullptr && soundFontLoaded.load(std::memory_order_relaxed))
    {
        fluid_synth_cc(synth, channel, controller, value);
    }
}

void SynthManager::programChange(int channel, int program) noexcept
{
    if (synth != nullptr && soundFontLoaded.load(std::memory_order_relaxed))
    {
        int res = fluid_synth_program_change(synth, channel, program);
        if (res != FLUID_OK)
        {
            int fallbackBank = (channel == 8 || channel == 9) ? 128 : 0;
            fluid_synth_bank_select(synth, channel, fallbackBank);
            fluid_synth_program_change(synth, channel, program);
        }
    }
}

void SynthManager::pitchBend(int channel, int value) noexcept
{
    if (synth != nullptr && soundFontLoaded.load(std::memory_order_relaxed))
    {
        fluid_synth_pitch_bend(synth, channel, value);
    }
}

void SynthManager::allNotesOff() noexcept
{
    if (synth != nullptr)
    {
        fluid_synth_all_notes_off(synth, -1);
    }
}

void SynthManager::setLeadPresetWithBank(int bank, int programNumber) noexcept
{
    currentLeadProgram.store(programNumber, std::memory_order_relaxed);
    if (synth != nullptr)
    {
        fluid_synth_bank_select(synth, 0, bank);
        fluid_synth_program_change(synth, 0, programNumber);
        fluid_synth_cc(synth, 0, 7, 127);
        fluid_synth_cc(synth, 0, 11, 127);
    }
}

std::vector<SoundFontPreset> SynthManager::getAvailablePresets() const
{
    std::vector<SoundFontPreset> list;
    if (synth == nullptr || currentSoundFontId < 0) return list;

    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(synth, currentSoundFontId);
    if (sfont == nullptr) return list;

    fluid_sfont_iteration_start(sfont);
    fluid_preset_t* preset = nullptr;

    while ((preset = fluid_sfont_iteration_next(sfont)) != nullptr)
    {
        SoundFontPreset sp;
        sp.name = juce::String::fromUTF8(fluid_preset_get_name(preset)).trim();
        sp.bank = fluid_preset_get_banknum(preset);
        sp.program = fluid_preset_get_num(preset);

        juce::String lower = sp.name.toLowerCase();
        if (sp.bank == 128) sp.category = "drums";
        else if (lower.contains("piano") || lower.contains("ep") || lower.contains("clav") || lower.contains("harpsi") || lower.contains("grand")) sp.category = "piano";
        else if (lower.contains("organ") || lower.contains("accord") || lower.contains("harmonica") || lower.contains("drawbar")) sp.category = "organ";
        else if (lower.contains("guitar") || lower.contains("gtr") || lower.contains("nylon") || lower.contains("steel") || lower.contains("strat") || lower.contains("overdrive") || lower.contains("dist")) sp.category = "guitar";
        else if (lower.contains("bass") || lower.contains("fretless") || lower.contains("slap")) sp.category = "bass";
        else if (lower.contains("string") || lower.contains("violin") || lower.contains("cello") || lower.contains("viola") || lower.contains("harp") || lower.contains("ensemble") || lower.contains("fidle")) sp.category = "strings";
        else if (lower.contains("brass") || lower.contains("trumpet") || lower.contains("trombone") || lower.contains("horn") || lower.contains("tuba")) sp.category = "brass";
        else if (lower.contains("sax") || lower.contains("flute") || lower.contains("clarinet") || lower.contains("oboe") || lower.contains("pipe") || lower.contains("recorder")) sp.category = "reed";
        else if (lower.contains("lead") || lower.contains("saw") || lower.contains("square") || lower.contains("syn")) sp.category = "lead";
        else if (lower.contains("pad") || lower.contains("warm") || lower.contains("choir") || lower.contains("voice") || lower.contains("sweep") || lower.contains("halo") || lower.contains("atmosphr")) sp.category = "pad";
        else sp.category = "other";

        list.push_back(sp);
    }

    return list;
}
