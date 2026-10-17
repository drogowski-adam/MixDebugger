#include <mixdebugger/analysis/ReferenceAnalysis.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
using namespace mixdebugger::analysis;
namespace {
struct Fixture {
    juce::File file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mixdebugger-test-" + juce::Uuid().toString() + ".wav");
    ~Fixture(){file.deleteFile();}
    void tone(double amplitude=0.1, double frequency=1000, double seconds=4, double rate=48000, bool reverse=false, int channels=2){
        juce::AudioBuffer<float> buffer(channels,static_cast<int>(seconds*rate));
        for(int i=0;i<buffer.getNumSamples();++i)for(int c=0;c<channels;++c)
            buffer.setSample(c,i,static_cast<float>(amplitude*std::sin(2*juce::MathConstants<double>::pi*frequency*i/rate)*(reverse&&c==1?-1:1)));
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(file),rate,static_cast<unsigned>(channels),24,{},0));
        REQUIRE(writer);REQUIRE(writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples()));
    }
    ReferenceResult analyse(double start=0,double end=0){std::atomic<bool> cancel{false};return analyseReferenceFile(file,start,end,cancel);}
};
}
TEST_CASE("Reference meter measures calibrated stereo sine and band placement"){
    Fixture f;f.tone();auto r=f.analyse();
    REQUIRE(r.integrated);CHECK(*r.integrated==Catch::Approx(-20.0).margin(0.15));
    CHECK(*r.peak==Catch::Approx(-20.0).margin(0.02));CHECK(*r.crest==Catch::Approx(3.0103).margin(0.02));
    CHECK(*r.correlation==Catch::Approx(1));CHECK(*r.sidePercent==Catch::Approx(0));CHECK(*r.monoLoss==Catch::Approx(0));
    CHECK(r.bandPercent[3]>99);REQUIRE(r.range);CHECK(*r.range<0.2);CHECK(r.timeline.size()==40);
}
TEST_CASE("Gain changes loudness but not tonal or stereo descriptors"){
    Fixture a,b;a.tone(0.1,90);b.tone(0.05,90);auto x=a.analyse(),y=b.analyse();
    CHECK(*x.integrated-*y.integrated==Catch::Approx(6.0206).margin(0.02));
    CHECK(x.bandPercent[1]==Catch::Approx(y.bandPercent[1]).margin(0.001));
    auto gains=matchedGainsDb(x,y);REQUIRE(gains);CHECK((*gains)[0]==Catch::Approx(-6.0206).margin(0.02));CHECK((*gains)[1]==Catch::Approx(0));
    CHECK(*x.integrated+(*gains)[0]==Catch::Approx(*y.integrated+(*gains)[1]));
}
TEST_CASE("Opposite polarity is not mistaken for silence in spectrum"){
    Fixture f;f.tone(0.1,1000,4,48000,true);auto r=f.analyse();
    CHECK(*r.correlation==Catch::Approx(-1));CHECK(*r.sidePercent==Catch::Approx(100));CHECK((!r.monoLoss || *r.monoLoss < -100));CHECK(r.bandPercent[3]>99);CHECK(r.integrated.has_value());
}
TEST_CASE("Silence has unavailable loudness and matching"){
    Fixture f;f.tone(0);auto r=f.analyse();CHECK_FALSE(r.integrated);CHECK_FALSE(r.peak);CHECK_FALSE(r.hasSpectrum);CHECK_FALSE(matchedGainsDb(r,r));
    CHECK(comparisonMarkdown(r,nullptr,"Mix").contains("silent"));
}
TEST_CASE("Independent ranges and sample rates are respected"){
    Fixture a,b;a.tone(0.1,1000,4,44100);b.tone(0.1,1000,4,96000);auto x=a.analyse(1,2),y=b.analyse(2,3);
    CHECK(x.start==1);CHECK(x.end==2);CHECK_FALSE(x.range);CHECK(*x.integrated==Catch::Approx(*y.integrated).margin(0.15));
    CHECK_THROWS(a.analyse(3,2));CHECK_THROWS(a.analyse(0,5));CHECK_THROWS(a.analyse(0,0.1));
}
TEST_CASE("Mono is measured as one channel and cancellation is honoured"){
    Fixture f;f.tone(0.1,1000,4,48000,false,1);auto r=f.analyse();CHECK(*r.integrated==Catch::Approx(-23.01).margin(0.15));
    std::atomic<bool> cancel{true};CHECK_THROWS(analyseReferenceFile(f.file,0,0,cancel));
}
TEST_CASE("Matched playback adds shared headroom without boosting"){
    ReferenceResult a,b;a.integrated=-9;b.integrated=-12;a.truePeak=2;b.truePeak=1;
    auto gains=matchedGainsDb(a,b);REQUIRE(gains);CHECK((*gains)[0]<=0);CHECK((*gains)[1]<=0);
    CHECK(*a.truePeak+(*gains)[0]<=-1);CHECK(*b.truePeak+(*gains)[1]<=-1);
    CHECK(*a.integrated+(*gains)[0]==Catch::Approx(*b.integrated+(*gains)[1]));
}
TEST_CASE("Mono playback duplication is compensated when matching stereo"){
    Fixture mono,stereo;mono.tone(0.1,1000,4,48000,false,1);stereo.tone();
    auto a=mono.analyse(),b=stereo.analyse();auto gains=matchedGainsDb(a,b);REQUIRE(gains);
    CHECK(std::abs((*gains)[0]-(*gains)[1])<0.02);
}
