#include "RayTracing2DInetPathLoss.h"

#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioSignal.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace inet;
using namespace inet::physicallayer;
using namespace inet::units::values;

Define_Module(RayTracing2DInetPathLoss);

namespace {
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> out; std::stringstream stream(text); std::string token;
    while (std::getline(stream, token, delimiter)) if (!token.empty()) out.push_back(token);
    return out;
}
}

void RayTracing2DInetPathLoss::initialize(int stage)
{
    if (stage != INITSTAGE_LOCAL) return;
    frequencyFallbackHz = par("frequencyFallback").doubleValue();
    for (const auto& wallText : split(par("walls").stdstringValue(), ';')) {
        auto f = split(wallText, ',');
        if (f.size() != 6 && f.size() != 7) throw cRuntimeError("INET adapter wall requires id,x1,y1,x2,y2,material[,buildingId]");
        walls.push_back({f[0], {std::stod(f[1]), std::stod(f[2])},
                         {std::stod(f[3]), std::stod(f[4])}, f[5], f.size()==7?f[6]:""});
    }
}

double RayTracing2DInetPathLoss::computePathLoss(const ITransmission *transmission,
                                                 const IArrival *arrival) const
{
    auto signal = check_and_cast<const INarrowbandSignal *>(transmission->getAnalogModel());
    auto a = transmission->getStartPosition();
    auto b = arrival->getStartPosition();
    RayTracer2D tracer(walls);
    auto result = tracer.trace({a.x, a.y}, {b.x, b.y}, true,
                               par("enableFirstOrderReflections"), false,
                               par("enableDiffraction"));
    PropagationConfig config;
    config.frequencyHz = signal ? signal->getCenterFrequency().get() : frequencyFallbackHz;
    config.txPowerDbm = 0.0;
    config.useFresnel = true;
    config.coherentCombination = par("coherentCombination");
    auto summary = Propagation2D::evaluate(result.paths, config);
    if (result.paths.empty()) return 0.0;
    return std::clamp(std::pow(10.0, summary.totalReceivedPowerDbm / 10.0), 0.0, 1.0);
}

double RayTracing2DInetPathLoss::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    if (distance.get() == 0.0) return 1.0;
    double wavelength = (propagationSpeed / frequency).get();
    return std::pow(wavelength / (4.0 * M_PI * distance.get()), 2.0);
}

m RayTracing2DInetPathLoss::computeRange(mps propagationSpeed, Hz frequency, double loss) const
{
    if (!(loss > 0.0)) return m(NAN);
    double wavelength = (propagationSpeed / frequency).get();
    return m(wavelength / (4.0 * M_PI * std::sqrt(loss)));
}

std::ostream& RayTracing2DInetPathLoss::printToStream(std::ostream& stream, int, int) const
{
    return stream << "RayTracing2DInetPathLoss(walls=" << walls.size() << ")";
}
