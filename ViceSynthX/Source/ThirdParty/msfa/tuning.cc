// VICE SYNTH X replacement (original code): standard tuning in MSFA's log-frequency format
// (Q24 log2 of frequency in Hz, the convention used by Dx7Note / Freqlut).
#include "tuning.h"
#include <cmath>

namespace
{
    struct StandardTuning : public TuningState
    {
        int32_t midinote_to_logfreq (int midinote) override
        {
            // (1 << 24) * (log2 (440) - 69 / 12) plus an integer semitone step, so pitches match Dexed bit for bit
            constexpr int32_t base = 50857777;
            constexpr int32_t step = (1 << 24) / 12;
            return base + step * midinote;
        }
    };
}

std::shared_ptr<TuningState> createStandardTuning() { return std::make_shared<StandardTuning>(); }
