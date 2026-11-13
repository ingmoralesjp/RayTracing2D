#include <omnetpp.h>
#include "inet/physicallayer/wireless/common/contract/packetlevel/IPathLoss.h"

using namespace omnetpp;
using namespace inet::units::values;

class InetPathLossProbe : public cSimpleModule {
  protected:
    void initialize() override {
        auto module = getModuleByPath("^.radioMedium.pathLoss");
        auto model = check_and_cast<inet::physicallayer::IPathLoss *>(module);
        double factor = model->computePathLoss(mps(299792458.0), Hz(916e6), m(1000.0));
        recordScalar("lossFactor1km", factor);
        recordScalar("pathLossDb1km", -10.0 * std::log10(factor));
    }
    void handleMessage(cMessage *message) override { delete message; }
};
Define_Module(InetPathLossProbe);
