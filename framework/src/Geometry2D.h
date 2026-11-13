#pragma once

#include <cmath>
#include <string>
#include <vector>

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Segment2D {
    Point2D a;
    Point2D b;
};

struct Wall2D {
    std::string id;
    Point2D a;
    Point2D b;
    std::string material = "generic";
    // Nonempty: explicit owning closed opaque polygon. With empty ownership,
    // RayTracer2D infers unambiguous closed rings; open components are screens.
    std::string buildingId = "";
};

enum class PointDomain2D { EXTERIOR = 0, INTERIOR = 1, BOUNDARY = 2 };
inline const char *domainName(PointDomain2D d) {
    return d == PointDomain2D::EXTERIOR ? "exterior" :
           d == PointDomain2D::INTERIOR ? "interior" : "boundary";
}
struct BuildingPolygon2D {
    std::string id;
    std::vector<Point2D> vertices;
    double minX, maxX, minY, maxY;
};

enum class SegmentIntersectionKind {
    NONE,
    POINT,
    OVERLAP
};

struct SegmentIntersectionResult {
    SegmentIntersectionKind kind = SegmentIntersectionKind::NONE;
    Point2D point;
    Point2D overlapStart;
    Point2D overlapEnd;
};

namespace Geometry2D {
// Domain tolerance is an absolute distance in metres, independent of origin.
PointDomain2D locateInPolygon(const Point2D& p, const BuildingPolygon2D& polygon, double toleranceM);
bool segmentEntersInterior(const Point2D& a, const Point2D& b,
                          const BuildingPolygon2D& polygon, double toleranceM);


double distance(const Point2D& a, const Point2D& b);
double distancePointToSegment(const Point2D& point, const Segment2D& segment);
double distanceSegmentToSegment(const Segment2D& first, const Segment2D& second,
                                double eps = 1e-9);
double cross(const Point2D& a, const Point2D& b, const Point2D& c);
bool nearlyEqual(double a, double b, double eps = 1e-9);
bool samePoint(const Point2D& a, const Point2D& b, double eps = 1e-9);
bool isFinite(const Point2D& p);
bool isPointOnSegment(const Point2D& p, const Segment2D& segment, double eps = 1e-9);

// Classifies proper crossings, endpoint contacts and collinear overlaps.
SegmentIntersectionResult classifySegmentIntersection(const Segment2D& s1, const Segment2D& s2,
                                                       double eps = 1e-9);

// Compatibility wrapper: returns true for a unique intersection point, false for no hit or overlap.
bool segmentIntersection(const Segment2D& s1, const Segment2D& s2, Point2D& outPoint, double eps = 1e-9);

// Intersection of an infinite line p->q with a finite segment wall.a->wall.b.
bool lineSegmentIntersection(const Point2D& p, const Point2D& q, const Wall2D& wall,
                             Point2D& outPoint, double& lineT, double& wallU, double eps = 1e-9);

// Mirror point p across the infinite line defined by wall.
Point2D mirrorAcrossLine(const Point2D& p, const Wall2D& wall);

// True when p and q are strictly on the same side of the infinite wall line.
bool sameSideOfLine(const Point2D& p, const Point2D& q, const Wall2D& wall, double eps = 1e-9);

} // namespace Geometry2D
