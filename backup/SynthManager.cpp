#include "SynthManager.h"
#include "StyleParser.h" // Needed for OneTouchSetting definition

SynthManager::SynthManager()
{
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
    if (synth != nullptr)
    {
        // Scale accompaniment channels 8 through 15
        for (int ch = 8; ch < 16; ++ch)
        {
            const int baseVol = channelVolumes[ch].load(std::memory_order_relaxed);
            const int scaled = juce::jlimit(0, 127, static_cast<int>(baseVol * volumeMultiplier));
            fluid_synth_cc(synth, ch, 7, scaled);
        }
    }
}

void SynthManager::setChannelVolume(int channel, int volume0To127) noexcept
{
    const int ch = juce::jlimit(0, 15, channel);
    const int val = juce::jlimit(0, 127, volume0To127);
    channelVolumes[ch].store(val, std::memory_order_relaxed);

    if (synth != nullptr)
    {
        if (ch >= 8 && ch < 16)
        {
            const float mult = styleVolume.load(std::memory_order_relaxed);
            fluid_synth_cc(synth, ch, 7, juce::jlimit(0, 127, static_cast<int>(val * mult)));
        }
        else
        {
            fluid_synth_cc(synth, ch, 7, val);
        }
    }
}

int SynthManager::getChannelVolume(int channel) const noexcept
{
    return channelVolumes[juce::jlimit(0, 15, channel)].load(std::memory_order_relaxed);
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
        destroySynth();
        initSynth(sampleRate);
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
    static const int defaultVols[16] = {
        127, 0, 0, 0,      // Ch 1-4: Lead Voices
        100, 100, 100, 100,// Ch 5-8: General
        85, 100,           // Ch 9 (Perc: 85), Ch 10 (Main Drums: 100)
        95,                // Ch 11 (Bass: 95)
        80, 75, 70,        // Ch 12 (Chd 1: 80), Ch 13 (Chd 2: 75), Ch 14 (Pad: 70)
        85, 85             // Ch 15 (Phr 1: 85), Ch 16 (Phr 2: 85)
    };

    for (int ch = 0; ch < 16; ++ch)
    {
        channelVolumes[ch].store(defaultVols[ch], std::memory_order_relaxed);
        fluid_synth_cc(synth, ch, 7, defaultVols[ch]);
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
        fluid_synth_noteon(synth, channel, noteNumber, velocity);
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