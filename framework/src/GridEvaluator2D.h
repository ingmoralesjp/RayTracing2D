#pragma once

#include "Propagation2D.h"
#include "RayTracer2D.h"

#include <vector>

struct GridDefinition2D {
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double stepX = 1.0;
    double stepY = 1.0;
};

enum class GridMode2D {
    RAY_TRACING,
    FSPL_BASELINE
};

struct GridTraceOptions2D {
    bool los = true;
    bool firstReflection = true;
    bool secondReflection = false;
    bool diffraction = false;
    GridMode2D mode = GridMode2D::RAY_TRACING;
    PathSelectionConfig2D selection;
};

struct GridSample2D {
    Point2D position;
    bool blocked = false;
    PointDomain2D txDomain = PointDomain2D::EXTERIOR;
    PointDomain2D rxDomain = PointDomain2D::EXTERIOR;

    int candidatePathCount = 0;
    int pathCount = 0;
    int losPathCount = 0;
    int firstOrderPathCount = 0;
    int secondOrderPathCount = 0;
    int diffractionPathCount = 0;

    long long firstOrderTheoretical = 0;
    long long firstOrderCandidates = 0;
    long long firstOrderPruned = 0;
    long long secondOrderTheoretical = 0;
    long long secondOrderCandidates = 0;
    long long secondOrderPruned = 0;
    int diffractionCandidates = 0;
    int materialEvaluatedPathCount = 0;
    int materialOutOfRangePathCount = 0;

    double receivedPowerDbm = -1e300;
    double incoherentPowerDbm = -1e300;
    double equivalentPathLossDb = 1e300;
    double strongestPathPowerDbm = -1e300;
    double meanDelayS = 0.0;
    double rmsDelaySpreadS = 0.0;
    double maxExcessDelayS = 0.0;
};

class GridEvaluator2D {
  public:
    static std::vector<GridSample2D> evaluate(const RayTracer2D& tracer,
                                               const Point2D& tx,
                                               const GridDefinition2D& grid,
                                               const GridTraceOptions2D& options,
                                               const PropagationConfig& propagation);
};
