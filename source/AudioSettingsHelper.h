#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <vector>

struct MidiDeviceState
{
    juce::String identifier;
    juce::String name;
    bool enabled { false };
};

struct DriverDisplayInfo
{
    juce::String id;          // e.g. "Windows Audio", "DirectSound", "ASIO: ASIO4ALL v2"
    juce::String displayName; // e.g. "Windows Audio (WASAPI)", "DirectSound", "ASIO - ASIO4ALL v2"
    juce::String juceType;    // e.g. "Windows Audio", "DirectSound", "ASIO"
    juce::String asioDevice;  // e.g. "ASIO4ALL v2" or empty if non-ASIO
};

class AudioSettingsHelper
{
public:
    static std::vector<DriverDisplayInfo> getAvailableDriverInfos(juce::AudioDeviceManager& dm)
    {
        std::vector<DriverDisplayInfo> list;

        for (auto* type : dm.getAvailableDeviceTypes())
        {
            juce::String typeName = type->getTypeName();
            if (typeName.equalsIgnoreCase("ASIO"))
            {
                type->scanForDevices();
                auto asioDevices = type->getDeviceNames(false);
                for (const auto& asioDev : asioDevices)
                {
                    DriverDisplayInfo info;
                    info.id = "ASIO: " + asioDev;
                    info.displayName = "ASIO: " + asioDev;
                    info.juceType = "ASIO";
                    info.asioDevice = asioDev;
                    list.push_back(info);
                }
            }
            else
            {
                DriverDisplayInfo info;
                info.id = typeName;
                if (typeName.equalsIgnoreCase("Windows Audio"))
                    info.displayName = "Windows Audio (WASAPI)";
                else if (typeName.equalsIgnoreCase("DirectSound"))
                    info.displayName = "DirectSound";
                else
                    info.displayName = typeName;

                info.juceType = typeName;
                info.asioDevice = {};
                list.push_back(info);
            }
        }

        return list;
    }

    static juce::String getCurrentDriverId(juce::AudioDeviceManager& dm)
    {
        juce::String currentType = dm.getCurrentAudioDeviceType();
        if (currentType.equalsIgnoreCase("ASIO"))
        {
            if (auto* dev = dm.getCurrentAudioDevice())
                return "ASIO: " + dev->getName();
        }
        return currentType;
    }

    static juce::StringArray getAvailableOutputDevices(juce::AudioDeviceManager& dm, const juce::String& driverId)
    {
        juce::StringArray devices;

        if (driverId.startsWithIgnoreCase("ASIO: "))
        {
            juce::String asioDev = driverId.substring(6).trim();
            devices.add(asioDev);
            return devices;
        }

        for (auto* type : dm.getAvailableDeviceTypes())
        {
            if (type->getTypeName().equalsIgnoreCase(driverId))
            {
                type->scanForDevices();
                devices = type->getDeviceNames(false); // false = output devices
                break;
            }
        }
        return devices;
    }

    static juce::String getCurrentOutputDevice(juce::AudioDeviceManager& dm)
    {
        if (auto* dev = dm.getCurrentAudioDevice())
            return dev->getName();
        return {};
    }

    static juce::Array<double> getAvailableSampleRates(juce::AudioDeviceManager& dm)
    {
        juce::Array<double> rates;
        if (auto* dev = dm.getCurrentAudioDevice())
            rates = dev->getAvailableSampleRates();

        if (rates.isEmpty())
            rates = { 44100.0, 48000.0, 96000.0 };

        // Ensure 44100 and 48000 are present
        if (!rates.contains(44100.0)) rates.insert(0, 44100.0);
        if (!rates.contains(48000.0)) rates.add(48000.0);

        return rates;
    }

    static double getCurrentSampleRate(juce::AudioDeviceManager& dm)
    {
        if (auto* dev = dm.getCurrentAudioDevice())
            return dev->getCurrentSampleRate();
        return 44100.0;
    }

    static juce::Array<int> getAvailableBufferSizes(juce::AudioDeviceManager& dm)
    {
        juce::Array<int> sizes;
        if (auto* dev = dm.getCurrentAudioDevice())
            sizes = dev->getAvailableBufferSizes();

        // Standard low-latency buffer sizes
        const int standardSizes[] = { 64, 96, 128, 192, 256, 384, 440, 512, 1024 };
        for (int s : standardSizes)
        {
            if (!sizes.contains(s))
                sizes.add(s);
        }

        // Sort ascending
        std::sort(sizes.begin(), sizes.end());
        return sizes;
    }

    static int getCurrentBufferSize(juce::AudioDeviceManager& dm)
    {
        if (auto* dev = dm.getCurrentAudioDevice())
            return dev->getCurrentBufferSizeSamples();
        return 512;
    }

    static bool showControlPanel(juce::AudioDeviceManager& dm)
    {
        if (auto* dev = dm.getCurrentAudioDevice())
        {
            if (dev->hasControlPanel())
                return dev->showControlPanel();
        }
        return false;
    }

    static bool applyDeviceSetup(juce::AudioDeviceManager& dm,
                                const juce::String& driverId,
                                const juce::String& deviceName,
                                double sampleRate,
                                int bufferSize)
    {
        juce::String targetType = driverId;
        juce::String targetDevice = deviceName;

        if (driverId.startsWithIgnoreCase("ASIO: "))
        {
            targetType = "ASIO";
            targetDevice = driverId.substring(6).trim();
        }

        if (targetType.isNotEmpty() && !targetType.equalsIgnoreCase(dm.getCurrentAudioDeviceType()))
        {
            dm.setCurrentAudioDeviceType(targetType, true);
        }

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        dm.getAudioDeviceSetup(setup);

        if (targetDevice.isNotEmpty())
            setup.outputDeviceName = targetDevice;
        if (sampleRate > 0)
            setup.sampleRate = sampleRate;
        if (bufferSize > 0)
            setup.bufferSize = bufferSize;

        setup.useDefaultOutputChannels = true;
        setup.outputChannels.setRange(0, 2, true);

        auto err = dm.setAudioDeviceSetup(setup, true);
        return err.isEmpty();
    }

    static std::vector<MidiDeviceState> getMidiInputs(juce::AudioDeviceManager& dm)
    {
        std::vector<MidiDeviceState> list;
        for (const auto& dev : juce::MidiInput::getAvailableDevices())
        {
            list.push_back({ dev.identifier, dev.name, dm.isMidiInputDeviceEnabled(dev.identifier) });
        }
        return list;
    }

    static void setMidiInputEnabled(juce::AudioDeviceManager& dm,
                                   const juce::String& identifier,
                                   bool enabled,
                                   juce::MidiInputCallback* callback)
    {
        dm.setMidiInputDeviceEnabled(identifier, enabled);
        if (callback != nullptr)
        {
            if (enabled)
                dm.addMidiInputDeviceCallback(identifier, callback);
            else
                dm.removeMidiInputDeviceCallback(identifier, callback);
        }
    }
};
