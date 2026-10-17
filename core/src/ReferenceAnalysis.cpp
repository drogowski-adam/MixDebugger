#include <mixdebugger/analysis/ReferenceAnalysis.h>
#include <juce_dsp/juce_dsp.h>
#include <ebur128.h>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace mixdebugger::analysis {
namespace {
std::optional<double> finite(double x) { return std::isfinite(x) ? std::optional<double>(x) : std::nullopt; }
double db(double x) { return x > 0 ? 10 * std::log10(x) : -150; }
juce::String value(std::optional<double> x, const juce::String& unit = {}) {
    return x ? juce::String(*x, 1) + unit : "N/A";
}
struct Meter {
    ebur128_state* state;
    Meter(unsigned channels, unsigned long rate) : state(ebur128_init(channels, rate,
        EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK)) {
        if (!state) throw std::runtime_error("Could not initialise loudness meter.");
    }
    ~Meter() { ebur128_destroy(&state); }
};
}
ReferenceResult analyseReferenceFile(const juce::File& file, double start, double end,
                                     const std::atomic<bool>& cancel) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) throw std::runtime_error("Cannot decode audio. Use WAV, AIFF or FLAC.");
    if (reader->numChannels < 1 || reader->numChannels > 2)
        throw std::runtime_error("Only mono and stereo files are supported.");
    const double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    if (end == 0) end = duration;
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end > duration + 0.001 || end <= start)
        throw std::runtime_error("Invalid range. Use seconds within the audio file; end 0 means full length.");
    if (end - start < 0.4) throw std::runtime_error("Select at least 0.4 seconds for loudness analysis.");
    ReferenceResult result;
    result.name = file.getFileName(); result.start = start; result.end = std::min(end, duration);
    result.sampleRate = reader->sampleRate; result.channels = reader->numChannels;
    Meter meter(reader->numChannels, static_cast<unsigned long>(reader->sampleRate));
    const auto first = static_cast<juce::int64>(std::llround(start * reader->sampleRate));
    const auto last = std::min(reader->lengthInSamples, static_cast<juce::int64>(std::llround(end * reader->sampleRate)));
    const int chunk = std::max(1, static_cast<int>(std::llround(reader->sampleRate / 10)));
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), chunk);
    std::vector<float> interleaved(static_cast<size_t>(chunk) * reader->numChannels);
    constexpr int fftSize = 4096;
    juce::dsp::FFT fft(12);
    std::array<float, fftSize * 2> left {}, right {};
    std::array<double, 6> bandEnergy {}, sideEnergy {};
    int fftCount = 0;
    double sumL = 0, sumR = 0, sumLR = 0, sumM = 0, sumS = 0;
    const auto spectrum = [&] {
        for (int i = 0; i < fftSize; ++i) {
            const auto w = static_cast<float>(0.5 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * i / (fftSize - 1)));
            left[i] *= w; right[i] *= w;
        }
        fft.performRealOnlyForwardTransform(left.data()); fft.performRealOnlyForwardTransform(right.data());
        for (int bin = 1; bin < fftSize / 2; ++bin) {
            const auto hz = bin * reader->sampleRate / fftSize;
            for (size_t b = 0; b < bandEnergy.size(); ++b) if (hz >= bandEdges[b] && hz < bandEdges[b+1]) {
                const double lr = left[2*bin], li = left[2*bin+1], rr = right[2*bin], ri = right[2*bin+1];
                bandEnergy[b] += (lr*lr + li*li + rr*rr + ri*ri) / 2;
                sideEnergy[b] += ((lr-rr)*(lr-rr) + (li-ri)*(li-ri)) / 4;
            }
        }
        left.fill(0); right.fill(0); fftCount = 0;
    };
    for (auto pos = first; pos < last; pos += chunk) {
        if (cancel.load()) throw std::runtime_error("Analysis cancelled.");
        const auto count = static_cast<int>(std::min<juce::int64>(chunk, last-pos));
        if (!reader->read(&buffer, 0, count, pos, true, true)) throw std::runtime_error("Audio read failed.");
        double blockPeak = 0;
        for (int i = 0; i < count; ++i) {
            const double l = buffer.getSample(0,i), r = buffer.getSample(reader->numChannels == 2 ? 1 : 0,i);
            if (!std::isfinite(l) || !std::isfinite(r)) throw std::runtime_error("Audio contains non-finite samples.");
            interleaved[static_cast<size_t>(i)*reader->numChannels] = static_cast<float>(l);
            if (reader->numChannels == 2) interleaved[static_cast<size_t>(i)*2+1] = static_cast<float>(r);
            sumL += l*l; sumR += r*r; sumLR += l*r;
            sumM += (l+r)*(l+r)/4; sumS += (l-r)*(l-r)/4;
            blockPeak = std::max({blockPeak, std::abs(l), std::abs(r)});
            left[fftCount] = static_cast<float>(l); right[fftCount] = static_cast<float>(r);
            if (++fftCount == fftSize) spectrum();
        }
        if (ebur128_add_frames_float(meter.state, interleaved.data(), static_cast<size_t>(count)) != EBUR128_SUCCESS)
            throw std::runtime_error("Loudness measurement failed.");
        const double elapsed = static_cast<double>(pos+count-first) / reader->sampleRate;
        TimePoint point; point.seconds = elapsed; point.peakDb = db(blockPeak*blockPeak);
        double v = 0;
        if (elapsed >= 0.4 && ebur128_loudness_momentary(meter.state, &v) == EBUR128_SUCCESS) point.momentary = finite(v);
        if (elapsed >= 3 && ebur128_loudness_shortterm(meter.state, &v) == EBUR128_SUCCESS) point.shortTerm = finite(v);
        result.timeline.push_back(point);
    }
    if (fftCount > 0) spectrum();
    double v = 0;
    if (ebur128_loudness_global(meter.state, &v) == EBUR128_SUCCESS) result.integrated = finite(v);
    if (end-start >= 3 && result.integrated && ebur128_loudness_range(meter.state, &v) == EBUR128_SUCCESS) result.range = finite(v);
    double peak = 0, truePeak = 0;
    for (unsigned c=0; c<reader->numChannels; ++c) {
        ebur128_sample_peak(meter.state,c,&v); peak = std::max(peak,v);
        ebur128_true_peak(meter.state,c,&v); truePeak = std::max(truePeak,v);
    }
    if (peak > 0) result.peak = db(peak*peak);
    if (truePeak > 0) result.truePeak = db(truePeak*truePeak);
    if (sumL+sumR > 0) {
        result.rms = db((sumL+sumR)/(2*static_cast<double>(last-first)));
        result.crest = *result.peak - *result.rms;
        result.sidePercent = 100*sumS/(sumM+sumS);
        result.monoLoss = sumM > 0 ? finite(db(sumM/((sumL+sumR)/2))) : std::nullopt;
    }
    if (sumL > 0 && sumR > 0) result.correlation = std::clamp(sumLR/std::sqrt(sumL*sumR), -1.0, 1.0);
    const double total = std::accumulate(bandEnergy.begin(),bandEnergy.end(),0.0);
    result.hasSpectrum = total > 0;
    for (size_t b=0; b<6; ++b) {
        result.bandPercent[b] = total > 0 ? 100*bandEnergy[b]/total : 0;
        result.bandSidePercent[b] = bandEnergy[b] > 0 ? 100*sideEnergy[b]/bandEnergy[b] : 0;
    }
    return result;
}
std::optional<std::array<double, 2>> matchedGainsDb(const ReferenceResult& a, const ReferenceResult& b) {
    if (!a.integrated || !b.integrated || !a.truePeak || !b.truePeak) return {};
    // Playback duplicates mono into two output channels. Compensate for that
    // channel energy here, while preserving single-channel meter readings.
    const double loudnessA = *a.integrated + (a.channels == 1 ? 3.01029995664 : 0);
    const double loudnessB = *b.integrated + (b.channels == 1 ? 3.01029995664 : 0);
    const double target = std::min(loudnessA,loudnessB);
    std::array<double,2> gain {target-loudnessA, target-loudnessB};
    const double trim = std::max({0.0,*a.truePeak+gain[0]+1,*b.truePeak+gain[1]+1});
    gain[0] -= trim; gain[1] -= trim; return gain;
}
juce::String comparisonMarkdown(const ReferenceResult& a, const ReferenceResult* b, const juce::String& material) {
    juce::String text = "# MixDebugger comparison\n\nMaterial: " + material + "\n\n";
    const auto source = [](const ReferenceResult& r) {
        return r.name.replaceCharacters("|\r\n", "   ") + " (" + juce::String(r.start,2) + "-" + juce::String(r.end,2) + " s, "
            + juce::String(r.sampleRate,0) + " Hz, " + juce::String(r.channels) + " channels)";
    };
    text += "Mix: " + source(a) + "\n\n";
    if (b) text += "Reference: " + source(*b) + "\n\n";
    text += "## What differs\n\n";
    if (!a.integrated) text += "- The mix is silent or below the loudness gate. Loudness matching is unavailable.\n";
    if (b) {
        if (a.integrated && b->integrated) {
            const auto delta = *a.integrated-*b->integrated;
            text += "- Your mix is " + juce::String(std::abs(delta),1) + " LU " + (delta >= 0 ? "louder" : "quieter") + " than the reference at original levels.\n";
        }
        if (a.hasSpectrum && b->hasSpectrum) {
            size_t largest=0;
            for (size_t i=1;i<6;++i) if (std::abs(a.bandPercent[i]-b->bandPercent[i]) > std::abs(a.bandPercent[largest]-b->bandPercent[largest])) largest=i;
            const double delta=a.bandPercent[largest]-b->bandPercent[largest];
            text += "- Largest tonal difference: " + juce::String(bandNames[largest]) + " occupies " + juce::String(std::abs(delta),1)
                + " percentage points " + (delta>=0 ? "more" : "less") + " of the measured spectrum in your mix.\n";
        }
        if (a.crest && b->crest) text += "- Peaks stand " + juce::String(std::abs(*a.crest-*b->crest),1) + " dB "
            + (*a.crest>=*b->crest ? "further above" : "closer to") + " the average level in your mix.\n";
        if (a.sidePercent && b->sidePercent) text += "- Your mix has " + juce::String(std::abs(*a.sidePercent-*b->sidePercent),1)
            + " percentage points " + (*a.sidePercent>=*b->sidePercent ? "more" : "less") + " side energy (a stereo-width indicator).\n";
        if (const auto gain=matchedGainsDb(a,*b)) text += "- Matched playback gain: mix " + juce::String((*gain)[0],1) + " dB, reference " + juce::String((*gain)[1],1) + " dB. Original measurements stay unchanged.\n";
    } else text += "- Load a reference to describe how your mix compares.\n";
    if (a.truePeak && *a.truePeak >= 0) text += "- Mix true peak reaches or exceeds 0 dBTP. This indicates limited peak headroom, not proof of audible distortion.\n";
    if (a.sidePercent && *a.sidePercent > 99.9) text += "- Almost all energy is in the side signal: the mono sum nearly cancels.\n";
    text += "\n## Measurements\n\n| Metric | Mix | Reference | Mix minus reference |\n|---|---:|---:|---:|\n";
    const auto row=[&](const juce::String& label, std::optional<double> x, std::optional<double> y, const juce::String& unit) {
        text += "| " + label + " | " + value(x,unit) + " | " + value(y,unit) + " | " + value(x && y ? finite(*x-*y) : std::nullopt) + " |\n";
    };
    row("Integrated loudness",a.integrated,b?b->integrated:std::nullopt," LUFS");
    row("Loudness range",a.range,b?b->range:std::nullopt," LU");
    row("Sample peak",a.peak,b?b->peak:std::nullopt," dBFS");
    row("True peak",a.truePeak,b?b->truePeak:std::nullopt," dBTP");
    row("Peak headroom",a.truePeak?finite(-*a.truePeak):std::nullopt,b&&b->truePeak?finite(-*b->truePeak):std::nullopt," dB");
    row("RMS",a.rms,b?b->rms:std::nullopt," dBFS");
    row("Crest factor",a.crest,b?b->crest:std::nullopt," dB");
    row("L/R correlation",a.correlation,b?b->correlation:std::nullopt,"");
    row("Side energy",a.sidePercent,b?b->sidePercent:std::nullopt," %");
    row("Mono energy change",a.monoLoss,b?b->monoLoss:std::nullopt," dB");
    text += "\n## Tonal balance\n\nBand shares are level-independent, relative to total energy from 20 Hz to 20 kHz (or Nyquist). Differences are percentage points.\n\n| Band | Mix | Reference | Difference |\n|---|---:|---:|---:|\n";
    for (size_t i=0;i<6;++i) row(juce::String(bandNames[i])+" ("+juce::String(bandEdges[i],0)+"-"+juce::String(bandEdges[i+1],0)+" Hz)",
        a.hasSpectrum?finite(a.bandPercent[i]):std::nullopt,b&&b->hasSpectrum?finite(b->bandPercent[i]):std::nullopt," %");
    text += "\n## Stereo by band\n\n| Band side energy | Mix | Reference | Difference |\n|---|---:|---:|---:|\n";
    for (size_t i=0;i<6;++i) row(bandNames[i],a.bandPercent[i]>0?finite(a.bandSidePercent[i]):std::nullopt,
        b&&b->bandPercent[i]>0?finite(b->bandSidePercent[i]):std::nullopt," %");
    text += "\n## Moments to compare\n\nTimes are relative to each selected passage. Loudness history uses original levels.\n\n";
    const auto moments=[&](const ReferenceResult& r,const juce::String& name) {
        const TimePoint* loudest=nullptr;
        const TimePoint* shortTerm=nullptr;
        for(const auto& p:r.timeline){
            if(p.momentary && (!loudest || *p.momentary>*loudest->momentary))loudest=&p;
            if(p.shortTerm && (!shortTerm || *p.shortTerm>*shortTerm->shortTerm))shortTerm=&p;
        }
        if(loudest)text += "- " + name + ": loudest 400 ms window ends at " + juce::String(loudest->seconds,1) + " s (" + value(loudest->momentary," LUFS") + ").\n";
        if(shortTerm)text += "- " + name + ": loudest 3 s window ends at " + juce::String(shortTerm->seconds,1) + " s (" + value(shortTerm->shortTerm," LUFS") + ").\n";
    };
    moments(a,"Mix");if(b)moments(*b,"Reference");
    text += "\n## Reading the results\n\nLUFS describes perceived programme level. LRA describes longer-term variation; short excerpts may not represent a song. Crest factor describes peak-to-average contrast, not a compressor diagnosis. Side energy describes stereo distribution, not quality. Mono change compares (L+R)/2 energy with average L/R energy; negative values mean energy loss. N/A means unavailable, including silence or complete mono cancellation.\n\nThese are observations, not quality scores or mastering targets. Different arrangements can explain differences. Select comparable passages; no automatic section alignment is performed. Loudness matching only affects playback.\n";
    return text;
}
}
