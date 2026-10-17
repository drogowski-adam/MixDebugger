#pragma once

#include "dsp/RingBuffer.h"
#include "dsp/WaveformHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace mixdebugger
{
class ProbeProcessor final : public juce::AudioProcessor
{
public:
    ProbeProcessor();
    ~ProbeProcessor() override;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] int getProbeId() const noexcept;
    [[nodiscard]] std::size_t getRegisteredProbeCount() const;
    [[nodiscard]] double getCapturedSeconds() const noexcept;
    [[nodiscard]] float consumeInputPeak() noexcept;
    [[nodiscard]] std::vector<dsp::WaveformPoint> getWaveformSnapshot() const;

private:
    static constexpr double captureLengthSeconds = 10.0;

    dsp::RingBuffer captureBuffer;
    dsp::WaveformHistory waveformHistory;
    std::atomic<float> pendingInputPeak{ 0.0f };
    std::atomic<double> currentSampleRate{ 0.0 };
    int probeId = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeProcessor)
};
} // namespace mixdebugger
