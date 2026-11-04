#include "cs01synth/IG05630BehavioralModel.h"

// Explicit instantiation of the shared DSP for this IC's stage/parameter profile.
template class cs01::VCFBehavioralCore<4, cs01::IG05630Parameters>;
