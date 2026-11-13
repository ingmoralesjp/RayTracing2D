#include "Materials2D.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double EPSILON_0 = 8.8541878128e-12;

const std::map<std::string, MaterialModel2D> MATERIALS = {
    {"air",          {"air",          1.00, 0.0, 0.0000, 0.0000, 0.001, 450.0}},
    {"generic",      {"generic",      4.00, 0.0, 0.0100, 0.0000, 0.100, 100.0}},
    {"concrete",     {"concrete",     5.24, 0.0, 0.0462, 0.7822, 1.000, 100.0}},
    {"brick",        {"brick",        3.91, 0.0, 0.0238, 0.1600, 1.000, 40.0}},
    {"plasterboard", {"plasterboard", 2.73, 0.0, 0.0085, 0.9395, 1.000, 100.0}},
    {"wood",         {"wood",         1.99, 0.0, 0.0047, 1.0718, 0.001, 100.0}},
    {"glass",        {"glass",        6.31, 0.0, 0.0036, 1.3394, 0.100, 100.0}},
    {"metal",        {"metal",        1.00, 0.0, 1.0e7, 0.0000, 1.000, 100.0}}
};
}

double MaterialModel2D::relativePermittivity(double frequencyHz) const
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        throw std::invalid_argument("Material frequency must be finite and positive");
    return permittivityA * std::pow(frequencyHz / 1e9, permittivityB);
}

double MaterialModel2D::conductivitySm(double frequencyHz) const
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        throw std::invalid_argument("Material frequency must be finite and positive");
    return conductivityC == 0.0 ? 0.0 : conductivityC * std::pow(frequencyHz / 1e9, conductivityD);
}

bool MaterialModel2D::isFrequencyInRange(double frequencyHz) const
{
    double ghz = frequencyHz / 1e9;
    return ghz >= minFrequencyGHz && ghz <= maxFrequencyGHz;
}

const MaterialModel2D& Materials2D::get(const std::string& name)
{
    auto it = MATERIALS.find(name);
    if (it == MATERIALS.end())
        throw std::invalid_argument("Unknown material: " + name);
    return it->second;
}

std::complex<double> Materials2D::complexRelativePermittivity(const MaterialModel2D& material,
                                                               double frequencyHz)
{
    double omega = 2.0 * PI * frequencyHz;
    return {material.relativePermittivity(frequencyHz),
            -material.conductivitySm(frequencyHz) / (omega * EPSILON_0)};
}

std::complex<double> Materials2D::fresnelReflection(const MaterialModel2D& material,
                                                     double frequencyHz,
                                                     double incidenceAngleRad,
                                                     Polarization2D polarization)
{
    if (!std::isfinite(incidenceAngleRad) || incidenceAngleRad < 0.0 || incidenceAngleRad > PI / 2.0)
        throw std::invalid_argument("Incidence angle must be in [0, pi/2]");

    auto epsilon = complexRelativePermittivity(material, frequencyHz);
    double sinTheta = std::sin(incidenceAngleRad);
    double cosTheta = std::cos(incidenceAngleRad);
    auto transmittedTerm = std::sqrt(epsilon - sinTheta * sinTheta);

    if (polarization == Polarization2D::TE)
        return (cosTheta - transmittedTerm) / (cosTheta + transmittedTerm);
    return (epsilon * cosTheta - transmittedTerm) / (epsilon * cosTheta + transmittedTerm);
}
