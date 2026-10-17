#include <mixdebugger/analysis/MasterBalanceAnalyzer.h>
#include <mixdebugger/dsp/ErbFilterBank.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Master balance is unavailable without analysed frames")
{
    const auto result = mixdebugger::analysis::MasterBalanceAnalyzer{}.analyse({});
    CHECK(result.assessment == mixdebugger::analysis::MasterBalanceAssessment::unavailable);
}

TEST_CASE("A deliberately exaggerated low end is reported as low heavy")
{
    mixdebugger::analysis::TrackFeatures features;
    features.frameCount = 1;
    features.centreFrequenciesHz = mixdebugger::dsp::ErbFilterBank{}.centreFrequenciesHz();
    features.averageDb.fill(-50.0f);
    for (std::size_t band = 0; band < features.bandCount; ++band)
    {
        if (features.centreFrequenciesHz[band] < 160.0f)
            features.averageDb[band] = -20.0f;
    }

    const auto result = mixdebugger::analysis::MasterBalanceAnalyzer{}.analyse(features);
    CHECK(result.assessment == mixdebugger::analysis::MasterBalanceAssessment::lowHeavy);
    CHECK(result.suggestedAdjustmentDb[0] < 0.0f);
    CHECK(result.suggestedAdjustmentDb[0] >= -3.0f);
}
