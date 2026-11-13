import inet.physicallayer.wireless.common.medium.RadioMedium;

simple InetPathLossProbe { parameters: @display("p=100,200"); }

network InetAdapterScenario
{
    submodules:
        radioMedium: RadioMedium {
            parameters:
                pathLoss.typename = "RayTracing2DInetPathLoss";
                @display("p=250,100");
        }
        probe: InetPathLossProbe { @display("p=250,250"); }
}
