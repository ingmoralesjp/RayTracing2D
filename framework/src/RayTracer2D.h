#pragma once

#include "Geometry2D.h"
#include "RayTracingTypes.h"

#include <vector>

class RayTracer2D {
  public:
    explicit RayTracer2D(std::vector<Wall2D> walls, double tolerance = 1e-8);

    TraceResult trace(const Point2D& tx, const Point2D& rx,
                      bool enableLos = true,
                      bool enableFirstOrderReflections = true,
                      bool enableSecondOrderReflections = false,
                      bool enableDiffraction = false,
                      double maxTraceDistanceM = 0.0) const;

    bool isBlocked(const Point2D& a, const Point2D& b,
                   const std::string& ignoredWallId = "") const;

    PointDomain2D classifyPoint(const Point2D& point) const;
    bool segmentEntersBuilding(const Point2D& a, const Point2D& b) const;
    const std::vector<BuildingPolygon2D>& getBuildings() const { return buildings; }
    const std::vector<Wall2D>& getWalls() const { return walls; }

  private:
    std::vector<Wall2D> walls;
    double eps;
    std::vector<BuildingPolygon2D> buildings;

    bool findFirstOrderReflection(const Point2D& tx, const Point2D& rx,
                                  const Wall2D& wall, RayPath& outPath) const;
    bool findSecondOrderReflection(const Point2D& tx, const Point2D& rx,
                                   const Wall2D& firstWall, const Wall2D& secondWall,
                                   RayPath& outPath) const;
    bool findEdgeDiffraction(const Point2D& tx, const Point2D& rx,
                             const Wall2D& wall, const Point2D& edge,
                             const char *edgeName, RayPath& outPath) const;
};
