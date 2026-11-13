#pragma once

#include <complex>
#include <string>

enum class Polarization2D { TE, TM };

struct MaterialModel2D {
    std::string name;
    double permittivityA = 1.0;
    double permittivityB = 0.0;
    double conductivityC = 0.0;
    double conductivityD = 0.0;
    double minFrequencyGHz = 0.001;
    double maxFrequencyGHz = 450.0;

    double relativePermittivity(double frequencyHz) const;
    double conductivitySm(double frequencyHz) const;
    bool isFrequencyInRange(double frequencyHz) const;
};

class Materials2D {
  public:
    static const MaterialModel2D& get(const std::string& name);
    static std::complex<double> complexRelativePermittivity(const MaterialModel2D& material,
                                                            double frequencyHz);
    static std::complex<double> fresnelReflection(const MaterialModel2D& material,
                                                  double frequencyHz,
                                                  double incidenceAngleRad,
                                                  Polarization2D polarization);
};
