#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <mixdebugger/analysis/ReferenceAnalysis.h>
#include <thread>

namespace mixdebugger {
class MainComponent final : public juce::AudioAppComponent, private juce::Timer {
public:
    explicit MainComponent(const juce::String& commandLine = {});
    ~MainComponent() override;
    void openFromCommandLine(const juce::String&);
    void prepareToPlay(int, double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Slot {
        juce::File file;
        juce::TextButton load;
        juce::Label name, rangeLabel;
        juce::TextEditor start, end;
        std::optional<analysis::ReferenceResult> result;
    };
    class Charts final : public juce::Component {
    public:
        std::array<std::optional<analysis::ReferenceResult>,2> data;
        void paint(juce::Graphics&) override;
    } charts;
    std::array<Slot,2> slots;
    juce::Label heading, subtitle, status, summary, position;
    juce::ComboBox material;
    juce::TextButton analyse {"Analyse / refresh"}, cancelButton {"Cancel"}, playA {"Play A: mix"}, playB {"Play B: reference"}, stop {"Stop"}, exportButton {"Export Markdown"};
    juce::ToggleButton match {"Match loudness"}, details {"Show measurements"};
    juce::TextEditor report;
    juce::String reportText;
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread worker;
    std::atomic<bool> cancelled {false};
    bool busy = false, deviceOpen = false;
    juce::AudioFormatManager formats;
    juce::AudioTransportSource transport;
    juce::TimeSliceThread readAhead {"Audio playback"};
    std::unique_ptr<juce::AudioFormatReaderSource> playbackSource;
    juce::CriticalSection playbackLock;
    int playingSlot = -1;
    double playbackStart = 0, playbackEnd = 0, deviceRate = 48000;
    void chooseFile(int);
    void setFile(int, const juce::File&);
    void invalidate();
    void runAnalysis();
    void updateReport();
    void play(int);
    void stopPlayback();
    void updateGain();
    void timerCallback() override;
};
}
