#pragma once

#include "dsp/WaveformHistory.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace mixdebugger
{
class ProbeProcessor;

class LevelMeter final : public juce::Component
{
public:
    void setLevel(float levelInDecibels);
    void paint(juce::Graphics& graphics) override;

private:
    float levelDb = -100.0f;
};

class WaveformView final : public juce::Component
{
public:
    void setPoints(std::vector<dsp::WaveformPoint> newPoints);
    void paint(juce::Graphics& graphics) override;

private:
    std::vector<dsp::WaveformPoint> points;
};

class ProbeEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit ProbeEditor(ProbeProcessor& processor);
    ~ProbeEditor() override = default;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshStatus();

    ProbeProcessor& processor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label signalLabel;
    juce::Label waveformLabel;
    LevelMeter levelMeter;
    WaveformView waveformView;
    float displayedLevelDb = -100.0f;
    int signalHoldFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeEditor)
};
} // namespace mixdebugger
