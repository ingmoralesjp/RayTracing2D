#pragma once

#include "inet/physicallayer/wireless/common/base/packetlevel/PathLossBase.h"
#include "RayTracer2D.h"
#include "Propagation2D.h"

class RayTracing2DInetPathLoss : public inet::physicallayer::PathLossBase {
  private:
    std::vector<Wall2D> walls;
    double frequencyFallbackHz = 916e6;
  protected:
    virtual void initialize(int stage) override;
  public:
    virtual double computePathLoss(const inet::physicallayer::ITransmission *transmission,
                                   const inet::physicallayer::IArrival *arrival) const override;
    virtual double computePathLoss(inet::units::values::mps propagationSpeed,
                                   inet::units::values::Hz frequency,
                                   inet::units::values::m distance) const override;
    virtual inet::units::values::m computeRange(inet::units::values::mps propagationSpeed,
                                                inet::units::values::Hz frequency,
                                                double loss) const override;
    virtual std::ostream& printToStream(std::ostream& stream, int level,
                                       int evFlags = 0) const override;
};
