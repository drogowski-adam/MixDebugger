#include "PairAnalysisView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>

namespace mixdebugger
{
namespace
{
juce::String utf8(const char* text)
{
    return juce::String::fromUTF8(text);
}

juce::Colour scoreColour(const float score, const float confidence)
{
    const auto amount = juce::jlimit(0.0f, 1.0f, score);
    const auto low = juce::Colour(0xff172033);
    const auto middle = juce::Colour(0xfff59e0b);
    const auto high = juce::Colour(0xffef4444);
    auto colour = amount < 0.5f
        ? low.interpolatedWith(middle, amount * 2.0f)
        : middle.interpolatedWith(high, (amount - 0.5f) * 2.0f);
    return colour.withMultipliedBrightness(0.55f + 0.45f * juce::jlimit(0.0f, 1.0f, confidence));
}

juce::Colour heatColour(const float intensity)
{
    const auto value = juce::jlimit(0.0f, 1.0f, intensity);
    if (value < 0.25f)
        return juce::Colour(0xff071426).interpolatedWith(juce::Colour(0xff0ea5e9),
                                                         value / 0.25f);
    if (value < 0.55f)
        return juce::Colour(0xff0ea5e9).interpolatedWith(juce::Colour(0xfffacc15),
                                                         (value - 0.25f) / 0.30f);
    if (value < 0.80f)
        return juce::Colour(0xfffacc15).interpolatedWith(juce::Colour(0xfff97316),
                                                         (value - 0.55f) / 0.25f);
    return juce::Colour(0xfff97316).interpolatedWith(juce::Colour(0xffef4444),
                                                     (value - 0.80f) / 0.20f);
}

juce::String formatTime(const double seconds)
{
    const auto rounded = std::max(0, static_cast<int>(std::round(seconds)));
    return juce::String::formatted("%d:%02d", rounded / 60, rounded % 60);
}

juce::String formatFrequency(const float frequencyHz)
{
    if (frequencyHz >= 1000.0f)
        return juce::String(frequencyHz / 1000.0f, frequencyHz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(static_cast<int>(std::round(frequencyHz))) + " Hz";
}

juce::String measurementAmountText(const float confidence)
{
    if (confidence >= 0.75f)
        return utf8("dużo danych");
    if (confidence >= 0.25f)
        return utf8("umiarkowana ilość danych");
    return utf8("mało danych");
}

juce::String characterText(const analysis::ConflictCharacter character)
{
    switch (character)
    {
        case analysis::ConflictCharacter::continuous: return utf8("CIĄGŁY");
        case analysis::ConflictCharacter::intermittent: return "PRZERYWANY";
        case analysis::ConflictCharacter::alternating: return "NAPRZEMIENNY";
    }
    return {};
}

juce::String dynamicsText(const analysis::DynamicsAssessment assessment)
{
    switch (assessment)
    {
        case analysis::DynamicsAssessment::clippingRisk:
            return utf8("możliwe przesterowanie");
        case analysis::DynamicsAssessment::veryUneven:
            return utf8("bardzo nierówny poziom");
        case analysis::DynamicsAssessment::veryFlat:
            return utf8("bardzo stały poziom — sprawdź kompresję");
        case analysis::DynamicsAssessment::unremarkable:
            return utf8("bez oczywistego problemu");
        case analysis::DynamicsAssessment::insufficientData:
            return utf8("za mało sygnału");
    }
    return {};
}

juce::String phaseText(const analysis::PhaseAlignmentAssessment assessment)
{
    switch (assessment)
    {
        case analysis::PhaseAlignmentAssessment::polaritySuspect:
            return utf8("sprawdź odwrócenie polaryzacji w mono");
        case analysis::PhaseAlignmentAssessment::timingSuspect:
            return utf8("sprawdź różnicę czasu między mikrofonami");
        case analysis::PhaseAlignmentAssessment::aligned:
            return utf8("bez wyraźnego alarmu");
        case analysis::PhaseAlignmentAssessment::lowCoherence:
            return utf8("za mało wspólnego sygnału do pewnego wniosku");
        case analysis::PhaseAlignmentAssessment::insufficientData:
            return utf8("za mało danych");
    }
    return {};
}

juce::String compressionText(const analysis::CompressionRecommendation& recommendation)
{
    switch (recommendation.advice)
    {
        case analysis::CompressionAdvice::reduceInputBeforeProcessing:
            return utf8("obniż poziom przed obróbką; kompresor nie naprawi clippingu");
        case analysis::CompressionAdvice::avoidMoreCompression:
            return utf8("nie dodawaj kolejnej kompresji bez wyraźnego celu");
        case analysis::CompressionAdvice::considerLeveling:
        case analysis::CompressionAdvice::considerPeakControl:
        {
            const auto purpose = recommendation.advice == analysis::CompressionAdvice::considerLeveling
                ? utf8("wyrównanie") : utf8("kontrola szczytów");
            return purpose + ": ratio "
                + juce::String(recommendation.ratio, 1) + utf8(":1, attack ")
                + juce::String(recommendation.attackMilliseconds, 0) + " ms, release "
                + juce::String(recommendation.releaseMilliseconds, 0) + " ms, threshold "
                + juce::String(recommendation.thresholdDb, 1) + " dBFS, cel "
                + juce::String(recommendation.targetGainReductionDb, 1) + " dB GR";
        }
        case analysis::CompressionAdvice::noAction:
            return utf8("brak oczywistej potrzeby kompresji");
        case analysis::CompressionAdvice::insufficientData:
            return utf8("za mało danych");
    }
    return {};
}
}

class PairAnalysisView::ProblemsComponent final : public juce::Component
{
public:
    void setData(const std::vector<TrackData>* newTracks,
                 const std::vector<analysis::PairSummary>* newSummaries,
                 const std::vector<PhasePairData>* newPhasePairs,
                 const std::optional<analysis::MixBalanceSummary>* newMixBalance,
                 const std::optional<analysis::MasterBalanceSummary>* newMasterBalance)
    {
        tracks = newTracks;
        summaries = newSummaries;
        phasePairs = newPhasePairs;
        mixBalance = newMixBalance;
        masterBalance = newMasterBalance;
        issues.clear();
        maskingRecommendations.clear();
        dynamicsIssues.clear();
        resonanceIssues.clear();
        phaseIssues.clear();
        stereoPhaseIssues.clear();
        compressionIssues.clear();
        planTasks.clear();
        relatedMicrophoneCount = 0;

        if (tracks != nullptr && summaries != nullptr)
        {
            const auto pairCount = tracks->size() * (tracks->size() - 1) / 2;
            std::vector<analysis::MixRole> roles;
            roles.reserve(tracks->size());
            for (const auto& track : *tracks)
                roles.push_back(track.role);
            const auto ranked = analysis::rankPairIssues(*summaries, roles, pairCount);
            std::vector<int> trackAppearances(tracks->size(), 0);
            for (const auto& issue : ranked)
            {
                const auto& summary = summaryFor(issue);
                if (analysis::pairIssueSeverity(summary) == analysis::PairIssueSeverity::low)
                    continue;

                const auto isRelatedCapture = phasePairs != nullptr
                    && std::ranges::any_of(*phasePairs, [&issue](const PhasePairData& pair)
                    {
                        return (pair.firstIndex == issue.maskerIndex
                                && pair.secondIndex == issue.targetIndex)
                            || (pair.firstIndex == issue.targetIndex
                                && pair.secondIndex == issue.maskerIndex);
                    });
                if (isRelatedCapture)
                    continue;

                const auto recommendation = analysis::MaskingAdvisor{}.recommend(
                    summary, *(*tracks)[issue.targetIndex].features,
                    (*tracks)[issue.maskerIndex].role,
                    (*tracks)[issue.targetIndex].role);
                if (recommendation.action == analysis::MaskingAction::none)
                    continue;

                if (trackAppearances[issue.maskerIndex] >= maximumTrackAppearances
                    || trackAppearances[issue.targetIndex] >= maximumTrackAppearances)
                    continue;

                issues.push_back(issue);
                maskingRecommendations.push_back(recommendation);
                ++trackAppearances[issue.maskerIndex];
                ++trackAppearances[issue.targetIndex];
                if (issues.size() == maximumDetailedIssues)
                    break;
            }

            for (std::size_t index = 0; index < tracks->size(); ++index)
            {
                const auto assessment = (*tracks)[index].dynamics.assessment;
                if (assessment != analysis::DynamicsAssessment::insufficientData
                    && assessment != analysis::DynamicsAssessment::unremarkable)
                    dynamicsIssues.push_back(index);

                if (!(*tracks)[index].resonances.empty()
                    && (*tracks)[index].resonances.front().score >= 5.0f
                    && (*tracks)[index].resonances.front().confidence >= 0.55f)
                    resonanceIssues.push_back(index);

                const auto compressionAdvice = (*tracks)[index].compression.advice;
                if (compressionAdvice != analysis::CompressionAdvice::noAction
                    && compressionAdvice != analysis::CompressionAdvice::insufficientData)
                    compressionIssues.push_back(index);

                if ((*tracks)[index].stereoWidth.available)
                    stereoPhaseIssues.push_back(index);
            }

            if (phasePairs != nullptr)
            {
                relatedMicrophoneCount = static_cast<int>(phasePairs->size());
                for (std::size_t index = 0; index < phasePairs->size(); ++index)
                {
                    const auto& pair = (*phasePairs)[index];
                    if (pair.relationship == analysis::TrackRelationship::sameSourceMicrophones
                        && (pair.summary.assessment
                                == analysis::PhaseAlignmentAssessment::polaritySuspect
                            || pair.summary.assessment
                                == analysis::PhaseAlignmentAssessment::timingSuspect))
                        phaseIssues.push_back(index);
                }
            }

            std::stable_sort(dynamicsIssues.begin(), dynamicsIssues.end(),
                [this](const std::size_t left, const std::size_t right)
                {
                    const auto rank = [this](const std::size_t index)
                    {
                        const auto assessment = (*tracks)[index].dynamics.assessment;
                        if (assessment == analysis::DynamicsAssessment::clippingRisk)
                            return 3;
                        if (assessment == analysis::DynamicsAssessment::veryUneven)
                            return 2;
                        return 1;
                    };
                    return rank(left) > rank(right);
                });
            std::stable_sort(resonanceIssues.begin(), resonanceIssues.end(),
                [this](const std::size_t left, const std::size_t right)
                {
                    return (*tracks)[left].resonances.front().score
                         > (*tracks)[right].resonances.front().score;
                });

            std::stable_sort(compressionIssues.begin(), compressionIssues.end(),
                [this](const std::size_t left, const std::size_t right)
                {
                    const auto rank = [this](const std::size_t index)
                    {
                        switch ((*tracks)[index].compression.advice)
                        {
                            case analysis::CompressionAdvice::reduceInputBeforeProcessing: return 4;
                            case analysis::CompressionAdvice::considerLeveling: return 3;
                            case analysis::CompressionAdvice::considerPeakControl: return 2;
                            case analysis::CompressionAdvice::avoidMoreCompression: return 1;
                            default: return 0;
                        }
                    };
                    return rank(left) > rank(right);
                });
            std::stable_sort(stereoPhaseIssues.begin(), stereoPhaseIssues.end(),
                [this](const std::size_t left, const std::size_t right)
                {
                    return (*tracks)[left].stereoWidth.confidence
                         > (*tracks)[right].stereoWidth.confidence;
                });
            buildPlanTasks();
        }

        hoveredRow = -1;
        updateSize(getWidth());
        repaint();
    }

    void updateSize(const int minimumWidth)
    {
        const auto contentHeight = issues.empty()
            ? contentStartY() + 180
            : contentStartY() + static_cast<int>(issues.size()) * cardHeight + 8;
        setSize(std::max(760, minimumWidth), contentHeight);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff111827));
        if (tracks == nullptr || summaries == nullptr)
            return;

        drawDashboard(graphics);
        drawPlanTasks(graphics);

        if (relatedMicrophoneCount > 0)
        {
            auto notice = juce::Rectangle<int>(8, phaseNoticeY() + 4,
                                               getWidth() - 16, noticeHeight - 8);
            graphics.setColour(juce::Colour(0xff0f2b3a));
            graphics.fillRoundedRectangle(notice.toFloat(), 7.0f);
            graphics.setColour(juce::Colour(0xff38bdf8));
            graphics.drawRoundedRectangle(notice.toFloat(), 7.0f, 1.0f);
            graphics.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            graphics.drawFittedText(
                utf8("Rozpoznano ") + juce::String(relatedMicrophoneCount)
                    + utf8(" relacji wielomikrofonowych. Nie traktujemy ich jako maskowania; konkretna korekta fazy pojawia się tylko przy stabilnym wyniku."),
                notice.reduced(14, 0), juce::Justification::centredLeft, 1, 0.85f);
        }

        if (issues.empty())
        {
            auto area = getLocalBounds().reduced(20);
            area.removeFromTop(contentStartY());
            graphics.setColour(juce::Colour(0xff34d399));
            graphics.setFont(juce::FontOptions(20.0f, juce::Font::bold));
            graphics.drawText(utf8("Brak wyraźnych konfliktów do zgłoszenia"),
                              area.removeFromTop(36), juce::Justification::centred);
            graphics.setColour(juce::Colour(0xff94a3b8));
            graphics.setFont(juce::FontOptions(14.0f));
            graphics.drawFittedText(
                utf8("To nie znaczy, że miks jest gotowy. Program nie znalazł par o wystarczająco silnej i dobrze udokumentowanej konkurencji widmowej."),
                area.removeFromTop(54), juce::Justification::centred, 2);
            return;
        }

        for (std::size_t index = 0; index < issues.size(); ++index)
            drawIssue(graphics, static_cast<int>(index), issues[index]);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        const auto nextRow = rowAt(event.y);
        const auto validRow = nextRow >= 0 && nextRow < static_cast<int>(issues.size())
            ? nextRow : -1;
        if (validRow != hoveredRow)
        {
            hoveredRow = validRow;
            setMouseCursor(hoveredRow >= 0 ? juce::MouseCursor::PointingHandCursor
                                           : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredRow = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto row = rowAt(event.y);
        if (row < 0 || row >= static_cast<int>(issues.size()))
            return;
        const auto& issue = issues[static_cast<std::size_t>(row)];
        if (onPairSelected)
            onPairSelected(issue.maskerIndex, issue.targetIndex);
    }

    std::function<void(std::size_t, std::size_t)> onPairSelected;

private:
    struct DashboardCard
    {
        juce::String title;
        juce::String headline;
        juce::String details;
        juce::Colour colour;
    };

    struct PlanTask
    {
        juce::String title;
        juce::String detail;
        juce::Colour colour;
    };

    juce::String balanceAdjustmentText() const
    {
        if (mixBalance == nullptr || !mixBalance->has_value() || !(**mixBalance).available)
            return utf8("Brak kompletnego modelu ról. Wróć do ŚCIEŻEK, ustaw FOCUS i role pozostałych grup.");

        const auto& balance = **mixBalance;
        std::vector<const analysis::BalanceGroupRecommendation*> ordered;
        for (const auto& group : balance.groups)
        {
            if (std::abs(group.gainDb) >= 0.25f)
                ordered.push_back(&group);
        }
        std::ranges::sort(ordered, [](const auto* left, const auto* right)
        {
            return std::abs(left->gainDb) > std::abs(right->gainDb);
        });

        juce::String result;
        if (ordered.empty())
        {
            result = utf8("Poziomy grup są już blisko neutralnego technicznego punktu startowego.");
        }
        else
        {
            for (std::size_t index = 0; index < std::min<std::size_t>(4, ordered.size()); ++index)
            {
                if (result.isNotEmpty())
                    result += utf8("  •  ");
                const auto& group = *ordered[index];
                result += juce::String(group.group)
                       + ": " + (group.gainDb > 0.0f ? "+" : "")
                       + juce::String(group.gainDb, 1) + " dB";
            }
            if (ordered.size() > 4)
                result += utf8("  •  oraz ") + juce::String(static_cast<int>(ordered.size() - 4))
                       + utf8(" dalszych ruchów");
        }

        if (tracks != nullptr)
        {
            for (const auto& track : *tracks)
            {
                if (std::abs(track.balance.internalTrimDb) < 0.25f)
                    continue;
                result += utf8("  •  Blend warstwy ") + track.name + ": "
                       + juce::String(track.balance.internalTrimDb, 1) + " dB";
            }
        }

        if (balance.headroomAvailable)
        {
            result += utf8("  •  Przewidywany headroom po balansie: co najmniej ")
                   + juce::String(std::abs(balance.predictedMasterPeakDb), 1) + " dB";
        }
        else
        {
            result += utf8("  •  Headroom niepotwierdzony — automatyczne zastosowanie jest zablokowane");
        }
        if (balance.hierarchyLimited)
        {
            result += utf8("  •  Uwaga: pełnej hierarchii ról nie da się osiągnąć bez przekroczenia bezpiecznych limitów; sprawdź źródło lub ustawienia grupy");
        }
        return result;
    }

    juce::String masterTonalAdjustmentText() const
    {
        if (masterBalance == nullptr || !masterBalance->has_value()
            || (**masterBalance).assessment == analysis::MasterBalanceAssessment::unavailable)
            return utf8("Brak renderu sumy do porównania z pink noise.");

        static constexpr std::array<const char*, 5> regionNames {
            "50–160 Hz", "160–500 Hz", "500 Hz–2 kHz", "2–6 kHz", "6–16 kHz"
        };
        juce::String result = utf8("Suma względem pink noise: ");
        auto count = 0;
        for (std::size_t region = 0; region < regionNames.size(); ++region)
        {
            const auto adjustment = (**masterBalance).suggestedAdjustmentDb[region];
            if (std::abs(adjustment) < 0.25f)
                continue;
            if (count > 0)
                result += utf8("  •  ");
            result += utf8(regionNames[region]);
            result += adjustment > 0.0f ? ": +" : ": ";
            result += juce::String(adjustment, 1) + " dB";
            if (++count == 3)
                break;
        }
        if (count == 0)
            result += utf8("bez szerokiego odchylenia przekraczającego próg.");
        return result;
    }

    void buildPlanTasks()
    {
        if (!phaseIssues.empty() && phasePairs != nullptr)
        {
            const auto& pair = (*phasePairs)[phaseIssues.front()];
            const auto& second = (*tracks)[pair.secondIndex];
            juce::String title;
            juce::String detail;
            if (pair.summary.assessment == analysis::PhaseAlignmentAssessment::polaritySuspect)
            {
                title = utf8("Odwróć polaryzację: ") + second.name;
                detail = utf8("Korelacja przy wyrównaniu zmieni się z ")
                       + juce::String(pair.summary.zeroLagCorrelation, 2) + utf8(" na około +")
                       + juce::String(std::abs(pair.summary.bestCorrelation), 2)
                       + utf8(". Zmiana dotyczy pary ")
                       + (*tracks)[pair.firstIndex].name + " + " + second.name + ".";
            }
            else
            {
                const auto moveEarlier = pair.summary.bestLagMilliseconds > 0.0;
                title = utf8("Przesuń ") + second.name
                      + (moveEarlier ? utf8(" wcześniej o ") : utf8(" później o "))
                      + juce::String(std::abs(pair.summary.bestLagMilliseconds), 1) + " ms";
                detail = utf8("To ustawienie maksymalizuje zmierzoną zgodność czasową z ")
                       + (*tracks)[pair.firstIndex].name + ".";
            }
            planTasks.push_back({ title, detail, juce::Colour(0xfff87171) });
        }
        else if (!stereoPhaseIssues.empty())
        {
            const auto index = stereoPhaseIssues.front();
            const auto& recommendation = (*tracks)[index].stereoWidth;
            if (recommendation.severeMidCancellation)
            {
                const auto sideAdvantageDb = 10.0f * std::log10(
                    std::max(1.0f, recommendation.sideToMidPowerRatio));
                planTasks.push_back({ utf8("Najpierw napraw stereo: ") + (*tracks)[index].name,
                    utf8("Mid jest słabszy od Side o około ")
                        + juce::String(sideAdvantageDb, 1)
                        + utf8(" dB, więc suma mono prawie znika. Program nie zmieni Width. Wyłącz na tej ścieżce ostatni widener/chorus i przeanalizuj ponownie; jeśli alarm zostanie, odwróć polaryzację tylko prawego kanału i ponów analizę."),
                    juce::Colour(0xfff87171) });
            }
            else
            {
                planTasks.push_back({ utf8("Zabezpiecz mono: zwęź ") + (*tracks)[index].name,
                    utf8("Ustaw Width na ")
                        + juce::String(static_cast<int>(std::round(
                            recommendation.widthMultiplier * 100.0f)))
                        + utf8("% obecnej wartości. Korelacja L/R: ")
                        + juce::String(recommendation.currentCorrelation, 2) + utf8(" → około ")
                        + juce::String(recommendation.predictedCorrelation, 2) + ".",
                    juce::Colour(0xfff87171) });
            }
        }

        planTasks.push_back({ utf8("Ustaw techniczny balans grup"),
            balanceAdjustmentText() + "\n" + masterTonalAdjustmentText(),
            juce::Colour(0xff38bdf8) });

        if (!issues.empty())
        {
            constexpr auto maximumPreviewActions = std::size_t { 3 };
            std::set<std::size_t> automaticMaskers;
            juce::String automaticActions;
            auto automaticCount = std::size_t {};
            auto manualDynamicCount = 0;
            for (std::size_t index = 0; index < issues.size(); ++index)
            {
                const auto& issue = issues[index];
                const auto& recommendation = maskingRecommendations[index];
                const auto safeAutomatic = recommendation.confidence >= 0.55f
                    && (recommendation.action == analysis::MaskingAction::lowerMasker
                        || recommendation.action == analysis::MaskingAction::staticEqCut)
                    && !automaticMaskers.contains(issue.maskerIndex);
                if (!safeAutomatic)
                {
                    if (recommendation.action == analysis::MaskingAction::dynamicEqCut)
                        ++manualDynamicCount;
                    continue;
                }

                automaticMaskers.insert(issue.maskerIndex);
                if (automaticCount < maximumPreviewActions)
                {
                    if (automaticActions.isNotEmpty())
                        automaticActions += utf8("  •  ");
                    if (recommendation.action == analysis::MaskingAction::lowerMasker)
                        automaticActions += utf8("Obniż ") + (*tracks)[issue.maskerIndex].name
                            + " o " + juce::String(std::abs(recommendation.gainDb), 1) + " dB";
                    else
                        automaticActions += (*tracks)[issue.maskerIndex].name + utf8(": EQ ")
                            + formatFrequency(recommendation.frequencyHz) + ", "
                            + juce::String(recommendation.gainDb, 1) + " dB, Q "
                            + juce::String(recommendation.q, 1);
                }
                ++automaticCount;
            }

            if (automaticActions.isEmpty())
                automaticActions = utf8("Brak korekty możliwej do bezpiecznego zastosowania bez sidechainu.");
            if (manualDynamicCount > 0)
                automaticActions += utf8("  •  Dynamiczne EQ ręcznie: ")
                    + juce::String(manualDynamicCount);
            if (automaticCount > maximumPreviewActions)
                automaticActions += utf8("  •  oraz ")
                    + juce::String(static_cast<int>(automaticCount - maximumPreviewActions))
                    + utf8(" dalszych pewnych korekt");

            planTasks.push_back({ automaticCount > 0
                                      ? utf8("Bezpieczne korekty maskowania: ")
                                            + juce::String(static_cast<int>(automaticCount))
                                      : utf8("Maskowanie wymaga dynamicznego EQ"),
                                  automaticActions,
                                  juce::Colour(0xfff59e0b) });
        }
        else if (!resonanceIssues.empty())
        {
            const auto trackIndex = resonanceIssues.front();
            const auto& finding = (*tracks)[trackIndex].resonances.front();
            planTasks.push_back({ utf8("Ogranicz stały wąski pik: ")
                                      + (*tracks)[trackIndex].name,
                utf8("Ustaw ręcznie EQ: ") + formatFrequency(finding.frequencyHz)
                    + ", " + juce::String(finding.recommendedGainDb, 1) + " dB, Q "
                    + juce::String(finding.q, 1) + utf8(". Pewność pomiaru: ")
                    + juce::String(static_cast<int>(std::round(finding.confidence * 100.0f)))
                    + utf8("%. Ta korekta jest częścią przycisku ZASTOSUJ SUGESTIE."),
                juce::Colour(0xffa78bfa) });
        }

        // Plan pokazuje trzy najważniejsze kroki, a dashboard podaje pełne liczby.
        // Przycisk stosuje wszystkie pewne korekty, nie tylko trzy karty podglądu.
        if (planTasks.size() < maximumPlanTasks && issues.empty()
            && resonanceIssues.empty() && !compressionIssues.empty())
        {
            const auto trackIndex = compressionIssues.front();
            planTasks.push_back({ utf8("Na końcu dynamika: ") + (*tracks)[trackIndex].name,
                                  compressionText((*tracks)[trackIndex].compression),
                                  juce::Colour(0xff34d399) });
        }

        if (planTasks.size() > maximumPlanTasks)
            planTasks.resize(maximumPlanTasks);
    }

    void drawDashboard(juce::Graphics& graphics)
    {
        const auto phaseCorrectionCount = phaseIssues.size() + stereoPhaseIssues.size();
        juce::String phaseDetails;
        if (!phaseIssues.empty())
            phaseDetails = utf8("Wyliczono polaryzację albo przesunięcie czasu jawnej pary mikrofonów.");
        else if (!stereoPhaseIssues.empty())
            phaseDetails = (*tracks)[stereoPhaseIssues.front()].stereoWidth.severeMidCancellation
                ? utf8("Suma mono prawie zanika; automatyczna zmiana Width została zablokowana.")
                : utf8("Silnie ujemna korelacja L/R; program wyliczył bezpieczniejsze Width.");
        else
            phaseDetails = utf8("Sprawdzono jawne pary mikrofonów i mono-kompatybilność stereo.");

        juce::String dynamicsDetails = dynamicsIssues.empty()
            ? utf8("Nie znaleziono oczywistego spłaszczenia, nierówności ani clippingu.")
            : juce::String();
        for (std::size_t item = 0; item < std::min<std::size_t>(2, dynamicsIssues.size()); ++item)
        {
            const auto index = dynamicsIssues[item];
            if (dynamicsDetails.isNotEmpty())
                dynamicsDetails += "\n";
            dynamicsDetails += utf8("• ") + (*tracks)[index].name + ": "
                            + dynamicsText((*tracks)[index].dynamics.assessment);
        }

        juce::String resonanceDetails = resonanceIssues.empty()
            ? utf8("Brak mocnych, długotrwałych wąskich pików.") : juce::String();
        for (std::size_t item = 0; item < std::min<std::size_t>(2, resonanceIssues.size()); ++item)
        {
            const auto index = resonanceIssues[item];
            if (resonanceDetails.isNotEmpty())
                resonanceDetails += "\n";
            const auto& finding = (*tracks)[index].resonances.front();
            resonanceDetails += utf8("• ") + (*tracks)[index].name + ": "
                              + formatFrequency(finding.frequencyHz) + ", "
                              + juce::String(finding.recommendedGainDb, 1) + " dB, Q "
                              + juce::String(finding.q, 1);
        }

        juce::String overlapDetails = issues.empty()
            ? utf8("Brak konfliktu spełniającego próg automatycznej korekty.")
            : utf8("Najważniej: zrób miejsce dla ")
                + (*tracks)[issues.front().targetIndex].name;

        auto balanceMoveCount = 0;
        if (mixBalance != nullptr && mixBalance->has_value() && (**mixBalance).available)
        {
            for (const auto& group : (**mixBalance).groups)
                balanceMoveCount += std::abs(group.gainDb) >= 0.25f ? 1 : 0;
            for (const auto& track : (**mixBalance).tracks)
                balanceMoveCount += std::abs(track.internalTrimDb) >= 0.25f ? 1 : 0;
        }

        const std::array<DashboardCard, 4> cards {
            DashboardCard { utf8("1. FAZA"),
                phaseCorrectionCount == 0 ? utf8("Brak wyraźnego alarmu")
                                    : utf8("Korekty: ")
                                        + juce::String(static_cast<int>(phaseCorrectionCount)),
                phaseDetails,
                juce::Colour(0xfff87171) },
            DashboardCard { utf8("2. BALANS"),
                balanceMoveCount > 0
                    ? utf8("Ruchy faderów: ") + juce::String(balanceMoveCount)
                    : utf8("Poziomy blisko celu"),
                balanceAdjustmentText() + "\n" + masterTonalAdjustmentText(),
                juce::Colour(0xff38bdf8) },
            DashboardCard { utf8("3. CZYTELNOŚĆ"),
                issues.empty() && resonanceIssues.empty() ? utf8("Bez mocnych alarmów")
                    : utf8("Korekty maskowania: ")
                        + juce::String(static_cast<int>(issues.size())),
                overlapDetails + (resonanceDetails.isNotEmpty()
                    ? "\n" + resonanceDetails : juce::String()), juce::Colour(0xfff59e0b) },
            DashboardCard { utf8("4. KOMPRESJA"),
                compressionIssues.empty() ? utf8("Brak oczywistej potrzeby")
                    : utf8("Ścieżki do sprawdzenia: ")
                        + juce::String(static_cast<int>(compressionIssues.size())),
                compressionIssues.empty() ? dynamicsDetails
                    : (*tracks)[compressionIssues.front()].name + ": "
                        + compressionText((*tracks)[compressionIssues.front()].compression),
                juce::Colour(0xff34d399) }
        };

        const auto gap = 10;
        const auto cardWidth = std::max(150, (getWidth() - gap * 5) / 4);
        for (std::size_t index = 0; index < cards.size(); ++index)
        {
            auto card = juce::Rectangle<int>(gap + static_cast<int>(index) * (cardWidth + gap),
                                             5, cardWidth, dashboardHeight - 14);
            graphics.setColour(juce::Colour(0xff0b1220));
            graphics.fillRoundedRectangle(card.toFloat(), 8.0f);
            graphics.setColour(cards[index].colour.withAlpha(0.7f));
            graphics.drawRoundedRectangle(card.toFloat(), 8.0f, 1.2f);
            auto content = card.reduced(14, 10);
            graphics.setColour(cards[index].colour);
            graphics.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            graphics.drawText(cards[index].title, content.removeFromTop(20),
                              juce::Justification::centredLeft);
            graphics.setColour(juce::Colours::white);
            graphics.setFont(juce::FontOptions(15.0f, juce::Font::bold));
            graphics.drawFittedText(cards[index].headline, content.removeFromTop(30),
                                    juce::Justification::centredLeft, 1, 0.8f);
            graphics.setColour(juce::Colour(0xffaeb9ca));
            graphics.setFont(juce::FontOptions(11.5f));
            graphics.drawFittedText(cards[index].details, content,
                                    juce::Justification::topLeft, 3, 0.78f);
        }

    }

    void drawPlanTasks(juce::Graphics& graphics)
    {
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        graphics.drawText(utf8("PLAN NA TERAZ — maksymalnie trzy rzeczy"),
                          8, dashboardHeight, getWidth() - 16, planHeadingHeight,
                          juce::Justification::centredLeft);

        for (std::size_t index = 0; index < planTasks.size(); ++index)
        {
            auto card = juce::Rectangle<int>(8,
                dashboardHeight + planHeadingHeight
                    + static_cast<int>(index) * planCardHeight + 4,
                getWidth() - 16, planCardHeight - 8);
            graphics.setColour(juce::Colour(0xff0b1220));
            graphics.fillRoundedRectangle(card.toFloat(), 7.0f);
            graphics.setColour(planTasks[index].colour.withAlpha(0.75f));
            graphics.drawRoundedRectangle(card.toFloat(), 7.0f, 1.1f);
            graphics.fillRoundedRectangle(card.removeFromLeft(5).toFloat(), 3.0f);
            auto content = card.reduced(14, 7);
            auto number = content.removeFromLeft(34);
            graphics.setColour(planTasks[index].colour.withAlpha(0.20f));
            graphics.fillEllipse(static_cast<float>(number.getX()),
                                 static_cast<float>(number.getCentreY() - 13), 26.0f, 26.0f);
            graphics.setColour(juce::Colours::white);
            graphics.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            graphics.drawText(juce::String(static_cast<int>(index + 1)), number.withWidth(26),
                              juce::Justification::centred);
            graphics.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            graphics.drawFittedText(planTasks[index].title, content.removeFromTop(24),
                                    juce::Justification::centredLeft, 1, 0.85f);
            graphics.setColour(juce::Colour(0xffaeb9ca));
            graphics.setFont(juce::FontOptions(12.0f));
            graphics.drawFittedText(planTasks[index].detail, content,
                                    juce::Justification::centredLeft, 2, 0.82f);
        }

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        graphics.drawText(utf8("CZYTELNOŚĆ — konkretne korekty o najwyższej pewności"),
                          8, pairHeadingY(), getWidth() - 16, pairHeadingHeight,
                          juce::Justification::centredLeft);
    }

    const analysis::PairSummary& summaryFor(const analysis::PairIssue& issue) const
    {
        return (*summaries)[issue.maskerIndex * tracks->size() + issue.targetIndex];
    }

    [[nodiscard]] int contentStartY() const noexcept
    {
        return pairHeadingY() + pairHeadingHeight;
    }

    [[nodiscard]] int phaseNoticeY() const noexcept
    {
        return dashboardHeight + planHeadingHeight
             + static_cast<int>(planTasks.size()) * planCardHeight;
    }

    [[nodiscard]] int pairHeadingY() const noexcept
    {
        return phaseNoticeY() + (relatedMicrophoneCount > 0 ? noticeHeight : 0);
    }

    [[nodiscard]] int rowAt(const int y) const noexcept
    {
        const auto relativeY = y - contentStartY();
        return relativeY >= 0 ? relativeY / cardHeight : -1;
    }

    void drawIssue(juce::Graphics& graphics,
                   const int row,
                   const analysis::PairIssue& issue)
    {
        auto card = juce::Rectangle<int>(8, contentStartY() + row * cardHeight + 4,
                                         getWidth() - 16, cardHeight - 8);
        const auto& masker = (*tracks)[issue.maskerIndex];
        const auto& target = (*tracks)[issue.targetIndex];
        const auto& summary = summaryFor(issue);
        const auto& recommendation = maskingRecommendations[static_cast<std::size_t>(row)];
        const auto severity = analysis::pairIssueSeverity(summary);
        const auto severityColour = severity == analysis::PairIssueSeverity::high
            ? juce::Colour(0xffef4444) : juce::Colour(0xfff59e0b);

        graphics.setColour(row == hoveredRow ? juce::Colour(0xff1e293b)
                                              : juce::Colour(0xff0b1220));
        graphics.fillRoundedRectangle(card.toFloat(), 8.0f);
        graphics.setColour(severityColour.withAlpha(0.75f));
        graphics.drawRoundedRectangle(card.toFloat(), 8.0f, 1.2f);
        graphics.fillRoundedRectangle(card.removeFromLeft(5).toFloat(), 3.0f);

        auto content = card.reduced(16, 8);
        auto rankArea = content.removeFromLeft(36);
        graphics.setColour(severityColour.withAlpha(0.18f));
        graphics.fillEllipse(static_cast<float>(rankArea.getX()),
                             static_cast<float>(rankArea.getCentreY() - 15),
                             30.0f, 30.0f);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        graphics.drawText(juce::String(row + 1), rankArea.withWidth(30),
                          juce::Justification::centred);

        auto badge = content.removeFromRight(150).reduced(4, 15);
        graphics.setColour(severityColour.withAlpha(0.18f));
        graphics.fillRoundedRectangle(badge.toFloat(), 5.0f);
        graphics.setColour(severityColour);
        graphics.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        graphics.drawText(severity == analysis::PairIssueSeverity::high
                              ? "WYSOKI PRIORYTET" : utf8("WARTO SPRAWDZIĆ"),
                          badge, juce::Justification::centred);

        auto title = content.removeFromTop(28);
        graphics.setColour(masker.colour);
        graphics.fillEllipse(static_cast<float>(title.getX()),
                             static_cast<float>(title.getCentreY() - 5), 10.0f, 10.0f);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(15.5f, juce::Font::bold));
        graphics.drawFittedText(utf8("Zrób miejsce dla ") + target.name
                                    + utf8(" na ścieżce ") + masker.name,
                                title.withTrimmedLeft(18),
                                juce::Justification::centredLeft, 1, 0.8f);

        juce::String details;
        if (recommendation.action == analysis::MaskingAction::lowerMasker)
            details = utf8("Obniż poziom o ")
                    + juce::String(std::abs(recommendation.gainDb), 1) + " dB";
        else if (recommendation.action == analysis::MaskingAction::staticEqCut)
            details = utf8("EQ: ") + formatFrequency(recommendation.frequencyHz)
                    + ", " + juce::String(recommendation.gainDb, 1) + " dB, Q "
                    + juce::String(recommendation.q, 1);
        else
            details = utf8("Dynamiczne EQ: ") + formatFrequency(recommendation.frequencyHz)
                    + utf8(", redukcja do ")
                    + juce::String(recommendation.maximumDynamicReductionDb, 1)
                    + " dB, Q " + juce::String(recommendation.q, 1);
        details += utf8("  •  dotyczy około ")
                + juce::String(static_cast<int>(std::round(
                    recommendation.affectedTargetFraction * 100.0f)))
                + utf8("% aktywnej partii celu  •  kliknij, aby zobaczyć dokładne miejsca");
        graphics.setColour(juce::Colour(0xffaeb9ca));
        graphics.setFont(juce::FontOptions(12.5f));
        graphics.drawFittedText(details, content, juce::Justification::centredLeft, 1, 0.8f);
    }

    static constexpr int cardHeight = 84;
    static constexpr int dashboardHeight = 150;
    static constexpr int planHeadingHeight = 32;
    static constexpr int planCardHeight = 76;
    static constexpr int pairHeadingHeight = 30;
    static constexpr int noticeHeight = 48;
    static constexpr int maximumTrackAppearances = 2;
    static constexpr std::size_t maximumPlanTasks = 3;
    static constexpr std::size_t maximumDetailedIssues = 12;
    const std::vector<TrackData>* tracks = nullptr;
    const std::vector<analysis::PairSummary>* summaries = nullptr;
    const std::vector<PhasePairData>* phasePairs = nullptr;
    const std::optional<analysis::MixBalanceSummary>* mixBalance = nullptr;
    const std::optional<analysis::MasterBalanceSummary>* masterBalance = nullptr;
    std::vector<analysis::PairIssue> issues;
    std::vector<analysis::MaskingRecommendation> maskingRecommendations;
    std::vector<std::size_t> dynamicsIssues;
    std::vector<std::size_t> resonanceIssues;
    std::vector<std::size_t> phaseIssues;
    std::vector<std::size_t> stereoPhaseIssues;
    std::vector<std::size_t> compressionIssues;
    std::vector<PlanTask> planTasks;
    int relatedMicrophoneCount = 0;
    int hoveredRow = -1;
};

class PairAnalysisView::MatrixComponent final : public juce::Component
{
public:
    void setData(const std::vector<TrackData>* newTracks,
                 const std::vector<analysis::PairSummary>* newSummaries)
    {
        tracks = newTracks;
        summaries = newSummaries;
        selectedMasker = -1;
        selectedTarget = -1;
        updateSize(0);
        repaint();
    }

    void updateSize(const int minimumWidth)
    {
        const auto count = tracks != nullptr ? static_cast<int>(tracks->size()) : 0;
        setSize(std::max(minimumWidth, leftHeaderWidth + count * cellWidth + 2),
                topHeaderHeight + count * cellHeight + 2);
    }

    void selectPair(const std::size_t masker, const std::size_t target)
    {
        selectedMasker = static_cast<int>(masker);
        selectedTarget = static_cast<int>(target);
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff0b1220));
        if (tracks == nullptr || summaries == nullptr)
            return;

        // Najpierw komórki, a potem nagłówki rysowane w aktualnej pozycji Viewportu.
        // Dzięki temu numery u góry i nazwy z lewej pozostają nieruchome.
        for (std::size_t row = 0; row < tracks->size(); ++row)
        {
            const auto y = topHeaderHeight + static_cast<int>(row) * cellHeight;
            for (std::size_t column = 0; column < tracks->size(); ++column)
            {
                const auto x = leftHeaderWidth + static_cast<int>(column) * cellWidth;
                auto cell = juce::Rectangle<int>(x, y, cellWidth, cellHeight).reduced(2);

                if (row == column)
                {
                    graphics.setColour(juce::Colour(0xff111827));
                    graphics.fillRoundedRectangle(cell.toFloat(), 4.0f);
                    graphics.setColour(juce::Colour(0xff475569));
                    graphics.drawText(utf8("—"), cell, juce::Justification::centred);
                    continue;
                }

                const auto index = row * tracks->size() + column;
                const auto summary = index < summaries->size()
                    ? (*summaries)[index] : analysis::PairSummary{};
                graphics.setColour(scoreColour(summary.score, summary.confidence));
                graphics.fillRoundedRectangle(cell.toFloat(), 4.0f);

                graphics.setColour((*tracks)[row].colour);
                graphics.fillRect(cell.removeFromLeft(3));
                graphics.setColour((*tracks)[column].colour);
                graphics.fillRect(cell.removeFromTop(3));

                graphics.setColour(juce::Colours::white);
                graphics.setFont(juce::FontOptions(13.0f, juce::Font::bold));
                graphics.drawText(juce::String(summary.score, 2),
                                  juce::Rectangle<int>(x, y, cellWidth, cellHeight),
                                  juce::Justification::centred);

                if (static_cast<int>(row) == selectedMasker
                    && static_cast<int>(column) == selectedTarget)
                {
                    graphics.setColour(juce::Colours::white);
                    graphics.drawRoundedRectangle(
                        juce::Rectangle<float>(static_cast<float>(x + 1),
                                               static_cast<float>(y + 1),
                                               static_cast<float>(cellWidth - 2),
                                               static_cast<float>(cellHeight - 2)),
                        5.0f,
                        2.0f);
                }
            }
        }

        const auto viewPosition = viewPositionProvider ? viewPositionProvider()
                                                       : juce::Point<int>{};

        for (std::size_t row = 0; row < tracks->size(); ++row)
        {
            const auto y = topHeaderHeight + static_cast<int>(row) * cellHeight;
            auto header = juce::Rectangle<int>(viewPosition.x, y,
                                               leftHeaderWidth, cellHeight);
            graphics.setColour(row % 2 == 0 ? juce::Colour(0xff101827)
                                             : juce::Colour(0xff0b1220));
            graphics.fillRect(header);
            graphics.setColour(juce::Colour(0xff273449));
            graphics.drawHorizontalLine(header.getBottom() - 1,
                                        static_cast<float>(header.getX()),
                                        static_cast<float>(header.getRight()));
            graphics.setColour((*tracks)[row].colour);
            graphics.fillEllipse(static_cast<float>(header.getX() + 9),
                                 static_cast<float>(y + cellHeight / 2 - 5), 10.0f, 10.0f);
            graphics.setColour(juce::Colours::white);
            graphics.setFont(juce::FontOptions(13.0f));
            graphics.drawFittedText(juce::String(static_cast<int>(row + 1)) + ". "
                                        + (*tracks)[row].name,
                                    header.withTrimmedLeft(26).withTrimmedRight(6),
                                    juce::Justification::centredLeft, 1, 0.8f);
        }

        for (std::size_t column = 0; column < tracks->size(); ++column)
        {
            const auto x = leftHeaderWidth + static_cast<int>(column) * cellWidth;
            auto header = juce::Rectangle<int>(x, viewPosition.y, cellWidth, topHeaderHeight);
            graphics.setColour(juce::Colour(0xff182235));
            graphics.fillRect(header);
            graphics.setColour(juce::Colour(0xff334155));
            graphics.drawRect(header);
            graphics.setColour(juce::Colours::white);
            graphics.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            graphics.drawText(juce::String(static_cast<int>(column + 1)),
                              header, juce::Justification::centred);
        }

        auto corner = juce::Rectangle<int>(viewPosition.x, viewPosition.y,
                                           leftHeaderWidth, topHeaderHeight);
        graphics.setColour(juce::Colour(0xff182235));
        graphics.fillRect(corner);
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        graphics.drawText(utf8("WIERSZ  →  KOLUMNA"), corner.reduced(8, 0),
                          juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto viewPosition = viewPositionProvider ? viewPositionProvider()
                                                       : juce::Point<int>{};
        if (tracks == nullptr
            || event.x < viewPosition.x + leftHeaderWidth
            || event.y < viewPosition.y + topHeaderHeight)
            return;

        const auto column = (event.x - leftHeaderWidth) / cellWidth;
        const auto row = (event.y - topHeaderHeight) / cellHeight;
        const auto count = static_cast<int>(tracks->size());
        if (row < 0 || column < 0 || row >= count || column >= count || row == column)
            return;

        selectPair(static_cast<std::size_t>(row), static_cast<std::size_t>(column));
        if (onPairSelected)
            onPairSelected(static_cast<std::size_t>(row), static_cast<std::size_t>(column));
    }

    std::function<void(std::size_t, std::size_t)> onPairSelected;
    std::function<juce::Point<int>()> viewPositionProvider;

private:
    static constexpr int leftHeaderWidth = 220;
    static constexpr int topHeaderHeight = 38;
    static constexpr int cellWidth = 72;
    static constexpr int cellHeight = 48;

    const std::vector<TrackData>* tracks = nullptr;
    const std::vector<analysis::PairSummary>* summaries = nullptr;
    int selectedMasker = -1;
    int selectedTarget = -1;
};

class PairAnalysisView::DetailComponent final : public juce::Component
{
public:
    void clear()
    {
        hasPair = false;
        loading = false;
        timeline = {};
        hoverText.clear();
        lastHeatmapBounds = {};
        repaint();
    }

    void showLoading(const TrackData& newMasker, const TrackData& newTarget)
    {
        masker = newMasker;
        target = newTarget;
        hasPair = true;
        loading = true;
        timeline = {};
        hoverText.clear();
        repaint();
    }

    void setTimeline(const TrackData& newMasker,
                     const TrackData& newTarget,
                     analysis::PairTimeline newTimeline)
    {
        masker = newMasker;
        target = newTarget;
        timeline = std::move(newTimeline);
        hasPair = true;
        loading = false;
        hoverText.clear();
        repaint();
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (!hasPair || loading || timeline.frameCount == 0
            || !lastHeatmapBounds.contains(event.getPosition()))
        {
            if (hoverText.isNotEmpty())
            {
                hoverText.clear();
                repaint();
            }
            return;
        }

        constexpr auto bands = analysis::TrackFeatures::bandCount;
        const auto xRatio = juce::jlimit(0.0f, 0.999999f,
            static_cast<float>(event.x - lastHeatmapBounds.getX())
                / static_cast<float>(std::max(1, lastHeatmapBounds.getWidth())));
        const auto yRatio = juce::jlimit(0.0f, 0.999999f,
            static_cast<float>(lastHeatmapBounds.getBottom() - event.y)
                / static_cast<float>(std::max(1, lastHeatmapBounds.getHeight())));
        const auto frame = std::min(timeline.frameCount - 1,
            static_cast<std::size_t>(xRatio * static_cast<float>(timeline.frameCount)));
        const auto band = std::min<std::size_t>(bands - 1,
            static_cast<std::size_t>(yRatio * static_cast<float>(bands)));
        const auto seconds = static_cast<double>(frame) / timeline.frameRate;
        const auto intensity = timeline.at(frame, band);
        hoverText = formatTime(seconds) + "  •  "
                  + formatFrequency(timeline.centreFrequenciesHz[band])
                  + utf8("  •  intensywność ")
                  + juce::String(static_cast<int>(std::round(intensity * 100.0f))) + "%";
        hoverPoint = event.getPosition();
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoverText.clear();
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff0b1220));
        const auto spacious = getHeight() >= 500;
        auto area = getLocalBounds().reduced(spacious ? 20 : 12);

        if (!hasPair)
        {
            graphics.setColour(juce::Colour(0xff94a3b8));
            graphics.setFont(juce::FontOptions(16.0f));
            graphics.drawText(utf8("Kliknij komórkę macierzy, aby zobaczyć przebieg konfliktu w czasie."),
                              area,
                              juce::Justification::centred);
            return;
        }

        auto titleArea = area.removeFromTop(spacious ? 44 : 30);
        graphics.setFont(juce::FontOptions(spacious ? 22.0f : 17.0f, juce::Font::bold));
        graphics.setColour(masker.colour);
        graphics.fillEllipse(static_cast<float>(titleArea.getX()),
                             static_cast<float>(titleArea.getCentreY() - 5), 10.0f, 10.0f);
        graphics.setColour(juce::Colours::white);
        graphics.drawText(masker.name + utf8("  →  ") + target.name,
                          titleArea.withTrimmedLeft(18),
                          juce::Justification::centredLeft);

        if (loading)
        {
            graphics.setColour(juce::Colour(0xff6ee7b7));
            graphics.drawText(utf8("Obliczanie mapy czas–pasmo…"),
                              area,
                              juce::Justification::centred);
            return;
        }

        auto summaryArea = area.removeFromTop(spacious ? 92 : 68);
        const auto& summary = timeline.summary;
        const auto severity = analysis::pairIssueSeverity(summary);
        const auto severityText = severity == analysis::PairIssueSeverity::high
            ? utf8("WYSOKI PRIORYTET")
            : (severity == analysis::PairIssueSeverity::medium
                ? utf8("WARTO SPRAWDZIĆ") : utf8("NISKI PRIORYTET"));
        const auto severityColour = severity == analysis::PairIssueSeverity::high
            ? juce::Colour(0xffef4444)
            : (severity == analysis::PairIssueSeverity::medium
                ? juce::Colour(0xfff59e0b) : juce::Colour(0xff60a5fa));

        auto firstLine = summaryArea.removeFromTop(25);
        auto badge = firstLine.removeFromLeft(145).reduced(0, 3);
        graphics.setColour(severityColour.withAlpha(0.18f));
        graphics.fillRoundedRectangle(badge.toFloat(), 4.0f);
        graphics.setColour(severityColour);
        graphics.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        graphics.drawText(severityText, badge, juce::Justification::centred);

        juce::String summaryText = utf8("Wskaźnik konkurencji ")
            + juce::String(summary.score, 2)
            + utf8("  •  wspólna aktywność ") + formatTime(summary.coActiveSeconds)
            + " (" + juce::String(static_cast<int>(std::round(summary.coverage * 100.0f)))
            + utf8("%)  •  ")
            + measurementAmountText(summary.confidence)
            + utf8("  •  ") + characterText(summary.character);
        if (summary.worstBand >= 0)
            summaryText += utf8("  •  pasmo ")
                + formatFrequency(timeline.centreFrequenciesHz[
                    static_cast<std::size_t>(summary.worstBand)]);
        graphics.setColour(juce::Colour(0xffcbd5e1));
        graphics.setFont(juce::FontOptions(spacious ? 14.0f : 13.0f));
        graphics.drawFittedText(summaryText, firstLine.withTrimmedLeft(12),
                                juce::Justification::centredLeft, 1, 0.8f);

        const auto recommendation = analysis::MaskingAdvisor{}.recommend(
            summary, *target.features, masker.role, target.role);
        juce::String listeningTip;
        if (recommendation.action == analysis::MaskingAction::lowerMasker)
            listeningTip = utf8("Proponowana korekta: obniż ") + masker.name + " o "
                + juce::String(std::abs(recommendation.gainDb), 1) + " dB.";
        else if (recommendation.action == analysis::MaskingAction::staticEqCut)
            listeningTip = utf8("Proponowana korekta na ") + masker.name + utf8(": EQ ")
                + formatFrequency(recommendation.frequencyHz) + ", "
                + juce::String(recommendation.gainDb, 1) + " dB, Q "
                + juce::String(recommendation.q, 1) + ".";
        else if (recommendation.action == analysis::MaskingAction::dynamicEqCut)
            listeningTip = utf8("Proponowana korekta na ") + masker.name
                + utf8(": dynamiczne EQ przy ")
                + formatFrequency(recommendation.frequencyHz) + utf8(", maks. redukcja ")
                + juce::String(recommendation.maximumDynamicReductionDb, 1) + " dB, Q "
                + juce::String(recommendation.q, 1) + ".";
        else
            listeningTip = utf8("Wynik nie przekracza progu bezpiecznej automatycznej korekty.");
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.setFont(juce::FontOptions(spacious ? 13.0f : 12.0f));
        graphics.drawFittedText(listeningTip, summaryArea,
                                juce::Justification::centredLeft, 2, 0.85f);

        area.removeFromTop(spacious ? 12 : 6);
        auto envelopeArea = area.removeFromTop(spacious
            ? std::clamp(area.getHeight() / 4, 120, 180)
            : std::min(90, area.getHeight() / 4));
        drawEnvelopes(graphics, envelopeArea.reduced(72, 4));
        area.removeFromTop(spacious ? 12 : 8);

        auto heatmapHeading = area.removeFromTop(spacious ? 42 : 28);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(spacious ? 15.0f : 12.5f, juce::Font::bold));
        graphics.drawText(utf8("GDZIE ŚCIEŻKI KONKURUJĄ — czas × częstotliwość"),
                          heatmapHeading.removeFromLeft(std::max(280, heatmapHeading.getWidth() / 2)),
                          juce::Justification::centredLeft);
        drawLegend(graphics, heatmapHeading.reduced(8, spacious ? 9 : 4));

        lastHeatmapBounds = area.withTrimmedLeft(72).withTrimmedBottom(28);
        drawHeatmap(graphics, lastHeatmapBounds);
        drawHover(graphics);
    }

private:
    void drawLegend(juce::Graphics& graphics, const juce::Rectangle<int> bounds)
    {
        if (bounds.getWidth() < 180 || bounds.getHeight() < 8)
            return;
        auto legend = bounds.withWidth(std::min(360, bounds.getWidth())).withTrimmedLeft(65)
                            .withTrimmedRight(55);
        juce::ColourGradient gradient(heatColour(0.0f),
                                     static_cast<float>(legend.getX()), 0.0f,
                                     heatColour(1.0f),
                                     static_cast<float>(legend.getRight()), 0.0f,
                                     false);
        gradient.addColour(0.25, heatColour(0.25f));
        gradient.addColour(0.55, heatColour(0.55f));
        gradient.addColour(0.80, heatColour(0.80f));
        graphics.setGradientFill(gradient);
        graphics.fillRoundedRectangle(legend.toFloat(), 4.0f);
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.setFont(juce::FontOptions(10.5f));
        graphics.drawText(utf8("słaba"), legend.translated(-62, 0).withWidth(58),
                          juce::Justification::centredRight);
        graphics.drawText("silna", legend.withX(legend.getRight() + 6).withWidth(48),
                          juce::Justification::centredLeft);
    }

    void drawHover(juce::Graphics& graphics)
    {
        if (hoverText.isEmpty() || lastHeatmapBounds.isEmpty())
            return;
        graphics.setColour(juce::Colours::white.withAlpha(0.75f));
        graphics.drawVerticalLine(hoverPoint.x,
                                  static_cast<float>(lastHeatmapBounds.getY()),
                                  static_cast<float>(lastHeatmapBounds.getBottom()));
        graphics.drawHorizontalLine(hoverPoint.y,
                                    static_cast<float>(lastHeatmapBounds.getX()),
                                    static_cast<float>(lastHeatmapBounds.getRight()));

        auto box = juce::Rectangle<int>(hoverPoint.x + 14, hoverPoint.y - 38, 285, 30);
        if (box.getRight() > getWidth() - 8)
            box.setX(hoverPoint.x - box.getWidth() - 14);
        if (box.getY() < 8)
            box.setY(hoverPoint.y + 14);
        graphics.setColour(juce::Colour(0xee111827));
        graphics.fillRoundedRectangle(box.toFloat(), 6.0f);
        graphics.setColour(juce::Colour(0xff38bdf8));
        graphics.drawRoundedRectangle(box.toFloat(), 6.0f, 1.0f);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        graphics.drawText(hoverText, box.reduced(10, 0), juce::Justification::centredLeft);
    }

    void drawEnvelopes(juce::Graphics& graphics, const juce::Rectangle<int> bounds)
    {
        graphics.setColour(juce::Colour(0xff07101f));
        graphics.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        graphics.setColour(juce::Colour(0xff334155));
        graphics.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

        for (const auto level : { -60.0f, -30.0f, 0.0f })
        {
            const auto y = juce::jmap(level, -80.0f, 0.0f,
                                     static_cast<float>(bounds.getBottom()),
                                     static_cast<float>(bounds.getY()));
            graphics.setColour(juce::Colour(0xff1e293b));
            graphics.drawHorizontalLine(static_cast<int>(y),
                                        static_cast<float>(bounds.getX()),
                                        static_cast<float>(bounds.getRight()));
            graphics.setColour(juce::Colour(0xff64748b));
            graphics.setFont(juce::FontOptions(9.5f));
            graphics.drawText(juce::String(level, 0), bounds.getX() - 38,
                              static_cast<int>(y) - 7, 32, 14,
                              juce::Justification::centredRight);
        }

        auto drawOne = [&](const std::vector<float>& values, const juce::Colour colour)
        {
            if (values.empty() || bounds.getWidth() <= 0)
                return;
            juce::Path path;
            for (int pixel = 0; pixel < bounds.getWidth(); ++pixel)
            {
                const auto first = static_cast<std::size_t>(pixel) * values.size()
                                 / static_cast<std::size_t>(bounds.getWidth());
                const auto last = std::max(first + 1,
                    static_cast<std::size_t>(pixel + 1) * values.size()
                    / static_cast<std::size_t>(bounds.getWidth()));
                auto level = -120.0f;
                for (auto frame = first; frame < std::min(last, values.size()); ++frame)
                    level = std::max(level, values[frame]);
                const auto y = juce::jmap(juce::jlimit(-80.0f, 0.0f, level),
                                         -80.0f, 0.0f,
                                         static_cast<float>(bounds.getBottom()),
                                         static_cast<float>(bounds.getY()));
                const auto x = static_cast<float>(bounds.getX() + pixel);
                if (pixel == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }
            auto fill = path;
            fill.lineTo(static_cast<float>(bounds.getRight()),
                        static_cast<float>(bounds.getBottom()));
            fill.lineTo(static_cast<float>(bounds.getX()),
                        static_cast<float>(bounds.getBottom()));
            fill.closeSubPath();
            graphics.setColour(colour.withAlpha(0.10f));
            graphics.fillPath(fill);
            graphics.setColour(colour);
            graphics.strokePath(path, juce::PathStrokeType(2.0f));
        };

        drawOne(timeline.maskerEnvelopeDb, masker.colour);
        drawOne(timeline.targetEnvelopeDb, target.colour);
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.setFont(juce::FontOptions(11.0f));
        graphics.drawText("dBFS", bounds.translated(-66, 0).withWidth(26),
                          juce::Justification::centredRight);

        auto legend = juce::Rectangle<int>(bounds.getX() + 10, bounds.getY() + 8,
                                           std::min(420, bounds.getWidth() - 20), 20);
        graphics.setColour(masker.colour);
        graphics.fillEllipse(static_cast<float>(legend.getX()),
                             static_cast<float>(legend.getCentreY() - 4), 8.0f, 8.0f);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        graphics.drawFittedText(masker.name, legend.removeFromLeft(190).withTrimmedLeft(14),
                                juce::Justification::centredLeft, 1, 0.8f);
        graphics.setColour(target.colour);
        graphics.fillEllipse(static_cast<float>(legend.getX()),
                             static_cast<float>(legend.getCentreY() - 4), 8.0f, 8.0f);
        graphics.setColour(juce::Colours::white);
        graphics.drawFittedText(target.name, legend.withTrimmedLeft(14),
                                juce::Justification::centredLeft, 1, 0.8f);
    }

    void drawHeatmap(juce::Graphics& graphics, const juce::Rectangle<int> bounds)
    {
        graphics.setColour(juce::Colour(0xff07101f));
        graphics.fillRect(bounds);
        if (timeline.frameCount == 0 || bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
            return;

        constexpr auto bands = analysis::TrackFeatures::bandCount;
        const auto bandHeight = static_cast<float>(bounds.getHeight()) / static_cast<float>(bands);

        for (std::size_t band = 0; band < bands; ++band)
        {
            const auto y = static_cast<float>(bounds.getBottom())
                         - static_cast<float>(band + 1) * bandHeight;
            if ((band / 5) % 2 != 0)
            {
                graphics.setColour(juce::Colour(0xff0c1728));
                graphics.fillRect(static_cast<float>(bounds.getX()), y,
                                  static_cast<float>(bounds.getWidth()), bandHeight + 0.5f);
            }
        }

        for (int pixel = 0; pixel < bounds.getWidth(); ++pixel)
        {
            const auto first = static_cast<std::size_t>(pixel) * timeline.frameCount
                             / static_cast<std::size_t>(bounds.getWidth());
            const auto last = std::max(first + 1,
                static_cast<std::size_t>(pixel + 1) * timeline.frameCount
                / static_cast<std::size_t>(bounds.getWidth()));

            for (std::size_t band = 0; band < bands; ++band)
            {
                auto intensity = 0.0f;
                for (auto frame = first; frame < std::min(last, timeline.frameCount); ++frame)
                    intensity = std::max(intensity, timeline.at(frame, band));
                if (intensity <= 0.005f)
                    continue;

                const auto y = static_cast<float>(bounds.getBottom())
                             - static_cast<float>(band + 1) * bandHeight;
                graphics.setColour(heatColour(intensity));
                graphics.fillRect(static_cast<float>(bounds.getX() + pixel),
                                  y,
                                  1.2f,
                                  bandHeight + 0.5f);
            }
        }

        graphics.setColour(juce::Colour(0xff334155));
        graphics.drawRect(bounds);
        graphics.setFont(juce::FontOptions(bounds.getHeight() > 260 ? 11.5f : 10.0f));
        const std::size_t labelBands[] { 0, 10, 20, 30, 39 };
        for (const auto band : labelBands)
        {
            const auto y = static_cast<int>(static_cast<float>(bounds.getBottom())
                         - (static_cast<float>(band) + 0.5f) * bandHeight);
            const auto frequency = timeline.centreFrequenciesHz[band];
            const auto label = frequency >= 1000.0f
                ? juce::String(frequency / 1000.0f, 1) + "k"
                : juce::String(frequency, 0);
            graphics.setColour(juce::Colour(0xff334155));
            graphics.drawHorizontalLine(y,
                                        static_cast<float>(bounds.getX()),
                                        static_cast<float>(bounds.getRight()));
            graphics.setColour(juce::Colour(0xff94a3b8));
            graphics.drawText(label,
                              bounds.getX() - 54,
                              y - 7,
                              48,
                              14,
                              juce::Justification::centredRight);
        }

        graphics.setColour(juce::Colour(0xff64748b));
        graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        graphics.drawText("Hz", bounds.getX() - 54, bounds.getY() - 2, 48, 14,
                          juce::Justification::centredRight);

        if (timeline.summary.worstBand >= 0
            && timeline.summary.worstBand < static_cast<int>(bands))
        {
            const auto worstBand = static_cast<std::size_t>(timeline.summary.worstBand);
            const auto y = static_cast<float>(bounds.getBottom())
                         - static_cast<float>(worstBand + 1) * bandHeight;
            const auto highlightedBand = juce::Rectangle<float>(
                static_cast<float>(bounds.getX()), y,
                static_cast<float>(bounds.getWidth()), std::max(2.0f, bandHeight));
            graphics.setColour(juce::Colour(0xffffd166));
            graphics.drawRect(highlightedBand, 1.5f);

            const auto frequency = timeline.centreFrequenciesHz[worstBand];
            auto label = juce::Rectangle<int>(bounds.getRight() - 196,
                                              std::max(bounds.getY() + 6,
                                                       static_cast<int>(y) - 25),
                                              186, 23);
            graphics.setColour(juce::Colour(0xe6111827));
            graphics.fillRoundedRectangle(label.toFloat(), 5.0f);
            graphics.setColour(juce::Colour(0xffffd166));
            graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            graphics.drawText(utf8("Najsilniej: ") + formatFrequency(frequency),
                              label.reduced(7, 0), juce::Justification::centredLeft);
        }

        const auto duration = static_cast<double>(timeline.frameCount) / timeline.frameRate;
        const auto tickCount = bounds.getWidth() >= 1300 ? 8
                             : (bounds.getWidth() >= 850 ? 6 : 4);
        for (int tick = 0; tick <= tickCount; ++tick)
        {
            const auto x = bounds.getX() + tick * bounds.getWidth() / tickCount;
            graphics.setColour(juce::Colour(0xff64748b));
            graphics.drawVerticalLine(x,
                                      static_cast<float>(bounds.getY()),
                                      static_cast<float>(bounds.getBottom()));
            graphics.drawText(formatTime(duration * static_cast<double>(tick)
                                                   / static_cast<double>(tickCount)),
                              x - 28,
                              bounds.getBottom() + 3,
                              56,
                              16,
                              juce::Justification::centred);
        }
    }

    TrackData masker;
    TrackData target;
    analysis::PairTimeline timeline;
    juce::Rectangle<int> lastHeatmapBounds;
    juce::String hoverText;
    juce::Point<int> hoverPoint;
    bool hasPair = false;
    bool loading = false;
};

PairAnalysisView::PairAnalysisView()
    : problemsComponent(std::make_unique<ProblemsComponent>()),
      matrixComponent(std::make_unique<MatrixComponent>()),
      detailComponent(std::make_unique<DetailComponent>())
{
    problemsViewport.setViewedComponent(problemsComponent.get(), false);
    problemsViewport.setScrollBarsShown(true, false);
    problemsViewport.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(problemsViewport);

    matrixViewport.setViewedComponent(matrixComponent.get(), false);
    matrixViewport.setScrollBarsShown(true, true);
    matrixViewport.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff38bdf8));
    addChildComponent(matrixViewport);
    addChildComponent(*detailComponent);

    matrixViewport.getHorizontalScrollBar().addListener(this);
    matrixViewport.getVerticalScrollBar().addListener(this);

    problemsButton.setButtonText(utf8("PLAN MIKSU"));
    fullMatrixButton.setButtonText(utf8("DANE TECHNICZNE"));
    detailButton.setButtonText(utf8("DUŻY WYKRES"));
    channelStripToggle.setButtonText("EQ + COMP NA ŚCIEŻKACH");
    channelStripToggle.setToggleState(true, juce::dontSendNotification);
    channelStripToggle.setTooltip(utf8("Dodaje zarządzany ReaEQ z bezpiecznym punktem startowym dla potwierdzonego typu oraz kompresor ustawiony z pomiaru dynamiki. Gdy kompresja nie jest potrzebna, ratio pozostaje 1:1."));
    applyPlanButton.setButtonText(utf8("ZASTOSUJ SUGESTIE"));
    undoPlanButton.setButtonText(utf8("PRZYWRÓĆ PRZED"));
    problemsButton.onClick = [this] { showProblems(); };
    fullMatrixButton.onClick = [this] { showFullMatrix(); };
    detailButton.onClick = [this] { showDetailFullscreen(); };
    applyPlanButton.onClick = [this]
    {
        if (onApplySafePlan)
            onApplySafePlan();
    };
    undoPlanButton.onClick = [this]
    {
        if (onUndoSafePlan)
            onUndoSafePlan();
    };
    addAndMakeVisible(problemsButton);
    addAndMakeVisible(fullMatrixButton);
    addAndMakeVisible(detailButton);
    addAndMakeVisible(channelStripToggle);
    addAndMakeVisible(applyPlanButton);
    addAndMakeVisible(undoPlanButton);
    detailButton.setEnabled(false);
    applyPlanButton.setEnabled(false);
    undoPlanButton.setEnabled(false);

    matrixComponent->viewPositionProvider = [this]
    {
        return matrixViewport.getViewPosition();
    };

    matrixComponent->onPairSelected = [this](const std::size_t masker,
                                              const std::size_t target)
    {
        showDetailFullscreen();
        if (onPairSelected)
            onPairSelected(masker, target);
    };

    problemsComponent->onPairSelected = [this](const std::size_t masker,
                                                const std::size_t target)
    {
        matrixComponent->selectPair(masker, target);
        showDetailFullscreen();
        if (onPairSelected)
            onPairSelected(masker, target);
    };

    showProblems();
}

PairAnalysisView::~PairAnalysisView()
{
    matrixViewport.getHorizontalScrollBar().removeListener(this);
    matrixViewport.getVerticalScrollBar().removeListener(this);
}

void PairAnalysisView::scrollBarMoved(juce::ScrollBar*, double)
{
    matrixComponent->repaint();
}

void PairAnalysisView::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff111827));
    graphics.setColour(juce::Colours::white);
    graphics.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    graphics.drawText(detailFullscreenVisible
                          ? utf8("Maskowanie w czasie — widok szczegółowy")
                          : (fullMatrixVisible
                              ? utf8("Dane techniczne — wiersz może ograniczać czytelność kolumny")
                              : utf8("Plan rozpoczęcia miksu")),
                      0, 0, getWidth(), 28,
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(0xff94a3b8));
    graphics.setFont(juce::FontOptions(12.0f));
    graphics.drawText(detailFullscreenVisible
                          ? utf8("Najedź na mapę, aby odczytać czas, częstotliwość i intensywność konkurencji.")
                          : (fullMatrixVisible
                              ? utf8("U góry są numery ścieżek z listy po lewej. Nagłówki pozostają widoczne podczas przewijania.")
                              : utf8("Kolejność ma znaczenie: faza, balans, czytelność i dopiero potem kompresja.")),
                      0, 27, getWidth(), 24,
                      juce::Justification::centredLeft);
}

void PairAnalysisView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(58);
    auto buttons = header.removeFromRight(1000).withTrimmedTop(8).withHeight(36);
    problemsButton.setBounds(buttons.removeFromLeft(115));
    buttons.removeFromLeft(8);
    fullMatrixButton.setBounds(buttons.removeFromLeft(145));
    buttons.removeFromLeft(8);
    detailButton.setBounds(buttons.removeFromLeft(145));
    buttons.removeFromLeft(10);
    channelStripToggle.setBounds(buttons.removeFromLeft(225));
    buttons.removeFromLeft(10);
    applyPlanButton.setBounds(buttons.removeFromLeft(190));
    buttons.removeFromLeft(8);
    undoPlanButton.setBounds(buttons.removeFromLeft(130));

    if (!fullMatrixVisible)
    {
        if (detailFullscreenVisible)
        {
            detailComponent->setBounds(area);
            return;
        }
        problemsViewport.setBounds(area);
        problemsComponent->updateSize(problemsViewport.getMaximumVisibleWidth());
        return;
    }

    matrixViewport.setBounds(area);
    matrixComponent->updateSize(matrixViewport.getMaximumVisibleWidth());
}

void PairAnalysisView::setMatrix(std::vector<TrackData> newTracks,
                                 std::vector<analysis::PairSummary> newSummaries,
                                 std::vector<PhasePairData> newPhasePairs,
                                 std::optional<analysis::MixBalanceSummary> newMixBalance,
                                 std::optional<analysis::MasterBalanceSummary> newMasterBalance)
{
    tracks = std::move(newTracks);
    summaries = std::move(newSummaries);
    phasePairs = std::move(newPhasePairs);
    mixBalance = std::move(newMixBalance);
    masterBalance = std::move(newMasterBalance);
    problemsComponent->setData(&tracks, &summaries, &phasePairs, &mixBalance, &masterBalance);
    matrixComponent->setData(&tracks, &summaries);
    detailComponent->clear();
    detailButton.setEnabled(false);
    showProblems();
    resized();
    repaint();
}

void PairAnalysisView::showProblems()
{
    fullMatrixVisible = false;
    detailFullscreenVisible = false;
    problemsViewport.setVisible(true);
    matrixViewport.setVisible(false);
    detailComponent->setVisible(false);
    problemsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    fullMatrixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    detailButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    resized();
    repaint();
}

void PairAnalysisView::showFullMatrix()
{
    fullMatrixVisible = true;
    detailFullscreenVisible = false;
    problemsViewport.setVisible(false);
    matrixViewport.setVisible(true);
    detailComponent->setVisible(false);
    problemsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    fullMatrixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    detailButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    resized();
    repaint();
}

void PairAnalysisView::showDetailFullscreen()
{
    fullMatrixVisible = false;
    detailFullscreenVisible = true;
    problemsViewport.setVisible(false);
    matrixViewport.setVisible(false);
    detailComponent->setVisible(true);
    problemsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    fullMatrixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    detailButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    resized();
    repaint();
}

void PairAnalysisView::updateTrackColours(const std::vector<juce::Colour>& colours)
{
    for (std::size_t index = 0; index < std::min(tracks.size(), colours.size()); ++index)
        tracks[index].colour = colours[index];
    detailComponent->clear();
    detailButton.setEnabled(false);
    if (detailFullscreenVisible)
        showProblems();
    problemsComponent->repaint();
    matrixComponent->repaint();
    repaint();
}

void PairAnalysisView::showLoading(const std::size_t maskerIndex,
                                   const std::size_t targetIndex)
{
    if (maskerIndex >= tracks.size() || targetIndex >= tracks.size())
        return;
    matrixComponent->selectPair(maskerIndex, targetIndex);
    detailButton.setEnabled(true);
    detailComponent->showLoading(tracks[maskerIndex], tracks[targetIndex]);
}

void PairAnalysisView::setTimeline(const std::size_t maskerIndex,
                                   const std::size_t targetIndex,
                                   analysis::PairTimeline timeline)
{
    if (maskerIndex >= tracks.size() || targetIndex >= tracks.size())
        return;
    detailButton.setEnabled(true);
    detailComponent->setTimeline(tracks[maskerIndex],
                                 tracks[targetIndex],
                                 std::move(timeline));
}

void PairAnalysisView::setApplyState(const bool canApply, const bool planIsApplied)
{
    applyPlanButton.setEnabled(canApply && !planIsApplied);
    undoPlanButton.setEnabled(canApply && planIsApplied);
    applyPlanButton.setButtonText(planIsApplied ? utf8("PLAN ZASTOSOWANY")
                                                : utf8("ZASTOSUJ SUGESTIE"));
}

bool PairAnalysisView::shouldPrepareChannelStrip() const noexcept
{
    return channelStripToggle.getToggleState();
}

const PairAnalysisView::TrackData* PairAnalysisView::trackAt(const std::size_t index) const noexcept
{
    return index < tracks.size() ? &tracks[index] : nullptr;
}

std::size_t PairAnalysisView::trackCount() const noexcept
{
    return tracks.size();
}
} // namespace mixdebugger
