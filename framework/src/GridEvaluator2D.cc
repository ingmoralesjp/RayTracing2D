#include "GridEvaluator2D.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void countPathTypes(const std::vector<RayPath>& paths, GridSample2D& sample)
{
    for (const auto& path : paths) {
        if (path.isDirect)
            ++sample.losPathCount;
        else if (path.diffractionOrder > 0)
            ++sample.diffractionPathCount;
        else if (path.reflectionOrder == 1)
            ++sample.firstOrderPathCount;
        else if (path.reflectionOrder == 2)
            ++sample.secondOrderPathCount;

        if (path.reflectionOrder > 0) {
            ++sample.materialEvaluatedPathCount;
            if (!path.materialModelInRange)
                ++sample.materialOutOfRangePathCount;
        }
    }
}
}

std::vector<GridSample2D> GridEvaluator2D::evaluate(const RayTracer2D& tracer,
                                                     const Point2D& tx,
                                                     const GridDefinition2D& grid,
                                                     const GridTraceOptions2D& options,
                                                     const PropagationConfig& propagation)
{
    if (!std::isfinite(grid.minX) || !std::isfinite(grid.maxX) ||
        !std::isfinite(grid.minY) || !std::isfinite(grid.maxY) ||
        !(grid.stepX > 0.0) || !(grid.stepY > 0.0) ||
        grid.maxX < grid.minX || grid.maxY < grid.minY)
        throw std::invalid_argument("Invalid grid definition");

    std::vector<GridSample2D> samples;
    for (double y = grid.minY; y <= grid.maxY + grid.stepY * 1e-9; y += grid.stepY) {
        for (double x = grid.minX; x <= grid.maxX + grid.stepX * 1e-9; x += grid.stepX) {
            Point2D rx{x, y};
            if (Geometry2D::samePoint(tx, rx))
                throw std::invalid_argument("Grid must not contain the transmitter point");

            GridSample2D sample;
            sample.position = rx;
            sample.txDomain=tracer.classifyPoint(tx);sample.rxDomain=tracer.classifyPoint(rx);
            std::vector<RayPath> paths;
            TraceResult result;

            if (sample.txDomain!=PointDomain2D::EXTERIOR || sample.rxDomain!=PointDomain2D::EXTERIOR) {
                sample.blocked=true; // excluded, distinct from exterior with no path
            }
            else if (options.mode == GridMode2D::FSPL_BASELINE) {
                RayPath direct;
                direct.id = "FSPL_BASELINE";
                direct.points = {tx, rx};
                direct.isDirect = true;
                paths.push_back(direct);
                sample.blocked = tracer.isBlocked(tx, rx); // diagnostic only; baseline ignores it.
            }
            else {
                result = tracer.trace(tx, rx, options.los, options.firstReflection,
                                      options.secondReflection, options.diffraction,
                                      options.selection.maxPathDistanceM);
                sample.blocked = result.directBlocked;
                sample.firstOrderTheoretical = result.statistics.firstOrderTheoretical;
                sample.firstOrderCandidates = result.statistics.firstOrderCandidates;
                sample.firstOrderPruned = result.statistics.firstOrderPruned;
                sample.secondOrderTheoretical = result.statistics.secondOrderTheoretical;
                sample.secondOrderCandidates = result.statistics.secondOrderCandidates;
                sample.secondOrderPruned = result.statistics.secondOrderPruned;
                sample.diffractionCandidates = result.statistics.diffractionCandidates;
                paths = std::move(result.paths);
            }

            int candidateCount = 0;
            auto summary = Propagation2D::evaluateAndSelect(paths, propagation,
                                                           options.selection, &candidateCount);
            sample.candidatePathCount = candidateCount;
            sample.pathCount = static_cast<int>(paths.size());
            countPathTypes(paths, sample);

            if (paths.empty()) {
                sample.receivedPowerDbm = std::numeric_limits<double>::quiet_NaN();
                sample.incoherentPowerDbm = std::numeric_limits<double>::quiet_NaN();
                sample.equivalentPathLossDb = std::numeric_limits<double>::quiet_NaN();
                sample.strongestPathPowerDbm = std::numeric_limits<double>::quiet_NaN();
                sample.meanDelayS = std::numeric_limits<double>::quiet_NaN();
                sample.rmsDelaySpreadS = std::numeric_limits<double>::quiet_NaN();
                sample.maxExcessDelayS = std::numeric_limits<double>::quiet_NaN();
            }
            else {
                sample.receivedPowerDbm = summary.totalReceivedPowerDbm;
                sample.incoherentPowerDbm = summary.totalIncoherentPowerDbm;
                sample.equivalentPathLossDb = summary.equivalentPathLossDb;
                sample.strongestPathPowerDbm = summary.strongestPathPowerDbm;
                sample.meanDelayS = summary.meanDelayS;
                sample.rmsDelaySpreadS = summary.rmsDelaySpreadS;
                sample.maxExcessDelayS = summary.maxExcessDelayS;
            }
            samples.push_back(sample);
        }
    }
    return samples;
}
