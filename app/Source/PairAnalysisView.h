#pragma once

#include <mixdebugger/analysis/CompressionAdvisor.h>
#include <mixdebugger/analysis/BalanceAdvisor.h>
#include <mixdebugger/analysis/MaskingAdvisor.h>
#include <mixdebugger/analysis/PairAnalyzer.h>
#include <mixdebugger/analysis/PairIssueRanking.h>
#include <mixdebugger/analysis/DynamicsAnalyzer.h>
#include <mixdebugger/analysis/MasterBalanceAnalyzer.h>
#include <mixdebugger/analysis/ResonanceAnalyzer.h>
#include <mixdebugger/analysis/PhaseAlignmentAnalyzer.h>
#include <mixdebugger/analysis/StereoWidthAdvisor.h>
#include <mixdebugger/analysis/TrackFeatures.h>

#include <juce_gui_extra/juce_gui_extra.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace mixdebugger
{
class PairAnalysisView final : public juce::Component,
                               private juce::ScrollBar::Listener
{
public:
    struct TrackData
    {
        juce::String name;
        juce::Colour colour;
        std::shared_ptr<const analysis::TrackFeatures> features;
        juce::String group;
        juce::String sourceGroup;
        analysis::DynamicsSummary dynamics;
        analysis::TrackFamily family = analysis::TrackFamily::unknown;
        analysis::MixRole role = analysis::MixRole::unassigned;
        int channelCount = 0;
        std::optional<float> stereoCorrelation;
        analysis::StereoWidthRecommendation stereoWidth;
        analysis::BalanceTrackRecommendation balance;
        analysis::CompressionRecommendation compression;
        std::vector<analysis::ResonanceFinding> resonances;
    };

    struct PhasePairData
    {
        std::size_t firstIndex = 0;
        std::size_t secondIndex = 0;
        analysis::TrackRelationship relationship = analysis::TrackRelationship::independent;
        analysis::PhaseAlignmentSummary summary;
    };

    PairAnalysisView();
    ~PairAnalysisView() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    void setMatrix(std::vector<TrackData> tracks,
                   std::vector<analysis::PairSummary> summaries,
                   std::vector<PhasePairData> phasePairs,
                   std::optional<analysis::MixBalanceSummary> mixBalance = std::nullopt,
                   std::optional<analysis::MasterBalanceSummary> masterBalance = std::nullopt);
    void updateTrackColours(const std::vector<juce::Colour>& colours);
    void showLoading(std::size_t maskerIndex, std::size_t targetIndex);
    void setTimeline(std::size_t maskerIndex,
                     std::size_t targetIndex,
                     analysis::PairTimeline timeline);
    void setApplyState(bool canApply, bool planIsApplied);
    [[nodiscard]] bool shouldPrepareChannelStrip() const noexcept;

    [[nodiscard]] const TrackData* trackAt(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t trackCount() const noexcept;

    std::function<void(std::size_t maskerIndex, std::size_t targetIndex)> onPairSelected;
    std::function<void()> onApplySafePlan;
    std::function<void()> onUndoSafePlan;

private:
    class ProblemsComponent;
    class MatrixComponent;
    class DetailComponent;

    void showProblems();
    void showFullMatrix();
    void showDetailFullscreen();
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    std::vector<TrackData> tracks;
    std::vector<analysis::PairSummary> summaries;
    std::vector<PhasePairData> phasePairs;
    std::optional<analysis::MixBalanceSummary> mixBalance;
    std::optional<analysis::MasterBalanceSummary> masterBalance;
    std::unique_ptr<ProblemsComponent> problemsComponent;
    std::unique_ptr<MatrixComponent> matrixComponent;
    std::unique_ptr<DetailComponent> detailComponent;
    juce::Viewport problemsViewport;
    juce::Viewport matrixViewport;
    juce::TextButton problemsButton;
    juce::TextButton fullMatrixButton;
    juce::TextButton detailButton;
    juce::ToggleButton channelStripToggle;
    juce::TextButton applyPlanButton;
    juce::TextButton undoPlanButton;
    bool fullMatrixVisible = false;
    bool detailFullscreenVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PairAnalysisView)
};
} // namespace mixdebugger
