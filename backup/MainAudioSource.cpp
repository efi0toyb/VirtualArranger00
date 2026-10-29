#include "MainAudioSource.h"

MainAudioSource::MainAudioSource()
{
}

void MainAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    synthManager.prepareToPlay(sampleRate, samplesPerBlockExpected);
    styleEngine.prepareToPlay(sampleRate);
}

void MainAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    // 1. Advance sequencer clock, apply chord transposition & trigger synth notes
    styleEngine.processBlock(bufferToFill.numSamples, synthManager, chordDetector);

    // 2. Synthesize all active voices into the audio buffer
    synthManager.renderAudio(*bufferToFill.buffer,
                             bufferToFill.startSample,
                             bufferToFill.numSamples);
}

void MainAudioSource::releaseResources()
{
    styleEngine.allNotesOff(synthManager);
    synthManager.releaseResources();
}

void MainAudioSource::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    chordDetector.processMidiMessage(message, synthManager);
}