#include <mixdebugger/analysis/ReferenceAnalysis.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: mixdebugger <mix.wav> [reference.wav]\nOutputs a Markdown report to stdout.\n";
        return 2;
    }
    try {
        std::atomic<bool> cancel {false};
        auto mix = mixdebugger::analysis::analyseReferenceFile(juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[1])),0,0,cancel);
        std::optional<mixdebugger::analysis::ReferenceResult> reference;
        if (argc == 3) reference = mixdebugger::analysis::analyseReferenceFile(juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[2])),0,0,cancel);
        std::cout << mixdebugger::analysis::comparisonMarkdown(mix,reference?&*reference:nullptr,"Mix / pre-master").toStdString();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
