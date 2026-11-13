#pragma once

#include "RayTracingTypes.h"
#include "Materials2D.h"

#include <limits>
#include <vector>

struct PropagationConfig {
    double frequencyHz = 916e6;
    double txPowerDbm = 14.0;

    // Retained only for controlled regression against v0.1-v0.3.
    double reflectionLossDb = 6.0;
    double reflectionPhaseShiftDeg = 180.0;
    bool useFresnel = true;
    Polarization2D polarization = Polarization2D::TE;

    bool coherentCombination = true;
};

// Optional post-trace path selection. Defaults preserve every geometrically
// valid path. Any non-default threshold/cap must be reported with results
// because it changes the coherent/incoherent sum.
struct PathSelectionConfig2D {
    double minReceivedPowerDbm = -std::numeric_limits<double>::infinity();
    double maxPathDistanceM = 0.0;             // <=0 means unlimited.
    double maxIncidenceAngleDeg = 90.0;        // Applied to every reflection.
    int maxPaths = 0;                          // <=0 means unlimited.
};

struct PropagationSummary {
    double totalReceivedPowerDbm = -1e300;
    double totalIncoherentPowerDbm = -1e300;
    double equivalentPathLossDb = 1e300;
    double strongestPathPowerDbm = -1e300;
    int strongestPathIndex = -1;

    // Power-delay-profile metrics use individual path powers before coherent
    // phase summation. They characterize resolved multipath components.
    double meanDelayS = 0.0;
    double rmsDelaySpreadS = 0.0;
    double maxExcessDelayS = 0.0;
};

class Propagation2D {
  public:
    static constexpr double C0_MPS = 299792458.0;

    static double fsplDb(double distanceM, double frequencyHz);
    static double knifeEdgeV(const Point2D& tx, const Point2D& edge,
                             const Point2D& rx, double frequencyHz);
    static double knifeEdgeLossDb(double v);
    static double wavelengthM(double frequencyHz);
    static double pathDistanceM(const RayPath& path);

    static PropagationSummary evaluate(std::vector<RayPath>& paths, const PropagationConfig& cfg);
    static PropagationSummary evaluateAndSelect(std::vector<RayPath>& paths,
                                                const PropagationConfig& cfg,
                                                const PathSelectionConfig2D& selection,
                                                int *candidateCount = nullptr);
};
