import inet.physicallayer.wireless.common.contract.packetlevel.IPathLoss;

module RayTracing2DInetPathLoss like IPathLoss
{
    parameters:
        string walls = default("");
        double frequencyFallback @unit(Hz) = default(916MHz);
        bool enableFirstOrderReflections = default(true);
        bool enableDiffraction = default(true);
        bool coherentCombination = default(true);
        @class(RayTracing2DInetPathLoss);
        @display("i=block/control");
}
