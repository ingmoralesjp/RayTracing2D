#include "Geometry2D.h"
#include "RayTracer2D.h"
#include "Propagation2D.h"
#include "Materials2D.h"
#include "OsmImporter2D.h"
#include "GridEvaluator2D.h"
#include "FrequencySweep2D.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr double PI = 3.14159265358979323846;
bool close(double a, double b, double eps = 1e-7) { return std::fabs(a - b) <= eps; }

template <typename Function>
bool throwsInvalidArgument(Function function)
{
    try {
        function();
    }
    catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
}

int main()
{
    // Test A: mirror point over a horizontal wall y=400.
    Wall2D reflector{"reflector", {100, 400}, {800, 400}, "generic"};
    Point2D tx{200, 100};
    Point2D rx{700, 100};
    Point2D image = Geometry2D::mirrorAcrossLine(tx, reflector);
    assert(close(image.x, 200.0));
    assert(close(image.y, 700.0));

    // Test B: known first-order reflection point P=(450,400).
    RayTracer2D tracer({reflector});
    auto result = tracer.trace(tx, rx, true, true);
    assert(!result.directBlocked);
    assert(result.paths.size() == 2);

    const RayPath *reflected = nullptr;
    for (const auto& p : result.paths)
        if (!p.isDirect)
            reflected = &p;

    assert(reflected != nullptr);
    assert(reflected->points.size() == 3);
    assert(close(reflected->points[1].x, 450.0));
    assert(close(reflected->points[1].y, 400.0));

    // Test C: direct path blocking.
    Wall2D blocker{"blocker", {450, 80}, {450, 340}, "generic"};
    RayTracer2D blockedTracer({blocker});
    auto blocked = blockedTracer.trace({200, 200}, {700, 200}, true, false);
    assert(blocked.directBlocked);
    assert(blocked.paths.empty());

    // Test D: FSPL unit sanity check: 1 km at 916 MHz is about 91.69 dB.
    double fspl = Propagation2D::fsplDb(1000.0, 916e6);
    assert(std::fabs(fspl - 91.69) < 0.1);

    // Test E: collinear intersections are distinguished from a unique point.
    auto overlap = Geometry2D::classifySegmentIntersection({{0, 0}, {10, 0}}, {{4, 0}, {12, 0}});
    assert(overlap.kind == SegmentIntersectionKind::OVERLAP);
    assert(close(overlap.overlapStart.x, 4.0));
    assert(close(overlap.overlapEnd.x, 10.0));

    auto endpoint = Geometry2D::classifySegmentIntersection({{0, 0}, {10, 0}}, {{10, 0}, {10, 5}});
    assert(endpoint.kind == SegmentIntersectionKind::POINT);
    assert(close(endpoint.point.x, 10.0));
    assert(close(endpoint.point.y, 0.0));

    auto parallel = Geometry2D::classifySegmentIntersection({{0, 0}, {10, 0}}, {{0, 1}, {10, 1}});
    assert(parallel.kind == SegmentIntersectionKind::NONE);

    // Test F: point segments, endpoint contact and collinear blocking.
    auto pointHit = Geometry2D::classifySegmentIntersection({{5, 0}, {5, 0}}, {{0, 0}, {10, 0}});
    assert(pointHit.kind == SegmentIntersectionKind::POINT);
    assert(RayTracer2D({{"wall", {2, 0}, {8, 0}, "generic"}}).isBlocked({0, 0}, {10, 0}));
    assert(!RayTracer2D({{"wall", {10, 0}, {10, 5}, "generic"}}).isBlocked({0, 0}, {10, 0}));

    // Test G: invalid and ambiguous geometry is rejected deterministically.
    assert(throwsInvalidArgument([] { RayTracer2D({{"zero", {1, 1}, {1, 1}, "generic"}}); }));
    assert(throwsInvalidArgument([] {
        RayTracer2D({{"same", {0, 0}, {1, 0}, "generic"},
                     {"same", {0, 1}, {1, 1}, "generic"}});
    }));
    assert(throwsInvalidArgument([] { RayTracer2D({}).trace({1, 1}, {1, 1}); }));
    assert(throwsInvalidArgument([] {
        RayTracer2D({}).trace({0, 0}, {std::numeric_limits<double>::infinity(), 1});
    }));

    // Test H: scale-aware point comparison remains stable for large coordinates.
    assert(Geometry2D::samePoint({1e9, 1e9}, {1e9 + 0.5, 1e9}, 1e-9));
    assert(!Geometry2D::samePoint({1e9, 1e9}, {1e9 + 2.0, 1e9}, 1e-9));

    // Test I: analytical second-order reflection between parallel walls.
    Wall2D ceiling{"ceiling", {0, 400}, {900, 400}, "generic"};
    Wall2D floor{"floor", {0, 0}, {900, 0}, "generic"};
    auto secondOrder = RayTracer2D({ceiling, floor}).trace(tx, rx, false, false, true);
    const RayPath *ceilingFloor = nullptr;
    for (const auto& path : secondOrder.paths)
        if (path.id == "REFL2_ceiling_floor")
            ceilingFloor = &path;
    assert(ceilingFloor != nullptr);
    assert(ceilingFloor->reflectionOrder == 2);
    assert(ceilingFloor->points.size() == 4);
    assert(close(ceilingFloor->points[1].x, 387.5));
    assert(close(ceilingFloor->points[1].y, 400.0));
    assert(close(ceilingFloor->points[2].x, 637.5));
    assert(close(ceilingFloor->points[2].y, 0.0));
    assert(close(Propagation2D::pathDistanceM(*ceilingFloor), std::hypot(500.0, 800.0)));

    // Test J: Fresnel coefficients for a lossless dielectric (epsilon_r=4).
    MaterialModel2D lossless{"lossless", 4.0, 0.0, 0.0, 0.0, 0.1, 100.0};
    auto normalTe = Materials2D::fresnelReflection(lossless, 2.4e9, 0.0, Polarization2D::TE);
    auto normalTm = Materials2D::fresnelReflection(lossless, 2.4e9, 0.0, Polarization2D::TM);
    assert(close(normalTe.real(), -1.0 / 3.0));
    assert(close(normalTm.real(), 1.0 / 3.0));
    double brewster = std::atan(std::sqrt(4.0));
    auto brewsterTm = Materials2D::fresnelReflection(lossless, 2.4e9, brewster, Polarization2D::TM);
    assert(std::abs(brewsterTm) < 1e-10);
    auto grazingTe = Materials2D::fresnelReflection(lossless, 2.4e9, PI / 2.0, Polarization2D::TE);
    assert(close(std::abs(grazingTe), 1.0));

    // Test K: ITU material model and propagation integration.
    auto concrete = Materials2D::get("concrete");
    assert(close(concrete.relativePermittivity(2.4e9), 5.24));
    assert(close(concrete.conductivitySm(2.4e9), 0.0462 * std::pow(2.4, 0.7822)));
    assert(concrete.isFrequencyInRange(2.4e9));
    assert(!concrete.isFrequencyInRange(916e6));
    assert(throwsInvalidArgument([] { Materials2D::get("unknown-material"); }));

    auto materialTrace = RayTracer2D({{"reflector", {100, 400}, {800, 400}, "concrete"}})
                             .trace(tx, rx, false, true, false);
    assert(materialTrace.paths.size() == 1);
    assert(materialTrace.paths[0].interactions[0].material == "concrete");
    PropagationConfig fresnelConfig;
    fresnelConfig.frequencyHz = 2.4e9;
    fresnelConfig.useFresnel = true;
    fresnelConfig.polarization = Polarization2D::TE;
    auto materialSummary = Propagation2D::evaluate(materialTrace.paths, fresnelConfig);
    assert(std::isfinite(materialSummary.totalReceivedPowerDbm));
    assert(materialTrace.paths[0].reflectionCoefficientMagnitude > 0.0);
    assert(materialTrace.paths[0].reflectionCoefficientMagnitude < 1.0);
    assert(materialTrace.paths[0].materialModelInRange);

    // Test L: isolated knife-edge endpoints on a finite blocking screen.
    auto diffraction = blockedTracer.trace({200, 200}, {700, 200}, false, false, false, true);
    assert(diffraction.directBlocked);
    assert(diffraction.paths.size() == 2);
    assert(diffraction.paths[0].diffractionOrder == 1);
    assert(diffraction.paths[1].diffractionOrder == 1);
    PropagationConfig diffractionConfig;
    diffractionConfig.frequencyHz = 916e6;
    auto diffractionSummary = Propagation2D::evaluate(diffraction.paths, diffractionConfig);
    assert(std::isfinite(diffractionSummary.totalReceivedPowerDbm));
    assert(close(diffraction.paths[0].diffractionParameterV,
                 Propagation2D::knifeEdgeV({200, 200}, {450, 80}, {700, 200}, 916e6)));
    assert(diffraction.paths[0].diffractionLossDb > 0.0);
    assert(diffraction.paths[1].diffractionLossDb > diffraction.paths[0].diffractionLossDb);
    assert(close(Propagation2D::knifeEdgeLossDb(-1.0), 0.0));

    // Test M: closed OSM building becomes four metric finite walls.
    auto osm = OsmImporter2D::load("simulations/data/tiny_building.osm", 23.0, -106.0);
    assert(osm.sourceNodeCount == 4);
    assert(osm.buildingCount == 1);
    assert(osm.walls.size() == 4);
    assert(osm.walls[0].id == "osm_100_e0");
    assert(osm.walls[0].material == "brick");
    assert(std::fabs(Geometry2D::distance(osm.walls[0].a, osm.walls[0].b) - 19.96) < 0.1);
    assert(std::fabs(Geometry2D::distance(osm.walls[1].a, osm.walls[1].b) - 20.02) < 0.1);

    // Test N: deterministic grid order and free-space monotonicity.
    GridDefinition2D grid{10, 30, 0, 0, 10, 1};
    GridTraceOptions2D gridOptions;
    gridOptions.los = true;
    gridOptions.firstReflection = false;
    gridOptions.secondReflection = false;
    gridOptions.diffraction = false;
    PropagationConfig gridPropagation;
    auto samples = GridEvaluator2D::evaluate(RayTracer2D({}), {0, 0}, grid,
                                             gridOptions, gridPropagation);
    assert(samples.size() == 3);
    assert(close(samples[0].position.x, 10.0));
    assert(samples[0].pathCount == 1);
    assert(samples[0].receivedPowerDbm > samples[1].receivedPowerDbm);
    assert(samples[1].receivedPowerDbm > samples[2].receivedPowerDbm);
    assert(throwsInvalidArgument([&] {
        GridEvaluator2D::evaluate(RayTracer2D({}), {0, 0}, {0, 1, 0, 1, 0, 1},
                                  gridOptions, gridPropagation);
    }));


    // Test O: coherent combination convention - equal paths add, pi-shift paths cancel.
    RayPath coherentA;
    coherentA.id = "A";
    coherentA.points = {{0, 0}, {100, 0}};
    coherentA.isDirect = true;
    RayPath coherentB = coherentA;
    coherentB.id = "B";
    coherentB.isDirect = false;
    coherentB.reflectionOrder = 1;
    coherentB.interactions = {{InteractionType::REFLECTION, {50, 0}, "virtual", "generic", 0.0}};

    PropagationConfig phaseCfg;
    phaseCfg.frequencyHz = 2.4e9;
    phaseCfg.txPowerDbm = 0.0;
    phaseCfg.useFresnel = false;
    phaseCfg.reflectionLossDb = 0.0;
    phaseCfg.reflectionPhaseShiftDeg = 0.0;
    phaseCfg.coherentCombination = true;
    std::vector<RayPath> inPhase{coherentA, coherentA};
    auto inPhaseSummary = Propagation2D::evaluate(inPhase, phaseCfg);
    double onePathPower = inPhase[0].receivedPowerDbm;
    assert(std::fabs((inPhaseSummary.totalReceivedPowerDbm - onePathPower) - 6.0206) < 0.02);

    phaseCfg.reflectionPhaseShiftDeg = 180.0;
    std::vector<RayPath> outOfPhase{coherentA, coherentB};
    auto outOfPhaseSummary = Propagation2D::evaluate(outOfPhase, phaseCfg);
    assert(outOfPhaseSummary.totalReceivedPowerDbm < onePathPower - 100.0);

    // Test P: explicit path selection keeps the strongest path and reports candidates.
    phaseCfg.reflectionPhaseShiftDeg = 0.0;
    std::vector<RayPath> selectionPaths{coherentA, coherentB};
    PathSelectionConfig2D selectOne;
    selectOne.maxPaths = 1;
    int beforeSelection = 0;
    auto selectionSummary = Propagation2D::evaluateAndSelect(selectionPaths, phaseCfg, selectOne,
                                                             &beforeSelection);
    assert(beforeSelection == 2);
    assert(selectionPaths.size() == 1);
    assert(std::isfinite(selectionSummary.totalReceivedPowerDbm));

    // Test Q: the FSPL grid baseline stays finite behind a wall while RT blockage does not.
    Wall2D gridBlocker{"gridBlocker", {15, -5}, {15, 5}, "concrete"};
    RayTracer2D gridTracer({gridBlocker});
    GridDefinition2D baselineGrid{10, 30, 0, 0, 10, 1};
    GridTraceOptions2D baselineOptions;
    baselineOptions.los = true;
    baselineOptions.firstReflection = false;
    baselineOptions.secondReflection = false;
    baselineOptions.diffraction = false;
    baselineOptions.mode = GridMode2D::FSPL_BASELINE;
    auto baselineSamples = GridEvaluator2D::evaluate(gridTracer, {0, 0}, baselineGrid,
                                                      baselineOptions, gridPropagation);
    assert(baselineSamples.size() == 3);
    assert(std::isfinite(baselineSamples[0].receivedPowerDbm));
    assert(std::isfinite(baselineSamples[1].receivedPowerDbm));
    assert(std::isfinite(baselineSamples[2].receivedPowerDbm));
    assert(baselineSamples[1].blocked);

    baselineOptions.mode = GridMode2D::RAY_TRACING;
    auto blockedGridSamples = GridEvaluator2D::evaluate(gridTracer, {0, 0}, baselineGrid,
                                                         baselineOptions, gridPropagation);
    assert(std::isfinite(blockedGridSamples[0].receivedPowerDbm));
    assert(!std::isfinite(blockedGridSamples[1].receivedPowerDbm));
    assert(!std::isfinite(blockedGridSamples[2].receivedPowerDbm));

    // Test R: v1.1 synthetic urban OSM imports multiple buildings and normalizes masonry->brick.
    auto urbanOsm = OsmImporter2D::load("simulations/data/urban_small.osm", 23.0, -106.0);
    assert(urbanOsm.buildingCount == 9);
    assert(urbanOsm.walls.size() == 36);
    bool masonryNormalized = false;
    for (const auto& wall : urbanOsm.walls)
        if (wall.id.rfind("osm_103_", 0) == 0 && wall.material == "brick")
            masonryNormalized = true;
    assert(masonryNormalized);

    // Test S: end-to-end urban grid.  Each additional mechanism must preserve
    // or increase the number of receivers with at least one valid path, while
    // the synthetic map must actually exercise R1, R2 and diffraction.
    RayTracer2D urbanTracer(urbanOsm.walls);
    PropagationConfig urbanPropagation;
    urbanPropagation.frequencyHz = 2.4e9;
    urbanPropagation.txPowerDbm = 14.0;
    urbanPropagation.useFresnel = true;
    urbanPropagation.polarization = Polarization2D::TE;
    urbanPropagation.coherentCombination = true;
    GridDefinition2D urbanGrid{5, 215, 5, 215, 10, 10};
    Point2D urbanTx{22, 92};

    auto runUrbanCase = [&](GridMode2D mode, bool r1, bool r2, bool diffraction) {
        GridTraceOptions2D options;
        options.los = true;
        options.firstReflection = r1;
        options.secondReflection = r2;
        options.diffraction = diffraction;
        options.mode = mode;
        return GridEvaluator2D::evaluate(urbanTracer, urbanTx, urbanGrid, options, urbanPropagation);
    };
    auto fsplUrban = runUrbanCase(GridMode2D::FSPL_BASELINE, false, false, false);
    auto blockedUrban = runUrbanCase(GridMode2D::RAY_TRACING, false, false, false);
    auto r1Urban = runUrbanCase(GridMode2D::RAY_TRACING, true, false, false);
    auto r2Urban = runUrbanCase(GridMode2D::RAY_TRACING, true, true, false);
    auto fullUrban = runUrbanCase(GridMode2D::RAY_TRACING, true, true, true);

    auto finiteCount = [](const std::vector<GridSample2D>& v) {
        int n = 0;
        for (const auto& sample : v)
            if (std::isfinite(sample.receivedPowerDbm))
                ++n;
        return n;
    };
    assert(fsplUrban.size() == 484);
    int exteriorUrban=0;
    for(const auto&s:fsplUrban)exteriorUrban+=s.rxDomain==PointDomain2D::EXTERIOR && s.txDomain==PointDomain2D::EXTERIOR;
    assert(finiteCount(fsplUrban) == exteriorUrban);
    int nBlocked = finiteCount(blockedUrban);
    int nR1 = finiteCount(r1Urban);
    int nR2 = finiteCount(r2Urban);
    int nFull = finiteCount(fullUrban);
    assert(nBlocked <= nR1 && nR1 <= nR2 && nR2 <= nFull);

    bool sawR1 = false, sawR2 = false, sawD1 = false;
    for (const auto& sample : r1Urban) sawR1 = sawR1 || sample.firstOrderPathCount > 0;
    for (const auto& sample : r2Urban) sawR2 = sawR2 || sample.secondOrderPathCount > 0;
    for (const auto& sample : fullUrban) sawD1 = sawD1 || sample.diffractionPathCount > 0;
    assert(sawR1 && sawR2 && sawD1);

    // Test T: material-range diagnostics propagate to every grid cell/path.
    urbanPropagation.frequencyHz = 916e6;
    auto lowFrequencyUrban = runUrbanCase(GridMode2D::RAY_TRACING, true, true, true);
    int evaluatedMaterials = 0;
    int extrapolatedMaterials = 0;
    for (const auto& sample : lowFrequencyUrban) {
        evaluatedMaterials += sample.materialEvaluatedPathCount;
        extrapolatedMaterials += sample.materialOutOfRangePathCount;
    }
    assert(evaluatedMaterials > 0);
    assert(extrapolatedMaterials > 0);

    // Test U: a one-path sweep follows the exact 20log10(f) FSPL slope and
    // includes both requested endpoints.
    RayPath sweepLos;
    sweepLos.id = "SWEEP_LOS";
    sweepLos.points = {{0, 0}, {100, 0}};
    sweepLos.isDirect = true;
    PropagationConfig sweepPropagation;
    sweepPropagation.txPowerDbm = 0.0;
    sweepPropagation.useFresnel = true;
    auto sweep = FrequencySweep2D::evaluate({sweepLos}, {1.0e9, 2.0e9, 3}, sweepPropagation);
    assert(sweep.size() == 3);
    assert(close(sweep.front().frequencyHz, 1.0e9));
    assert(close(sweep.back().frequencyHz, 2.0e9));
    assert(close(sweep.front().receivedPowerDbm - sweep.back().receivedPowerDbm,
                 20.0 * std::log10(2.0), 1e-9));
    assert(sweep.front().candidatePathCount == 1 && sweep.front().retainedPathCount == 1);

    // Test V: range diagnostics are recalculated at every frequency rather
    // than inherited from the nominal-frequency evaluation.
    RayPath concreteReflection = *reflected;
    concreteReflection.interactions[0].material = "concrete";
    auto materialSweep = FrequencySweep2D::evaluate(
        {concreteReflection}, {0.9e9, 1.1e9, 3}, sweepPropagation);
    assert(materialSweep[0].materialOutOfRangePathCount == 1);
    assert(materialSweep[1].materialOutOfRangePathCount == 0);
    assert(materialSweep[2].materialOutOfRangePathCount == 0);
    assert(throwsInvalidArgument([&] {
        FrequencySweep2D::evaluate({sweepLos}, {2.0e9, 1.0e9, 2}, sweepPropagation);
    }));

    // Test W: segment-distance lower bounds used by v1.3 pruning.
    assert(close(Geometry2D::distancePointToSegment({5, 3}, {{0, 0}, {10, 0}}), 3.0));
    assert(close(Geometry2D::distancePointToSegment({-4, 0}, {{0, 0}, {10, 0}}), 4.0));
    assert(close(Geometry2D::distanceSegmentToSegment({{0, 0}, {10, 0}},
                                                       {{5, -2}, {5, 2}}), 0.0));
    assert(close(Geometry2D::distanceSegmentToSegment({{0, 0}, {10, 0}},
                                                       {{0, 4}, {10, 4}}), 4.0));

    // Test X: pre-trace pruning is conservative relative to the historical
    // post-trace max-distance filter and reports its accounting explicitly.
    Wall2D farWall{"far", {10000, 0}, {10000, 1000}, "generic"};
    RayTracer2D pruningTracer({reflector, farWall});
    auto exhaustivePaths = pruningTracer.trace(tx, rx, true, true, true, false);
    auto prunedPaths = pruningTracer.trace(tx, rx, true, true, true, false, 1000.0);
    PathSelectionConfig2D distanceSelection;
    distanceSelection.maxPathDistanceM = 1000.0;
    PropagationConfig pruningPropagation;
    pruningPropagation.useFresnel = false;
    Propagation2D::evaluateAndSelect(exhaustivePaths.paths, pruningPropagation, distanceSelection);
    Propagation2D::evaluateAndSelect(prunedPaths.paths, pruningPropagation, distanceSelection);
    assert(exhaustivePaths.paths.size() == prunedPaths.paths.size());
    for (std::size_t i = 0; i < exhaustivePaths.paths.size(); ++i) {
        assert(exhaustivePaths.paths[i].id == prunedPaths.paths[i].id);
        assert(close(exhaustivePaths.paths[i].receivedPowerDbm,
                     prunedPaths.paths[i].receivedPowerDbm, 1e-10));
    }
    assert(prunedPaths.statistics.firstOrderTheoretical == 2);
    assert(prunedPaths.statistics.firstOrderCandidates == 1);
    assert(prunedPaths.statistics.firstOrderPruned == 1);
    assert(prunedPaths.statistics.secondOrderTheoretical == 2);
    assert(prunedPaths.statistics.secondOrderCandidates == 0);
    assert(prunedPaths.statistics.secondOrderPruned == 2);

    // Test Y: coincident wall endpoints must not duplicate a physical D1 edge.
    // Two finite walls share the corner (5,0), and the direct segment grazes
    // that common corner. Older code produced two identical diffraction paths
    // at (5,0), one per incident wall, artificially double-counting power.
    Wall2D cornerWall1{"corner_w1", {5, 0}, {5, 3}, "concrete"};
    Wall2D cornerWall2{"corner_w2", {5, 0}, {8, 0}, "concrete"};
    RayTracer2D cornerTracer({cornerWall1, cornerWall2});
    auto cornerTrace = cornerTracer.trace({0, -1}, {10, 1}, true, false, false, true);
    int sharedCornerCount = 0;
    for (const auto& path : cornerTrace.paths) {
        if (path.diffractionOrder == 1 && path.points.size() == 3 &&
            close(path.points[1].x, 5.0) && close(path.points[1].y, 0.0))
            ++sharedCornerCount;
    }
    assert(cornerTrace.directBlocked);
    assert(sharedCornerCount == 1);
    assert(cornerTrace.statistics.diffractionCandidates == 3); // unique endpoints
    assert(cornerTrace.statistics.diffractionValid == 3);

    std::cout << "All RayTracing2D v1.6 Thesis Release core tests passed (25 groups).\n";
    std::cout << "Known reflection point: (450, 400) m\n";
    std::cout << "FSPL(1 km, 916 MHz): " << fspl << " dB\n";
    return 0;
}
