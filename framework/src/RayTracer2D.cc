#include "RayTracer2D.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {
double incidenceAngle(const Point2D& previous, const Point2D& point, const Wall2D& wall)
{
    double ix = point.x - previous.x;
    double iy = point.y - previous.y;
    double wx = wall.b.x - wall.a.x;
    double wy = wall.b.y - wall.a.y;
    double incomingLength = std::hypot(ix, iy);
    double wallLength = std::hypot(wx, wy);
    double cosine = std::fabs(ix * (-wy) + iy * wx) / (incomingLength * wallLength);
    return std::acos(std::clamp(cosine, 0.0, 1.0));
}
}

RayTracer2D::RayTracer2D(std::vector<Wall2D> walls, double tolerance)
    : walls(std::move(walls)), eps(tolerance)
{
    if (!std::isfinite(eps) || eps <= 0.0)
        throw std::invalid_argument("RayTracer2D tolerance must be finite and positive");

    // Backwards-compatible wall-list API: an unambiguous connected closed
    // ring is a solid building too. Open components remain ideal screens.
    // Explicit ownership from OSM takes precedence and does not use inference.
    std::vector<bool> visited(this->walls.size(),false);
    for(std::size_t seed=0;seed<this->walls.size();++seed) {
        if(visited[seed] || !this->walls[seed].buildingId.empty())continue;
        std::vector<std::size_t> component{seed};visited[seed]=true;
        for(std::size_t k=0;k<component.size();++k) {
            const auto&w=this->walls[component[k]];
            for(std::size_t j=0;j<this->walls.size();++j) {
                if(visited[j] || !this->walls[j].buildingId.empty())continue;
                const auto&v=this->walls[j];
                if(Geometry2D::distance(w.a,v.a)<=eps || Geometry2D::distance(w.a,v.b)<=eps ||
                   Geometry2D::distance(w.b,v.a)<=eps || Geometry2D::distance(w.b,v.b)<=eps) {
                    visited[j]=true;component.push_back(j);
                }
            }
        }
        bool closed=component.size()>=3;
        for(auto i:component)for(const auto&p:{this->walls[i].a,this->walls[i].b}) {
            int degree=0;
            for(auto j:component) {
                degree+=Geometry2D::distance(p,this->walls[j].a)<=eps;
                degree+=Geometry2D::distance(p,this->walls[j].b)<=eps;
            }
            closed=closed && degree==2;
        }
        if(closed)for(auto i:component)this->walls[i].buildingId="auto_building_"+this->walls[seed].id;
    }

    std::unordered_set<std::string> ids;
    for (const auto& wall : this->walls) {
        if (wall.id.empty())
            throw std::invalid_argument("Wall IDs must not be empty");
        if (!ids.insert(wall.id).second)
            throw std::invalid_argument("Duplicate wall ID: " + wall.id);
        if (!Geometry2D::isFinite(wall.a) || !Geometry2D::isFinite(wall.b))
            throw std::invalid_argument("Wall coordinates must be finite: " + wall.id);
        if (Geometry2D::samePoint(wall.a, wall.b, eps))
            throw std::invalid_argument("Zero-length wall: " + wall.id);
    }
    std::map<std::string,std::vector<const Wall2D*>> owners;
    for (const auto& w:this->walls) if (!w.buildingId.empty()) owners[w.buildingId].push_back(&w);
    for (auto& entry:owners) {
        auto remaining=entry.second;
        BuildingPolygon2D g;g.id=entry.first;
        const Point2D start=remaining.front()->a;Point2D next=remaining.front()->b;
        g.vertices.push_back(start);remaining.erase(remaining.begin());
        while (!remaining.empty()) {
            g.vertices.push_back(next);
            std::size_t match=remaining.size();bool reverse=false;
            for (std::size_t i=0;i<remaining.size();++i) {
                if (Geometry2D::distance(next,remaining[i]->a)<=eps || Geometry2D::distance(next,remaining[i]->b)<=eps) {
                    if (match!=remaining.size()) throw std::invalid_argument("Branched polygon: "+g.id);
                    match=i;reverse=Geometry2D::distance(next,remaining[i]->b)<=eps;
                }
            }
            if (match==remaining.size()) throw std::invalid_argument("Open/disconnected polygon: "+g.id);
            next=reverse?remaining[match]->a:remaining[match]->b;remaining.erase(remaining.begin()+match);
        }
        if (g.vertices.size()<3 || Geometry2D::distance(next,start)>eps)
            throw std::invalid_argument("Building must be a closed polygon: "+g.id);
        g.minX=g.maxX=start.x;g.minY=g.maxY=start.y;double area=0;
        for (std::size_t i=0;i<g.vertices.size();++i) {
            const auto&a=g.vertices[i];const auto&b=g.vertices[(i+1)%g.vertices.size()];
            g.minX=std::min(g.minX,a.x);g.maxX=std::max(g.maxX,a.x);
            g.minY=std::min(g.minY,a.y);g.maxY=std::max(g.maxY,a.y);
            area+=(a.x-start.x)*(b.y-start.y)-(b.x-start.x)*(a.y-start.y);
            for (std::size_t j=i+1;j<g.vertices.size();++j) {
                if(j==i+1 || (i==0&&j+1==g.vertices.size()))continue;
                if(Geometry2D::classifySegmentIntersection({a,b},{g.vertices[j],g.vertices[(j+1)%g.vertices.size()]},1e-12).kind!=SegmentIntersectionKind::NONE)
                    throw std::invalid_argument("Self-intersecting polygon: "+g.id);
            }
        }
        if(std::abs(area)<=eps*eps)throw std::invalid_argument("Degenerate polygon: "+g.id);
        buildings.push_back(std::move(g));
    }
}

PointDomain2D RayTracer2D::classifyPoint(const Point2D& p) const
{
    if(!Geometry2D::isFinite(p))throw std::invalid_argument("Nonfinite domain point");
    bool boundary=false;
    for(const auto& g:buildings){
        auto d=Geometry2D::locateInPolygon(p,g,eps);
        if(d==PointDomain2D::INTERIOR)return d;
        boundary=boundary || d==PointDomain2D::BOUNDARY;
    }
    if(!boundary)for(const auto&w:walls)if(w.buildingId.empty()&&Geometry2D::distancePointToSegment(p,{w.a,w.b})<=eps){boundary=true;break;}
    return boundary?PointDomain2D::BOUNDARY:PointDomain2D::EXTERIOR;
}
bool RayTracer2D::segmentEntersBuilding(const Point2D&a,const Point2D&b)const
{
    for(const auto&g:buildings)if(Geometry2D::segmentEntersInterior(a,b,g,eps))return true;
    return false;
}

bool RayTracer2D::isBlocked(const Point2D& a, const Point2D& b, const std::string& ignoredWallId) const
{
    // Never ignore the solid interior, even when the source facade is ignored
    // for endpoint reflection/diffraction contact.
    if (segmentEntersBuilding(a,b)) return true;
    Segment2D path{a, b};
    for (const auto& wall : walls) {
        if (!ignoredWallId.empty() && wall.id == ignoredWallId)
            continue;

        auto intersection = Geometry2D::classifySegmentIntersection(path, Segment2D{wall.a, wall.b}, eps);
        if (intersection.kind == SegmentIntersectionKind::OVERLAP)
            return true;
        if (intersection.kind == SegmentIntersectionKind::POINT) {
            // Touching exactly at TX or RX is not treated as a blocking crossing.
            if (Geometry2D::samePoint(intersection.point, a, eps) || Geometry2D::samePoint(intersection.point, b, eps))
                continue;
            return true;
        }
    }
    return false;
}

bool RayTracer2D::findFirstOrderReflection(const Point2D& tx, const Point2D& rx,
                                           const Wall2D& wall, RayPath& outPath) const
{
    // Specular reflection on a wall requires transmitter and receiver on the same side.
    if (!Geometry2D::sameSideOfLine(tx, rx, wall, eps))
        return false;

    Point2D imageTx = Geometry2D::mirrorAcrossLine(tx, wall);
    Point2D reflectionPoint;
    double lineT = 0.0;
    double wallU = 0.0;

    if (!Geometry2D::lineSegmentIntersection(imageTx, rx, wall, reflectionPoint, lineT, wallU, eps))
        return false;

    // Avoid numerically degenerate solutions at wall endpoints in the first prototype.
    if (wallU <= eps || wallU >= 1.0 - eps)
        return false;

    // Both physical legs must be visible. The reflecting wall itself is ignored because both legs end at it.
    if (isBlocked(tx, reflectionPoint, wall.id))
        return false;
    if (isBlocked(reflectionPoint, rx, wall.id))
        return false;

    outPath.id = "REFL1_" + wall.id;
    outPath.points = {tx, reflectionPoint, rx};
    outPath.interactions = {{InteractionType::REFLECTION, reflectionPoint, wall.id,
                             wall.material, incidenceAngle(tx, reflectionPoint, wall)}};
    outPath.reflectionOrder = 1;
    outPath.isDirect = false;
    return true;
}

bool RayTracer2D::findSecondOrderReflection(const Point2D& tx, const Point2D& rx,
                                            const Wall2D& firstWall, const Wall2D& secondWall,
                                            RayPath& outPath) const
{
    if (firstWall.id == secondWall.id)
        return false;

    Point2D firstImage = Geometry2D::mirrorAcrossLine(tx, firstWall);
    Point2D secondImage = Geometry2D::mirrorAcrossLine(firstImage, secondWall);
    Point2D secondPoint;
    double lineT = 0.0;
    double secondU = 0.0;
    if (!Geometry2D::lineSegmentIntersection(secondImage, rx, secondWall,
                                             secondPoint, lineT, secondU, eps))
        return false;
    if (secondU <= eps || secondU >= 1.0 - eps)
        return false;

    Point2D firstPoint;
    double firstLineT = 0.0;
    double firstU = 0.0;
    if (!Geometry2D::lineSegmentIntersection(firstImage, secondPoint, firstWall,
                                             firstPoint, firstLineT, firstU, eps))
        return false;
    if (firstU <= eps || firstU >= 1.0 - eps)
        return false;

    // Incoming and outgoing legs must lie on the same physical side of each surface.
    if (!Geometry2D::sameSideOfLine(tx, secondPoint, firstWall, eps) ||
        !Geometry2D::sameSideOfLine(firstPoint, rx, secondWall, eps))
        return false;

    // Each segment ignores only the surfaces at its endpoints.
    if (isBlocked(tx, firstPoint, firstWall.id))
        return false;
    if (segmentEntersBuilding(firstPoint,secondPoint)) return false;
    Segment2D middle{firstPoint, secondPoint};
    for (const auto& wall : walls) {
        if (wall.id == firstWall.id || wall.id == secondWall.id)
            continue;
        auto hit = Geometry2D::classifySegmentIntersection(middle, {wall.a, wall.b}, eps);
        if (hit.kind != SegmentIntersectionKind::NONE)
            return false;
    }
    if (isBlocked(secondPoint, rx, secondWall.id))
        return false;

    outPath.id = "REFL2_" + firstWall.id + "_" + secondWall.id;
    outPath.points = {tx, firstPoint, secondPoint, rx};
    outPath.interactions = {
        {InteractionType::REFLECTION, firstPoint, firstWall.id, firstWall.material,
         incidenceAngle(tx, firstPoint, firstWall)},
        {InteractionType::REFLECTION, secondPoint, secondWall.id, secondWall.material,
         incidenceAngle(firstPoint, secondPoint, secondWall)}
    };
    outPath.reflectionOrder = 2;
    outPath.isDirect = false;
    return true;
}

bool RayTracer2D::findEdgeDiffraction(const Point2D& tx, const Point2D& rx,
                                      const Wall2D& wall, const Point2D& edge,
                                      const char *edgeName, RayPath& outPath) const
{
    // v0.5 models each endpoint of a finite wall that blocks the direct ray as
    // an isolated ideal knife edge. Both legs must be visible when the source
    // wall is ignored at their common endpoint.
    Segment2D direct{tx, rx};
    auto hit = Geometry2D::classifySegmentIntersection(direct, {wall.a, wall.b}, eps);
    if (hit.kind == SegmentIntersectionKind::NONE)
        return false;

    // The v1.1 knife-edge model implemented in Propagation2D assumes the edge
    // projects onto the open TX--RX segment.  Reject geometrically visible wall
    // endpoints outside that interval here, before creating a diffraction path.
    // This keeps the tracer and the knife-edge propagation approximation under
    // the same domain assumptions and avoids runtime exceptions in urban grids.
    const double dx = rx.x - tx.x;
    const double dy = rx.y - tx.y;
    const double directLengthSquared = dx * dx + dy * dy;
    if (!(directLengthSquared > eps * eps))
        return false;
    const double t = ((edge.x - tx.x) * dx + (edge.y - tx.y) * dy) /
                     directLengthSquared;
    if (!(t > eps && t < 1.0 - eps))
        return false;

    if (isBlocked(tx, edge, wall.id) || isBlocked(edge, rx, wall.id))
        return false;

    outPath.id = "DIFF1_" + wall.id + "_" + edgeName;
    outPath.points = {tx, edge, rx};
    outPath.interactions = {{InteractionType::DIFFRACTION, edge, wall.id,
                             wall.material, 0.0}};
    outPath.reflectionOrder = 0;
    outPath.diffractionOrder = 1;
    outPath.isDirect = false;
    return true;
}

TraceResult RayTracer2D::trace(const Point2D& tx, const Point2D& rx,
                               bool enableLos, bool enableFirstOrderReflections,
                               bool enableSecondOrderReflections,
                               bool enableDiffraction,
                               double maxTraceDistanceM) const
{
    if (!Geometry2D::isFinite(tx) || !Geometry2D::isFinite(rx))
        throw std::invalid_argument("TX and RX coordinates must be finite");
    if (Geometry2D::samePoint(tx, rx, eps))
        throw std::invalid_argument("TX and RX must be distinct points");
    if (!std::isfinite(maxTraceDistanceM) || maxTraceDistanceM < 0.0)
        throw std::invalid_argument("Maximum trace distance must be finite and non-negative");

    TraceResult result;
    result.txDomain=classifyPoint(tx);result.rxDomain=classifyPoint(rx);
    if (!result.supportedExterior()) { result.directBlocked=true;return result; }
    result.directBlocked = isBlocked(tx, rx);

    if (enableLos && !result.directBlocked) {
        RayPath direct;
        direct.id = "LOS";
        direct.points = {tx, rx};
        direct.isDirect = true;
        direct.reflectionOrder = 0;
        result.paths.push_back(direct);
    }

    // A path touching wall W cannot be shorter than
    // minDistance(TX,W)+minDistance(RX,W). Therefore a wall whose lower bound
    // already exceeds the reported maximum path distance can be discarded
    // without changing any path that the post-trace distance filter retains.
    std::vector<std::size_t> eligibleWalls;
    eligibleWalls.reserve(walls.size());
    for (std::size_t i = 0; i < walls.size(); ++i) {
        const Segment2D segment{walls[i].a, walls[i].b};
        const double lowerBound = Geometry2D::distancePointToSegment(tx, segment) +
                                  Geometry2D::distancePointToSegment(rx, segment);
        if (!(maxTraceDistanceM > 0.0) || lowerBound <= maxTraceDistanceM + eps)
            eligibleWalls.push_back(i);
    }

    if (enableFirstOrderReflections) {
        result.statistics.firstOrderTheoretical = static_cast<long long>(walls.size());
        result.statistics.firstOrderPruned = static_cast<long long>(walls.size() - eligibleWalls.size());
        for (const auto wallIndex : eligibleWalls) {
            const auto& wall = walls[wallIndex];
            ++result.statistics.firstOrderCandidates;
            RayPath reflected;
            if (findFirstOrderReflection(tx, rx, wall, reflected)) {
                result.paths.push_back(reflected);
                ++result.statistics.firstOrderValid;
            }
        }
    }

    if (enableSecondOrderReflections) {
        const long long wallCount = static_cast<long long>(walls.size());
        result.statistics.secondOrderTheoretical = wallCount * std::max(0LL, wallCount - 1);
        for (const auto firstIndex : eligibleWalls) {
            const auto& firstWall = walls[firstIndex];
            for (const auto secondIndex : eligibleWalls) {
                const auto& secondWall = walls[secondIndex];
                if (firstWall.id == secondWall.id)
                    continue;

                // Stronger safe lower bound for an ordered R2 path:
                // d(T,W1)+d(W1,W2)+d(W2,R) <= actual T-P1-P2-R distance.
                if (maxTraceDistanceM > 0.0) {
                    const double lowerBound =
                        Geometry2D::distancePointToSegment(tx, {firstWall.a, firstWall.b}) +
                        Geometry2D::distanceSegmentToSegment({firstWall.a, firstWall.b},
                                                             {secondWall.a, secondWall.b}, eps) +
                        Geometry2D::distancePointToSegment(rx, {secondWall.a, secondWall.b});
                    if (lowerBound > maxTraceDistanceM + eps)
                        continue;
                }
                ++result.statistics.secondOrderCandidates;
                RayPath reflected;
                if (findSecondOrderReflection(tx, rx, firstWall, secondWall, reflected)) {
                    result.paths.push_back(reflected);
                    ++result.statistics.secondOrderValid;
                }
            }
        }
        result.statistics.secondOrderPruned = result.statistics.secondOrderTheoretical -
                                               result.statistics.secondOrderCandidates;
    }

    if (enableDiffraction && result.directBlocked) {
        // A polygon corner belongs to two adjacent wall segments. Evaluating
        // every wall endpoint independently therefore double-counts the same
        // physical diffraction edge whenever the direct ray grazes that corner.
        // Group coincident endpoints first, then retain at most one D1 path per
        // unique geometric vertex. This is essential before using D1 power in
        // a coherent/incoherent sum.
        struct EdgeReference {
            const Wall2D *wall = nullptr;
            const char *edgeName = nullptr;
        };
        std::map<std::pair<double,double>, std::vector<EdgeReference>> uniqueEdges;
        for (const auto& wall : walls) {
            uniqueEdges[{wall.a.x, wall.a.y}].push_back({&wall, "A"});
            uniqueEdges[{wall.b.x, wall.b.y}].push_back({&wall, "B"});
        }

        for (const auto& entry : uniqueEdges) {
            const Point2D edge{entry.first.first, entry.first.second};
            ++result.statistics.diffractionCandidates;

            // Several incident walls may share this corner. The current D1
            // approximation models the corner as one knife edge, so accept the
            // first incident wall that yields a geometrically valid path and do
            // not add duplicate copies of the same TX-edge-RX trajectory.
            for (const auto& ref : entry.second) {
                RayPath diffracted;
                if (findEdgeDiffraction(tx, rx, *ref.wall, edge, ref.edgeName, diffracted)) {
                    result.paths.push_back(diffracted);
                    ++result.statistics.diffractionValid;
                    break;
                }
            }
        }
    }

    return result;
}
