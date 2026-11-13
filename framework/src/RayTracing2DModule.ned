simple RayTracing2DModule
{
    parameters:
        // -----------------------------------------------------------------
        // TERMINALS / LOCAL GEOMETRY
        // Coordinates are local Cartesian metres. For OSM scenarios they
        // must be inside the projected map extent reported by osm_inspect.py.
        // -----------------------------------------------------------------
        double txX @unit(m) = default(200m);
        double txY @unit(m) = default(100m);
        double rxX @unit(m) = default(700m);
        double rxY @unit(m) = default(100m);

        // Manual wall format: id,x1,y1,x2,y2[,material];id2,...
        string walls = default("reflector,100,400,800,400,generic");

        // -----------------------------------------------------------------
        // OSM INPUT
        // v1.1 supports closed building ways. Relation/multipolygon support
        // remains future work. Unknown building:material tags fall back to
        // osmDefaultMaterial after common aliases are normalized.
        // -----------------------------------------------------------------
        string osmFile = default("");
        double osmOriginLatDeg = default(0);
        double osmOriginLonDeg = default(0);
        string osmDefaultMaterial = default("concrete");
        // Empty = honor normalized building:material tags from OSM and use
        // osmDefaultMaterial only when a tag is missing/unsupported.
        // Non-empty = force EVERY imported OSM wall to one catalog material.
        // This is useful for controlled material sensitivity experiments.
        // Allowed: generic|concrete|brick|plasterboard|wood|glass|metal
        string osmForceMaterial = default("");

        // -----------------------------------------------------------------
        // RF / PROPAGATION
        // Density (kg/m3) is NOT used by the current RF reflection model.
        // Fresnel reflection uses relative permittivity and conductivity from
        // the material catalog, together with frequency, polarization and the
        // incidence angle computed from geometry.
        // -----------------------------------------------------------------
        double frequency @unit(Hz) = default(2.4GHz);
        double txPowerDbm = default(14);

        // "fresnel" is the physical v1.1 mode. "fixed" is retained only for
        // regression against early versions.
        double reflectionLossDb = default(6);
        double reflectionPhaseShiftDeg = default(180);
        string reflectionModel = default("fresnel"); // "fresnel" or "fixed"
        string polarization = default("TE");        // "TE" or "TM"
        bool coherentCombination = default(true);

        // -----------------------------------------------------------------
        // RAY GENERATION
        // This project uses the IMAGE METHOD, not angular ray launching.
        // Therefore there is no angle step or fixed number of launched rays.
        // The number of candidate/valid paths emerges from the number and
        // arrangement of finite surfaces. Incidence angles are computed, not
        // swept. Order two tests ordered wall pairs and can become expensive.
        // -----------------------------------------------------------------
        bool enableLos = default(true);
        bool enableFirstOrderReflections = default(true);
        bool enableSecondOrderReflections = default(false);
        bool enableDiffraction = default(false); // isolated knife-edge approximation

        // -----------------------------------------------------------------
        // PATH SELECTION / EXPERIMENTAL GUARDRAILS
        // Defaults keep all paths. Non-default values alter the resulting
        // coherent sum and MUST be reported in any scientific experiment.
        // maxPathsPerReceiver=0       -> unlimited retained paths
        // maxPathDistance=0 m         -> no distance cutoff
        // minPathPowerDbm=-300        -> effectively disabled
        // maxIncidenceAngleDeg=90     -> no grazing-angle rejection
        // -----------------------------------------------------------------
        int maxPathsPerReceiver = default(0);
        double maxPathDistance @unit(m) = default(0m);
        double minPathPowerDbm = default(-300);
        double maxIncidenceAngleDeg = default(90);

        // -----------------------------------------------------------------
        // GRID / COVERAGE
        // gridMode="fspl" ignores building blocking in the calculation but
        // keeps the same OSM scene for a controlled free-space baseline.
        // gridMode="raytracing" uses the enabled LoS/R1/R2/diffraction modes.
        // -----------------------------------------------------------------
        bool enableGrid = default(false);
        string gridMode = default("raytracing"); // "raytracing" or "fspl"
        double gridMinX @unit(m) = default(0m);
        double gridMaxX @unit(m) = default(100m);
        double gridMinY @unit(m) = default(0m);
        double gridMaxY @unit(m) = default(100m);
        double gridStepX @unit(m) = default(20m);
        double gridStepY @unit(m) = default(20m);
        string gridOutputCsv = default("");

        // -----------------------------------------------------------------
        // v1.2 FREQUENCY RESPONSE
        // Geometry is traced once. FSPL, Fresnel, phase and path selection are
        // then recomputed at linearly spaced frequencies for the same paths.
        // -----------------------------------------------------------------
        bool enableFrequencySweep = default(false);
        double sweepStartFrequency @unit(Hz) = default(2.3GHz);
        double sweepStopFrequency @unit(Hz) = default(2.5GHz);
        int sweepPoints = default(201);
        string frequencySweepOutputCsv = default("");

        // Qtenv heatmap can display received power, retained path count or
        // RMS delay spread. Scaling is deliberately configurable.
        string heatmapMetric = default("receivedPowerDbm"); // receivedPowerDbm|pathCount|rmsDelayNs
        double heatmapMinDbm = default(-120);
        double heatmapMaxDbm = default(-40);
        double heatmapPathCountMax = default(10);
        double heatmapDelaySpreadMaxNs = default(1000);

        // -----------------------------------------------------------------
        // NUMERICAL / VISUAL CONTROLS
        //
        // IMPORTANT: txX/txY/rxX/rxY and all walls are PHYSICAL local Cartesian
        // coordinates in metres (x=east, y=north). Qtenv's cCanvas uses visual
        // canvas units, not geographic metres. v1.6 therefore applies an
        // explicit WORLD->CANVAS transform only while drawing. The ray tracer
        // and all exported CSV/scalars remain in physical metres.
        //
        // viewAutoFit=true:
        //   derive world bounds from walls + TX/RX + enabled grid and fit them
        //   into viewCanvasWidth x viewCanvasHeight while preserving aspect ratio.
        //
        // viewAutoFit=false:
        //   use viewWorldMin/Max* as an explicit local viewport. Geometry outside
        //   the viewport is culled from drawing only; propagation is unchanged.
        //
        // viewNorthUp=true:
        //   local +Y (north) is drawn upward, so Qtenv's visual Y direction is
        //   inverted by the rendering transform.
        // -----------------------------------------------------------------
        double numericalTolerance = default(1e-8);
        bool drawScene = default(true);

        bool viewAutoFit = default(true);
        double viewCanvasWidth = default(1200);
        double viewCanvasHeight = default(800);
        double viewMargin = default(35);
        bool viewNorthUp = default(true);
        double viewWorldMinX @unit(m) = default(0m);
        double viewWorldMaxX @unit(m) = default(1000m);
        double viewWorldMinY @unit(m) = default(0m);
        double viewWorldMaxY @unit(m) = default(1000m);

        // OSM buildings are drawn as filled footprints reconstructed from their
        // ordered wall edges. drawRawOsmWalls can be enabled for geometry debug.
        bool drawBuildingFootprints = default(true);
        bool fillBuildingFootprints = default(true);
        bool drawRawOsmWalls = default(false);
        bool drawMapDecorations = default(true);

        bool drawWallLabels = default(true);
        bool drawRayLabels = default(true);
        int maxRayPathsToDraw = default(0);

        // Visual sizes below are CANVAS units, not physical metres.
        double markerRadius = default(6);
        double summaryX = default(12);
        double summaryY = default(12);

        // Optional per-ray CSV for the explicit single RX. Empty disables it.
        string outputCsv = default("");

        @class(RayTracing2DModule);
        @display("p=25,25;i=block/cogwheel;is=s");
}
