#include "FrequencySweep2D.h"

#include <cmath>
#include <limits>
#include <stdexcept>

std::vector<FrequencySweepSample2D> FrequencySweep2D::evaluate(
    const std::vector<RayPath>& geometricPaths,
    const FrequencySweepDefinition2D& sweep,
    const PropagationConfig& basePropagation,
    const PathSelectionConfig2D& selection)
{
    if (!(sweep.startHz > 0.0) || !(sweep.stopHz >= sweep.startHz) ||
        !std::isfinite(sweep.startHz) || !std::isfinite(sweep.stopHz) ||
        sweep.points <= 0)
        throw std::invalid_argument("Invalid frequency sweep definition");
    if (sweep.points == 1 && sweep.stopHz != sweep.startHz)
        throw std::invalid_argument("A one-point frequency sweep requires start == stop");

    std::vector<FrequencySweepSample2D> samples;
    samples.reserve(static_cast<std::size_t>(sweep.points));
    for (int index = 0; index < sweep.points; ++index) {
        const double fraction = sweep.points == 1 ? 0.0
                               : static_cast<double>(index) / (sweep.points - 1);
        const double frequencyHz = sweep.startHz + fraction * (sweep.stopHz - sweep.startHz);
        PropagationConfig propagation = basePropagation;
        propagation.frequencyHz = frequencyHz;
        std::vector<RayPath> paths = geometricPaths;
        int candidateCount = 0;
        const auto summary = Propagation2D::evaluateAndSelect(paths, propagation, selection,
                                                              &candidateCount);

        FrequencySweepSample2D sample;
        sample.frequencyHz = frequencyHz;
        sample.candidatePathCount = candidateCount;
        sample.retainedPathCount = static_cast<int>(paths.size());
        sample.receivedPowerDbm = paths.empty()
                                  ? std::numeric_limits<double>::quiet_NaN()
                                  : summary.totalReceivedPowerDbm;
        sample.incoherentPowerDbm = paths.empty()
                                    ? std::numeric_limits<double>::quiet_NaN()
                                    : summary.totalIncoherentPowerDbm;
        sample.equivalentPathLossDb = paths.empty()
                                      ? std::numeric_limits<double>::quiet_NaN()
                                      : summary.equivalentPathLossDb;
        sample.rmsDelaySpreadS = paths.empty()
                                 ? std::numeric_limits<double>::quiet_NaN()
                                 : summary.rmsDelaySpreadS;
        for (const auto& path : paths) {
            if (path.reflectionOrder > 0) {
                ++sample.reflectedPathCount;
                if (!path.materialModelInRange)
                    ++sample.materialOutOfRangePathCount;
            }
        }
        samples.push_back(sample);
    }
    return samples;
}
