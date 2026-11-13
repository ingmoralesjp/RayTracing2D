#include "Propagation2D.h"
#include "Geometry2D.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>

namespace {
constexpr double PI = 3.14159265358979323846;

double dbmToW(double dbm)
{
    return 1e-3 * std::pow(10.0, dbm / 10.0);
}

double wToDbm(double w)
{
    if (w <= 0.0)
        return -std::numeric_limits<double>::infinity();
    return 10.0 * std::log10(w / 1e-3);
}

bool exceedsIncidenceLimit(const RayPath& path, double maxAngleDeg)
{
    if (!(maxAngleDeg < 90.0))
        return false;
    const double maxAngleRad = maxAngleDeg * PI / 180.0;
    for (const auto& interaction : path.interactions) {
        if (interaction.type == InteractionType::REFLECTION &&
            interaction.incidenceAngleRad > maxAngleRad)
            return true;
    }
    return false;
}
}

double Propagation2D::wavelengthM(double frequencyHz)
{
    return C0_MPS / frequencyHz;
}

double Propagation2D::fsplDb(double distanceM, double frequencyHz)
{
    if (distanceM <= 0.0 || frequencyHz <= 0.0)
        return 0.0;

    double lambda = wavelengthM(frequencyHz);
    return 20.0 * std::log10((4.0 * PI * distanceM) / lambda);
}

double Propagation2D::pathDistanceM(const RayPath& path)
{
    double d = 0.0;
    for (std::size_t i = 1; i < path.points.size(); ++i)
        d += Geometry2D::distance(path.points[i - 1], path.points[i]);
    return d;
}

double Propagation2D::knifeEdgeV(const Point2D& tx, const Point2D& edge,
                                 const Point2D& rx, double frequencyHz)
{
    if (!(frequencyHz > 0.0))
        throw std::invalid_argument("Knife-edge frequency must be positive");
    double dx = rx.x - tx.x;
    double dy = rx.y - tx.y;
    double directLength = std::hypot(dx, dy);
    if (!(directLength > 0.0))
        throw std::invalid_argument("Knife-edge terminals must be distinct");
    double ux = dx / directLength;
    double uy = dy / directLength;
    double ex = edge.x - tx.x;
    double ey = edge.y - tx.y;
    double d1 = ex * ux + ey * uy;
    double d2 = directLength - d1;
    if (!(d1 > 0.0 && d2 > 0.0))
        throw std::invalid_argument("Knife edge must project between TX and RX");
    double h = std::fabs(ex * uy - ey * ux);
    double lambda = wavelengthM(frequencyHz);
    return h * std::sqrt((2.0 / lambda) * (1.0 / d1 + 1.0 / d2));
}

double Propagation2D::knifeEdgeLossDb(double v)
{
    if (v <= -0.78)
        return 0.0;
    return 6.9 + 20.0 * std::log10(std::sqrt((v - 0.1) * (v - 0.1) + 1.0) + v - 0.1);
}

PropagationSummary Propagation2D::evaluate(std::vector<RayPath>& paths, const PropagationConfig& cfg)
{
    if (!(cfg.frequencyHz > 0.0) || !std::isfinite(cfg.frequencyHz))
        throw std::invalid_argument("Propagation frequency must be finite and positive");
    PropagationSummary summary;
    if (paths.empty())
        return summary;

    double lambda = wavelengthM(cfg.frequencyHz);
    double reflectionPhaseShiftRad = cfg.reflectionPhaseShiftDeg * PI / 180.0;

    std::complex<double> coherentField(0.0, 0.0);
    double incoherentPowerW = 0.0;
    double weightedDelay = 0.0;
    double weightedDelaySquared = 0.0;
    double minDelay = std::numeric_limits<double>::infinity();
    double maxDelay = 0.0;

    for (std::size_t i = 0; i < paths.size(); ++i) {
        auto& path = paths[i];
        path.totalDistanceM = pathDistanceM(path);
        path.delayS = path.totalDistanceM / C0_MPS;
        std::complex<double> reflectionProduct(1.0, 0.0);
        path.materialModelInRange = true;
        for (const auto& interaction : path.interactions) {
            if (interaction.type != InteractionType::REFLECTION)
                continue;
            if (cfg.useFresnel) {
                const auto& material = Materials2D::get(interaction.material);
                path.materialModelInRange = path.materialModelInRange &&
                                            material.isFrequencyInRange(cfg.frequencyHz);
                reflectionProduct *= Materials2D::fresnelReflection(
                    material, cfg.frequencyHz, interaction.incidenceAngleRad, cfg.polarization);
            }
            else {
                double magnitude = std::pow(10.0, -cfg.reflectionLossDb / 20.0);
                reflectionProduct *= std::polar(magnitude, reflectionPhaseShiftRad);
            }
        }

        path.reflectionCoefficientMagnitude = std::abs(reflectionProduct);
        path.reflectionCoefficientPhaseRad = std::arg(reflectionProduct);
        double reflectionGainDb = path.reflectionCoefficientMagnitude > 0.0
                                  ? 20.0 * std::log10(path.reflectionCoefficientMagnitude)
                                  : -std::numeric_limits<double>::infinity();
        path.diffractionParameterV = 0.0;
        path.diffractionLossDb = 0.0;
        for (const auto& interaction : path.interactions) {
            if (interaction.type == InteractionType::DIFFRACTION) {
                path.diffractionParameterV = knifeEdgeV(path.points.front(), interaction.position,
                                                        path.points.back(), cfg.frequencyHz);
                path.diffractionLossDb += knifeEdgeLossDb(path.diffractionParameterV);
            }
        }
        path.pathLossDb = fsplDb(path.totalDistanceM, cfg.frequencyHz) - reflectionGainDb
                          + path.diffractionLossDb;
        path.receivedPowerDbm = cfg.txPowerDbm - path.pathLossDb;

        // Phase-candidate1: exp(+j*omega*t), passive epsilon_r-j*sigma/(omega*epsilon0).
        // The physical outgoing channel uses phi=-k*d+arg(Gamma).
        // This is an isolated candidate; historical v1.6 results are preserved.
        double rawPhase = -2.0 * PI * path.totalDistanceM / lambda
                          + path.reflectionCoefficientPhaseRad;
        path.phaseRad = std::fmod(rawPhase, 2.0 * PI);
        if (path.phaseRad < 0.0)
            path.phaseRad += 2.0 * PI;

        double powerW = dbmToW(path.receivedPowerDbm);
        incoherentPowerW += powerW;
        weightedDelay += powerW * path.delayS;
        weightedDelaySquared += powerW * path.delayS * path.delayS;
        minDelay = std::min(minDelay, path.delayS);
        maxDelay = std::max(maxDelay, path.delayS);

        // Normalized coherent sum. sqrt(power) is proportional to field amplitude.
        coherentField += std::polar(std::sqrt(powerW), path.phaseRad);

        if (summary.strongestPathIndex < 0 || path.receivedPowerDbm > summary.strongestPathPowerDbm) {
            summary.strongestPathIndex = static_cast<int>(i);
            summary.strongestPathPowerDbm = path.receivedPowerDbm;
        }
    }

    double totalPowerW = cfg.coherentCombination ? std::norm(coherentField) : incoherentPowerW;
    summary.totalReceivedPowerDbm = wToDbm(totalPowerW);
    summary.totalIncoherentPowerDbm = wToDbm(incoherentPowerW);
    summary.equivalentPathLossDb = cfg.txPowerDbm - summary.totalReceivedPowerDbm;

    if (incoherentPowerW > 0.0) {
        summary.meanDelayS = weightedDelay / incoherentPowerW;
        double secondMoment = weightedDelaySquared / incoherentPowerW;
        summary.rmsDelaySpreadS = std::sqrt(std::max(0.0,
                                                     secondMoment - summary.meanDelayS * summary.meanDelayS));
        summary.maxExcessDelayS = maxDelay - minDelay;
    }
    return summary;
}

PropagationSummary Propagation2D::evaluateAndSelect(std::vector<RayPath>& paths,
                                                    const PropagationConfig& cfg,
                                                    const PathSelectionConfig2D& selection,
                                                    int *candidateCount)
{
    if (candidateCount)
        *candidateCount = static_cast<int>(paths.size());

    // First evaluation fills distance, Fresnel, power and phase fields required
    // by physically meaningful selection criteria.
    evaluate(paths, cfg);

    paths.erase(std::remove_if(paths.begin(), paths.end(), [&](const RayPath& path) {
        if (selection.maxPathDistanceM > 0.0 && path.totalDistanceM > selection.maxPathDistanceM)
            return true;
        if (path.receivedPowerDbm < selection.minReceivedPowerDbm)
            return true;
        if (exceedsIncidenceLimit(path, selection.maxIncidenceAngleDeg))
            return true;
        return false;
    }), paths.end());

    if (selection.maxPaths > 0 && static_cast<int>(paths.size()) > selection.maxPaths) {
        std::stable_sort(paths.begin(), paths.end(), [](const RayPath& a, const RayPath& b) {
            return a.receivedPowerDbm > b.receivedPowerDbm;
        });
        paths.resize(static_cast<std::size_t>(selection.maxPaths));
    }

    return evaluate(paths, cfg);
}
