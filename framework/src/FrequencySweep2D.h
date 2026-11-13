#pragma once

#include "Propagation2D.h"

#include <vector>

struct FrequencySweepDefinition2D {
    double startHz = 0.0;
    double stopHz = 0.0;
    int points = 0;
};

struct FrequencySweepSample2D {
    double frequencyHz = 0.0;
    int candidatePathCount = 0;
    int retainedPathCount = 0;
    int reflectedPathCount = 0;
    int materialOutOfRangePathCount = 0;
    double receivedPowerDbm = -1e300;
    double incoherentPowerDbm = -1e300;
    double equivalentPathLossDb = 1e300;
    double rmsDelaySpreadS = 0.0;
};

// Re-evaluates a fixed geometric multipath set over frequency. Ray geometry is
// intentionally generated once: v1.2 studies the transfer response of the same
// deterministic paths, while Fresnel, FSPL, phase and selection are recomputed
// independently at every frequency.
class FrequencySweep2D {
  public:
    static std::vector<FrequencySweepSample2D> evaluate(
        const std::vector<RayPath>& geometricPaths,
        const FrequencySweepDefinition2D& sweep,
        const PropagationConfig& basePropagation,
        const PathSelectionConfig2D& selection = {});
};
