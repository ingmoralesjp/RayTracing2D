network RayTracingScenario
{
    parameters:
        @display("bgb=1200,800");
    submodules:
        tracer: RayTracing2DModule {
            @display("p=25,25;i=block/cogwheel;is=s");
        }
}
