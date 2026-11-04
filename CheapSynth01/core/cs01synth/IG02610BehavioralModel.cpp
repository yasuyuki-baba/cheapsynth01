#include "cs01synth/IG02610BehavioralModel.h"

// Explicit instantiation of the shared DSP for this IC's stage/parameter profile.
template class cs01::VCFBehavioralCore<2, cs01::IG02610Parameters>;
