#pragma once

#include "PairAnalysisView.h"

#include <mixdebugger/analysis/DynamicsAnalyzer.h>
#include <mixdebugger/analysis/BalanceAdvisor.h>
#include <mixdebugger/analysis/CompressionAdvisor.h>
#include <mixdebugger/analysis/GroupBlendAdvisor.h>
#include <mixdebugger/analysis/MasterBalanceAnalyzer.h>
#include <mixdebugger/analysis/MixTemplateAdvisor.h>
#include <mixdebugger/analysis/PairAnalyzer.h>
#include <mixdebugger/analysis/ResonanceAnalyzer.h>
#include <mixdebugger/analysis/PhaseAlignmentAnalyzer.h>
#include <mixdebugger/analysis/TrackFeatures.h>
#include <mixdebugger/analysis/TemplateProcessingAdvisor.h>
#include <mixdebugger/dsp/StftAnalyzer.h>

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace mixdebugger
{
class MainComponent final : public juce::Component,
                            private juce::TableListBoxModel,
                            private juce::Timer
{
public:
    explicit MainComponent(const juce::String& commandLine = {});
    ~MainComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void openFromCommandLine(const juce::String& commandLine);

private:
    struct TrackEntry
    {
        juce::File file;
        juce::String reaperGuid;
        juce::String name;
        double durationSeconds = 0.0;
        double sampleRate = 0.0;
        int channels = 0;
        juce::Colour colour;
        std::optional<float> rmsDb;
        std::optional<float> peakDb;
        std::optional<float> stereoCorrelation;
        std::optional<float> sideToMidPowerRatio;
        std::size_t stftFrames = 0;
        std::optional<float> dominantFrequencyHz;
        std::optional<std::array<float, 40>> erbAverageDb;
        std::shared_ptr<const analysis::TrackFeatures> features;
        analysis::DynamicsSummary dynamics;
        std::optional<float> measuredGroupActiveRmsDb;
        std::optional<float> measuredGroupPeakDb;
        std::size_t measuredGroupActiveWindowCount = 0;
        std::vector<analysis::ResonanceFinding> resonances;
        std::optional<analysis::TrackFamily> assignedFamily;
        analysis::MixRole mixRole = analysis::MixRole::unassigned;
        juce::String analysisGroup;
        juce::String sourceGroup;
        juce::String error;
        bool fromReaper = false;
        bool mutedInReaper = false;
        bool isReaperFolder = false;
        juce::String reaperFolderPath;
        // Skumulowany gain faderów folderów pomiędzy stemem a masterem.
        // Zwykły render selected-track stem nie zawiera tego odcinka toru.
        float ancestorFolderGainDb = 0.0f;
        float balanceInternalTrimDb = 0.0f;
    };

    struct ReaperBridgeSession
    {
        juce::File manifestFile;
        juce::File requestFile;
        juce::File responseFile;
        juce::File cacheDirectory;
        juce::String projectName;
        juce::String projectPath;
        double rangeStartSeconds = 0.0;
        double rangeEndSeconds = 0.0;
        bool usesTimeSelection = false;
        std::uint64_t reaperWindow = 0;
        int renderCommandId = 0;
        juce::File masterFile;
    };

    struct AnalysisResult
    {
        juce::String path;
        std::optional<float> rmsDb;
        std::optional<float> peakDb;
        std::optional<float> stereoCorrelation;
        std::optional<float> sideToMidPowerRatio;
        std::size_t stftFrames = 0;
        std::optional<float> dominantFrequencyHz;
        std::optional<std::array<float, 40>> erbAverageDb;
        std::shared_ptr<const analysis::TrackFeatures> features;
        analysis::DynamicsSummary dynamics;
        std::vector<analysis::ResonanceFinding> resonances;
        std::vector<float> phaseSamples;
        std::optional<float> measuredGroupActiveRmsDb;
        std::optional<float> measuredGroupPeakDb;
        std::size_t measuredGroupActiveWindowCount = 0;
        float balanceInternalTrimDb = 0.0f;
        juce::String error;
    };

    struct MaskingPlanAction
    {
        std::size_t maskerIndex = 0;
        analysis::MaskingRecommendation recommendation;
    };

    struct CompressionPlanAction
    {
        std::size_t trackIndex = 0;
        analysis::CompressionRecommendation recommendation;
    };

    struct StereoWidthPlanAction
    {
        std::size_t trackIndex = 0;
        analysis::StereoWidthRecommendation recommendation;
    };

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& graphics,
                            int rowNumber,
                            int width,
                            int height,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& graphics,
                   int rowNumber,
                   int columnId,
                   int width,
                   int height,
                   bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber,
                                             int columnId,
                                             bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void cellClicked(int rowNumber,
                     int columnId,
                     const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void chooseFiles();
    void addFiles(const juce::Array<juce::File>& files);
    void removeSelectedTracks();
    void selectAllTracks();
    void groupSelectedTracks();
    void joinSelectedAsOneSource();
    void clearSelectedGrouping();
    void confirmSuggestedFamilies();
    void applySelectedMixTemplate();
    void setTrackFamily(int rowNumber, std::optional<analysis::TrackFamily> family);
    void setTrackRole(int rowNumber, analysis::MixRole role);
    void renameMappingGroup(int rowNumber, bool sourceGroup, const juce::String& newName);
    void invalidateResultsAfterMappingChange();
    void saveTrackMapping() const;
    void loadTrackMapping();
    [[nodiscard]] juce::File mappingFile() const;
    void startAnalysis();
    void beginFileAnalysis(std::vector<juce::File> files,
                           bool validateRenderedAudio,
                           std::optional<juce::File> masterFile);
    bool requestReaperRender(const std::vector<juce::String>& guids);
    bool requestReaperApplyPlan();
    bool requestReaperUndoPlan();
    void importReaperManifest(const juce::File& manifestFile);
    void consumeReaperRenderResponse();
    void consumeReaperApplyResponse();
    void timerCallback() override;
    void finishAnalysis(std::vector<AnalysisResult> results,
                        std::vector<juce::String> matrixPaths,
                        std::vector<analysis::PairSummary> summaries,
                        std::vector<PairAnalysisView::PhasePairData> phasePairs,
                        std::vector<analysis::MixRole> matrixRoles,
                        std::optional<AnalysisResult> masterResult,
                        std::optional<float> predictedPreTrimMasterPeakDb,
                        bool rejectedSilentRender);
    void startPairDetail(std::size_t maskerIndex, std::size_t targetIndex);
    void setAnalysisRunning(bool shouldBeRunning);
    void updateStatus();
    void applySelectedPalette();
    void refreshMatrixColours();
    void showTracksView();
    void showMatrixView();

    static AnalysisResult analyseFile(const juce::File& file,
                                      std::stop_token stopToken,
                                      bool collectPhaseSamples,
                                      float inputGainDb = 0.0f);
    static analysis::DynamicsSummary analyseAlignedGroup(
        const std::vector<juce::File>& files,
        const std::vector<float>& inputGainsDb,
        std::stop_token stopToken,
        std::optional<float>& peakDb);
    static std::optional<float> predictPreTrimMasterPeak(
        const std::vector<juce::File>& files,
        const std::vector<float>& baseGainsDb,
        const std::vector<float>& relativeGainsDb,
        float currentRenderedMasterPeakDb,
        std::stop_token stopToken);
    static juce::String formatDuration(double seconds);

    juce::AudioFormatManager formatManager;
    std::vector<TrackEntry> tracks;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::jthread analysisThread;
    std::jthread pairDetailThread;
    bool analysisRunning = false;
    bool matrixAvailable = false;
    bool waitingForReaperRender = false;
    bool waitingForReaperApply = false;
    bool pendingUndoRequest = false;
    bool planAppliedInReaper = false;
    bool usePreparedReaperFilesOnce = false;
    juce::int64 renderRequestStartedMs = 0;
    juce::String bridgeMessage;
    std::optional<ReaperBridgeSession> reaperBridge;
    std::vector<juce::String> activeMatrixPaths;
    std::optional<analysis::MixBalanceSummary> currentMixBalance;
    std::vector<PairAnalysisView::PhasePairData> currentPhasePairs;
    std::vector<MaskingPlanAction> currentMaskingActions;
    bool currentHasMaskingIssues = false;
    std::vector<CompressionPlanAction> currentCompressionActions;
    std::optional<StereoWidthPlanAction> currentStereoWidthAction;
    std::optional<analysis::MixTemplateId> activeMixTemplate;

    juce::Label titleLabel;
    juce::Label descriptionLabel;
    juce::Label statusLabel;
    juce::TextButton addFilesButton;
    juce::TextButton removeButton;
    juce::TextButton selectAllButton{ "Zaznacz wszystkie" };
    juce::TextButton analyseButton{ "ANALIZUJ" };
    juce::TextButton groupButton;
    juce::TextButton sourceGroupButton;
    juce::TextButton clearGroupingButton;
    juce::TextButton confirmSuggestionsButton;
    juce::ComboBox bulkFamilySelector;
    juce::ComboBox bulkRoleSelector;
    juce::Label mappingHintLabel;
    juce::ComboBox mixTemplateSelector;
    juce::TextButton applyTemplateButton;
    juce::Label templateHintLabel;
    juce::TextButton tracksViewButton;
    juce::TextButton matrixViewButton;
    juce::ComboBox paletteSelector;
    juce::ComboBox workflowSelector;
    juce::TableListBox trackTable{ "Tracks", this };
    PairAnalysisView pairAnalysisView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
} // namespace mixdebugger
