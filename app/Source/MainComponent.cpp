#include "MainComponent.h"
#include "AudioValidation.h"
#include "ReaperCommandLine.h"

#include <mixdebugger/analysis/LevelAccumulator.h>
#include <mixdebugger/dsp/ErbFilterBank.h>
#include <mixdebugger/dsp/LanczosResampler.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace mixdebugger
{
namespace
{
constexpr int nameColumn = 1;
constexpr int durationColumn = 2;
constexpr int sampleRateColumn = 3;
constexpr int channelsColumn = 4;
constexpr int rmsColumn = 5;
constexpr int peakColumn = 6;
constexpr int framesColumn = 7;
constexpr int dominantFrequencyColumn = 8;
constexpr int erbSpectrumColumn = 9;
constexpr int colourColumn = 10;
constexpr int stereoCorrelationColumn = 11;
constexpr int trackFamilyColumn = 12;
constexpr int analysisGroupColumn = 13;
constexpr int sourceGroupColumn = 14;
constexpr int mixRoleColumn = 15;
// 8 kHz daje krok 0,125 ms, wystarczający do technicznego wyrównania mikrofonów,
// bez kosztu pamięci pełnego audio 48 kHz. Próbki są przechowywane wyłącznie dla
// grup jawnie oznaczonych jako JEDNO ŹRÓDŁO.
constexpr double phaseAnalysisSampleRate = 8000.0;

using ColourPalette = std::array<juce::Colour, 12>;

const ColourPalette& paletteForId(const int paletteId)
{
    static const ColourPalette studio {
        juce::Colour(0xff38bdf8), juce::Colour(0xfff97316), juce::Colour(0xffa78bfa),
        juce::Colour(0xff34d399), juce::Colour(0xfff472b6), juce::Colour(0xfffacc15),
        juce::Colour(0xff60a5fa), juce::Colour(0xfffb7185), juce::Colour(0xff2dd4bf),
        juce::Colour(0xffc084fc), juce::Colour(0xffa3e635), juce::Colour(0xfffbbf24)
    };
    static const ColourPalette neon {
        juce::Colour(0xff00e5ff), juce::Colour(0xffff2bd6), juce::Colour(0xff8cff00),
        juce::Colour(0xffff7a00), juce::Colour(0xff7c4dff), juce::Colour(0xffffff00),
        juce::Colour(0xff00ffa8), juce::Colour(0xffff1744), juce::Colour(0xff18ffff),
        juce::Colour(0xffd500f9), juce::Colour(0xff76ff03), juce::Colour(0xffff9100)
    };
    static const ColourPalette pastel {
        juce::Colour(0xff93c5fd), juce::Colour(0xfffda4af), juce::Colour(0xffc4b5fd),
        juce::Colour(0xff86efac), juce::Colour(0xfff9a8d4), juce::Colour(0xfffde68a),
        juce::Colour(0xff67e8f9), juce::Colour(0xfffdba74), juce::Colour(0xffa7f3d0),
        juce::Colour(0xffd8b4fe), juce::Colour(0xffbef264), juce::Colour(0xfffca5a5)
    };
    static const ColourPalette accessible {
        juce::Colour(0xff0072b2), juce::Colour(0xffe69f00), juce::Colour(0xff009e73),
        juce::Colour(0xffcc79a7), juce::Colour(0xff56b4e9), juce::Colour(0xffd55e00),
        juce::Colour(0xfff0e442), juce::Colour(0xff7a3e9d), juce::Colour(0xff2a9d8f),
        juce::Colour(0xffe76f51), juce::Colour(0xff264653), juce::Colour(0xff8ab17d)
    };

    switch (paletteId)
    {
        case 2: return neon;
        case 3: return pastel;
        case 4: return accessible;
        default: return studio;
    }
}

juce::String utf8(const char* text)
{
    return juce::String::fromUTF8(text);
}

juce::String familyName(const analysis::TrackFamily family)
{
    switch (family)
    {
        case analysis::TrackFamily::kick: return "Kick";
        case analysis::TrackFamily::snare: return "Snare";
        case analysis::TrackFamily::cymbals: return utf8("Talerze / OH");
        case analysis::TrackFamily::toms: return "Tomy";
        case analysis::TrackFamily::drumRoom: return "Room";
        case analysis::TrackFamily::bass: return "Bas";
        case analysis::TrackFamily::vocal: return "Wokal";
        case analysis::TrackFamily::guitar: return "Gitara";
        case analysis::TrackFamily::keys: return "Klawisze";
        case analysis::TrackFamily::effect: return "Efekt / send";
        case analysis::TrackFamily::parallelDrums: return "Parallel drums";
        case analysis::TrackFamily::unknown: return "Inna";
    }
    return "Inna";
}

int familyItemId(const analysis::TrackFamily family)
{
    return family == analysis::TrackFamily::unknown
        ? 100 : static_cast<int>(family) + 1;
}

std::optional<analysis::TrackFamily> familyForItemId(const int itemId)
{
    if (itemId == 1)
        return std::nullopt;
    if (itemId == 100)
        return analysis::TrackFamily::unknown;
    if (itemId >= 2 && itemId <= 12)
        return static_cast<analysis::TrackFamily>(itemId - 1);
    return std::nullopt;
}

juce::String roleName(const analysis::MixRole role)
{
    switch (role)
    {
        case analysis::MixRole::unassigned: return utf8("Wybierz rolę");
        case analysis::MixRole::focus: return "FOCUS";
        case analysis::MixRole::foundation: return "FUNDAMENT";
        case analysis::MixRole::support: return "WSPARCIE";
        case analysis::MixRole::background: return utf8("TŁO / FX");
        case analysis::MixRole::ignore: return utf8("POMIŃ");
    }
    return utf8("Wybierz rolę");
}

int roleItemId(const analysis::MixRole role)
{
    return static_cast<int>(role) + 1;
}

analysis::MixRole roleForItemId(const int itemId)
{
    if (itemId >= roleItemId(analysis::MixRole::unassigned)
        && itemId <= roleItemId(analysis::MixRole::ignore))
        return static_cast<analysis::MixRole>(itemId - 1);
    return analysis::MixRole::unassigned;
}

analysis::TrackRelationship relationshipForMapping(
    const juce::String& firstSource,
    const juce::String& secondSource)
{
    if (firstSource.isNotEmpty() && firstSource == secondSource)
        return analysis::TrackRelationship::sameSourceMicrophones;
    return analysis::TrackRelationship::independent;
}

class TrackFamilySelector final : public juce::ComboBox
{
public:
    TrackFamilySelector()
    {
        onChange = [this]
        {
            if (!updating && familyChanged)
                familyChanged(row, familyForItemId(getSelectedId()));
        };
    }

    void configure(const int newRow,
                   const analysis::TrackFamily suggestion,
                   const std::optional<analysis::TrackFamily> assigned)
    {
        updating = true;
        row = newRow;
        clear(juce::dontSendNotification);
        addItem(utf8("Nie przypisano • wskazówka z nazwy: ") + familyName(suggestion), 1);
        for (const auto family : { analysis::TrackFamily::kick, analysis::TrackFamily::snare,
                                  analysis::TrackFamily::cymbals, analysis::TrackFamily::toms,
                                  analysis::TrackFamily::drumRoom, analysis::TrackFamily::bass,
                                  analysis::TrackFamily::vocal, analysis::TrackFamily::guitar,
                                  analysis::TrackFamily::keys, analysis::TrackFamily::effect,
                                  analysis::TrackFamily::parallelDrums })
            addItem(familyName(family), familyItemId(family));
        addItem(familyName(analysis::TrackFamily::unknown), 100);
        setSelectedId(assigned ? familyItemId(*assigned) : 1, juce::dontSendNotification);
        updating = false;
    }

    std::function<void(int, std::optional<analysis::TrackFamily>)> familyChanged;

private:
    int row = -1;
    bool updating = false;
};

class MixRoleSelector final : public juce::ComboBox
{
public:
    MixRoleSelector()
    {
        for (const auto role : { analysis::MixRole::unassigned, analysis::MixRole::focus,
                                analysis::MixRole::foundation, analysis::MixRole::support,
                                analysis::MixRole::background, analysis::MixRole::ignore })
            addItem(roleName(role), roleItemId(role));
        onChange = [this]
        {
            if (!updating && roleChanged)
                roleChanged(row, roleForItemId(getSelectedId()));
        };
    }

    void configure(const int newRow, const analysis::MixRole role)
    {
        updating = true;
        row = newRow;
        setSelectedId(roleItemId(role), juce::dontSendNotification);
        setColour(juce::ComboBox::backgroundColourId,
                  role == analysis::MixRole::focus ? juce::Colour(0xff1d4ed8)
                                                   : juce::Colour(0xff263238));
        updating = false;
    }

    std::function<void(int, analysis::MixRole)> roleChanged;

private:
    int row = -1;
    bool updating = false;
};

class MappingNameLabel final : public juce::Label
{
public:
    MappingNameLabel()
    {
        setEditable(false, true, false);
        setColour(juce::Label::textColourId, juce::Colours::white);
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colour(0xff38bdf8));
        onTextChange = [this]
        {
            if (!updating && nameChanged)
            {
                auto value = getText().trim();
                if (value == utf8("—"))
                    value.clear();
                nameChanged(row, source, value);
            }
        };
    }

    void configure(const int newRow, const bool isSource, const juce::String& value)
    {
        updating = true;
        row = newRow;
        source = isSource;
        setText(value.isEmpty() ? utf8("—") : value, juce::dontSendNotification);
        setTooltip(value.isEmpty()
            ? utf8("Dwuklik pozwala wpisać nazwę. Wygodniej najpierw zaznaczyć kilka ścieżek i użyć przycisku grupowania.")
            : utf8("Dwuklik zmienia nazwę całej grupy."));
        updating = false;
    }

    std::function<void(int, bool, const juce::String&)> nameChanged;

private:
    int row = -1;
    bool source = false;
    bool updating = false;
};
}

MainComponent::MainComponent(const juce::String& commandLine)
{
    formatManager.registerBasicFormats();

    titleLabel.setText("MixDebugger", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(30.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    descriptionLabel.setText(
        utf8("Początek miksu: faza → balans → czytelność → kompresja. Potem wykonaj ponowną analizę."),
        juce::dontSendNotification);
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    addAndMakeVisible(descriptionLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6ee7b7));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    addFilesButton.setButtonText(utf8("Dodaj ścieżki"));
    removeButton.setButtonText(utf8("Usuń zaznaczone"));
    tracksViewButton.setButtonText(utf8("ŚCIEŻKI"));
    matrixViewButton.setButtonText(utf8("WYNIKI"));
    groupButton.setButtonText(utf8("GRUPUJ"));
    sourceGroupButton.setButtonText(utf8("JEDNO ŹRÓDŁO"));
    clearGroupingButton.setButtonText(utf8("ROZŁĄCZ"));
    confirmSuggestionsButton.setButtonText(utf8("UZUPEŁNIJ BRAKI NEUTRALNIE"));
    applyTemplateButton.setButtonText(utf8("ZASTOSUJ TEMPLATE"));
    groupButton.setTooltip(utf8("Połącz zaznaczone ścieżki w jedną grupę miksu. Dostaną wspólną rolę i ten sam ruch balansu."));
    sourceGroupButton.setTooltip(utf8("Tylko dla kilku mikrofonów nagrywających dokładnie to samo źródło. Włącza analizę polaryzacji i czasu."));
    clearGroupingButton.setTooltip(utf8("Usuń grupę miksu i powiązanie wielomikrofonowe zaznaczonych ścieżek."));
    confirmSuggestionsButton.setTooltip(utf8("Nie zgaduje instrumentów z nazw. Brakujące typy ustawia jako INNA, a role jako WSPARCIE."));
    addFilesButton.onClick = [this] { chooseFiles(); };
    removeButton.onClick = [this] { removeSelectedTracks(); };
    selectAllButton.onClick = [this] { selectAllTracks(); };
    analyseButton.onClick = [this] { startAnalysis(); };
    tracksViewButton.onClick = [this] { showTracksView(); };
    matrixViewButton.onClick = [this] { showMatrixView(); };
    groupButton.onClick = [this] { groupSelectedTracks(); };
    sourceGroupButton.onClick = [this] { joinSelectedAsOneSource(); };
    clearGroupingButton.onClick = [this] { clearSelectedGrouping(); };
    confirmSuggestionsButton.onClick = [this] { confirmSuggestedFamilies(); };
    applyTemplateButton.onClick = [this] { applySelectedMixTemplate(); };

    for (auto* button : { &addFilesButton, &removeButton, &selectAllButton, &analyseButton,
                          &tracksViewButton, &matrixViewButton })
        addAndMakeVisible(*button);

    for (auto* button : { &groupButton, &sourceGroupButton, &clearGroupingButton,
                          &confirmSuggestionsButton, &applyTemplateButton })
        addAndMakeVisible(*button);

    bulkFamilySelector.addItem(utf8("Ustaw typ zaznaczonych…"), 1);
    for (const auto family : { analysis::TrackFamily::kick, analysis::TrackFamily::snare,
                              analysis::TrackFamily::cymbals, analysis::TrackFamily::toms,
                              analysis::TrackFamily::drumRoom, analysis::TrackFamily::bass,
                              analysis::TrackFamily::vocal, analysis::TrackFamily::guitar,
                              analysis::TrackFamily::keys, analysis::TrackFamily::effect,
                              analysis::TrackFamily::parallelDrums })
        bulkFamilySelector.addItem(familyName(family), familyItemId(family));
    bulkFamilySelector.addItem(familyName(analysis::TrackFamily::unknown), 100);
    bulkFamilySelector.setSelectedId(1, juce::dontSendNotification);
    bulkFamilySelector.setTooltip(utf8("Opcjonalnie: typ zmienia wyłącznie profil startowy kompresora. Nie steruje fazą ani maskowaniem."));
    bulkFamilySelector.onChange = [this]
    {
        const auto family = familyForItemId(bulkFamilySelector.getSelectedId());
        const auto selected = trackTable.getSelectedRows();
        if (family && !selected.isEmpty())
        {
            for (int index = 0; index < selected.size(); ++index)
                tracks[static_cast<std::size_t>(selected[index])].assignedFamily = family;
            invalidateResultsAfterMappingChange();
        }
        bulkFamilySelector.setSelectedId(1, juce::dontSendNotification);
    };
    addAndMakeVisible(bulkFamilySelector);

    bulkRoleSelector.addItem(utf8("Ustaw rolę zaznaczonych…"), 100);
    for (const auto role : { analysis::MixRole::focus, analysis::MixRole::foundation,
                            analysis::MixRole::support, analysis::MixRole::background,
                            analysis::MixRole::ignore })
        bulkRoleSelector.addItem(roleName(role), roleItemId(role));
    bulkRoleSelector.setSelectedId(100, juce::dontSendNotification);
    bulkRoleSelector.setTooltip(utf8("FOCUS jest chroniony. FUNDAMENT utrzymuje dół i puls. WSPARCIE oraz TŁO ustępują ważniejszym grupom."));
    bulkRoleSelector.onChange = [this]
    {
        const auto role = roleForItemId(bulkRoleSelector.getSelectedId());
        const auto selected = trackTable.getSelectedRows();
        if (role != analysis::MixRole::unassigned && !selected.isEmpty())
        {
            juce::StringArray affectedGroups;
            for (int index = 0; index < selected.size(); ++index)
            {
                const auto& track = tracks[static_cast<std::size_t>(selected[index])];
                if (!track.isReaperFolder && track.analysisGroup.isNotEmpty())
                    affectedGroups.addIfNotAlreadyThere(track.analysisGroup);
            }
            for (auto& track : tracks)
            {
                const auto directlySelected = selected.contains(
                    static_cast<int>(&track - tracks.data()));
                if (!track.isReaperFolder
                    && (directlySelected || (track.analysisGroup.isNotEmpty()
                    && affectedGroups.contains(track.analysisGroup))))
                    track.mixRole = role;
            }
            invalidateResultsAfterMappingChange();
        }
        bulkRoleSelector.setSelectedId(100, juce::dontSendNotification);
    };
    addAndMakeVisible(bulkRoleSelector);

    mappingHintLabel.setText(
        utf8("Nadaj grupom role i wybierz co najmniej jeden FOCUS. Typ doprecyzowuje kompresję; jawne Room i Parallel drums dostają też bezpieczny blend wewnątrz grupy."),
        juce::dontSendNotification);
    mappingHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    mappingHintLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(mappingHintLabel);

    mixTemplateSelector.addItem(utf8("Template ról…"), 1);
    mixTemplateSelector.addItem(utf8("Jackie Down the Line — Fontaines D.C."), 2);
    mixTemplateSelector.addItem(utf8("Następny template — wkrótce"), 3);
    mixTemplateSelector.setItemEnabled(3, false);
    mixTemplateSelector.setSelectedId(1, juce::dontSendNotification);
    mixTemplateSelector.setTooltip(utf8("Template wypełnia role na podstawie potwierdzonych typów i grup. Nie analizuje nazw ścieżek."));
    mixTemplateSelector.onChange = [this]
    {
        const auto jackieSelected = mixTemplateSelector.getSelectedId() == 2;
        if (!jackieSelected)
            activeMixTemplate.reset();
        applyTemplateButton.setEnabled(!analysisRunning && !tracks.empty() && jackieSelected);
        templateHintLabel.setText(
            jackieSelected
                ? utf8("Gitarowy post-punk: wokal z przodu, bas + rdzeń perkusji jako fundament, gitary we wsparciu, ambience/FX z tyłu.")
                : utf8("Wybierz punkt startowy ról. Każdą decyzję możesz potem zmienić ręcznie."),
            juce::dontSendNotification);
    };
    addAndMakeVisible(mixTemplateSelector);

    applyTemplateButton.setEnabled(false);
    applyTemplateButton.setTooltip(utf8("Ustawia role w całym projekcie. Zachowuje POMIŃ i nie zmienia typów ani grup."));

    templateHintLabel.setText(
        utf8("Wybierz punkt startowy ról. Każdą decyzję możesz potem zmienić ręcznie."),
        juce::dontSendNotification);
    templateHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    templateHintLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(templateHintLabel);

    analyseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    matrixViewButton.setEnabled(false);

    paletteSelector.addItem("Studio", 1);
    paletteSelector.addItem("Neon", 2);
    paletteSelector.addItem("Pastel", 3);
    paletteSelector.addItem("Czytelna", 4);
    paletteSelector.setSelectedId(1, juce::dontSendNotification);
    paletteSelector.onChange = [this] { applySelectedPalette(); };
    paletteSelector.setTooltip(utf8("Paleta kolorów ścieżek"));
    addAndMakeVisible(paletteSelector);

    workflowSelector.addItem(utf8("Początek miksu"), 1);
    workflowSelector.addItem(utf8("Gotowy miks — wkrótce"), 2);
    workflowSelector.setSelectedId(1, juce::dontSendNotification);
    workflowSelector.setTooltip(utf8("Etap pracy nad miksem"));
    workflowSelector.onChange = [this]
    {
        if (workflowSelector.getSelectedId() == 2)
        {
            workflowSelector.setSelectedId(1, juce::dontSendNotification);
            bridgeMessage = utf8("Tryb gotowego miksu powstanie później. Teraz pracujemy nad początkiem miksu.");
            updateStatus();
        }
    };
    addAndMakeVisible(workflowSelector);

    trackTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0b1220));
    trackTable.setMultipleSelectionEnabled(true);
    trackTable.setRowHeight(62);
    trackTable.getHeader().addColumn("", colourColumn, 42, 42, 42);
    trackTable.getHeader().addColumn(utf8("Ścieżka"), nameColumn, 260, 160, 600);
    trackTable.getHeader().addColumn(utf8("Typ — potwierdź"), trackFamilyColumn, 210, 170, 260);
    trackTable.getHeader().addColumn("Rola w miksie", mixRoleColumn, 155, 135, 190);
    trackTable.getHeader().addColumn("Grupa", analysisGroupColumn, 135, 100, 240);
    trackTable.getHeader().addColumn(utf8("Jedno źródło"), sourceGroupColumn, 145, 110, 240);
    trackTable.getHeader().addColumn("Czas", durationColumn, 80);
    trackTable.getHeader().addColumn(utf8("Próbkowanie"), sampleRateColumn, 105);
    trackTable.getHeader().addColumn(utf8("Kanały"), channelsColumn, 70);
    trackTable.getHeader().addColumn("RMS", rmsColumn, 90);
    trackTable.getHeader().addColumn("Peak", peakColumn, 90);
    trackTable.getHeader().addColumn("Korelacja L/R", stereoCorrelationColumn, 105);
    trackTable.getHeader().addColumn("Ramki", framesColumn, 70);
    trackTable.getHeader().addColumn("Dominanta", dominantFrequencyColumn, 100);
    trackTable.getHeader().addColumn("ERB / EQ", erbSpectrumColumn, 300, 220, 500);
    addAndMakeVisible(trackTable);

    pairAnalysisView.onPairSelected = [this](const std::size_t masker,
                                              const std::size_t target)
    {
        startPairDetail(masker, target);
    };
    pairAnalysisView.onApplySafePlan = [this] { requestReaperApplyPlan(); };
    pairAnalysisView.onUndoSafePlan = [this] { requestReaperUndoPlan(); };
    addChildComponent(pairAnalysisView);

    showTracksView();
    updateStatus();
    setSize(1480, 800);
    openFromCommandLine(commandLine);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (analysisThread.joinable())
    {
        analysisThread.request_stop();
        analysisThread.join();
    }

    if (pairDetailThread.joinable())
    {
        pairDetailThread.request_stop();
        pairDetailThread.join();
    }
}

void MainComponent::openFromCommandLine(const juce::String& commandLine)
{
    if (const auto manifest = findReaperProjectManifest(commandLine))
        importReaperManifest(*manifest);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff111827));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(24);
    titleLabel.setBounds(area.removeFromTop(48));
    descriptionLabel.setBounds(area.removeFromTop(34));

    auto buttons = area.removeFromTop(42);
    addFilesButton.setBounds(buttons.removeFromLeft(150));
    buttons.removeFromLeft(8);
    removeButton.setBounds(buttons.removeFromLeft(160));
    buttons.removeFromLeft(8);
    selectAllButton.setBounds(buttons.removeFromLeft(150));
    buttons.removeFromLeft(18);
    analyseButton.setBounds(buttons.removeFromLeft(150));
    buttons.removeFromLeft(16);
    tracksViewButton.setBounds(buttons.removeFromLeft(95));
    buttons.removeFromLeft(6);
    matrixViewButton.setBounds(buttons.removeFromLeft(95));
    buttons.removeFromLeft(12);
    workflowSelector.setBounds(buttons.removeFromLeft(190));
    buttons.removeFromLeft(8);
    paletteSelector.setBounds(buttons.removeFromLeft(135));

    area.removeFromTop(12);
    if (trackTable.isVisible())
    {
        auto mappingBar = area.removeFromTop(38);
        groupButton.setBounds(mappingBar.removeFromLeft(105));
        mappingBar.removeFromLeft(6);
        sourceGroupButton.setBounds(mappingBar.removeFromLeft(145));
        mappingBar.removeFromLeft(6);
        clearGroupingButton.setBounds(mappingBar.removeFromLeft(105));
        mappingBar.removeFromLeft(8);
        bulkRoleSelector.setBounds(mappingBar.removeFromLeft(205));
        mappingBar.removeFromLeft(8);
        bulkFamilySelector.setBounds(mappingBar.removeFromLeft(185));
        mappingBar.removeFromLeft(8);
        confirmSuggestionsButton.setBounds(mappingBar.removeFromLeft(245));
        mappingBar.removeFromLeft(12);
        mappingHintLabel.setBounds(mappingBar);

        area.removeFromTop(8);
        auto templateBar = area.removeFromTop(38);
        mixTemplateSelector.setBounds(templateBar.removeFromLeft(360));
        templateBar.removeFromLeft(8);
        applyTemplateButton.setBounds(templateBar.removeFromLeft(190));
        templateBar.removeFromLeft(12);
        templateHintLabel.setBounds(templateBar);
        area.removeFromTop(8);
    }
    statusLabel.setBounds(area.removeFromBottom(36));
    area.removeFromBottom(8);
    trackTable.setBounds(area);
    pairAnalysisView.setBounds(area);
}

int MainComponent::getNumRows()
{
    return static_cast<int>(tracks.size());
}

void MainComponent::paintRowBackground(juce::Graphics& graphics,
                                       int rowNumber,
                                       int,
                                       int,
                                       const bool rowIsSelected)
{
    if (rowIsSelected)
        graphics.fillAll(juce::Colour(0xff1d4ed8).withAlpha(0.55f));
    else if (rowNumber % 2 != 0)
        graphics.fillAll(juce::Colour(0xff111827));
}

void MainComponent::paintCell(juce::Graphics& graphics,
                              const int rowNumber,
                              const int columnId,
                              const int width,
                              const int height,
                              bool)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return;

    const auto& track = tracks[static_cast<std::size_t>(rowNumber)];

    if (columnId == colourColumn)
    {
        graphics.setColour(track.colour.withAlpha(0.20f));
        graphics.fillEllipse(7.0f,
                             static_cast<float>(height / 2 - 13),
                             26.0f,
                             26.0f);
        graphics.setColour(track.colour);
        graphics.fillEllipse(11.0f,
                             static_cast<float>(height / 2 - 9),
                             18.0f,
                             18.0f);
        return;
    }

    if (columnId == stereoCorrelationColumn)
    {
        if (track.channels < 2)
        {
            graphics.setColour(juce::Colour(0xff94a3b8));
            graphics.drawText("mono", 5, 0, width - 10, height, juce::Justification::centred);
            return;
        }

        if (!track.stereoCorrelation)
        {
            graphics.setColour(juce::Colour(0xff94a3b8));
            graphics.drawText(utf8("—"), 5, 0, width - 10, height, juce::Justification::centred);
            return;
        }

        const auto correlation = juce::jlimit(-1.0f, 1.0f, *track.stereoCorrelation);
        auto meter = juce::Rectangle<float>(7.0f,
                                            static_cast<float>(height / 2 - 7),
                                            static_cast<float>(width - 14),
                                            14.0f);
        graphics.setColour(juce::Colour(0xff0f172a));
        graphics.fillRoundedRectangle(meter, 4.0f);
        graphics.setColour(juce::Colour(0xff475569));
        graphics.drawVerticalLine(static_cast<int>(meter.getCentreX()),
                                  meter.getY(), meter.getBottom());
        const auto markerX = juce::jmap(correlation, -1.0f, 1.0f,
                                       meter.getX(), meter.getRight());
        graphics.setColour(correlation < 0.0f ? juce::Colour(0xfff87171)
                                               : juce::Colour(0xff6ee7b7));
        graphics.fillEllipse(markerX - 4.0f, meter.getCentreY() - 4.0f, 8.0f, 8.0f);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(11.0f));
        graphics.drawText(juce::String(correlation, 2),
                          meter.toNearestInt(),
                          juce::Justification::centred);
        return;
    }

    if (columnId == erbSpectrumColumn)
    {
        auto plot = juce::Rectangle<float>(0.0f,
                                           0.0f,
                                           static_cast<float>(width),
                                           static_cast<float>(height)).reduced(6.0f, 5.0f);
        graphics.setColour(juce::Colour(0xff07101f));
        graphics.fillRoundedRectangle(plot, 5.0f);
        graphics.setColour(juce::Colour(0xff334155));
        graphics.drawRoundedRectangle(plot, 5.0f, 1.0f);

        for (int line = 1; line < 4; ++line)
        {
            const auto y = plot.getY() + plot.getHeight() * static_cast<float>(line) / 4.0f;
            graphics.setColour(juce::Colour(0xff1e293b));
            graphics.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        }

        if (!track.erbAverageDb)
        {
            graphics.setColour(juce::Colour(0xff64748b));
            graphics.drawText(utf8("Analizuj, aby zobaczyć widmo"),
                              plot.toNearestInt(),
                              juce::Justification::centred);
            return;
        }

        juce::Path curve;
        const auto& bands = *track.erbAverageDb;
        for (std::size_t band = 0; band < bands.size(); ++band)
        {
            const auto x = juce::jmap(static_cast<float>(band),
                                     0.0f,
                                     static_cast<float>(bands.size() - 1),
                                     plot.getX(),
                                     plot.getRight());
            const auto level = juce::jlimit(-80.0f, 0.0f, bands[band]);
            const auto y = juce::jmap(level, -80.0f, 0.0f, plot.getBottom(), plot.getY());
            if (band == 0)
                curve.startNewSubPath(x, y);
            else
                curve.lineTo(x, y);
        }

        auto fill = curve;
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.lineTo(plot.getX(), plot.getBottom());
        fill.closeSubPath();
        graphics.setColour(juce::Colour(0xff22d3a7).withAlpha(0.20f));
        graphics.fillPath(fill);
        graphics.setColour(juce::Colour(0xff6ee7b7));
        graphics.strokePath(curve, juce::PathStrokeType(1.8f));
        return;
    }

    juce::String text;

    switch (columnId)
    {
        case nameColumn:
            text = track.isReaperFolder ? utf8("GRUPA  •  ") + track.name
                : track.analysisGroup.isNotEmpty() ? utf8("↳  ") + track.name
                                                   : track.name;
            break;
        case durationColumn: text = formatDuration(track.durationSeconds); break;
        case sampleRateColumn: text = juce::String(track.sampleRate / 1000.0, 1) + " kHz"; break;
        case channelsColumn: text = juce::String(track.channels); break;
        case rmsColumn:
            text = track.rmsDb ? juce::String(*track.rmsDb, 1) + " dB" : utf8("—");
            break;
        case peakColumn:
            text = track.peakDb ? juce::String(*track.peakDb, 1) + " dB" : utf8("—");
            break;
        case framesColumn:
            text = track.stftFrames > 0 ? juce::String(static_cast<int>(track.stftFrames)) : utf8("—");
            break;
        case dominantFrequencyColumn:
            text = track.dominantFrequencyHz
                 ? juce::String(*track.dominantFrequencyHz, 0) + " Hz"
                 : utf8("—");
            break;
        default: break;
    }

    graphics.setColour(track.error.isEmpty() ? juce::Colours::white : juce::Colour(0xfff87171));
    graphics.setFont(juce::FontOptions(14.0f));
    graphics.drawText(text, 8, 0, width - 12, height, juce::Justification::centredLeft, true);
}

juce::Component* MainComponent::refreshComponentForCell(
    const int rowNumber,
    const int columnId,
    bool,
    juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return nullptr;

    auto& track = tracks[static_cast<std::size_t>(rowNumber)];
    if (columnId == trackFamilyColumn)
    {
        auto* selector = dynamic_cast<TrackFamilySelector*>(existingComponentToUpdate);
        if (selector == nullptr)
        {
            delete existingComponentToUpdate;
            selector = new TrackFamilySelector();
            selector->familyChanged = [this](const int row,
                                              const std::optional<analysis::TrackFamily> family)
            {
                setTrackFamily(row, family);
            };
        }
        const auto suggestion = analysis::classifyTrackFamily(track.name.toStdString(),
                                                               track.reaperFolderPath.toStdString());
        selector->configure(rowNumber, suggestion, track.assignedFamily);
        return selector;
    }

    if (columnId == mixRoleColumn)
    {
        auto* selector = dynamic_cast<MixRoleSelector*>(existingComponentToUpdate);
        if (selector == nullptr)
        {
            delete existingComponentToUpdate;
            selector = new MixRoleSelector();
            selector->roleChanged = [this](const int row, const analysis::MixRole role)
            {
                setTrackRole(row, role);
            };
        }
        selector->configure(rowNumber, track.mixRole);
        return selector;
    }

    if (columnId == analysisGroupColumn || columnId == sourceGroupColumn)
    {
        auto* label = dynamic_cast<MappingNameLabel*>(existingComponentToUpdate);
        if (label == nullptr)
        {
            delete existingComponentToUpdate;
            label = new MappingNameLabel();
            label->nameChanged = [this](const int row,
                                        const bool source,
                                        const juce::String& value)
            {
                renameMappingGroup(row, source, value);
            };
        }
        const auto source = columnId == sourceGroupColumn;
        label->configure(rowNumber, source, source ? track.sourceGroup : track.analysisGroup);
        return label;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void MainComponent::cellClicked(const int rowNumber,
                                const int columnId,
                                const juce::MouseEvent&)
{
    if (columnId != colourColumn
        || rowNumber < 0
        || rowNumber >= static_cast<int>(tracks.size()))
        return;

    auto& track = tracks[static_cast<std::size_t>(rowNumber)];
    const auto& palette = paletteForId(paletteSelector.getSelectedId());
    auto current = std::find(palette.begin(), palette.end(), track.colour);
    const auto next = current == palette.end()
        ? static_cast<std::size_t>(rowNumber) % palette.size()
        : (static_cast<std::size_t>(std::distance(palette.begin(), current)) + 1) % palette.size();
    track.colour = palette[next];
    trackTable.repaintRow(rowNumber);
    refreshMatrixColours();
}

void MainComponent::selectedRowsChanged(int)
{
    updateStatus();
}

void MainComponent::groupSelectedTracks()
{
    const auto selected = trackTable.getSelectedRows();
    std::vector<int> usableRows;
    for (int index = 0; index < selected.size(); ++index)
    {
        const auto row = selected[index];
        if (!tracks[static_cast<std::size_t>(row)].isReaperFolder)
            usableRows.push_back(row);
    }
    if (usableRows.size() < 2)
    {
        bridgeMessage = utf8("Zaznacz co najmniej dwie ścieżki do zgrupowania.");
        updateStatus();
        return;
    }

    auto number = 1;
    juce::String name;
    do
    {
        name = "Grupa " + juce::String(number++);
    }
    while (std::ranges::any_of(tracks, [&name](const TrackEntry& track)
    {
        return track.analysisGroup == name;
    }));

    const auto sharedRole = tracks[static_cast<std::size_t>(usableRows.front())].mixRole;
    const auto rolesAgree = std::ranges::all_of(usableRows, [this, sharedRole](const int row)
    {
        return tracks[static_cast<std::size_t>(row)].mixRole == sharedRole;
    });
    for (const auto row : usableRows)
    {
        auto& track = tracks[static_cast<std::size_t>(row)];
        track.analysisGroup = name;
        if (!rolesAgree)
            track.mixRole = analysis::MixRole::unassigned;
    }
    invalidateResultsAfterMappingChange();
}

void MainComponent::joinSelectedAsOneSource()
{
    const auto selected = trackTable.getSelectedRows();
    std::vector<int> usableRows;
    for (int index = 0; index < selected.size(); ++index)
    {
        const auto row = selected[index];
        if (!tracks[static_cast<std::size_t>(row)].isReaperFolder)
            usableRows.push_back(row);
    }
    if (usableRows.size() < 2)
    {
        bridgeMessage = utf8("Zaznacz co najmniej dwie zwykłe ścieżki reprezentujące mikrofony tego samego źródła.");
        updateStatus();
        return;
    }

    auto number = 1;
    juce::String name;
    do
    {
        name = utf8("Źródło ") + juce::String(number++);
    }
    while (std::ranges::any_of(tracks, [&name](const TrackEntry& track)
    {
        return track.sourceGroup == name;
    }));

    for (const auto row : usableRows)
    {
        auto& track = tracks[static_cast<std::size_t>(row)];
        track.sourceGroup = name;
        if (track.analysisGroup.isEmpty())
            track.analysisGroup = name;
    }
    const auto sharedRole = tracks[static_cast<std::size_t>(usableRows.front())].mixRole;
    const auto rolesAgree = std::ranges::all_of(usableRows, [this, sharedRole](const int row)
    {
        return tracks[static_cast<std::size_t>(row)].mixRole == sharedRole;
    });
    if (!rolesAgree)
    {
        for (const auto row : usableRows)
            tracks[static_cast<std::size_t>(row)].mixRole = analysis::MixRole::unassigned;
    }
    invalidateResultsAfterMappingChange();
}

void MainComponent::clearSelectedGrouping()
{
    const auto selected = trackTable.getSelectedRows();
    if (selected.isEmpty())
        return;
    for (int index = 0; index < selected.size(); ++index)
    {
        auto& track = tracks[static_cast<std::size_t>(selected[index])];
        track.analysisGroup.clear();
        track.sourceGroup.clear();
    }
    invalidateResultsAfterMappingChange();
}

void MainComponent::confirmSuggestedFamilies()
{
    const auto selected = trackTable.getSelectedRows();
    auto changed = false;
    for (int row = 0; row < static_cast<int>(tracks.size()); ++row)
    {
        if (!selected.isEmpty() && !selected.contains(row))
            continue;
        auto& track = tracks[static_cast<std::size_t>(row)];
        if (track.isReaperFolder)
            continue;
        if (!track.assignedFamily)
        {
            track.assignedFamily = analysis::TrackFamily::unknown;
            changed = true;
        }
        if (track.mixRole == analysis::MixRole::unassigned)
        {
            track.mixRole = analysis::MixRole::support;
            changed = true;
        }
    }
    if (changed)
    {
        invalidateResultsAfterMappingChange();
        bridgeMessage = utf8("Braki ustawiono neutralnie jako INNA / WSPARCIE — bez zgadywania z nazw. Teraz wybierz grupę FOCUS.");
        updateStatus();
    }
}

void MainComponent::applySelectedMixTemplate()
{
    if (mixTemplateSelector.getSelectedId() != 2)
        return;

    std::vector<analysis::MixTemplateTrackInput> inputs;
    std::vector<std::size_t> trackIndices;
    inputs.reserve(tracks.size());
    trackIndices.reserve(tracks.size());
    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        const auto& track = tracks[index];
        if (track.isReaperFolder)
            continue;
        inputs.push_back({ track.analysisGroup.toStdString(),
                           track.assignedFamily.value_or(analysis::TrackFamily::unknown),
                           track.mixRole });
        trackIndices.push_back(index);
    }

    const auto recommendation = analysis::MixTemplateAdvisor{}.recommend(
        analysis::MixTemplateId::jackieDownTheLine, inputs);
    if (!recommendation.canApply)
    {
        bridgeMessage = utf8("Template nie zgaduje z nazw. Najpierw potwierdź typ WOKAL albo GITARA dla co najmniej jednej grupy.");
        updateStatus();
        return;
    }

    for (std::size_t index = 0; index < trackIndices.size(); ++index)
        tracks[trackIndices[index]].mixRole = recommendation.roles[index];

    invalidateResultsAfterMappingChange();
    activeMixTemplate = analysis::MixTemplateId::jackieDownTheLine;
    bridgeMessage = utf8("Zastosowano template „Jackie Down the Line”: sprawdź role i kliknij ANALIZUJ. To punkt startowy, nie kopia miksu.");
    updateStatus();
}

void MainComponent::setTrackFamily(const int rowNumber,
                                   const std::optional<analysis::TrackFamily> family)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return;
    if (tracks[static_cast<std::size_t>(rowNumber)].assignedFamily == family)
        return;
    tracks[static_cast<std::size_t>(rowNumber)].assignedFamily = family;
    invalidateResultsAfterMappingChange();
}

void MainComponent::setTrackRole(const int rowNumber, const analysis::MixRole role)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return;
    const auto group = tracks[static_cast<std::size_t>(rowNumber)].analysisGroup;
    auto changed = false;
    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        auto& track = tracks[index];
        const auto sameGroup = group.isNotEmpty() && track.analysisGroup == group;
        if ((static_cast<int>(index) == rowNumber || sameGroup) && track.mixRole != role)
        {
            track.mixRole = role;
            changed = true;
        }
    }
    if (changed)
        invalidateResultsAfterMappingChange();
}

void MainComponent::renameMappingGroup(const int rowNumber,
                                       const bool source,
                                       const juce::String& newName)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(tracks.size()))
        return;
    auto& selectedTrack = tracks[static_cast<std::size_t>(rowNumber)];
    const auto oldName = source ? selectedTrack.sourceGroup : selectedTrack.analysisGroup;
    const auto cleanedName = newName.trim();
    if (oldName == cleanedName)
        return;

    for (auto& track : tracks)
    {
        auto& value = source ? track.sourceGroup : track.analysisGroup;
        if ((!oldName.isEmpty() && value == oldName) || (&track == &selectedTrack))
            value = cleanedName;
        if (source && !oldName.isEmpty() && track.analysisGroup == oldName)
            track.analysisGroup = cleanedName;
    }
    invalidateResultsAfterMappingChange();
}

void MainComponent::invalidateResultsAfterMappingChange()
{
    matrixAvailable = false;
    activeMatrixPaths.clear();
    currentMixBalance.reset();
    currentPhasePairs.clear();
    currentMaskingActions.clear();
    currentHasMaskingIssues = false;
    currentCompressionActions.clear();
    currentStereoWidthAction.reset();
    planAppliedInReaper = false;
    matrixViewButton.setEnabled(false);
    bridgeMessage = utf8("Mapowanie projektu zmienione. Kliknij ANALIZUJ, aby przeliczyć wyniki.");
    saveTrackMapping();
    trackTable.updateContent();
    trackTable.repaint();
    showTracksView();
    updateStatus();
}

juce::File MainComponent::mappingFile() const
{
    juce::String key;
    if (reaperBridge && reaperBridge->projectPath.isNotEmpty())
        key = "reaper:" + reaperBridge->projectPath;
    else if (!tracks.empty())
    {
        juce::StringArray directories;
        for (const auto& track : tracks)
        {
            if (track.file.getFullPathName().isNotEmpty())
                directories.addIfNotAlreadyThere(track.file.getParentDirectory().getFullPathName());
        }
        directories.sort(true);
        key = "files:" + directories.joinIntoString("|");
    }

    if (key.isEmpty())
        return {};
    const auto hash = juce::String::toHexString(static_cast<juce::int64>(key.hashCode64()));
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MixDebugger")
        .getChildFile("Mappings")
        .getChildFile(hash + ".json");
}

void MainComponent::saveTrackMapping() const
{
    const auto file = mappingFile();
    if (file.getFullPathName().isEmpty())
        return;
    file.getParentDirectory().createDirectory();

    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", 2);
    juce::Array<juce::var> entries;
    for (const auto& track : tracks)
    {
        auto* entry = new juce::DynamicObject();
        const auto id = track.fromReaper ? track.reaperGuid : track.file.getFullPathName();
        entry->setProperty("id", id);
        entry->setProperty("family", track.assignedFamily
            ? static_cast<int>(*track.assignedFamily) : -1);
        entry->setProperty("role", static_cast<int>(track.mixRole));
        entry->setProperty("group", track.analysisGroup);
        entry->setProperty("source", track.sourceGroup);
        entries.add(juce::var(entry));
    }
    root->setProperty("tracks", juce::var(entries));
    file.replaceWithText(juce::JSON::toString(juce::var(root), true), false, false, "\n");
}

void MainComponent::loadTrackMapping()
{
    const auto file = mappingFile();
    if (!file.existsAsFile())
        return;
    auto parsed = juce::JSON::parse(file);
    auto* root = parsed.getDynamicObject();
    const auto schemaVersion = root != nullptr
        ? static_cast<int>(root->getProperty("schemaVersion")) : 0;
    if (root == nullptr || (schemaVersion != 1 && schemaVersion != 2))
        return;
    auto* entries = root->getProperty("tracks").getArray();
    if (entries == nullptr)
        return;

    for (const auto& value : *entries)
    {
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            continue;
        const auto id = object->getProperty("id").toString();
        const auto track = std::ranges::find_if(tracks, [&id](const TrackEntry& candidate)
        {
            return (candidate.fromReaper ? candidate.reaperGuid
                                         : candidate.file.getFullPathName()) == id;
        });
        if (track == tracks.end())
            continue;
        const auto family = static_cast<int>(object->getProperty("family"));
        track->assignedFamily = family >= static_cast<int>(analysis::TrackFamily::unknown)
                             && family <= static_cast<int>(analysis::TrackFamily::parallelDrums)
            ? std::optional<analysis::TrackFamily>(static_cast<analysis::TrackFamily>(family))
            : std::nullopt;
        track->analysisGroup = object->getProperty("group").toString();
        track->sourceGroup = object->getProperty("source").toString();
        if (schemaVersion >= 2)
        {
            const auto role = static_cast<int>(object->getProperty("role"));
            if (role >= static_cast<int>(analysis::MixRole::unassigned)
                && role <= static_cast<int>(analysis::MixRole::ignore))
                track->mixRole = static_cast<analysis::MixRole>(role);
        }
    }
}

void MainComponent::chooseFiles()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        utf8("Wybierz ścieżki audio"),
        juce::File{},
        "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync(chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent>(this)]
                                             (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->addFiles(chooser.getResults());
        safeThis->fileChooser.reset();
    });
}

void MainComponent::addFiles(const juce::Array<juce::File>& files)
{
    const auto firstNewRow = static_cast<int>(tracks.size());

    for (const auto& file : files)
    {
        const auto duplicate = std::ranges::any_of(tracks, [&file](const TrackEntry& track)
        {
            return track.file.existsAsFile() && track.file == file;
        });

        if (duplicate)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr || reader->sampleRate <= 0.0)
            continue;

        TrackEntry entry;
        entry.file = file;
        entry.name = file.getFileNameWithoutExtension();
        entry.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        entry.sampleRate = reader->sampleRate;
        entry.channels = static_cast<int>(reader->numChannels);
        const auto& palette = paletteForId(paletteSelector.getSelectedId());
        entry.colour = palette[tracks.size() % palette.size()];
        tracks.push_back(std::move(entry));
    }

    loadTrackMapping();
    trackTable.updateContent();
    if (static_cast<int>(tracks.size()) > firstNewRow)
        trackTable.selectRangeOfRows(firstNewRow, static_cast<int>(tracks.size()) - 1);
    updateStatus();
}

void MainComponent::removeSelectedTracks()
{
    auto selected = trackTable.getSelectedRows();
    for (int index = selected.size() - 1; index >= 0; --index)
        tracks.erase(tracks.begin() + selected[index]);

    trackTable.deselectAllRows();
    saveTrackMapping();
    trackTable.updateContent();
    matrixAvailable = false;
    activeMatrixPaths.clear();
    matrixViewButton.setEnabled(false);
    showTracksView();
    updateStatus();
}

void MainComponent::selectAllTracks()
{
    if (!tracks.empty())
        trackTable.selectRangeOfRows(0, static_cast<int>(tracks.size()) - 1);
}

void MainComponent::startAnalysis()
{
    if (analysisRunning || tracks.empty())
        return;

    auto selected = trackTable.getSelectedRows();
    std::vector<bool> included(tracks.size(), selected.isEmpty());
    if (!selected.isEmpty())
    {
        for (int index = 0; index < selected.size(); ++index)
            included[static_cast<std::size_t>(selected[index])] = true;

        auto changed = true;
        while (changed)
        {
            changed = false;
            for (std::size_t index = 0; index < tracks.size(); ++index)
            {
                if (!included[index] || tracks[index].isReaperFolder)
                    continue;
                for (std::size_t candidate = 0; candidate < tracks.size(); ++candidate)
                {
                    if (included[candidate] || tracks[candidate].isReaperFolder)
                        continue;
                    const auto sameMixGroup = tracks[index].analysisGroup.isNotEmpty()
                        && tracks[index].analysisGroup == tracks[candidate].analysisGroup;
                    const auto sameSource = tracks[index].sourceGroup.isNotEmpty()
                        && tracks[index].sourceGroup == tracks[candidate].sourceGroup;
                    if (sameMixGroup || sameSource)
                    {
                        included[candidate] = true;
                        changed = true;
                    }
                }
            }
        }
    }
    auto unassignedRoleCount = 0;
    auto focusCount = 0;
    auto inconsistentGroupRoles = false;
    std::map<std::string, analysis::MixRole> rolePerGroup;
    for (int row = 0; row < static_cast<int>(tracks.size()); ++row)
    {
        if (!included[static_cast<std::size_t>(row)])
            continue;
        const auto& track = tracks[static_cast<std::size_t>(row)];
        if (track.isReaperFolder)
            continue;
        if (track.mixRole == analysis::MixRole::unassigned)
            ++unassignedRoleCount;
        if (track.mixRole == analysis::MixRole::focus)
            ++focusCount;
        if (track.analysisGroup.isNotEmpty())
        {
            const auto key = track.analysisGroup.toStdString();
            const auto [entry, inserted] = rolePerGroup.emplace(key, track.mixRole);
            if (!inserted && entry->second != track.mixRole)
                inconsistentGroupRoles = true;
        }
    }
    if (unassignedRoleCount > 0)
    {
        bridgeMessage = utf8("Najpierw ustaw role analizowanych ścieżek. Pozostało bez roli: ")
                      + juce::String(unassignedRoleCount)
                      + utf8(". Możesz zaznaczyć wiele ścieżek i użyć pola Ustaw rolę.");
        showTracksView();
        updateStatus();
        return;
    }
    if (inconsistentGroupRoles)
    {
        bridgeMessage = utf8("Jedna grupa ma różne role. Zaznacz dowolną ścieżkę tej grupy i ustaw wspólną rolę.");
        showTracksView();
        updateStatus();
        return;
    }
    if (focusCount == 0)
    {
        bridgeMessage = utf8("Wybierz co najmniej jedną grupę FOCUS — element, który ma pozostać najbardziej czytelny.");
        showTracksView();
        updateStatus();
        return;
    }

    std::vector<juce::File> files;
    std::vector<juce::String> reaperGuids;

    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        const auto& track = tracks[index];
        if (!included[index] || track.isReaperFolder
            || track.mixRole == analysis::MixRole::ignore)
            continue;
        files.push_back(track.file);
        if (track.fromReaper)
            reaperGuids.push_back(track.reaperGuid);
    }

    if (!reaperGuids.empty() && !usePreparedReaperFilesOnce)
    {
        requestReaperRender(reaperGuids);
        return;
    }

    const auto validateRenderedAudio = usePreparedReaperFilesOnce;
    usePreparedReaperFilesOnce = false;
    files.erase(std::remove_if(files.begin(), files.end(), [](const juce::File& file)
    {
        return !file.existsAsFile();
    }), files.end());
    if (files.empty())
    {
        bridgeMessage = utf8("Brak plików audio gotowych do analizy.");
        updateStatus();
        return;
    }

    std::optional<juce::File> masterFile;
    if (validateRenderedAudio && reaperBridge && reaperBridge->masterFile.existsAsFile())
        masterFile = reaperBridge->masterFile;
    beginFileAnalysis(std::move(files), validateRenderedAudio, std::move(masterFile));
}

void MainComponent::beginFileAnalysis(std::vector<juce::File> files,
                                      const bool validateRenderedAudio,
                                      std::optional<juce::File> masterFile)
{
    std::vector<juce::String> sourceGroups;
    std::vector<juce::String> analysisGroups;
    std::vector<analysis::MixRole> roles;
    std::vector<float> ancestorFolderGainsDb;
    std::vector<analysis::GroupBlendTrackInput> blendInputs;
    sourceGroups.reserve(files.size());
    analysisGroups.reserve(files.size());
    roles.reserve(files.size());
    ancestorFolderGainsDb.reserve(files.size());
    blendInputs.reserve(files.size());
    for (const auto& file : files)
    {
        const auto track = std::ranges::find_if(tracks, [&file](const TrackEntry& entry)
        {
            return entry.file.getFullPathName() == file.getFullPathName();
        });
        sourceGroups.push_back(track != tracks.end() ? track->sourceGroup : juce::String());
        analysisGroups.push_back(track != tracks.end() ? track->analysisGroup : juce::String());
        roles.push_back(track != tracks.end() ? track->mixRole
                                              : analysis::MixRole::unassigned);
        ancestorFolderGainsDb.push_back(track != tracks.end()
            ? track->ancestorFolderGainDb : 0.0f);
        blendInputs.push_back({
            track != tracks.end() ? track->analysisGroup.toStdString() : std::string(),
            track != tracks.end()
                ? track->assignedFamily.value_or(analysis::TrackFamily::unknown)
                : analysis::TrackFamily::unknown
        });
    }
    auto balanceInternalTrimsDb
        = analysis::GroupBlendAdvisor{}.recommendInternalTrimsDb(blendInputs);

    bridgeMessage = utf8("Analiza pełnych ścieżek trwa…");
    setAnalysisRunning(true);
    analysisThread = std::jthread(
        [safeThis = juce::Component::SafePointer<MainComponent>(this),
         files = std::move(files),
         sourceGroups = std::move(sourceGroups),
         analysisGroups = std::move(analysisGroups),
         roles = std::move(roles),
         ancestorFolderGainsDb = std::move(ancestorFolderGainsDb),
         balanceInternalTrimsDb = std::move(balanceInternalTrimsDb),
         masterFile = std::move(masterFile),
         validateRenderedAudio]
        (const std::stop_token stopToken) mutable
        {
            std::vector<AnalysisResult> results;
            results.reserve(files.size());

            for (std::size_t index = 0; index < files.size(); ++index)
            {
                if (stopToken.stop_requested())
                    return;
                results.push_back(analyseFile(files[index], stopToken,
                    sourceGroups[index].isNotEmpty(), ancestorFolderGainsDb[index]));
                results.back().balanceInternalTrimDb = balanceInternalTrimsDb[index];
            }

            std::map<std::string, std::vector<std::size_t>> groupedIndices;
            for (std::size_t index = 0; index < analysisGroups.size(); ++index)
            {
                if (analysisGroups[index].isNotEmpty())
                    groupedIndices[analysisGroups[index].toStdString()].push_back(index);
            }
            for (const auto& [group, indices] : groupedIndices)
            {
                (void) group;
                if (indices.size() < 2 || stopToken.stop_requested())
                    continue;
                std::vector<juce::File> groupFiles;
                std::vector<float> groupInputGainsDb;
                groupFiles.reserve(indices.size());
                groupInputGainsDb.reserve(indices.size());
                for (const auto index : indices)
                {
                    groupFiles.push_back(files[index]);
                    groupInputGainsDb.push_back(
                        ancestorFolderGainsDb[index] + balanceInternalTrimsDb[index]);
                }
                std::optional<float> groupPeakDb;
                const auto groupDynamics = analyseAlignedGroup(
                    groupFiles, groupInputGainsDb, stopToken, groupPeakDb);
                if (groupDynamics.activeWindowCount < 5 || !groupPeakDb)
                    continue;
                for (const auto index : indices)
                {
                    results[index].measuredGroupActiveRmsDb = groupDynamics.activeRmsDb;
                    results[index].measuredGroupPeakDb = groupPeakDb;
                    results[index].measuredGroupActiveWindowCount
                        = groupDynamics.activeWindowCount;
                }
            }

            std::vector<std::optional<float>> peakLevels;
            peakLevels.reserve(results.size());
            for (const auto& result : results)
                peakLevels.push_back(result.peakDb);
            const auto rejectedSilentRender = validateRenderedAudio
                && renderedAudioIsSuspiciouslySilent(peakLevels);

            if (rejectedSilentRender)
            {
                juce::MessageManager::callAsync(
                    [safeThis, results = std::move(results)]() mutable
                    {
                        if (safeThis != nullptr)
                            safeThis->finishAnalysis(std::move(results), {}, {}, {}, {},
                                                     std::nullopt, std::nullopt, true);
                    });
                return;
            }

            std::optional<AnalysisResult> masterResult;
            if (masterFile && masterFile->existsAsFile() && !stopToken.stop_requested())
                masterResult = analyseFile(*masterFile, stopToken, false);

            std::vector<juce::String> matrixPaths;
            std::vector<std::shared_ptr<const analysis::TrackFeatures>> matrixFeatures;
            std::vector<std::size_t> matrixResultIndices;
            std::vector<juce::String> matrixSourceGroups;
            std::vector<analysis::MixRole> matrixRoles;
            for (std::size_t index = 0; index < results.size(); ++index)
            {
                const auto& result = results[index];
                if (result.features)
                {
                    matrixPaths.push_back(result.path);
                    matrixFeatures.push_back(result.features);
                    matrixResultIndices.push_back(index);
                    matrixSourceGroups.push_back(sourceGroups[index]);
                    matrixRoles.push_back(roles[index]);
                }
            }

            // Najpierw wyliczamy wyłącznie względne ruchy grup. Następnie jeszcze
            // na wątku roboczym sumujemy wyrównane stemy próbka po próbce z tymi
            // gainami. Dopiero dokładnie przewidziany peak trafia do etapu
            // wspólnego trimu gwarantującego docelowe -6 dBFS.
            std::optional<float> predictedPreTrimMasterPeakDb;
            if (masterResult && masterResult->peakDb && !stopToken.stop_requested())
            {
                std::vector<analysis::BalanceTrackInput> preliminaryInputs;
                preliminaryInputs.reserve(matrixResultIndices.size());
                for (std::size_t matrixIndex = 0;
                     matrixIndex < matrixResultIndices.size(); ++matrixIndex)
                {
                    const auto resultIndex = matrixResultIndices[matrixIndex];
                    const auto& result = results[resultIndex];
                    preliminaryInputs.push_back({
                        analysisGroups[resultIndex].toStdString(),
                        matrixRoles[matrixIndex],
                        result.dynamics.activeRmsDb,
                        result.peakDb.value_or(-100.0f),
                        result.dynamics.activeWindowCount,
                        result.measuredGroupActiveRmsDb,
                        result.measuredGroupPeakDb,
                        result.measuredGroupActiveWindowCount,
                        balanceInternalTrimsDb[resultIndex]
                    });
                }
                const auto preliminary = analysis::BalanceAdvisor{}.recommend(
                    preliminaryInputs);
                if (preliminary.available
                    && preliminary.tracks.size() == matrixResultIndices.size())
                {
                    std::vector<juce::File> matrixFiles;
                    std::vector<float> matrixBaseGainsDb;
                    std::vector<float> relativeGainsDb;
                    matrixFiles.reserve(matrixResultIndices.size());
                    matrixBaseGainsDb.reserve(matrixResultIndices.size());
                    relativeGainsDb.reserve(matrixResultIndices.size());
                    for (std::size_t matrixIndex = 0;
                         matrixIndex < matrixResultIndices.size(); ++matrixIndex)
                    {
                        matrixFiles.push_back(files[matrixResultIndices[matrixIndex]]);
                        matrixBaseGainsDb.push_back(
                            ancestorFolderGainsDb[matrixResultIndices[matrixIndex]]);
                        relativeGainsDb.push_back(
                            preliminary.tracks[matrixIndex].gainDb);
                    }
                    predictedPreTrimMasterPeakDb = predictPreTrimMasterPeak(
                        matrixFiles, matrixBaseGainsDb, relativeGainsDb,
                        *masterResult->peakDb, stopToken);
                }
            }

            std::vector<analysis::PairSummary> summaries(matrixFeatures.size()
                                                         * matrixFeatures.size());
            const analysis::PairAnalyzer pairAnalyzer;
            for (std::size_t masker = 0; masker < matrixFeatures.size(); ++masker)
            {
                for (std::size_t target = 0; target < matrixFeatures.size(); ++target)
                {
                    if (stopToken.stop_requested())
                        return;
                    if (masker == target)
                        continue;
                    summaries[masker * matrixFeatures.size() + target]
                        = pairAnalyzer.summarise(*matrixFeatures[masker],
                                                *matrixFeatures[target],
                                                stopToken);
                }
            }

            std::vector<PairAnalysisView::PhasePairData> phasePairs;
            const analysis::PhaseAlignmentAnalyzer phaseAnalyzer;
            for (std::size_t first = 0; first < matrixFeatures.size(); ++first)
            {
                for (std::size_t second = first + 1; second < matrixFeatures.size(); ++second)
                {
                    const auto relationship = relationshipForMapping(
                        matrixSourceGroups[first], matrixSourceGroups[second]);
                    if (relationship != analysis::TrackRelationship::sameSourceMicrophones
                        && relationship != analysis::TrackRelationship::relatedMicrophones)
                        continue;

                    const auto& firstSamples = results[matrixResultIndices[first]].phaseSamples;
                    const auto& secondSamples = results[matrixResultIndices[second]].phaseSamples;
                    if (firstSamples.empty() || secondSamples.empty())
                        continue;
                    phasePairs.push_back({ first, second, relationship,
                        phaseAnalyzer.analyse(firstSamples, secondSamples,
                                              phaseAnalysisSampleRate) });
                }
            }

            for (auto& result : results)
            {
                result.phaseSamples.clear();
                result.phaseSamples.shrink_to_fit();
            }

            juce::MessageManager::callAsync([safeThis,
                                             results = std::move(results),
                                             matrixPaths = std::move(matrixPaths),
                                             summaries = std::move(summaries),
                                             phasePairs = std::move(phasePairs),
                                             matrixRoles = std::move(matrixRoles),
                                             masterResult = std::move(masterResult),
                                             predictedPreTrimMasterPeakDb]() mutable
            {
                if (safeThis != nullptr)
                    safeThis->finishAnalysis(std::move(results),
                                             std::move(matrixPaths),
                                             std::move(summaries),
                                             std::move(phasePairs),
                                             std::move(matrixRoles),
                                             std::move(masterResult),
                                             predictedPreTrimMasterPeakDb,
                                             false);
            });
        });
}

void MainComponent::importReaperManifest(const juce::File& manifestFile)
{
    if (!manifestFile.existsAsFile())
    {
        bridgeMessage = utf8("Nie znaleziono danych projektu z REAPERA.");
        updateStatus();
        return;
    }

    if (analysisThread.joinable())
    {
        analysisThread.request_stop();
        analysisThread.join();
    }
    if (pairDetailThread.joinable())
    {
        pairDetailThread.request_stop();
        pairDetailThread.join();
    }
    stopTimer();
    analysisRunning = false;
    waitingForReaperRender = false;
    waitingForReaperApply = false;
    planAppliedInReaper = false;
    currentMixBalance.reset();
    currentPhasePairs.clear();
    currentMaskingActions.clear();
    currentHasMaskingIssues = false;
    currentCompressionActions.clear();
    currentStereoWidthAction.reset();

    auto parsed = juce::JSON::parse(manifestFile);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr || static_cast<int>(root->getProperty("schemaVersion")) != 1)
    {
        bridgeMessage = utf8("Nieobsługiwana wersja danych z REAPERA.");
        updateStatus();
        return;
    }

    auto* project = root->getProperty("project").getDynamicObject();
    auto* bridge = root->getProperty("bridge").getDynamicObject();
    auto* trackArray = root->getProperty("tracks").getArray();
    if (project == nullptr || bridge == nullptr || trackArray == nullptr)
    {
        bridgeMessage = utf8("Dane projektu z REAPERA są niekompletne.");
        updateStatus();
        return;
    }

    ReaperBridgeSession next;
    next.manifestFile = manifestFile;
    next.projectName = project->getProperty("name").toString();
    next.projectPath = project->getProperty("path").toString();
    next.requestFile = juce::File(bridge->getProperty("requestPath").toString());
    next.responseFile = juce::File(bridge->getProperty("responsePath").toString());
    next.cacheDirectory = juce::File(bridge->getProperty("cacheDirectory").toString());
    const auto projectLength = static_cast<double>(project->getProperty("lengthSeconds"));
    next.rangeEndSeconds = projectLength;
    if (auto* range = root->getProperty("analysisRange").getDynamicObject())
    {
        next.rangeStartSeconds = static_cast<double>(range->getProperty("startSeconds"));
        next.rangeEndSeconds = static_cast<double>(range->getProperty("endSeconds"));
        next.usesTimeSelection = range->getProperty("mode").toString() == "timeSelection";
    }
    next.reaperWindow = static_cast<std::uint64_t>(
        static_cast<juce::int64>(bridge->getProperty("reaperWindow")));
    next.renderCommandId = static_cast<int>(bridge->getProperty("renderCommandId"));

    tracks.clear();
    activeMixTemplate.reset();
    reaperBridge = std::move(next);
    activeMatrixPaths.clear();
    matrixAvailable = false;
    matrixViewButton.setEnabled(false);
    showTracksView();

    const auto analysisDuration = std::max(
        0.0, reaperBridge->rangeEndSeconds - reaperBridge->rangeStartSeconds);
    const auto& palette = paletteForId(paletteSelector.getSelectedId());
    juce::StringArray folderStack;
    for (const auto& value : *trackArray)
    {
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            continue;

        TrackEntry entry;
        entry.fromReaper = true;
        entry.reaperGuid = object->getProperty("guid").toString();
        entry.name = object->getProperty("name").toString();
        if (entry.name.isEmpty())
            entry.name = "Track " + object->getProperty("index").toString();
        entry.durationSeconds = analysisDuration;
        entry.sampleRate = 48000.0;
        entry.channels = std::max(1, static_cast<int>(object->getProperty("channels")));
        entry.mutedInReaper = static_cast<bool>(object->getProperty("muted"));
        const auto folderDepthChange = static_cast<int>(object->getProperty("folderDepth"));
        entry.isReaperFolder = folderDepthChange > 0;
        entry.reaperFolderPath = folderStack.joinIntoString(" / ");
        entry.analysisGroup = entry.reaperFolderPath;
        if (object->hasProperty("ancestorFolderGainDb"))
        {
            const auto gain = static_cast<double>(
                object->getProperty("ancestorFolderGainDb"));
            entry.ancestorFolderGainDb = std::isfinite(gain)
                ? static_cast<float>(std::clamp(gain, -150.0, 24.0)) : 0.0f;
        }

        const auto colourText = object->getProperty("colour").toString().trim();
        if (colourText.startsWithChar('#') && colourText.length() == 7)
            entry.colour = juce::Colour::fromString("ff" + colourText.substring(1));
        else
            entry.colour = palette[tracks.size() % palette.size()];
        const auto folderName = entry.name;
        tracks.push_back(std::move(entry));

        if (folderDepthChange > 0)
        {
            for (auto level = 0; level < folderDepthChange; ++level)
                folderStack.add(folderName);
        }
        else
        {
            for (auto level = 0; level < -folderDepthChange && !folderStack.isEmpty(); ++level)
                folderStack.remove(folderStack.size() - 1);
        }
    }

    loadTrackMapping();
    trackTable.updateContent();
    trackTable.deselectAllRows();
    for (int index = 0; index < static_cast<int>(tracks.size()); ++index)
    {
        if (!tracks[static_cast<std::size_t>(index)].mutedInReaper
            && !tracks[static_cast<std::size_t>(index)].isReaperFolder)
            trackTable.selectRow(index, false, false);
    }
    const auto rangeDescription = reaperBridge->usesTimeSelection
        ? utf8(" | Time selection: ") + formatDuration(reaperBridge->rangeStartSeconds)
          + " – " + formatDuration(reaperBridge->rangeEndSeconds)
          + utf8(" (długość ") + formatDuration(analysisDuration) + ")"
        : utf8(" | Brak time selection — analizowany będzie cały projekt");
    descriptionLabel.setText(utf8("Projekt REAPER: ") + reaperBridge->projectName
                           + rangeDescription + utf8(" | Wybierz ścieżki i kliknij ANALIZUJ."),
                             juce::dontSendNotification);
    bridgeMessage = utf8("Połączono z REAPEREM. Domyślnie wybrano ścieżki źródłowe; grupy pominięto, aby nie dublować sygnału.");
    updateStatus();
    repaint();
}

bool MainComponent::requestReaperRender(const std::vector<juce::String>& guids)
{
    if (!reaperBridge || reaperBridge->renderCommandId <= 0 || guids.empty())
    {
        bridgeMessage = utf8("Połączenie z REAPEREM nie jest aktywne.");
        updateStatus();
        return false;
    }

    juce::String request = "MIXDEBUGGER_REQUEST_V1\n";
    for (const auto& guid : guids)
        request += "guid=" + guid + "\n";

    reaperBridge->responseFile.deleteFile();
    reaperBridge->masterFile = {};
    if (!reaperBridge->requestFile.replaceWithText(request, false, false, "\n"))
    {
        bridgeMessage = utf8("Nie udało się przekazać wyboru ścieżek do REAPERA.");
        updateStatus();
        return false;
    }

#if JUCE_WINDOWS
    const auto reaperWindow = reinterpret_cast<HWND>(
        static_cast<std::uintptr_t>(reaperBridge->reaperWindow));
    if (IsIconic(reaperWindow) != FALSE)
        ShowWindow(reaperWindow, SW_RESTORE);
    BringWindowToTop(reaperWindow);
    SetForegroundWindow(reaperWindow);

    const auto posted = PostMessageW(
        reaperWindow,
        WM_COMMAND, static_cast<WPARAM>(reaperBridge->renderCommandId), 0);
    if (posted == FALSE)
    {
        bridgeMessage = utf8("REAPER nie odpowiada. Otwórz projekt i spróbuj ponownie.");
        updateStatus();
        return false;
    }
#else
    bridgeMessage = "Integracja z REAPEREM jest obecnie dostępna w Windows.";
    updateStatus();
    return false;
#endif

    waitingForReaperRender = true;
    renderRequestStartedMs = juce::Time::currentTimeMillis();
    bridgeMessage = utf8("REAPER przygotowuje wyrównane stemy post-FX/post-fader…");
    setAnalysisRunning(true);
    startTimer(250);
    return true;
}

bool MainComponent::requestReaperApplyPlan()
{
    if (!reaperBridge || !currentMixBalance || !currentMixBalance->available
        || reaperBridge->renderCommandId <= 0)
    {
        bridgeMessage = utf8("Plan można zastosować automatycznie tylko w aktywnym projekcie REAPER.");
        updateStatus();
        return false;
    }
    if (!currentMixBalance->headroomAvailable)
    {
        bridgeMessage = utf8("Nie zastosowano balansu: brak poprawnego renderu sumy do wyliczenia 6 dB headroomu.");
        updateStatus();
        return false;
    }

    juce::String request = "MIXDEBUGGER_APPLY_V1\n";
    auto actionCount = 0;
    const auto prepareChannelStrip = pairAnalysisView.shouldPrepareChannelStrip();
    const auto useJackieProcessing = prepareChannelStrip
        && activeMixTemplate == analysis::MixTemplateId::jackieDownTheLine;
    std::map<std::string, double> gainByGuid;
    std::set<std::size_t> tracksWithEq;
    std::set<std::size_t> tracksWithCompressor;
    const auto trackForMatrixIndex = [this](const std::size_t index)
        -> const TrackEntry*
    {
        if (index >= activeMatrixPaths.size())
            return nullptr;
        const auto track = std::ranges::find_if(tracks, [this, index](const TrackEntry& entry)
        {
            return entry.file.getFullPathName() == activeMatrixPaths[index];
        });
        return track != tracks.end() && track->fromReaper && track->reaperGuid.isNotEmpty()
            ? &*track : nullptr;
    };

    for (std::size_t index = 0;
        index < std::min(activeMatrixPaths.size(), currentMixBalance->tracks.size()); ++index)
    {
        const auto& recommendation = currentMixBalance->tracks[index];
        // Wspólny trim headroomu musi trafić do każdej analizowanej grupy.
        // Pominięcie ścieżki o krótszej aktywności unieważniłoby przewidziany peak.
        if (!std::isfinite(recommendation.gainDb)
            || std::abs(recommendation.gainDb) < 0.0001f)
            continue;
        if (const auto* track = trackForMatrixIndex(index))
            gainByGuid[track->reaperGuid.toStdString()] += recommendation.gainDb;
    }

    for (const auto& masking : currentMaskingActions)
    {
        if (masking.recommendation.confidence < 0.55f)
            continue;
        const auto* track = trackForMatrixIndex(masking.maskerIndex);
        if (track == nullptr)
            continue;
        if (masking.recommendation.action == analysis::MaskingAction::lowerMasker)
        {
            gainByGuid[track->reaperGuid.toStdString()]
                += masking.recommendation.gainDb;
        }
        else if (masking.recommendation.action == analysis::MaskingAction::staticEqCut)
        {
            request += "eq\t" + track->reaperGuid + "\t"
                    + juce::String(masking.recommendation.frequencyHz, 4) + "\t"
                    + juce::String(masking.recommendation.gainDb, 4) + "\t"
                    + juce::String(masking.recommendation.q, 4) + "\n";
            ++actionCount;
            tracksWithEq.insert(masking.maskerIndex);
        }
    }

    // Techniczne EQ jest osobnym, zarządzanym procesorem: pojawia się wyłącznie
    // dla zmierzonego maskowania albo pewnego rezonansu. Podstawowy ReaEQ channel
    // stripu jest dodawany niżej i nie zastępuje korekty wynikającej z analizy.
    for (std::size_t index = 0; index < activeMatrixPaths.size(); ++index)
    {
        if (tracksWithEq.contains(index))
            continue;
        const auto* track = trackForMatrixIndex(index);
        const auto* analysed = pairAnalysisView.trackAt(index);
        if (track == nullptr || analysed == nullptr)
            continue;

        const auto resonance = std::ranges::find_if(analysed->resonances,
            [](const analysis::ResonanceFinding& finding)
            {
                return finding.score >= 5.0f && finding.confidence >= 0.55f
                    && finding.frequencyHz >= 50.0f
                    && finding.frequencyHz <= 16000.0f;
            });
        if (resonance != analysed->resonances.end())
        {
            request += "eq\t" + track->reaperGuid + "\t"
                    + juce::String(resonance->frequencyHz, 4) + "\t"
                    + juce::String(resonance->recommendedGainDb, 4) + "\t"
                    + juce::String(resonance->q, 4) + "\n";
            ++actionCount;
            tracksWithEq.insert(index);
        }
    }

    if (prepareChannelStrip && !useJackieProcessing)
    {
        for (std::size_t index = 0; index < activeMatrixPaths.size(); ++index)
        {
            const auto* track = trackForMatrixIndex(index);
            const auto* analysed = pairAnalysisView.trackAt(index);
            if (track == nullptr || analysed == nullptr)
                continue;
            const auto preset = analysis::TemplateProcessingAdvisor::basicEqPresetFor(
                analysed->family, analysed->role);
            request += "stockeq\t" + track->reaperGuid + "\t"
                    + juce::String(preset) + "\n";
            ++actionCount;
        }
    }

    for (const auto& [guid, gain] : gainByGuid)
    {
        const auto safeGain = std::clamp(gain, -48.0, 9.0);
        if (std::abs(safeGain) < 0.05)
            continue;
        request += "gain\t" + juce::String(guid) + "\t"
                + juce::String(safeGain, 6) + "\n";
        ++actionCount;
    }

    // Headroom jest jednym wspólnym ruchem mastera. Nie dokładamy tego samego
    // trymu do każdego fadera ścieżki, bo przy dużym projekcie prowadziło to do
    // absurdalnych pozycji faderów mimo zachowanej relacji między rolami.
    if (std::isfinite(currentMixBalance->globalHeadroomTrimDb)
        && std::abs(currentMixBalance->globalHeadroomTrimDb) >= 0.05f)
    {
        request += "gain\t__MASTER__\t"
                + juce::String(currentMixBalance->globalHeadroomTrimDb, 6) + "\n";
        ++actionCount;
    }

    const auto hasVisibleSourcePhase = std::ranges::any_of(currentPhasePairs,
        [](const PairAnalysisView::PhasePairData& pair)
        {
            return pair.relationship == analysis::TrackRelationship::sameSourceMicrophones
                && (pair.summary.assessment
                        == analysis::PhaseAlignmentAssessment::polaritySuspect
                    || pair.summary.assessment
                        == analysis::PhaseAlignmentAssessment::timingSuspect);
        });
    for (const auto& pair : currentPhasePairs)
    {
        if (pair.relationship != analysis::TrackRelationship::sameSourceMicrophones
            || pair.summary.confidence < 0.55f
            || pair.secondIndex >= activeMatrixPaths.size())
            continue;
        const auto* track = trackForMatrixIndex(pair.secondIndex);
        if (track == nullptr)
            continue;
        if (pair.summary.assessment == analysis::PhaseAlignmentAssessment::polaritySuspect)
        {
            request += "phase\t" + track->reaperGuid + "\n";
            ++actionCount;
            break;
        }
        else if (pair.summary.assessment == analysis::PhaseAlignmentAssessment::timingSuspect)
        {
            const auto offsetDeltaSeconds = -pair.summary.bestLagMilliseconds / 1000.0;
            request += "offset\t" + track->reaperGuid + "\t"
                    + juce::String(offsetDeltaSeconds, 9) + "\n";
            ++actionCount;
            break;
        }
    }

    if (!hasVisibleSourcePhase && currentStereoWidthAction
        && currentStereoWidthAction->recommendation.safeToApply
        && currentStereoWidthAction->recommendation.confidence >= 0.40f)
    {
        if (const auto* track = trackForMatrixIndex(currentStereoWidthAction->trackIndex))
        {
            request += "width\t" + track->reaperGuid + "\t"
                    + juce::String(
                        currentStereoWidthAction->recommendation.widthMultiplier, 6)
                    + "\n";
            ++actionCount;
        }
    }

    if (!useJackieProcessing && !prepareChannelStrip)
    {
        for (const auto& compression : currentCompressionActions)
        {
            const auto* track = trackForMatrixIndex(compression.trackIndex);
            if (track == nullptr)
                continue;
            const auto& recommendation = compression.recommendation;
            request += "compressor\t" + track->reaperGuid + "\t"
                    + juce::String(recommendation.thresholdDb
                                   - track->ancestorFolderGainDb, 4) + "\t"
                    + juce::String(recommendation.ratio, 4) + "\t"
                    + juce::String(recommendation.attackMilliseconds, 4) + "\t"
                    + juce::String(recommendation.releaseMilliseconds, 4) + "\n";
            ++actionCount;
            tracksWithCompressor.insert(compression.trackIndex);
        }
    }

    if (prepareChannelStrip && !useJackieProcessing)
    {
        for (std::size_t index = 0; index < activeMatrixPaths.size(); ++index)
        {
            const auto* track = trackForMatrixIndex(index);
            const auto* analysed = pairAnalysisView.trackAt(index);
            if (track == nullptr || analysed == nullptr)
                continue;
            const auto active
                = analysed->compression.advice
                    == analysis::CompressionAdvice::considerLeveling
               || analysed->compression.advice
                    == analysis::CompressionAdvice::considerPeakControl;
            const auto preset
                = analysis::TemplateProcessingAdvisor::basicCompressorPresetFor(
                    analysed->family, analysed->role);
            request += "stockcomp\t" + track->reaperGuid + "\t"
                    + juce::String(preset) + "\t"
                    + juce::String(active
                        ? analysed->compression.thresholdDb
                            - track->ancestorFolderGainDb : 0.0f, 4) + "\t"
                    + juce::String(active ? analysed->compression.ratio : 1.0f, 4) + "\t"
                    + juce::String(active
                        ? analysed->compression.attackMilliseconds : 20.0f, 4) + "\t"
                    + juce::String(active
                        ? analysed->compression.releaseMilliseconds : 100.0f, 4) + "\n";
            ++actionCount;
            tracksWithCompressor.insert(index);
        }
    }

    if (useJackieProcessing)
    {
        const analysis::TemplateProcessingAdvisor advisor;
        for (std::size_t index = 0; index < activeMatrixPaths.size(); ++index)
        {
            const auto* track = trackForMatrixIndex(index);
            const auto* analysed = pairAnalysisView.trackAt(index);
            if (track == nullptr || analysed == nullptr)
                continue;
            const auto processing = advisor.recommend(
                analysis::MixTemplateId::jackieDownTheLine,
                { analysed->family, analysed->role, analysed->dynamics });
            if (!processing.available)
                continue;
            if (!processing.stockEqPreset.empty())
            {
                request += "stockeq\t" + track->reaperGuid + "\t"
                        + juce::String(processing.stockEqPreset) + "\n";
                ++actionCount;
            }
            if (!processing.stockCompressorPreset.empty())
            {
                const auto thresholdDb = processing.compressorActive
                    ? processing.compression.thresholdDb
                        - track->ancestorFolderGainDb
                    : 0.0f;
                const auto ratio = processing.compressorActive
                    ? processing.compression.ratio : 1.0f;
                const auto attack = processing.compressorActive
                    ? processing.compression.attackMilliseconds : 20.0f;
                const auto release = processing.compressorActive
                    ? processing.compression.releaseMilliseconds : 100.0f;
                request += "stockcomp\t" + track->reaperGuid + "\t"
                        + juce::String(processing.stockCompressorPreset) + "\t"
                        + juce::String(thresholdDb, 4) + "\t"
                        + juce::String(ratio, 4) + "\t"
                        + juce::String(attack, 4) + "\t"
                        + juce::String(release, 4) + "\n";
                ++actionCount;
            }
            if (processing.saturationAmountPercent > 0.0f)
            {
                request += "saturation\t" + track->reaperGuid + "\t"
                        + juce::String(processing.saturationAmountPercent, 2) + "\n";
                ++actionCount;
            }
        }
    }

    if (actionCount == 0)
    {
        bridgeMessage = utf8("Plan nie zawiera zmian o wystarczającej pewności do automatycznego zastosowania.");
        updateStatus();
        return false;
    }

    reaperBridge->responseFile.deleteFile();
    if (!reaperBridge->requestFile.replaceWithText(request, false, false, "\n"))
    {
        bridgeMessage = utf8("Nie udało się przekazać planu do REAPERA.");
        updateStatus();
        return false;
    }

#if JUCE_WINDOWS
    const auto reaperWindow = reinterpret_cast<HWND>(
        static_cast<std::uintptr_t>(reaperBridge->reaperWindow));
    const auto posted = PostMessageW(reaperWindow, WM_COMMAND,
        static_cast<WPARAM>(reaperBridge->renderCommandId), 0);
    if (posted == FALSE)
    {
        bridgeMessage = utf8("REAPER nie odpowiada. Plan nie został zastosowany.");
        updateStatus();
        return false;
    }
#else
    return false;
#endif

    waitingForReaperApply = true;
    pendingUndoRequest = false;
    renderRequestStartedMs = juce::Time::currentTimeMillis();
    bridgeMessage = utf8("REAPER stosuje odwracalny plan: poziomy, Time Adjustment, Stereo Width, EQ i kompresję…");
    setAnalysisRunning(true);
    startTimer(100);
    return true;
}

bool MainComponent::requestReaperUndoPlan()
{
    if (!reaperBridge || !planAppliedInReaper)
        return false;
    reaperBridge->responseFile.deleteFile();
    if (!reaperBridge->requestFile.replaceWithText("MIXDEBUGGER_UNDO_V1\n",
                                                    false, false, "\n"))
        return false;
#if JUCE_WINDOWS
    const auto reaperWindow = reinterpret_cast<HWND>(
        static_cast<std::uintptr_t>(reaperBridge->reaperWindow));
    if (PostMessageW(reaperWindow, WM_COMMAND,
                     static_cast<WPARAM>(reaperBridge->renderCommandId), 0) == FALSE)
        return false;
#else
    return false;
#endif
    waitingForReaperApply = true;
    pendingUndoRequest = true;
    renderRequestStartedMs = juce::Time::currentTimeMillis();
    bridgeMessage = utf8("REAPER przywraca stan sprzed zastosowania planu…");
    setAnalysisRunning(true);
    startTimer(100);
    return true;
}

void MainComponent::timerCallback()
{
    if ((!waitingForReaperRender && !waitingForReaperApply) || !reaperBridge)
        return;
    if (reaperBridge->responseFile.existsAsFile())
    {
        if (waitingForReaperApply)
            consumeReaperApplyResponse();
        else
            consumeReaperRenderResponse();
        return;
    }
    if (juce::Time::currentTimeMillis() - renderRequestStartedMs > 30 * 60 * 1000)
    {
        stopTimer();
        waitingForReaperRender = false;
        waitingForReaperApply = false;
        bridgeMessage = utf8("Przekroczono czas oczekiwania na odpowiedź REAPERA.");
        setAnalysisRunning(false);
    }
}

void MainComponent::consumeReaperApplyResponse()
{
    stopTimer();
    waitingForReaperApply = false;
    const auto lines = juce::StringArray::fromLines(reaperBridge->responseFile.loadFileAsString());
    const auto ok = lines.size() >= 2
        && lines[0] == "MIXDEBUGGER_APPLY_RESPONSE_V1" && lines[1] == "status=ok";
    auto message = lines.size() > 2 && lines[2].startsWith("message=")
        ? lines[2].substring(8)
        : (ok ? utf8("Plan zastosowany.") : utf8("REAPER odrzucił plan zmian."));
    if (ok)
        planAppliedInReaper = !pendingUndoRequest;
    pendingUndoRequest = false;
    bridgeMessage = utf8("REAPER: ") + message;
    setAnalysisRunning(false);
    pairAnalysisView.setApplyState(reaperBridge.has_value(), planAppliedInReaper);
}

void MainComponent::consumeReaperRenderResponse()
{
    stopTimer();
    waitingForReaperRender = false;
    const auto lines = juce::StringArray::fromLines(reaperBridge->responseFile.loadFileAsString());
    if (lines.size() < 2 || lines[0] != "MIXDEBUGGER_RESPONSE_V1"
        || lines[1] != "status=ok")
    {
        const auto message = lines.size() > 2 && lines[2].startsWith("message=")
                           ? lines[2].substring(8)
                           : utf8("Nieznany błąd renderowania w REAPERZE.");
        bridgeMessage = utf8("REAPER: ") + message;
        setAnalysisRunning(false);
        return;
    }

    reaperBridge->masterFile = {};
    for (int lineIndex = 2; lineIndex < lines.size(); ++lineIndex)
    {
        if (lines[lineIndex].startsWith("master="))
        {
            reaperBridge->masterFile = juce::File(lines[lineIndex].substring(7));
            continue;
        }
        if (!lines[lineIndex].startsWith("track="))
            continue;
        const auto separator = lines[lineIndex].indexOfChar('\t');
        if (separator < 0)
            continue;
        const auto guid = lines[lineIndex].substring(6, separator);
        const auto path = lines[lineIndex].substring(separator + 1);
        const auto track = std::ranges::find_if(tracks, [&guid](const TrackEntry& entry)
        {
            return entry.reaperGuid == guid;
        });
        if (track != tracks.end())
            track->file = juce::File(path);
    }

    bridgeMessage = utf8("Stemy gotowe. Rozpoczynam analizę…");
    usePreparedReaperFilesOnce = true;
    setAnalysisRunning(false);
    startAnalysis();
}

analysis::DynamicsSummary MainComponent::analyseAlignedGroup(
    const std::vector<juce::File>& files,
    const std::vector<float>& inputGainsDb,
    const std::stop_token stopToken,
    std::optional<float>& peakDb)
{
    peakDb.reset();
    if (files.size() < 2 || files.size() != inputGainsDb.size())
        return {};

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<float> inputGains;
    readers.reserve(files.size());
    inputGains.reserve(files.size());
    auto sampleRate = 0.0;
    juce::int64 sampleCount = 0;
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(
            manager.createReaderFor(files[index]));
        if (reader == nullptr || reader->sampleRate <= 0.0
            || !std::isfinite(inputGainsDb[index]))
            return {};
        if (sampleRate == 0.0)
            sampleRate = reader->sampleRate;
        else if (std::abs(reader->sampleRate - sampleRate) > 0.5)
            return {};
        sampleCount = std::max(sampleCount, reader->lengthInSamples);
        inputGains.push_back(std::pow(10.0f, inputGainsDb[index] / 20.0f));
        readers.push_back(std::move(reader));
    }
    if (sampleCount <= 0 || sampleRate <= 0.0)
        return {};

    constexpr int blockSize = 8192;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> buffers;
    buffers.reserve(readers.size());
    for (std::size_t index = 0; index < readers.size(); ++index)
        buffers.push_back(std::make_unique<juce::AudioBuffer<float>>(2, blockSize));
    std::vector<float> sumLeft(blockSize);
    std::vector<float> sumRight(blockSize);
    std::vector<float> monoSamples;
    monoSamples.reserve(static_cast<std::size_t>(sampleCount));
    analysis::LevelAccumulator accumulator;

    for (juce::int64 position = 0; position < sampleCount; position += blockSize)
    {
        if (stopToken.stop_requested())
            return {};
        const auto samplesThisBlock = static_cast<int>(
            std::min<juce::int64>(blockSize, sampleCount - position));
        std::fill_n(sumLeft.begin(), samplesThisBlock, 0.0f);
        std::fill_n(sumRight.begin(), samplesThisBlock, 0.0f);

        for (std::size_t readerIndex = 0; readerIndex < readers.size(); ++readerIndex)
        {
            const auto& reader = readers[readerIndex];
            if (position >= reader->lengthInSamples)
                continue;
            const auto available = static_cast<int>(std::min<juce::int64>(
                samplesThisBlock, reader->lengthInSamples - position));
            auto& buffer = *buffers[readerIndex];
            buffer.clear();
            const auto stereo = reader->numChannels > 1;
            if (!reader->read(&buffer, 0, available, position, true, stereo))
                return {};
            const auto* left = buffer.getReadPointer(0);
            const auto* right = stereo ? buffer.getReadPointer(1) : left;
            const auto inputGain = inputGains[readerIndex];
            for (auto sample = 0; sample < available; ++sample)
            {
                sumLeft[static_cast<std::size_t>(sample)] += left[sample] * inputGain;
                sumRight[static_cast<std::size_t>(sample)] += right[sample] * inputGain;
            }
        }

        accumulator.addStereoBlock(sumLeft.data(), sumRight.data(),
                                   static_cast<std::size_t>(samplesThisBlock));
        for (auto sample = 0; sample < samplesThisBlock; ++sample)
            monoSamples.push_back(0.5f * (sumLeft[static_cast<std::size_t>(sample)]
                                        + sumRight[static_cast<std::size_t>(sample)]));
    }

    const auto level = accumulator.result();
    if (isUsableAudioPeak(level.peakDb))
        peakDb = level.peakDb;
    return analysis::DynamicsAnalyzer{}.analyse(monoSamples, sampleRate);
}

std::optional<float> MainComponent::predictPreTrimMasterPeak(
    const std::vector<juce::File>& files,
    const std::vector<float>& baseGainsDb,
    const std::vector<float>& relativeGainsDb,
    const float currentRenderedMasterPeakDb,
    const std::stop_token stopToken)
{
    if (files.empty() || files.size() != baseGainsDb.size()
        || files.size() != relativeGainsDb.size()
        || !std::isfinite(currentRenderedMasterPeakDb))
        return std::nullopt;

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;
    readers.reserve(files.size());
    std::vector<float> baseGains;
    std::vector<float> adjustedGains;
    baseGains.reserve(files.size());
    adjustedGains.reserve(files.size());
    auto sampleRate = 0.0;
    juce::int64 sampleCount = 0;
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(
            manager.createReaderFor(files[index]));
        if (reader == nullptr || reader->sampleRate <= 0.0
            || !std::isfinite(baseGainsDb[index])
            || !std::isfinite(relativeGainsDb[index]))
            return std::nullopt;
        if (sampleRate == 0.0)
            sampleRate = reader->sampleRate;
        else if (std::abs(reader->sampleRate - sampleRate) > 0.5)
            return std::nullopt;
        sampleCount = std::max(sampleCount, reader->lengthInSamples);
        baseGains.push_back(std::pow(10.0f, baseGainsDb[index] / 20.0f));
        adjustedGains.push_back(std::pow(
            10.0f, (baseGainsDb[index] + relativeGainsDb[index]) / 20.0f));
        readers.push_back(std::move(reader));
    }
    if (sampleCount <= 0)
        return std::nullopt;

    constexpr int blockSize = 8192;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> buffers;
    buffers.reserve(readers.size());
    for (std::size_t index = 0; index < readers.size(); ++index)
        buffers.push_back(std::make_unique<juce::AudioBuffer<float>>(2, blockSize));
    std::vector<float> originalLeft(blockSize);
    std::vector<float> originalRight(blockSize);
    std::vector<float> adjustedLeft(blockSize);
    std::vector<float> adjustedRight(blockSize);
    auto originalPeak = 0.0f;
    auto adjustedPeak = 0.0f;

    for (juce::int64 position = 0; position < sampleCount; position += blockSize)
    {
        if (stopToken.stop_requested())
            return std::nullopt;
        const auto samplesThisBlock = static_cast<int>(
            std::min<juce::int64>(blockSize, sampleCount - position));
        std::fill_n(originalLeft.begin(), samplesThisBlock, 0.0f);
        std::fill_n(originalRight.begin(), samplesThisBlock, 0.0f);
        std::fill_n(adjustedLeft.begin(), samplesThisBlock, 0.0f);
        std::fill_n(adjustedRight.begin(), samplesThisBlock, 0.0f);

        for (std::size_t readerIndex = 0; readerIndex < readers.size(); ++readerIndex)
        {
            const auto& reader = readers[readerIndex];
            if (position >= reader->lengthInSamples)
                continue;
            const auto available = static_cast<int>(std::min<juce::int64>(
                samplesThisBlock, reader->lengthInSamples - position));
            auto& buffer = *buffers[readerIndex];
            buffer.clear();
            const auto stereo = reader->numChannels > 1;
            if (!reader->read(&buffer, 0, available, position, true, stereo))
                return std::nullopt;
            const auto* left = buffer.getReadPointer(0);
            const auto* right = stereo ? buffer.getReadPointer(1) : left;
            const auto baseGain = baseGains[readerIndex];
            const auto adjustedGain = adjustedGains[readerIndex];
            for (auto sample = 0; sample < available; ++sample)
            {
                const auto positionInBlock = static_cast<std::size_t>(sample);
                originalLeft[positionInBlock] += left[sample] * baseGain;
                originalRight[positionInBlock] += right[sample] * baseGain;
                adjustedLeft[positionInBlock] += left[sample] * adjustedGain;
                adjustedRight[positionInBlock] += right[sample] * adjustedGain;
            }
        }

        for (auto sample = 0; sample < samplesThisBlock; ++sample)
        {
            const auto index = static_cast<std::size_t>(sample);
            originalPeak = std::max({ originalPeak,
                std::abs(originalLeft[index]), std::abs(originalRight[index]) });
            adjustedPeak = std::max({ adjustedPeak,
                std::abs(adjustedLeft[index]), std::abs(adjustedRight[index]) });
        }
    }

    if (originalPeak <= 1.0e-8f || adjustedPeak <= 1.0e-8f)
        return std::nullopt;
    const auto stemPeakDeltaDb = 20.0f * std::log10(adjustedPeak / originalPeak);
    return currentRenderedMasterPeakDb + stemPeakDeltaDb;
}

MainComponent::AnalysisResult MainComponent::analyseFile(const juce::File& file,
                                                         const std::stop_token stopToken,
                                                         const bool collectPhaseSamples,
                                                         const float inputGainDb)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));

    if (reader == nullptr)
    {
        AnalysisResult result;
        result.path = file.getFullPathName();
        result.error = utf8("Nie można odczytać pliku");
        return result;
    }

    constexpr int blockSize = 8192;
    const auto channelsToRead = std::clamp(static_cast<int>(reader->numChannels), 1, 2);
    const auto samplesToAnalyse = reader->lengthInSamples;
    juce::AudioBuffer<float> buffer(channelsToRead, blockSize);
    analysis::LevelAccumulator accumulator;
    std::vector<float> monoSamples;
    monoSamples.reserve(static_cast<std::size_t>(samplesToAnalyse));
    std::uint64_t clippedChannelSamples = 0;
    const auto inputGain = std::isfinite(inputGainDb)
        ? std::pow(10.0f, inputGainDb / 20.0f) : 1.0f;

    for (juce::int64 position = 0; position < samplesToAnalyse; position += blockSize)
    {
        if (stopToken.stop_requested())
        {
            AnalysisResult result;
            result.path = file.getFullPathName();
            result.error = "Anulowano";
            return result;
        }

        const auto samplesToRead = static_cast<int>(
            std::min<juce::int64>(blockSize, samplesToAnalyse - position));
        buffer.clear();

        if (!reader->read(&buffer, 0, samplesToRead, position, true, channelsToRead > 1))
        {
            AnalysisResult result;
            result.path = file.getFullPathName();
            result.error = utf8("Błąd odczytu audio");
            return result;
        }

        buffer.applyGain(0, samplesToRead, inputGain);
        const auto* left = buffer.getReadPointer(0);
        const auto* right = channelsToRead > 1 ? buffer.getReadPointer(1) : left;
        const auto inverseInputGain = 1.0f / std::max(inputGain, 1.0e-12f);
        for (int sample = 0; sample < samplesToRead; ++sample)
        {
            // Clipping źródłowy pozostaje problemem nawet po ściszeniu folderu,
            // a clipping powstały po gainie folderów także musi zablokować
            // kompresję. Każdą próbkę kanału liczymy najwyżej raz.
            clippedChannelSamples += static_cast<std::uint64_t>(
                std::abs(left[sample] * inverseInputGain) >= 0.999f
                || std::abs(left[sample]) >= 0.999f);
            if (channelsToRead > 1)
            {
                clippedChannelSamples += static_cast<std::uint64_t>(
                    std::abs(right[sample] * inverseInputGain) >= 0.999f
                    || std::abs(right[sample]) >= 0.999f);
            }
        }
        accumulator.addStereoBlock(left,
                                   right,
                                   static_cast<std::size_t>(samplesToRead));

        for (int sample = 0; sample < samplesToRead; ++sample)
            monoSamples.push_back(0.5f * (left[sample] + right[sample]));
    }

    const auto level = accumulator.result();
    auto dynamics = analysis::DynamicsAnalyzer{}.analyse(monoSamples, reader->sampleRate);
    dynamics.absolutePeakDb = level.peakDb;
    dynamics.crestFactorDb = std::max(
        0.0f, dynamics.absolutePeakDb - dynamics.activeRmsDb);
    dynamics.clippedSampleCount = clippedChannelSamples;
    if (clippedChannelSamples > 0)
        dynamics.assessment = analysis::DynamicsAssessment::clippingRisk;

    if (!isUsableAudioPeak(level.peakDb))
    {
        AnalysisResult result;
        result.path = file.getFullPathName();
        result.rmsDb = level.rmsDb;
        result.peakDb = level.peakDb;
        if (reader->numChannels > 1)
        {
            result.stereoCorrelation = level.stereoCorrelation;
            result.sideToMidPowerRatio = level.sideToMidPowerRatio;
        }
        result.dynamics = dynamics;
        result.error = utf8("Brak użytecznego sygnału audio");
        return result;
    }

    auto resampled = dsp::resampleLanczos(monoSamples, reader->sampleRate, 48000.0);
    auto phaseSamples = collectPhaseSamples
        ? dsp::resampleLanczos(monoSamples, reader->sampleRate, phaseAnalysisSampleRate)
        : std::vector<float>{};
    const auto spectrogram = dsp::StftAnalyzer{}.analyse(resampled, 48000.0);
    auto resonances = analysis::ResonanceAnalyzer{}.analyse(spectrogram);
    auto erb = dsp::ErbFilterBank(48000.0).analyse(spectrogram);
    auto features = std::make_shared<analysis::TrackFeatures>(
        analysis::TrackFeatureExtractor{}.extract(std::move(erb)));

    AnalysisResult result;
    result.path = file.getFullPathName();
    result.rmsDb = level.rmsDb;
    result.peakDb = level.peakDb;
    if (reader->numChannels > 1)
    {
        result.stereoCorrelation = level.stereoCorrelation;
        result.sideToMidPowerRatio = level.sideToMidPowerRatio;
    }
    result.stftFrames = spectrogram.frameCount;
    result.dominantFrequencyHz = spectrogram.dominantFrequencyHz;
    result.erbAverageDb = features->averageDb;
    result.features = std::move(features);
    result.dynamics = dynamics;
    result.resonances = std::move(resonances);
    result.phaseSamples = std::move(phaseSamples);
    return result;
}

void MainComponent::finishAnalysis(std::vector<AnalysisResult> results,
                                   std::vector<juce::String> matrixPaths,
                                   std::vector<analysis::PairSummary> summaries,
                                   std::vector<PairAnalysisView::PhasePairData> phasePairs,
                                   std::vector<analysis::MixRole> matrixRoles,
                                   std::optional<AnalysisResult> masterResult,
                                   const std::optional<float> predictedPreTrimMasterPeakDb,
                                   const bool rejectedSilentRender)
{
    if (pairDetailThread.joinable())
    {
        pairDetailThread.request_stop();
        pairDetailThread.join();
    }

    for (auto& result : results)
    {
        const auto track = std::ranges::find_if(tracks, [&result](const TrackEntry& entry)
        {
            return entry.file.getFullPathName() == result.path;
        });

        if (track == tracks.end())
            continue;

        track->rmsDb = result.rmsDb;
        track->peakDb = result.peakDb;
        track->stereoCorrelation = result.stereoCorrelation;
        track->sideToMidPowerRatio = result.sideToMidPowerRatio;
        track->stftFrames = result.stftFrames;
        track->dominantFrequencyHz = result.dominantFrequencyHz;
        track->erbAverageDb = result.erbAverageDb;
        track->features = std::move(result.features);
        track->dynamics = result.dynamics;
        track->measuredGroupActiveRmsDb = result.measuredGroupActiveRmsDb;
        track->measuredGroupPeakDb = result.measuredGroupPeakDb;
        track->measuredGroupActiveWindowCount = result.measuredGroupActiveWindowCount;
        track->balanceInternalTrimDb = result.balanceInternalTrimDb;
        track->resonances = std::move(result.resonances);
        track->error = std::move(result.error);
    }

    activeMatrixPaths = matrixPaths;
    std::vector<PairAnalysisView::TrackData> matrixTracks;
    std::vector<analysis::BalanceTrackInput> balanceInputs;
    matrixTracks.reserve(matrixPaths.size());
    balanceInputs.reserve(matrixPaths.size());
    for (std::size_t matrixIndex = 0; matrixIndex < matrixPaths.size(); ++matrixIndex)
    {
        const auto& path = matrixPaths[matrixIndex];
        const auto track = std::ranges::find_if(tracks, [&path](const TrackEntry& entry)
        {
            return entry.file.getFullPathName() == path;
        });
        if (track != tracks.end() && track->features)
        {
            const auto family = track->assignedFamily.value_or(analysis::TrackFamily::unknown);
            const auto compression = analysis::CompressionAdvisor{}.recommend(track->dynamics,
                                                                                family);
            const auto role = matrixIndex < matrixRoles.size()
                ? matrixRoles[matrixIndex] : track->mixRole;
            const auto stereoWidth = track->stereoCorrelation
                ? analysis::StereoWidthAdvisor{}.recommend(*track->stereoCorrelation,
                                                            track->channels,
                                                            track->sideToMidPowerRatio.value_or(-1.0f))
                : analysis::StereoWidthRecommendation{};
            matrixTracks.push_back({ track->name, track->colour, track->features,
                                     track->analysisGroup, track->sourceGroup,
                                     track->dynamics, family, role, track->channels,
                                     track->stereoCorrelation, stereoWidth, {},
                                     compression, track->resonances });
            balanceInputs.push_back({
                track->analysisGroup.isNotEmpty() ? track->analysisGroup.toStdString()
                                                  : track->name.toStdString(),
                role,
                track->dynamics.activeRmsDb,
                track->peakDb.value_or(-100.0f),
                track->dynamics.activeWindowCount,
                track->measuredGroupActiveRmsDb,
                track->measuredGroupPeakDb,
                track->measuredGroupActiveWindowCount,
                track->balanceInternalTrimDb
            });
        }
    }

    matrixAvailable = !rejectedSilentRender && !matrixTracks.empty();
    std::optional<analysis::MasterBalanceSummary> masterBalance;
    if (masterResult && masterResult->features)
        masterBalance = analysis::MasterBalanceAnalyzer{}.analyse(*masterResult->features);
    std::optional<analysis::MixBalanceSummary> mixBalance;
    if (!balanceInputs.empty())
    {
        mixBalance = analysis::BalanceAdvisor{}.recommend(
            balanceInputs, predictedPreTrimMasterPeakDb);
        if (mixBalance->available && mixBalance->tracks.size() == matrixTracks.size())
        {
            for (std::size_t index = 0; index < matrixTracks.size(); ++index)
                matrixTracks[index].balance = mixBalance->tracks[index];
        }
    }

    std::stable_sort(phasePairs.begin(), phasePairs.end(),
        [](const PairAnalysisView::PhasePairData& left,
           const PairAnalysisView::PhasePairData& right)
        {
            const auto isActionable = [](const auto& pair)
            {
                return pair.relationship == analysis::TrackRelationship::sameSourceMicrophones
                    && (pair.summary.assessment
                            == analysis::PhaseAlignmentAssessment::polaritySuspect
                        || pair.summary.assessment
                            == analysis::PhaseAlignmentAssessment::timingSuspect);
            };
            if (isActionable(left) != isActionable(right))
                return isActionable(left);
            return left.summary.confidence > right.summary.confidence;
        });

    currentMaskingActions.clear();
    currentHasMaskingIssues = false;
    if (matrixTracks.size() >= 2
        && summaries.size() >= matrixTracks.size() * matrixTracks.size())
    {
        std::set<std::size_t> automaticMaskers;
        std::vector<analysis::MixRole> roles;
        roles.reserve(matrixTracks.size());
        for (const auto& track : matrixTracks)
            roles.push_back(track.role);

        const auto possiblePairCount = matrixTracks.size() * (matrixTracks.size() - 1) / 2;
        const auto ranked = analysis::rankPairIssues(summaries, roles, possiblePairCount);
        for (const auto& issue : ranked)
        {
            if (analysis::pairIssueSeverity(
                    summaries[issue.maskerIndex * matrixTracks.size() + issue.targetIndex])
                == analysis::PairIssueSeverity::low)
                continue;
            const auto relatedCapture = std::ranges::any_of(phasePairs,
                [&issue](const PairAnalysisView::PhasePairData& pair)
                {
                    return (pair.firstIndex == issue.maskerIndex
                            && pair.secondIndex == issue.targetIndex)
                        || (pair.firstIndex == issue.targetIndex
                            && pair.secondIndex == issue.maskerIndex);
                });
            if (relatedCapture)
                continue;

            const auto& summary = summaries[
                issue.maskerIndex * matrixTracks.size() + issue.targetIndex];
            const auto recommendation = analysis::MaskingAdvisor{}.recommend(
                summary, *matrixTracks[issue.targetIndex].features,
                matrixTracks[issue.maskerIndex].role,
                matrixTracks[issue.targetIndex].role);
            if (recommendation.action == analysis::MaskingAction::none)
                continue;

            currentHasMaskingIssues = true;

            // Dynamiczne EQ wymaga sidechainu celu, więc pozostaje dokładną
            // instrukcją ręczną. Szukamy dalej bezpiecznych korekt, zamiast
            // przeskakiwać od razu do kompresji.
            const auto canApplyAutomatically
                = recommendation.confidence >= 0.55f
                && (recommendation.action == analysis::MaskingAction::lowerMasker
                    || recommendation.action == analysis::MaskingAction::staticEqCut);
            if (!canApplyAutomatically
                || automaticMaskers.contains(issue.maskerIndex))
                continue;

            automaticMaskers.insert(issue.maskerIndex);
            currentMaskingActions.push_back({ issue.maskerIndex, recommendation });
        }
    }

    currentCompressionActions.clear();
    struct RankedCompression
    {
        int priority = 0;
        CompressionPlanAction action;
    };
    std::vector<RankedCompression> compressionCandidates;
    for (std::size_t index = 0; index < matrixTracks.size(); ++index)
    {
        const auto& track = matrixTracks[index];
        if (track.compression.advice == analysis::CompressionAdvice::noAction
            || track.compression.advice == analysis::CompressionAdvice::insufficientData)
            continue;
        auto priority = 0;
        switch (track.compression.advice)
        {
            case analysis::CompressionAdvice::reduceInputBeforeProcessing: priority = 4; break;
            case analysis::CompressionAdvice::considerLeveling: priority = 3; break;
            case analysis::CompressionAdvice::considerPeakControl: priority = 2; break;
            case analysis::CompressionAdvice::avoidMoreCompression: priority = 1; break;
            case analysis::CompressionAdvice::noAction:
            case analysis::CompressionAdvice::insufficientData: break;
        }
        compressionCandidates.push_back({ priority, { index, track.compression } });
    }
    std::stable_sort(compressionCandidates.begin(), compressionCandidates.end(),
        [](const RankedCompression& left, const RankedCompression& right)
        {
            return left.priority > right.priority;
        });
    currentStereoWidthAction.reset();
    for (std::size_t index = 0; index < matrixTracks.size(); ++index)
    {
        const auto& recommendation = matrixTracks[index].stereoWidth;
        if (!recommendation.available)
            continue;
        if (!currentStereoWidthAction
            || recommendation.confidence
                > currentStereoWidthAction->recommendation.confidence)
            currentStereoWidthAction = StereoWidthPlanAction { index, recommendation };
    }

    for (const auto& candidate : compressionCandidates)
    {
        const auto advice = candidate.action.recommendation.advice;
        if (advice == analysis::CompressionAdvice::considerLeveling
            || advice == analysis::CompressionAdvice::considerPeakControl)
            currentCompressionActions.push_back(candidate.action);
    }

    currentMixBalance = mixBalance;
    currentPhasePairs = phasePairs;
    planAppliedInReaper = false;
    pairAnalysisView.setMatrix(std::move(matrixTracks), std::move(summaries),
                               std::move(phasePairs),
                               std::move(mixBalance),
                               std::move(masterBalance));
    pairAnalysisView.setApplyState(reaperBridge.has_value() && matrixAvailable, false);
    bridgeMessage = rejectedSilentRender
        ? utf8("REAPER wyrenderował praktycznie ciszę. Sprawdź, czy media są ONLINE i czy time selection obejmuje audio, po czym kliknij ANALIZUJ ponownie.")
        : utf8("Analiza zakończona.");
    setAnalysisRunning(false);
    matrixViewButton.setEnabled(matrixAvailable);
    if (matrixAvailable)
        showMatrixView();
    else
        showTracksView();
    trackTable.repaint();
}

void MainComponent::startPairDetail(const std::size_t maskerIndex,
                                    const std::size_t targetIndex)
{
    const auto* masker = pairAnalysisView.trackAt(maskerIndex);
    const auto* target = pairAnalysisView.trackAt(targetIndex);
    if (masker == nullptr || target == nullptr || !masker->features || !target->features)
        return;

    if (pairDetailThread.joinable())
    {
        pairDetailThread.request_stop();
        pairDetailThread.join();
    }

    pairAnalysisView.showLoading(maskerIndex, targetIndex);
    const auto maskerFeatures = masker->features;
    const auto targetFeatures = target->features;
    pairDetailThread = std::jthread(
        [safeThis = juce::Component::SafePointer<MainComponent>(this),
         maskerFeatures,
         targetFeatures,
         maskerIndex,
         targetIndex](const std::stop_token stopToken)
        {
            auto timeline = analysis::PairAnalyzer{}.analyseTimeline(*maskerFeatures,
                                                                     *targetFeatures,
                                                                     stopToken);
            if (stopToken.stop_requested())
                return;

            juce::MessageManager::callAsync([safeThis,
                                             maskerIndex,
                                             targetIndex,
                                             timeline = std::move(timeline)]() mutable
            {
                if (safeThis != nullptr)
                    safeThis->pairAnalysisView.setTimeline(maskerIndex,
                                                           targetIndex,
                                                           std::move(timeline));
            });
        });
}

void MainComponent::setAnalysisRunning(const bool shouldBeRunning)
{
    analysisRunning = shouldBeRunning;
    addFilesButton.setEnabled(!analysisRunning);
    removeButton.setEnabled(!analysisRunning && !tracks.empty());
    selectAllButton.setEnabled(!analysisRunning && !tracks.empty());
    analyseButton.setEnabled(!analysisRunning && !tracks.empty());
    groupButton.setEnabled(!analysisRunning && trackTable.getNumSelectedRows() >= 2);
    sourceGroupButton.setEnabled(!analysisRunning && trackTable.getNumSelectedRows() >= 2);
    clearGroupingButton.setEnabled(!analysisRunning && trackTable.getNumSelectedRows() > 0);
    bulkFamilySelector.setEnabled(!analysisRunning && trackTable.getNumSelectedRows() > 0);
    bulkRoleSelector.setEnabled(!analysisRunning && trackTable.getNumSelectedRows() > 0);
    confirmSuggestionsButton.setEnabled(!analysisRunning && !tracks.empty());
    mixTemplateSelector.setEnabled(!analysisRunning);
    applyTemplateButton.setEnabled(!analysisRunning && !tracks.empty()
                                   && mixTemplateSelector.getSelectedId() == 2);
    trackTable.setEnabled(!analysisRunning);
    paletteSelector.setEnabled(!analysisRunning);
    workflowSelector.setEnabled(!analysisRunning);
    pairAnalysisView.setApplyState(!analysisRunning && reaperBridge.has_value()
                                   && matrixAvailable, planAppliedInReaper);
    updateStatus();
}

void MainComponent::updateStatus()
{
    if (analysisRunning)
    {
        statusLabel.setText(bridgeMessage.isNotEmpty() ? bridgeMessage : utf8("Analiza trwa…"),
                            juce::dontSendNotification);
        return;
    }

    const auto selectedCount = trackTable.getNumSelectedRows();
    const auto analysedCount = selectedCount == 0 ? tracks.size() : static_cast<std::size_t>(selectedCount);
    const auto source = reaperBridge ? juce::String("REAPER    |    ") : juce::String();
    statusLabel.setText(source + utf8("Załadowano: ")
                        + juce::String(static_cast<int>(tracks.size()))
                        + "    |    Do analizy: " + juce::String(static_cast<int>(analysedCount))
                        + (bridgeMessage.isNotEmpty() ? "    |    " + bridgeMessage : juce::String()),
                        juce::dontSendNotification);

    removeButton.setEnabled(!tracks.empty() && selectedCount > 0);
    selectAllButton.setEnabled(!tracks.empty());
    analyseButton.setEnabled(!tracks.empty());
    groupButton.setEnabled(selectedCount >= 2);
    sourceGroupButton.setEnabled(selectedCount >= 2);
    clearGroupingButton.setEnabled(selectedCount > 0);
    bulkFamilySelector.setEnabled(selectedCount > 0);
    bulkRoleSelector.setEnabled(selectedCount > 0);
    confirmSuggestionsButton.setEnabled(!tracks.empty());
    mixTemplateSelector.setEnabled(true);
    applyTemplateButton.setEnabled(!tracks.empty()
                                   && mixTemplateSelector.getSelectedId() == 2);
}

void MainComponent::applySelectedPalette()
{
    const auto& palette = paletteForId(paletteSelector.getSelectedId());
    for (std::size_t index = 0; index < tracks.size(); ++index)
        tracks[index].colour = palette[index % palette.size()];
    trackTable.repaint();
    refreshMatrixColours();
}

void MainComponent::refreshMatrixColours()
{
    std::vector<juce::Colour> colours;
    colours.reserve(activeMatrixPaths.size());
    for (const auto& path : activeMatrixPaths)
    {
        const auto track = std::ranges::find_if(tracks, [&path](const TrackEntry& entry)
        {
            return entry.file.getFullPathName() == path;
        });
        if (track != tracks.end())
            colours.push_back(track->colour);
    }
    pairAnalysisView.updateTrackColours(colours);
}

void MainComponent::showTracksView()
{
    trackTable.setVisible(true);
    pairAnalysisView.setVisible(false);
    groupButton.setVisible(true);
    sourceGroupButton.setVisible(true);
    clearGroupingButton.setVisible(true);
    bulkFamilySelector.setVisible(true);
    bulkRoleSelector.setVisible(true);
    confirmSuggestionsButton.setVisible(true);
    mappingHintLabel.setVisible(true);
    mixTemplateSelector.setVisible(true);
    applyTemplateButton.setVisible(true);
    templateHintLabel.setVisible(true);
    tracksViewButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    matrixViewButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    resized();
}

void MainComponent::showMatrixView()
{
    if (!matrixAvailable)
        return;
    trackTable.setVisible(false);
    pairAnalysisView.setVisible(true);
    groupButton.setVisible(false);
    sourceGroupButton.setVisible(false);
    clearGroupingButton.setVisible(false);
    bulkFamilySelector.setVisible(false);
    bulkRoleSelector.setVisible(false);
    confirmSuggestionsButton.setVisible(false);
    mappingHintLabel.setVisible(false);
    mixTemplateSelector.setVisible(false);
    applyTemplateButton.setVisible(false);
    templateHintLabel.setVisible(false);
    tracksViewButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    matrixViewButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    resized();
}

juce::String MainComponent::formatDuration(const double seconds)
{
    const auto totalSeconds = std::max(0, static_cast<int>(std::round(seconds)));
    return juce::String::formatted("%d:%02d", totalSeconds / 60, totalSeconds % 60);
}
} // namespace mixdebugger
