#include "Geometry2D.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
Point2D subtract(const Point2D& a, const Point2D& b) { return {a.x - b.x, a.y - b.y}; }
Point2D add(const Point2D& a, const Point2D& b) { return {a.x + b.x, a.y + b.y}; }
Point2D scale(const Point2D& a, double s) { return {a.x * s, a.y * s}; }
double crossVec(const Point2D& a, const Point2D& b) { return a.x * b.y - a.y * b.x; }
double dot(const Point2D& a, const Point2D& b) { return a.x * b.x + a.y * b.y; }
double norm(const Point2D& a) { return std::hypot(a.x, a.y); }

double coordinateScale(const Point2D& a, const Point2D& b)
{
    return std::max({1.0, std::fabs(a.x), std::fabs(a.y), std::fabs(b.x), std::fabs(b.y)});
}
}

namespace Geometry2D {

double distance(const Point2D& a, const Point2D& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

double distancePointToSegment(const Point2D& point, const Segment2D& segment)
{
    Point2D direction = subtract(segment.b, segment.a);
    const double lengthSquared = dot(direction, direction);
    if (!(lengthSquared > 0.0))
        return distance(point, segment.a);
    const double projection = std::clamp(dot(subtract(point, segment.a), direction) /
                                         lengthSquared, 0.0, 1.0);
    return distance(point, add(segment.a, scale(direction, projection)));
}

double distanceSegmentToSegment(const Segment2D& first, const Segment2D& second, double eps)
{
    if (classifySegmentIntersection(first, second, eps).kind != SegmentIntersectionKind::NONE)
        return 0.0;
    return std::min({distancePointToSegment(first.a, second),
                     distancePointToSegment(first.b, second),
                     distancePointToSegment(second.a, first),
                     distancePointToSegment(second.b, first)});
}

double cross(const Point2D& a, const Point2D& b, const Point2D& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool nearlyEqual(double a, double b, double eps)
{
    return std::fabs(a - b) <= eps;
}

bool samePoint(const Point2D& a, const Point2D& b, double eps)
{
    return distance(a, b) <= eps * coordinateScale(a, b);
}

bool isFinite(const Point2D& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}

bool isPointOnSegment(const Point2D& p, const Segment2D& segment, double eps)
{
    Point2D d = subtract(segment.b, segment.a);
    Point2D ap = subtract(p, segment.a);
    double length = norm(d);
    if (length <= eps * coordinateScale(segment.a, segment.b))
        return samePoint(p, segment.a, eps);
    double crossTolerance = eps * std::max(1.0, length * norm(ap));
    if (std::fabs(crossVec(d, ap)) > crossTolerance)
        return false;
    double projection = dot(ap, d) / dot(d, d);
    return projection >= -eps && projection <= 1.0 + eps;
}

SegmentIntersectionResult classifySegmentIntersection(const Segment2D& s1, const Segment2D& s2, double eps)
{
    SegmentIntersectionResult result;
    Point2D r = subtract(s1.b, s1.a);
    Point2D s = subtract(s2.b, s2.a);
    Point2D qp = subtract(s2.a, s1.a);
    double rLength = norm(r);
    double sLength = norm(s);
    bool rDegenerate = rLength <= eps * coordinateScale(s1.a, s1.b);
    bool sDegenerate = sLength <= eps * coordinateScale(s2.a, s2.b);

    if (rDegenerate && sDegenerate) {
        if (samePoint(s1.a, s2.a, eps)) {
            result.kind = SegmentIntersectionKind::POINT;
            result.point = s1.a;
        }
        return result;
    }
    if (rDegenerate) {
        if (isPointOnSegment(s1.a, s2, eps)) {
            result.kind = SegmentIntersectionKind::POINT;
            result.point = s1.a;
        }
        return result;
    }
    if (sDegenerate) {
        if (isPointOnSegment(s2.a, s1, eps)) {
            result.kind = SegmentIntersectionKind::POINT;
            result.point = s2.a;
        }
        return result;
    }

    double rxs = crossVec(r, s);
    double qpxr = crossVec(qp, r);
    double crossTolerance = eps * std::max(1.0, rLength * sLength);

    if (std::fabs(rxs) <= crossTolerance) {
        double collinearTolerance = eps * std::max(1.0, norm(qp) * rLength);
        if (std::fabs(qpxr) > collinearTolerance)
            return result;

        double rr = dot(r, r);
        double t0 = dot(qp, r) / rr;
        double t1 = t0 + dot(s, r) / rr;
        double low = std::max(0.0, std::min(t0, t1));
        double high = std::min(1.0, std::max(t0, t1));
        if (high < low - eps)
            return result;
        if (std::fabs(high - low) <= eps) {
            result.kind = SegmentIntersectionKind::POINT;
            result.point = add(s1.a, scale(r, 0.5 * (low + high)));
            return result;
        }
        result.kind = SegmentIntersectionKind::OVERLAP;
        result.overlapStart = add(s1.a, scale(r, low));
        result.overlapEnd = add(s1.a, scale(r, high));
        return result;
    }

    double t = crossVec(qp, s) / rxs;
    double u = crossVec(qp, r) / rxs;

    if (t < -eps || t > 1.0 + eps || u < -eps || u > 1.0 + eps)
        return result;

    result.kind = SegmentIntersectionKind::POINT;
    result.point = add(s1.a, scale(r, std::clamp(t, 0.0, 1.0)));
    return result;
}

bool segmentIntersection(const Segment2D& s1, const Segment2D& s2, Point2D& outPoint, double eps)
{
    auto result = classifySegmentIntersection(s1, s2, eps);
    if (result.kind != SegmentIntersectionKind::POINT)
        return false;
    outPoint = result.point;
    return true;
}

bool lineSegmentIntersection(const Point2D& p, const Point2D& q, const Wall2D& wall,
                             Point2D& outPoint, double& lineT, double& wallU, double eps)
{
    Point2D r = subtract(q, p);
    Point2D s = subtract(wall.b, wall.a);
    Point2D ap = subtract(wall.a, p);
    double rxs = crossVec(r, s);
    double rLength = norm(r);
    double sLength = norm(s);

    if (rLength <= eps * coordinateScale(p, q) ||
        sLength <= eps * coordinateScale(wall.a, wall.b))
        return false;
    if (std::fabs(rxs) <= eps * std::max(1.0, rLength * sLength))
        return false;

    lineT = crossVec(ap, s) / rxs;
    wallU = crossVec(ap, r) / rxs;

    // The image-source ray must connect the image point and receiver, and hit the finite wall.
    if (lineT < -eps || lineT > 1.0 + eps || wallU < -eps || wallU > 1.0 + eps)
        return false;

    outPoint = {p.x + lineT * r.x, p.y + lineT * r.y};
    return true;
}

Point2D mirrorAcrossLine(const Point2D& p, const Wall2D& wall)
{
    double dx = wall.b.x - wall.a.x;
    double dy = wall.b.y - wall.a.y;
    double len2 = dx * dx + dy * dy;
    if (!isFinite(p) || !isFinite(wall.a) || !isFinite(wall.b))
        throw std::invalid_argument("mirrorAcrossLine requires finite coordinates");
    if (len2 <= std::numeric_limits<double>::epsilon())
        throw std::invalid_argument("Cannot mirror across a zero-length wall");

    double t = ((p.x - wall.a.x) * dx + (p.y - wall.a.y) * dy) / len2;
    Point2D projection{wall.a.x + t * dx, wall.a.y + t * dy};
    return {2.0 * projection.x - p.x, 2.0 * projection.y - p.y};
}

bool sameSideOfLine(const Point2D& p, const Point2D& q, const Wall2D& wall, double eps)
{
    double cp = cross(wall.a, wall.b, p);
    double cq = cross(wall.a, wall.b, q);
    return (cp > eps && cq > eps) || (cp < -eps && cq < -eps);
}

} // namespace Geometry2D

namespace Geometry2D {
PointDomain2D locateInPolygon(const Point2D& p, const BuildingPolygon2D& g, double tol)
{
    if (p.x < g.minX-tol || p.x > g.maxX+tol || p.y < g.minY-tol || p.y > g.maxY+tol)
        return PointDomain2D::EXTERIOR;
    bool odd = false;
    for (std::size_t i=0; i<g.vertices.size(); ++i) {
        const auto& a=g.vertices[i]; const auto& b=g.vertices[(i+1)%g.vertices.size()];
        if (distancePointToSegment(p,{a,b}) <= tol) return PointDomain2D::BOUNDARY;
        if ((a.y>p.y)!=(b.y>p.y) && p.x < a.x+(b.x-a.x)*(p.y-a.y)/(b.y-a.y)) odd=!odd;
    }
    return odd ? PointDomain2D::INTERIOR : PointDomain2D::EXTERIOR;
}
bool segmentEntersInterior(const Point2D& a, const Point2D& b, const BuildingPolygon2D& g, double tol)
{
    if (std::max(a.x,b.x)<g.minX-tol || std::min(a.x,b.x)>g.maxX+tol ||
        std::max(a.y,b.y)<g.minY-tol || std::min(a.y,b.y)>g.maxY+tol) return false;
    if (locateInPolygon(a,g,tol)==PointDomain2D::INTERIOR ||
        locateInPolygon(b,g,tol)==PointDomain2D::INTERIOR) return true;
    const double dx=b.x-a.x, dy=b.y-a.y, length=std::hypot(dx,dy);
    if (length<=tol) return false;
    std::vector<double> cuts{0.0,1.0};
    // Partition at every boundary crossing (including vertex contacts), then
    // classify open intervals. This works for concave simple polygons as well.
    for (std::size_t i=0;i<g.vertices.size();++i) {
        const auto& c=g.vertices[i]; const auto& d=g.vertices[(i+1)%g.vertices.size()];
        const double sx=d.x-c.x, sy=d.y-c.y, den=dx*sy-dy*sx;
        const double edgeLength=std::hypot(sx,sy);
        if (std::abs(den)<=1e-14*length*edgeLength) continue;
        const double t=((c.x-a.x)*sy-(c.y-a.y)*sx)/den;
        const double u=((c.x-a.x)*dy-(c.y-a.y)*dx)/den;
        if (t>=-tol/length && t<=1+tol/length && u>=-tol/edgeLength && u<=1+tol/edgeLength)
            cuts.push_back(std::clamp(t,0.0,1.0));
    }
    std::sort(cuts.begin(),cuts.end());
    for (std::size_t i=1;i<cuts.size();++i) {
        double t=(cuts[i]+cuts[i-1])*0.5;
        if (locateInPolygon({a.x+t*dx,a.y+t*dy},g,tol)==PointDomain2D::INTERIOR) return true;
    }
    return false;
}
}
