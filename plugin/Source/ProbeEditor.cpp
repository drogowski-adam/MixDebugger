#include "ProbeEditor.h"

#include "ProbeProcessor.h"

namespace mixdebugger
{
void LevelMeter::setLevel(const float levelInDecibels)
{
    levelDb = levelInDecibels;
    repaint();
}

void LevelMeter::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(0xff1f2937));
    graphics.fillRoundedRectangle(bounds, 5.0f);

    const auto normalised = juce::jmap(juce::jlimit(-60.0f, 0.0f, levelDb), -60.0f, 0.0f, 0.0f, 1.0f);
    auto levelBounds = bounds.reduced(3.0f);
    levelBounds.setWidth(levelBounds.getWidth() * normalised);

    graphics.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff22c55e), bounds.getX(), bounds.getY(),
        juce::Colour(0xfff59e0b), bounds.getRight(), bounds.getY(), false));
    graphics.fillRoundedRectangle(levelBounds, 3.0f);

    graphics.setColour(juce::Colours::white.withAlpha(0.8f));
    graphics.setFont(juce::FontOptions(13.0f));
    graphics.drawText(juce::String(levelDb, 1) + " dBFS", getLocalBounds(), juce::Justification::centred);
}

void WaveformView::setPoints(std::vector<dsp::WaveformPoint> newPoints)
{
    points = std::move(newPoints);
    repaint();
}

void WaveformView::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(0xff0b1220));
    graphics.fillRoundedRectangle(bounds, 6.0f);

    graphics.setColour(juce::Colour(0xff374151));
    graphics.drawHorizontalLine(juce::roundToInt(bounds.getCentreY()), bounds.getX(), bounds.getRight());

    if (points.empty())
        return;

    auto visiblePeak = 0.001f;
    for (const auto& point : points)
        visiblePeak = std::max({ visiblePeak, std::abs(point.minimum), std::abs(point.maximum) });

    graphics.setColour(juce::Colour(0xff60a5fa));
    const auto xStep = bounds.getWidth() / static_cast<float>(points.size());
    const auto centreY = bounds.getCentreY();
    const auto amplitude = bounds.getHeight() * 0.44f / visiblePeak;

    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const auto x = bounds.getX() + (static_cast<float>(index) + 0.5f) * xStep;
        const auto top = centreY - juce::jlimit(-1.0f, 1.0f, points[index].maximum) * amplitude;
        const auto bottom = centreY - juce::jlimit(-1.0f, 1.0f, points[index].minimum) * amplitude;
        graphics.drawVerticalLine(juce::roundToInt(x), top, bottom);
    }
}

ProbeEditor::ProbeEditor(ProbeProcessor& processorToUse)
    : AudioProcessorEditor(processorToUse), processor(processorToUse)
{
    titleLabel.setText("Mix Debugger Probe", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    statusLabel.setFont(juce::FontOptions(17.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6ee7b7));
    addAndMakeVisible(statusLabel);

    signalLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    signalLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(signalLabel);

    waveformLabel.setText("Przebieg — ostatnie 4 sekundy", juce::dontSendNotification);
    waveformLabel.setFont(juce::FontOptions(14.0f));
    waveformLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
    addAndMakeVisible(waveformLabel);

    addAndMakeVisible(levelMeter);
    addAndMakeVisible(waveformView);

    setSize(680, 430);
    refreshStatus();
    startTimerHz(30);
}

void ProbeEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff111827));
    graphics.setColour(juce::Colour(0xff374151));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f, 1.0f);
}

void ProbeEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(55));

    auto statusRow = area.removeFromTop(42);
    signalLabel.setBounds(statusRow.removeFromLeft(180));
    statusLabel.setBounds(statusRow);

    area.removeFromTop(8);
    levelMeter.setBounds(area.removeFromTop(30));
    area.removeFromTop(12);
    waveformLabel.setBounds(area.removeFromTop(28));
    waveformView.setBounds(area);
}

void ProbeEditor::timerCallback()
{
    refreshStatus();
}

void ProbeEditor::refreshStatus()
{
    constexpr auto silenceThresholdDb = -60.0f;
    constexpr auto meterFloorDb = -100.0f;

    const auto inputPeak = processor.consumeInputPeak();
    const auto inputPeakDb = juce::Decibels::gainToDecibels(inputPeak, meterFloorDb);
    displayedLevelDb = inputPeakDb >= displayedLevelDb
                     ? inputPeakDb
                     : std::max(meterFloorDb, displayedLevelDb - 0.8f);
    levelMeter.setLevel(displayedLevelDb);

    if (inputPeakDb > silenceThresholdDb)
        signalHoldFrames = 15;
    else if (signalHoldFrames > 0)
        --signalHoldFrames;

    const auto hasSignal = signalHoldFrames > 0;
    signalLabel.setText(hasSignal ? "●  SYGNAL" : "○  CISZA", juce::dontSendNotification);
    signalLabel.setColour(juce::Label::textColourId,
                          hasSignal ? juce::Colour(0xff22c55e) : juce::Colour(0xff9ca3af));

    const auto text = "ID sondy: " + juce::String(processor.getProbeId())
                    + "    |    Aktywne sondy: "
                    + juce::String(static_cast<int>(processor.getRegisteredProbeCount()))
                    + "    |    Bufor: "
                    + juce::String(processor.getCapturedSeconds(), 1)
                    + " / 10.0 s";
    statusLabel.setText(text, juce::dontSendNotification);
    waveformView.setPoints(processor.getWaveformSnapshot());
}
} // namespace mixdebugger
