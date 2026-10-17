#include "ProbeProcessor.h"

#include "ProbeEditor.h"
#include "TrackRegistry.h"

#include <algorithm>
#include <cmath>

namespace mixdebugger
{
ProbeProcessor::ProbeProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    setLatencySamples(0);
    probeId = TrackRegistry::get().registerProbe(this);
}

ProbeProcessor::~ProbeProcessor()
{
    TrackRegistry::get().unregisterProbe(probeId);
}

void ProbeProcessor::prepareToPlay(const double sampleRate, int)
{
    currentSampleRate.store(sampleRate, std::memory_order_release);
    const auto capacity = static_cast<std::size_t>(
        std::max(0.0, std::ceil(sampleRate * captureLengthSeconds)));
    captureBuffer.prepare(capacity);
    waveformHistory.prepare(sampleRate);
    pendingInputPeak.store(0.0f, std::memory_order_release);
}

void ProbeProcessor::releaseResources()
{
}

bool ProbeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const auto isMonoOrStereo = input == juce::AudioChannelSet::mono()
                             || input == juce::AudioChannelSet::stereo();
    return isMonoOrStereo && input == output;
}

void ProbeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto inputChannels = std::min(getTotalNumInputChannels(), buffer.getNumChannels());
    if (inputChannels == 0)
        return;

    const auto* left = buffer.getReadPointer(0);
    const auto* right = inputChannels > 1 ? buffer.getReadPointer(1) : left;
    auto blockPeak = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto monoSample = 0.5f * (left[sample] + right[sample]);
        captureBuffer.push(monoSample);
        waveformHistory.push(monoSample);
        blockPeak = std::max(blockPeak, std::abs(monoSample));
    }

    auto publishedPeak = pendingInputPeak.load(std::memory_order_relaxed);
    while (blockPeak > publishedPeak
           && !pendingInputPeak.compare_exchange_weak(publishedPeak,
                                                      blockPeak,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed))
    {
    }

    // Nie modyfikujemy bufora: JUCE przekazuje wejscie bezposrednio na wyjscie.
}

juce::AudioProcessorEditor* ProbeProcessor::createEditor()
{
    return new ProbeEditor(*this);
}

bool ProbeProcessor::hasEditor() const
{
    return true;
}

const juce::String ProbeProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ProbeProcessor::acceptsMidi() const
{
    return false;
}

bool ProbeProcessor::producesMidi() const
{
    return false;
}

bool ProbeProcessor::isMidiEffect() const
{
    return false;
}

double ProbeProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ProbeProcessor::getNumPrograms()
{
    return 1;
}

int ProbeProcessor::getCurrentProgram()
{
    return 0;
}

void ProbeProcessor::setCurrentProgram(int)
{
}

const juce::String ProbeProcessor::getProgramName(int)
{
    return {};
}

void ProbeProcessor::changeProgramName(int, const juce::String&)
{
}

void ProbeProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void ProbeProcessor::setStateInformation(const void*, int)
{
}

int ProbeProcessor::getProbeId() const noexcept
{
    return probeId;
}

std::size_t ProbeProcessor::getRegisteredProbeCount() const
{
    return TrackRegistry::get().probeCount();
}

double ProbeProcessor::getCapturedSeconds() const noexcept
{
    const auto sampleRate = currentSampleRate.load(std::memory_order_acquire);
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(captureBuffer.size()) / sampleRate;
}

float ProbeProcessor::consumeInputPeak() noexcept
{
    return pendingInputPeak.exchange(0.0f, std::memory_order_acq_rel);
}

std::vector<dsp::WaveformPoint> ProbeProcessor::getWaveformSnapshot() const
{
    return waveformHistory.snapshot();
}
} // namespace mixdebugger

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixdebugger::ProbeProcessor();
}
