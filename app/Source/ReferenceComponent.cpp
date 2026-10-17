#include "ReferenceComponent.h"
#include <cmath>

namespace mixdebugger {
namespace {
const juce::Colour background(0xff101822), panel(0xff1c2836), ink(0xffedf3f7), muted(0xffa9bbca), cyan(0xff5de0cf), orange(0xffffbb76);
juce::String number(std::optional<double> value) { return value ? juce::String(*value,1) : "N/A"; }
}
MainComponent::MainComponent(const juce::String& commandLine) {
    formats.registerBasicFormats();
    readAhead.startThread();
    heading.setText("MixDebugger  /  Reference comparison", juce::dontSendNotification);
    heading.setFont(juce::Font(juce::FontOptions(26.0f,juce::Font::bold)));
    subtitle.setText("Offline analysis. Understand how your mix compares. All audio stays on your computer.",juce::dontSendNotification);
    material.addItem("Mix / pre-master",1); material.addItem("Finished master",2); material.setSelectedId(1);
    material.onChange=[this]{ updateReport(); };
    for (int i=0;i<2;++i) {
        auto& slot=slots[static_cast<size_t>(i)];
        slot.load.setButtonText(i==0?"Load mix (A)":"Load reference (B)");
        slot.load.onClick=[this,i]{ chooseFile(i); };
        slot.name.setText(i==0?"Choose audio or send a master render from REAPER":"Choose a reference track",juce::dontSendNotification);
        slot.rangeLabel.setText("Range in seconds: start / end (0 = file end)",juce::dontSendNotification);
        for (auto* edit : {&slot.start,&slot.end}) {
            edit->setText("0"); edit->setInputRestrictions(12,"0123456789.");
            edit->onTextChange=[this]{ invalidate(); };
            edit->setColour(juce::TextEditor::backgroundColourId,background);
            edit->setColour(juce::TextEditor::textColourId,ink);
            addAndMakeVisible(*edit);
        }
        slot.start.setTooltip("Start of this file's comparison passage, in seconds.");
        slot.end.setTooltip("End of this passage in seconds. Zero selects the end of the file.");
        addAndMakeVisible(slot.load); addAndMakeVisible(slot.name); addAndMakeVisible(slot.rangeLabel);
    }
    for (auto* component : std::initializer_list<juce::Component*>{&heading,&subtitle,&status,&summary,&position,&material,&analyse,&cancelButton,&playA,&playB,&stop,&exportButton,&match,&details,&charts,&report}) addAndMakeVisible(*component);
    for (auto* label : {&heading,&subtitle,&status,&summary,&position,&slots[0].name,&slots[1].name,&slots[0].rangeLabel,&slots[1].rangeLabel}) label->setColour(juce::Label::textColourId,ink);
    subtitle.setColour(juce::Label::textColourId,muted);
    summary.setJustificationType(juce::Justification::topLeft);
    summary.setFont(juce::Font(juce::FontOptions(16.0f)));
    report.setMultiLine(true); report.setReadOnly(true); report.setScrollbarsShown(true);
    report.setColour(juce::TextEditor::backgroundColourId,panel); report.setColour(juce::TextEditor::textColourId,ink);
    report.setFont(juce::Font(juce::FontOptions(14.0f))); report.setVisible(false);
    match.setToggleState(true,juce::dontSendNotification); match.onClick=[this]{updateGain();};
    details.onClick=[this]{ report.setVisible(details.getToggleState()); charts.setVisible(!details.getToggleState()); resized(); };
    analyse.onClick=[this]{runAnalysis();}; cancelButton.onClick=[this]{cancelled=true;status.setText("Cancelling...",juce::dontSendNotification);};
    playA.onClick=[this]{play(0);}; playB.onClick=[this]{play(1);}; stop.onClick=[this]{stopPlayback();};
    exportButton.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Save comparison report",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("mix-comparison.md"),"*.md");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
            [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& c){if(safe && c.getResult()!=juce::File()) {
                const auto ok=c.getResult().replaceWithText(safe->reportText);
                safe->status.setText(ok?"Report exported.":"Could not save report.",juce::dontSendNotification);
            }});
    };
    setSize(1180,850); startTimerHz(10); invalidate(); openFromCommandLine(commandLine);
}
MainComponent::~MainComponent() {
    stopTimer(); cancelled=true; if(worker.joinable()) worker.join();
    shutdownAudio(); transport.setSource(nullptr); playbackSource.reset(); readAhead.stopThread(2000);
}
void MainComponent::chooseFile(int index) {
    chooser=std::make_unique<juce::FileChooser>(index==0?"Choose your mix":"Choose reference",juce::File(),"*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this),index](const juce::FileChooser& c){if(safe && c.getResult().existsAsFile()) safe->setFile(index,c.getResult());});
}
void MainComponent::setFile(int index,const juce::File& file) {
    if(busy) { status.setText("Finish or cancel analysis before loading another file.",juce::dontSendNotification);return; }
    slots[static_cast<size_t>(index)].file=file;
    slots[static_cast<size_t>(index)].name.setText(file.getFileName(),juce::dontSendNotification);
    slots[static_cast<size_t>(index)].start.setText("0",false);slots[static_cast<size_t>(index)].end.setText("0",false);
    invalidate();
}
void MainComponent::openFromCommandLine(const juce::String& commandLine) {
    const auto args=juce::StringArray::fromTokens(commandLine,true);
    for(int i=0;i+1<args.size();++i) {
        if(args[i]=="--mix-file") setFile(0,juce::File(args[i+1].unquoted()));
        if(args[i]=="--reference-file") setFile(1,juce::File(args[i+1].unquoted()));
    }
    if(args.contains("--analyse"))runAnalysis();
}
void MainComponent::invalidate() {
    stopPlayback();
    for(auto& slot:slots) slot.result.reset();
    charts.data={}; charts.repaint(); reportText.clear(); report.clear();
    summary.setText("Load your mix and a reference, then choose whole tracks or comparable passages.\nThe report describes differences without scoring your music.",juce::dontSendNotification);
    status.setText("Ready for offline analysis. End = 0 selects the rest of the file.",juce::dontSendNotification);
    playA.setEnabled(false);playB.setEnabled(false);exportButton.setEnabled(false);cancelButton.setEnabled(false);
}
void MainComponent::runAnalysis() {
    if(busy)return;
    if(!slots[0].file.existsAsFile()){status.setText("Load a mix first.",juce::dontSendNotification);return;}
    for(const auto& slot:slots)for(const auto* edit:{&slot.start,&slot.end}){
        const auto input=edit->getText();
        if(input.isEmpty() || input=="." || input.retainCharacters(".").length()>1){status.setText("Enter a valid number of seconds in each range field.",juce::dontSendNotification);return;}
    }
    if(worker.joinable())worker.join();
    invalidate(); cancelled=false; busy=true;
    analyse.setEnabled(false);cancelButton.setEnabled(true);
    std::array<juce::File,2> files; std::array<double,2> starts{},ends{};
    for(size_t i=0;i<2;++i){
        files[i]=slots[i].file; starts[i]=slots[i].start.getText().getDoubleValue();ends[i]=slots[i].end.getText().getDoubleValue();
        slots[i].load.setEnabled(false);slots[i].start.setEnabled(false);slots[i].end.setEnabled(false);
    }
    status.setText("Analysing audio in the background...",juce::dontSendNotification);
    worker=std::thread([this,files,starts,ends,safe=juce::Component::SafePointer<MainComponent>(this)]{
        std::array<std::optional<analysis::ReferenceResult>,2> results; juce::String error;
        try {for(size_t i=0;i<2;++i)if(files[i]!=juce::File())results[i]=analysis::analyseReferenceFile(files[i],starts[i],ends[i],cancelled);if(cancelled.load())throw std::runtime_error("Analysis cancelled.");}
        catch(const std::exception& e){error=e.what();}
        juce::MessageManager::callAsync([safe,results=std::move(results),error]()mutable{
            if(!safe)return;
            safe->busy=false;safe->analyse.setEnabled(true);safe->cancelButton.setEnabled(false);
            for(size_t i=0;i<2;++i){safe->slots[i].load.setEnabled(true);safe->slots[i].start.setEnabled(true);safe->slots[i].end.setEnabled(true);}
            if(error.isNotEmpty()){safe->status.setText(error,juce::dontSendNotification);return;}
            for(size_t i=0;i<2;++i)safe->slots[i].result=std::move(results[i]);
            safe->updateReport();safe->status.setText("Analysis complete. After changing your REAPER mix, send a new render from Extensions.",juce::dontSendNotification);
        });
    });
}
void MainComponent::updateReport() {
    if(!slots[0].result)return;
    reportText=analysis::comparisonMarkdown(*slots[0].result,slots[1].result?&*slots[1].result:nullptr,material.getText());
    report.setText(reportText,false);
    summary.setText(reportText.fromFirstOccurrenceOf("## What differs\n\n",false,false).upToFirstOccurrenceOf("\n## Measurements",false,false),juce::dontSendNotification);
    for(size_t i=0;i<2;++i)charts.data[i]=slots[i].result;
    charts.repaint();playA.setEnabled(true);playB.setEnabled(slots[1].result.has_value());exportButton.setEnabled(true);
    match.setEnabled(slots[1].result && analysis::matchedGainsDb(*slots[0].result,*slots[1].result).has_value());
}
void MainComponent::prepareToPlay(int size,double rate){const juce::ScopedLock lock(playbackLock);deviceRate=rate;transport.prepareToPlay(size,rate);}
void MainComponent::releaseResources(){const juce::ScopedLock lock(playbackLock);transport.releaseResources();}
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info){
    const juce::ScopedLock lock(playbackLock);info.clearActiveBufferRegion();
    if(playingSlot<0)return;
    const auto remaining=std::max(0.0,playbackEnd-transport.getCurrentPosition());
    const auto count=std::min(info.numSamples,static_cast<int>(std::min(remaining*deviceRate,static_cast<double>(info.numSamples))));
    if(count>0){auto limited=info;limited.numSamples=count;transport.getNextAudioBlock(limited);}
}
void MainComponent::stopPlayback(){const juce::ScopedLock lock(playbackLock);transport.setSource(nullptr);playbackSource.reset();playingSlot=-1;}
void MainComponent::play(int index){
    const auto& slot=slots[static_cast<size_t>(index)];if(!slot.result)return;
    if(!deviceOpen){setAudioChannels(0,2);deviceOpen=true;}
    const juce::ScopedLock lock(playbackLock);
    const auto elapsed=playingSlot>=0?std::max(0.0,transport.getCurrentPosition()-playbackStart):0.0;
    transport.setSource(nullptr);playbackSource.reset();
    auto* reader=formats.createReaderFor(slot.file);
    if(!reader){status.setText("Cannot open audio for playback.",juce::dontSendNotification);playingSlot=-1;return;}
    playbackSource=std::make_unique<juce::AudioFormatReaderSource>(reader,true);
    transport.setSource(playbackSource.get(),32768,&readAhead,reader->sampleRate);
    playingSlot=index;playbackStart=slot.result->start;playbackEnd=slot.result->end;
    transport.setPosition(playbackStart+(elapsed<playbackEnd-playbackStart?elapsed:0));
    updateGain();transport.start();
}
void MainComponent::updateGain(){
    const juce::ScopedLock lock(playbackLock);if(playingSlot<0)return;
    double gain=0;
    if(match.getToggleState() && slots[0].result && slots[1].result)
        if(const auto gains=analysis::matchedGainsDb(*slots[0].result,*slots[1].result))gain=(*gains)[static_cast<size_t>(playingSlot)];
    transport.setGain(static_cast<float>(std::pow(10.0,gain/20)));
}
void MainComponent::timerCallback(){
    const juce::ScopedLock lock(playbackLock);
    if(playingSlot>=0){
        const auto current=transport.getCurrentPosition();
        position.setText(juce::String(playingSlot==0?"A  ":"B  ")+juce::String(current-playbackStart,1)+" / "+juce::String(playbackEnd-playbackStart,1)+" s",juce::dontSendNotification);
        if(current>=playbackEnd-1.0/deviceRate){transport.setSource(nullptr);playbackSource.reset();playingSlot=-1;}
    }
}
void MainComponent::paint(juce::Graphics& g){g.fillAll(background);g.setColour(panel);g.fillRoundedRectangle(20,96,static_cast<float>(getWidth()-40),128,12);}
void MainComponent::resized(){
    const int w=getWidth();heading.setBounds(24,16,w-280,34);subtitle.setBounds(24,54,w-48,26);material.setBounds(w-246,20,222,30);
    const int half=(w-64)/2;
    for(int i=0;i<2;++i){auto& s=slots[static_cast<size_t>(i)];const int x=32+i*(half+16);
        s.load.setBounds(x,108,150,30);s.name.setBounds(x+158,108,half-166,30);
        s.rangeLabel.setBounds(x,144,half,24);s.start.setBounds(x,176,100,28);s.end.setBounds(x+112,176,100,28);
    }
    analyse.setBounds(24,238,160,32);cancelButton.setBounds(192,238,80,32);exportButton.setBounds(w-188,238,164,32);
    status.setBounds(24,278,w-48,26);summary.setBounds(24,310,w-48,164);
    playA.setBounds(24,482,132,32);playB.setBounds(164,482,150,32);stop.setBounds(322,482,60,32);match.setBounds(394,482,160,32);position.setBounds(560,482,160,32);details.setBounds(w-220,482,196,32);
    charts.setBounds(24,530,w-48,getHeight()-550);report.setBounds(charts.getBounds());
}
void MainComponent::Charts::paint(juce::Graphics& g){
    g.fillAll(panel);if(!data[0]){g.setColour(muted);g.drawText("Tonal balance and loudness over time appear after analysis.",getLocalBounds(),juce::Justification::centred);return;}
    const int w=getWidth(),h=getHeight();
    g.setColour(ink);g.drawText("TONAL BALANCE  |  share of 20 Hz - 20 kHz energy",12,8,w/2-20,24,juce::Justification::left);
    g.drawText("LOUDNESS OVER TIME  |  momentary LUFS",w/2+12,8,w/2-24,24,juce::Justification::left);
    for(int i=0;i<6;++i){const int x=12+i*(w/2-24)/6;const int barW=(w/2-24)/6-10;
        g.setColour(muted);g.drawText(analysis::bandNames[static_cast<size_t>(i)],x,h-30,barW,22,juce::Justification::centred);
        for(size_t s=0;s<2;++s)if(data[s]){
            const auto p=data[s]->bandPercent[static_cast<size_t>(i)];const float height=static_cast<float>((h-92)*p/100);
            g.setColour(s==0?cyan:orange);const auto bx=static_cast<float>(x)+static_cast<float>(s*static_cast<size_t>(barW/2));
            g.fillRect(bx,static_cast<float>(h-34)-height,static_cast<float>(barW/2-2),height);
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(juce::String(p,1),static_cast<int>(bx),h-54-static_cast<int>(height),barW/2,20,juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(14.0f)));
        }
    }
    const juce::Rectangle<float> plot(static_cast<float>(w/2+48),42,static_cast<float>(w/2-64),static_cast<float>(h-84));
    for(int level=-60;level<=0;level+=15){const float y=plot.getBottom()-static_cast<float>(level+60)/60*plot.getHeight();
        g.setColour(muted.withAlpha(0.2f));g.drawHorizontalLine(static_cast<int>(y),plot.getX(),plot.getRight());
        g.setColour(muted);g.drawText(juce::String(level),w/2+10,static_cast<int>(y)-8,32,16,juce::Justification::right);
    }
    for(size_t s=0;s<2;++s)if(data[s]){
        juce::Path path;bool connected=false;const auto& r=*data[s];
        for(const auto& point:r.timeline){if(!point.momentary){connected=false;continue;}
            const float x=plot.getX()+static_cast<float>(point.seconds/(r.end-r.start))*plot.getWidth();
            const float y=plot.getBottom()-static_cast<float>(std::clamp(*point.momentary,-60.0,0.0)+60)/60*plot.getHeight();
            if(!connected)path.startNewSubPath(x,y);else path.lineTo(x,y);connected=true;
        }
        g.setColour(s==0?cyan:orange);g.strokePath(path,juce::PathStrokeType(1.8f));
    }
    g.setColour(muted);g.drawText("0%                  Relative passage position                  100%",w/2+40,h-32,w/2-56,22,juce::Justification::centred);
    g.setColour(cyan);g.drawText("A Mix: "+number(data[0]->integrated)+" LUFS",12,32,200,22,juce::Justification::left);
    if(data[1]){g.setColour(orange);g.drawText("B Reference: "+number(data[1]->integrated)+" LUFS",212,32,230,22,juce::Justification::left);}
}
}
