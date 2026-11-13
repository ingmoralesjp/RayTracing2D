#pragma once

#include "Geometry2D.h"

#include <string>
#include <vector>

enum class InteractionType {
    LOS,
    REFLECTION,
    DIFFRACTION
};

struct RayInteraction {
    InteractionType type = InteractionType::LOS;
    Point2D position;
    std::string objectId;
    std::string material = "generic";
    double incidenceAngleRad = 0.0;
};

struct RayPath {
    std::string id;
    std::vector<Point2D> points;
    std::vector<RayInteraction> interactions;

    int reflectionOrder = 0;
    int diffractionOrder = 0;
    bool isDirect = false;

    // Filled by Propagation2D.
    double totalDistanceM = 0.0;
    double delayS = 0.0;
    double pathLossDb = 0.0;
    double phaseRad = 0.0;
    double receivedPowerDbm = 0.0;
    double reflectionCoefficientMagnitude = 1.0;
    double reflectionCoefficientPhaseRad = 0.0;
    bool materialModelInRange = true;
    double diffractionParameterV = 0.0;
    double diffractionLossDb = 0.0;
};

// Geometry-side accounting. These counters are deliberately separated from
// valid-path counts: in an urban map the expensive operation is testing many
// candidate surfaces/pairs even when only a few paths survive visibility tests.
struct TraceStatistics2D {
    long long firstOrderTheoretical = 0;
    long long firstOrderCandidates = 0;
    long long firstOrderPruned = 0;
    int firstOrderValid = 0;
    long long secondOrderTheoretical = 0;
    long long secondOrderCandidates = 0;
    long long secondOrderPruned = 0;
    int secondOrderValid = 0;
    int diffractionCandidates = 0;
    int diffractionValid = 0;
};

struct TraceResult {
    PointDomain2D txDomain = PointDomain2D::EXTERIOR;
    PointDomain2D rxDomain = PointDomain2D::EXTERIOR;
    bool supportedExterior() const { return txDomain==PointDomain2D::EXTERIOR && rxDomain==PointDomain2D::EXTERIOR; }
    bool directBlocked = false;
    std::vector<RayPath> paths;
    TraceStatistics2D statistics;
};
