#include "RayTracing2DModule.h"
#include "Materials2D.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace omnetpp;

Define_Module(RayTracing2DModule);

namespace {
constexpr double PI = 3.14159265358979323846;

std::vector<std::string> split(const std::string& text, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty())
            out.push_back(token);
    }
    return out;
}

const char *typeName(const RayPath& p)
{
    if (p.isDirect)
        return "LOS";
    return p.diffractionOrder > 0 ? "DIFFRACTION" : "REFLECTION";
}

std::string interactionSummary(const RayPath& path)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < path.interactions.size(); ++i) {
        if (i)
            out << ';';
        const auto& interaction = path.interactions[i];
        if (interaction.type == InteractionType::REFLECTION)
            out << "REFLECTION";
        else if (interaction.type == InteractionType::DIFFRACTION)
            out << "DIFFRACTION";
        else
            out << "LOS";
        out << '@' << interaction.objectId << '@' << interaction.material;
        if (interaction.type == InteractionType::REFLECTION)
            out << "@incDeg=" << interaction.incidenceAngleRad * 180.0 / PI;
    }
    return out.str();
}
}

std::vector<Wall2D> RayTracing2DModule::parseWalls(const std::string& specification) const
{
    std::vector<Wall2D> parsed;
    if (specification.empty())
        return parsed;

    for (const auto& wallText : split(specification, ';')) {
        auto fields = split(wallText, ',');
        if (fields.size() != 5 && fields.size() != 6 && fields.size() != 7)
            throw cRuntimeError("Invalid wall specification '%s'. Expected id,x1,y1,x2,y2[,material[,buildingId]]", wallText.c_str());

        Wall2D wall;
        wall.id = fields[0];
        wall.a = {std::stod(fields[1]), std::stod(fields[2])};
        wall.b = {std::stod(fields[3]), std::stod(fields[4])};
        if (fields.size() >= 6)
            wall.material = fields[5];
        if (fields.size() == 7) wall.buildingId=fields[6];
        parsed.push_back(wall);
    }
    return parsed;
}

PathSelectionConfig2D RayTracing2DModule::pathSelectionFromParameters() const
{
    PathSelectionConfig2D selection;
    selection.minReceivedPowerDbm = par("minPathPowerDbm").doubleValue();
    selection.maxPathDistanceM = par("maxPathDistance").doubleValue();
    selection.maxIncidenceAngleDeg = par("maxIncidenceAngleDeg").doubleValue();
    selection.maxPaths = par("maxPathsPerReceiver").intValue();
    if (selection.maxPaths < 0)
        throw cRuntimeError("maxPathsPerReceiver must be >= 0");
    if (selection.maxPathDistanceM < 0.0)
        throw cRuntimeError("maxPathDistance must be >= 0 m");
    if (!(selection.maxIncidenceAngleDeg > 0.0 && selection.maxIncidenceAngleDeg <= 90.0))
        throw cRuntimeError("maxIncidenceAngleDeg must be in (0,90]");
    return selection;
}

cCanvas *RayTracing2DModule::sceneCanvas()
{
    cModule *parent = getParentModule();
    return parent ? parent->getCanvas() : getCanvas();
}

void RayTracing2DModule::configureView()
{
    view.canvasWidth = par("viewCanvasWidth").doubleValue();
    view.canvasHeight = par("viewCanvasHeight").doubleValue();
    view.margin = par("viewMargin").doubleValue();
    view.northUp = par("viewNorthUp").boolValue();

    if (!(view.canvasWidth > 2.0 * view.margin && view.canvasHeight > 2.0 * view.margin))
        throw cRuntimeError("Invalid Qtenv viewport: canvas dimensions must exceed twice viewMargin");

    if (par("viewAutoFit").boolValue()) {
        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();

        auto includePoint = [&](const Point2D& p) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return;
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        };

        includePoint(tx);
        includePoint(rx);
        for (const auto& wall : walls) {
            includePoint(wall.a);
            includePoint(wall.b);
        }

        if (par("enableGrid").boolValue()) {
            includePoint({par("gridMinX").doubleValue(), par("gridMinY").doubleValue()});
            includePoint({par("gridMaxX").doubleValue(), par("gridMaxY").doubleValue()});
        }

        if (!std::isfinite(minX) || !std::isfinite(maxX) ||
            !std::isfinite(minY) || !std::isfinite(maxY)) {
            minX = 0.0; maxX = 1.0; minY = 0.0; maxY = 1.0;
        }

        // Avoid a singular transform for collinear analytical fixtures.
        if (std::fabs(maxX - minX) < 1e-9) {
            minX -= 0.5;
            maxX += 0.5;
        }
        if (std::fabs(maxY - minY) < 1e-9) {
            minY -= 0.5;
            maxY += 0.5;
        }

        view.worldMinX = minX;
        view.worldMaxX = maxX;
        view.worldMinY = minY;
        view.worldMaxY = maxY;
    }
    else {
        view.worldMinX = par("viewWorldMinX").doubleValue();
        view.worldMaxX = par("viewWorldMaxX").doubleValue();
        view.worldMinY = par("viewWorldMinY").doubleValue();
        view.worldMaxY = par("viewWorldMaxY").doubleValue();
        if (!(view.worldMaxX > view.worldMinX && view.worldMaxY > view.worldMinY))
            throw cRuntimeError("Manual Qtenv viewport requires max > min on both axes");
    }

    const double worldWidth = view.worldMaxX - view.worldMinX;
    const double worldHeight = view.worldMaxY - view.worldMinY;
    const double availableWidth = view.canvasWidth - 2.0 * view.margin;
    const double availableHeight = view.canvasHeight - 2.0 * view.margin;
    view.scale = std::min(availableWidth / worldWidth, availableHeight / worldHeight);

    const double drawnWidth = worldWidth * view.scale;
    const double drawnHeight = worldHeight * view.scale;
    view.left = view.margin + (availableWidth - drawnWidth) / 2.0;
    view.top = view.margin + (availableHeight - drawnHeight) / 2.0;
    view.valid = true;
}

cFigure::Point RayTracing2DModule::toCanvasPoint(const Point2D& p) const
{
    if (!view.valid)
        throw cRuntimeError("Qtenv viewport transform used before configuration");

    const double x = view.left + (p.x - view.worldMinX) * view.scale;
    const double y = view.northUp
                     ? view.top + (view.worldMaxY - p.y) * view.scale
                     : view.top + (p.y - view.worldMinY) * view.scale;
    return {x, y};
}

cFigure::Rectangle RayTracing2DModule::toCanvasRectangle(const Point2D& center,
                                                          double widthWorld,
                                                          double heightWorld) const
{
    Point2D a{center.x - widthWorld / 2.0, center.y - heightWorld / 2.0};
    Point2D b{center.x + widthWorld / 2.0, center.y + heightWorld / 2.0};
    auto ca = toCanvasPoint(a);
    auto cb = toCanvasPoint(b);
    double x = std::min(ca.x, cb.x);
    double y = std::min(ca.y, cb.y);
    return {x, y, std::fabs(cb.x - ca.x), std::fabs(cb.y - ca.y)};
}

bool RayTracing2DModule::worldBoxIntersectsView(double minX, double minY,
                                                 double maxX, double maxY) const
{
    return !(maxX < view.worldMinX || minX > view.worldMaxX ||
             maxY < view.worldMinY || minY > view.worldMaxY);
}

void RayTracing2DModule::drawMarker(const Point2D& p, const char *label,
                                    const cFigure::Color& color)
{
    if (!worldBoxIntersectsView(p.x, p.y, p.x, p.y))
        return;

    cCanvas *canvas = sceneCanvas();
    double r = par("markerRadius").doubleValue();
    auto cp = toCanvasPoint(p);

    auto *circle = new cOvalFigure();
    circle->setBounds(cFigure::Rectangle(cp.x - r, cp.y - r, 2 * r, 2 * r));
    circle->setFilled(true);
    circle->setFillColor(color);
    circle->setLineColor(cFigure::BLACK);
    canvas->addFigure(circle);

    auto *text = new cTextFigure();
    text->setText(label);
    text->setPosition(cFigure::Point(cp.x, cp.y - 2.2 * r));
    text->setAnchor(cFigure::ANCHOR_S);
    text->setColor(cFigure::BLACK);
    canvas->addFigure(text);
}

void RayTracing2DModule::drawHeatmap()
{
    if (gridSamples.empty())
        return;

    cCanvas *canvas = sceneCanvas();
    double width = par("gridStepX").doubleValue();
    double height = par("gridStepY").doubleValue();
    std::string metric = par("heatmapMetric").stdstringValue();

    for (const auto& sample : gridSamples) {
        if (!worldBoxIntersectsView(sample.position.x - width / 2.0,
                                    sample.position.y - height / 2.0,
                                    sample.position.x + width / 2.0,
                                    sample.position.y + height / 2.0))
            continue;

        double value = std::numeric_limits<double>::quiet_NaN();
        double low = 0.0;
        double high = 1.0;

        if (metric == "receivedPowerDbm") {
            value = sample.receivedPowerDbm;
            low = par("heatmapMinDbm").doubleValue();
            high = par("heatmapMaxDbm").doubleValue();
        }
        else if (metric == "pathCount") {
            value = sample.pathCount;
            low = 0.0;
            high = par("heatmapPathCountMax").doubleValue();
        }
        else if (metric == "rmsDelayNs") {
            value = sample.rmsDelaySpreadS * 1e9;
            low = 0.0;
            high = par("heatmapDelaySpreadMaxNs").doubleValue();
        }
        else {
            throw cRuntimeError("Unknown heatmapMetric '%s'", metric.c_str());
        }

        if (!(high > low))
            throw cRuntimeError("Invalid heatmap scale: maximum must exceed minimum");

        auto *cell = new cRectangleFigure();
        cell->setBounds(toCanvasRectangle(sample.position, width, height));
        cell->setFilled(true);

        if (!std::isfinite(value)) {
            // "No resolved path" is not the same quantity as a very weak finite
            // path. Keep it visually neutral instead of mapping NaN to the
            // minimum of the power colour scale.
            cell->setFillColor(cFigure::Color(242, 242, 242));
        }
        else {
            double normalized = std::clamp((value - low) / (high - low), 0.0, 1.0);
            cell->setFillColor(cFigure::Color(static_cast<int>(255 * normalized), 0,
                                              static_cast<int>(255 * (1.0 - normalized))));
        }
        cell->setLineColor(cFigure::Color(205, 205, 205));
        canvas->addFigure(cell);
    }
}

void RayTracing2DModule::drawBuildingsAndWalls()
{
    cCanvas *canvas = sceneCanvas();
    const bool footprints = par("drawBuildingFootprints").boolValue();
    const bool fillFootprints = par("fillBuildingFootprints").boolValue();
    const bool rawOsmWalls = par("drawRawOsmWalls").boolValue();
    const bool drawWallLabels = par("drawWallLabels").boolValue();

    // Reconstruct each closed OSM building from the ordered wall IDs
    // osm_<wayId>_e0, osm_<wayId>_e1, ... . This is VISUAL only: the ray tracer
    // continues consuming the exact finite Wall2D segments imported above.
    std::map<std::string, std::vector<std::pair<int, const Wall2D*>>> buildings;
    if (footprints) {
        for (const auto& wall : walls) {
            if (wall.id.rfind("osm_", 0) != 0)
                continue;
            auto pos = wall.id.rfind("_e");
            if (pos == std::string::npos)
                continue;
            try {
                int edge = std::stoi(wall.id.substr(pos + 2));
                buildings[wall.id.substr(0, pos)].push_back({edge, &wall});
            }
            catch (...) {
                // If an ID does not match the importer convention, it will
                // still be available through raw wall drawing when enabled.
            }
        }

        for (auto& item : buildings) {
            auto& edges = item.second;
            std::sort(edges.begin(), edges.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            if (edges.size() < 3)
                continue;

            double minX = std::numeric_limits<double>::infinity();
            double maxX = -std::numeric_limits<double>::infinity();
            double minY = std::numeric_limits<double>::infinity();
            double maxY = -std::numeric_limits<double>::infinity();
            std::vector<cFigure::Point> points;
            points.reserve(edges.size());

            for (const auto& edge : edges) {
                const Point2D& p = edge.second->a;
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
                points.push_back(toCanvasPoint(p));
            }

            if (!worldBoxIntersectsView(minX, minY, maxX, maxY))
                continue;

            auto *polygon = new cPolygonFigure();
            polygon->setPoints(points);
            polygon->setFilled(fillFootprints);
            if (fillFootprints)
                polygon->setFillColor(cFigure::Color(226, 226, 226));
            polygon->setLineColor(cFigure::Color(105, 105, 105));
            polygon->setLineWidth(1);
            canvas->addFigure(polygon);
        }
    }

    for (const auto& wall : walls) {
        const bool isOsm = wall.id.rfind("osm_", 0) == 0;
        if (isOsm && footprints && !rawOsmWalls)
            continue;

        double minX = std::min(wall.a.x, wall.b.x);
        double maxX = std::max(wall.a.x, wall.b.x);
        double minY = std::min(wall.a.y, wall.b.y);
        double maxY = std::max(wall.a.y, wall.b.y);
        if (!worldBoxIntersectsView(minX, minY, maxX, maxY))
            continue;

        auto *line = new cLineFigure();
        line->setStart(toCanvasPoint(wall.a));
        line->setEnd(toCanvasPoint(wall.b));
        line->setLineColor(isOsm ? cFigure::Color(105, 105, 105) : cFigure::BLACK);
        line->setLineWidth(isOsm ? 1 : 3);
        canvas->addFigure(line);

        if (drawWallLabels) {
            Point2D mid{(wall.a.x + wall.b.x) / 2.0, (wall.a.y + wall.b.y) / 2.0};
            auto cm = toCanvasPoint(mid);
            auto *label = new cTextFigure();
            label->setText(wall.id.c_str());
            label->setPosition(cFigure::Point(cm.x, cm.y - 8.0));
            label->setColor(cFigure::BLACK);
            canvas->addFigure(label);
        }
    }
}

void RayTracing2DModule::drawMapDecorations()
{
    if (!par("drawMapDecorations").boolValue())
        return;

    cCanvas *canvas = sceneCanvas();
    const double W = view.canvasWidth;
    const double H = view.canvasHeight;

    // North arrow. The physical local frame is +Y north; when viewNorthUp=true
    // this arrow points toward decreasing canvas Y.
    if (view.northUp) {
        double x = W - 55.0;
        double yBottom = 100.0;
        double yTop = 55.0;
        auto *northLine = new cLineFigure();
        northLine->setStart({x, yBottom});
        northLine->setEnd({x, yTop});
        northLine->setLineWidth(2);
        northLine->setLineColor(cFigure::BLACK);
        canvas->addFigure(northLine);

        auto *northText = new cTextFigure();
        northText->setText("N");
        northText->setPosition({x, yTop - 8.0});
        northText->setAnchor(cFigure::ANCHOR_S);
        northText->setColor(cFigure::BLACK);
        canvas->addFigure(northText);
    }

    // A simple 1/2/5 scale bar based on the currently visible world width.
    double target = (view.worldMaxX - view.worldMinX) * 0.18;
    if (target > 0.0 && std::isfinite(target)) {
        double decade = std::pow(10.0, std::floor(std::log10(target)));
        double scaled = target / decade;
        double factor = scaled >= 5.0 ? 5.0 : (scaled >= 2.0 ? 2.0 : 1.0);
        double lengthM = factor * decade;
        double lengthCanvas = lengthM * view.scale;

        double x0 = 55.0;
        double y = H - 45.0;
        auto *bar = new cLineFigure();
        bar->setStart({x0, y});
        bar->setEnd({x0 + lengthCanvas, y});
        bar->setLineWidth(3);
        bar->setLineColor(cFigure::BLACK);
        canvas->addFigure(bar);

        for (double x : {x0, x0 + lengthCanvas}) {
            auto *tick = new cLineFigure();
            tick->setStart({x, y - 5.0});
            tick->setEnd({x, y + 5.0});
            tick->setLineWidth(2);
            tick->setLineColor(cFigure::BLACK);
            canvas->addFigure(tick);
        }

        std::ostringstream label;
        if (lengthM >= 1000.0)
            label << std::fixed << std::setprecision(lengthM >= 10000.0 ? 0 : 1)
                  << lengthM / 1000.0 << " km";
        else
            label << std::fixed << std::setprecision(0) << lengthM << " m";

        auto *scaleText = new cTextFigure();
        scaleText->setText(label.str().c_str());
        scaleText->setPosition({x0 + lengthCanvas / 2.0, y - 8.0});
        scaleText->setAnchor(cFigure::ANCHOR_S);
        scaleText->setColor(cFigure::BLACK);
        canvas->addFigure(scaleText);
    }
}

void RayTracing2DModule::drawScene()
{
    if (!getEnvir()->isGUI() || !par("drawScene").boolValue())
        return;

    if (!view.valid)
        configureView();

    cCanvas *canvas = sceneCanvas();
    canvas->setBackgroundColor(cFigure::WHITE);

    drawHeatmap();
    drawBuildingsAndWalls();

    // Always draw the geometric direct segment for the explicit RX. Red dashed
    // means blocked by geometry; green dashed means geometrically visible.
    double directMinX = std::min(tx.x, rx.x);
    double directMaxX = std::max(tx.x, rx.x);
    double directMinY = std::min(tx.y, rx.y);
    double directMaxY = std::max(tx.y, rx.y);
    if (worldBoxIntersectsView(directMinX, directMinY, directMaxX, directMaxY)) {
        auto *direct = new cLineFigure();
        direct->setStart(toCanvasPoint(tx));
        direct->setEnd(toCanvasPoint(rx));
        direct->setLineWidth(2);
        direct->setLineStyle(cFigure::LINE_DASHED);
        direct->setLineColor(traceResult.directBlocked ? cFigure::RED : cFigure::GREEN);
        canvas->addFigure(direct);
    }

    bool drawRayLabels = par("drawRayLabels").boolValue();
    int drawLimit = par("maxRayPathsToDraw").intValue();
    int drawn = 0;
    int pathIndex = 1;
    for (const auto& path : traceResult.paths) {
        if (path.isDirect)
            continue;
        if (drawLimit > 0 && drawn >= drawLimit)
            break;

        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        for (const auto& p : path.points) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        if (!worldBoxIntersectsView(minX, minY, maxX, maxY))
            continue;

        auto *polyline = new cPolylineFigure();
        std::vector<cFigure::Point> points;
        for (const auto& p : path.points)
            points.push_back(toCanvasPoint(p));
        polyline->setPoints(points);
        polyline->setLineWidth(2);
        polyline->setLineColor(path.diffractionOrder > 0 ? cFigure::MAGENTA : cFigure::BLUE);
        canvas->addFigure(polyline);

        if (drawRayLabels) {
            for (std::size_t interactionIndex = 0; interactionIndex < path.interactions.size(); ++interactionIndex) {
                std::string label = (path.diffractionOrder > 0 ? "D" : "R") +
                                    std::to_string(pathIndex) + "." +
                                    std::to_string(interactionIndex + 1);
                drawMarker(path.interactions[interactionIndex].position, label.c_str(), cFigure::YELLOW);
            }
        }
        ++pathIndex;
        ++drawn;
    }

    drawMarker(tx, "TX", cFigure::RED);
    drawMarker(rx, "RX", cFigure::BLUE);
    drawMapDecorations();

    std::ostringstream summary;
    summary << "RayTracing2D v1.6 Thesis Release\n"
            << "walls=" << walls.size() << " buildings=" << osmBuildingCount
            << " paths=" << traceResult.paths.size() << "\n"
            << "direct=" << (traceResult.directBlocked ? "BLOCKED" : "LOS") << "\n"
            << std::fixed << std::setprecision(2)
            << "Pr(total)=" << propagationSummary.totalReceivedPowerDbm << " dBm\n"
            << "view=[" << view.worldMinX << "," << view.worldMaxX << "] x ["
            << view.worldMinY << "," << view.worldMaxY << "] m\n"
            << "scale=" << view.scale << " canvas-units/m\n"
            << "grid=" << gridSamples.size() << " samples / " << gridEvaluationSeconds << " s";

    auto *text = new cTextFigure();
    text->setText(summary.str().c_str());
    text->setPosition(cFigure::Point(par("summaryX").doubleValue(), par("summaryY").doubleValue()));
    text->setAnchor(cFigure::ANCHOR_NW);
    text->setColor(cFigure::BLACK);
    canvas->addFigure(text);
}

void RayTracing2DModule::writeGridCsv(const std::string& fileName) const
{
    if (fileName.empty())
        return;
    std::ofstream out(fileName);
    if (!out)
        throw cRuntimeError("Cannot open grid CSV output file '%s'", fileName.c_str());

    out << "x_m,y_m,direct_blocked,candidate_paths,retained_paths,los_paths,reflection1_paths,reflection2_paths,diffraction_paths,"
           "first_order_theoretical,first_order_candidates,first_order_pruned,"
           "second_order_theoretical,second_order_candidates,second_order_pruned,"
           "diffraction_candidates,material_evaluated_paths,material_out_of_range_paths,"
           "received_power_dbm,incoherent_power_dbm,"
           "equivalent_path_loss_db,strongest_path_dbm,mean_delay_ns,rms_delay_spread_ns,max_excess_delay_ns,tx_domain,rx_domain,evaluation_status\n";
    out << std::setprecision(12);
    for (const auto& sample : gridSamples) {
        out << sample.position.x << ',' << sample.position.y << ',' << (sample.blocked ? 1 : 0) << ','
            << sample.candidatePathCount << ',' << sample.pathCount << ',' << sample.losPathCount << ','
            << sample.firstOrderPathCount << ',' << sample.secondOrderPathCount << ',' << sample.diffractionPathCount << ','
            << sample.firstOrderTheoretical << ',' << sample.firstOrderCandidates << ',' << sample.firstOrderPruned << ','
            << sample.secondOrderTheoretical << ',' << sample.secondOrderCandidates << ',' << sample.secondOrderPruned << ','
            << sample.diffractionCandidates << ','
            << sample.materialEvaluatedPathCount << ',' << sample.materialOutOfRangePathCount << ','
            << sample.receivedPowerDbm << ',' << sample.incoherentPowerDbm << ',' << sample.equivalentPathLossDb << ','
            << sample.strongestPathPowerDbm << ',' << sample.meanDelayS * 1e9 << ',' << sample.rmsDelaySpreadS * 1e9 << ','
            << sample.maxExcessDelayS * 1e9 << ',' << domainName(sample.txDomain) << ',' << domainName(sample.rxDomain) << ','
            << (sample.txDomain!=PointDomain2D::EXTERIOR ? "excluded_tx" : sample.rxDomain!=PointDomain2D::EXTERIOR ? "excluded_rx" : sample.pathCount>0 ? "exterior_with_path" : "exterior_no_path") << '\n';
    }
}

void RayTracing2DModule::writeCsv(const std::string& fileName) const
{
    if (fileName.empty())
        return;

    std::ofstream out(fileName);
    if (!out)
        throw cRuntimeError("Cannot open CSV output file '%s'", fileName.c_str());

    out << "path_id,type,reflection_order,diffraction_order,distance_m,delay_s,path_loss_db,phase_rad,received_power_dbm,"
           "reflection_magnitude,reflection_phase_rad,material_model_in_range,diffraction_v,diffraction_loss_db,interactions,points\n";
    out << std::setprecision(12);

    for (const auto& path : traceResult.paths) {
        std::ostringstream pointList;
        for (std::size_t i = 0; i < path.points.size(); ++i) {
            if (i)
                pointList << "->";
            pointList << "(" << path.points[i].x << ":" << path.points[i].y << ")";
        }

        out << path.id << ',' << typeName(path) << ',' << path.reflectionOrder << ',' << path.diffractionOrder << ','
            << path.totalDistanceM << ',' << path.delayS << ',' << path.pathLossDb << ','
            << path.phaseRad << ',' << path.receivedPowerDbm << ','
            << path.reflectionCoefficientMagnitude << ',' << path.reflectionCoefficientPhaseRad << ','
            << (path.materialModelInRange ? 1 : 0) << ',' << path.diffractionParameterV << ','
            << path.diffractionLossDb << ',' << '"' << interactionSummary(path) << '"' << ','
            << '"' << pointList.str() << '"' << "\n";
    }
}

void RayTracing2DModule::writeFrequencySweepCsv(const std::string& fileName) const
{
    if (fileName.empty())
        return;
    std::ofstream out(fileName);
    if (!out)
        throw cRuntimeError("Cannot open frequency-sweep CSV output file '%s'", fileName.c_str());
    out << "frequency_hz,candidate_paths,retained_paths,reflected_paths,material_out_of_range_paths,"
           "received_power_dbm,incoherent_power_dbm,equivalent_path_loss_db,rms_delay_spread_ns\n";
    out << std::setprecision(12);
    for (const auto& sample : frequencySweepSamples) {
        out << sample.frequencyHz << ',' << sample.candidatePathCount << ','
            << sample.retainedPathCount << ',' << sample.reflectedPathCount << ','
            << sample.materialOutOfRangePathCount << ',' << sample.receivedPowerDbm << ','
            << sample.incoherentPowerDbm << ',' << sample.equivalentPathLossDb << ','
            << sample.rmsDelaySpreadS * 1e9 << '\n';
    }
}

void RayTracing2DModule::recordResults()
{
    recordScalar("osmNodeCount", osmNodeCount);
    recordScalar("osmBuildingCount", osmBuildingCount);
    recordScalar("wallCount", static_cast<double>(walls.size()));
    recordScalar("viewWorldMinX", view.worldMinX);
    recordScalar("viewWorldMaxX", view.worldMaxX);
    recordScalar("viewWorldMinY", view.worldMinY);
    recordScalar("viewWorldMaxY", view.worldMaxY);
    recordScalar("viewScaleCanvasUnitsPerM", view.scale);
    recordScalar("gridSampleCount", static_cast<double>(gridSamples.size()));
    recordScalar("gridEvaluationSeconds", gridEvaluationSeconds);
    recordScalar("frequencySweepSampleCount", static_cast<double>(frequencySweepSamples.size()));
    recordScalar("directBlocked", traceResult.directBlocked ? 1.0 : 0.0);
    recordScalar("candidatePaths", singleCandidatePathCount);
    recordScalar("numPaths", static_cast<double>(traceResult.paths.size()));
    recordScalar("firstOrderCandidates", traceResult.statistics.firstOrderCandidates);
    recordScalar("firstOrderTheoretical", static_cast<double>(traceResult.statistics.firstOrderTheoretical));
    recordScalar("firstOrderPruned", static_cast<double>(traceResult.statistics.firstOrderPruned));
    recordScalar("firstOrderValid", traceResult.statistics.firstOrderValid);
    recordScalar("secondOrderCandidates", traceResult.statistics.secondOrderCandidates);
    recordScalar("secondOrderTheoretical", static_cast<double>(traceResult.statistics.secondOrderTheoretical));
    recordScalar("secondOrderPruned", static_cast<double>(traceResult.statistics.secondOrderPruned));
    recordScalar("secondOrderValid", traceResult.statistics.secondOrderValid);
    recordScalar("diffractionCandidates", traceResult.statistics.diffractionCandidates);
    recordScalar("diffractionValid", traceResult.statistics.diffractionValid);
    recordScalar("totalReceivedPowerDbm", propagationSummary.totalReceivedPowerDbm);
    recordScalar("totalIncoherentPowerDbm", propagationSummary.totalIncoherentPowerDbm);
    recordScalar("equivalentPathLossDb", propagationSummary.equivalentPathLossDb);
    recordScalar("strongestPathPowerDbm", propagationSummary.strongestPathPowerDbm);
    recordScalar("meanDelayS", propagationSummary.meanDelayS);
    recordScalar("rmsDelaySpreadS", propagationSummary.rmsDelaySpreadS);
    recordScalar("maxExcessDelayS", propagationSummary.maxExcessDelayS);

    int finiteGrid = 0;
    int noPathGrid = 0;
    int interiorGrid=0,boundaryGrid=0,excludedTxGrid=0,exteriorGrid=0;
    long long totalGridCandidates = 0;
    long long totalGridRetained = 0;
    long long totalGridMaterialEvaluated = 0;
    long long totalGridMaterialOutOfRange = 0;
    int gridCellsWithMaterialExtrapolation = 0;
    double gridPowerSum = 0.0;
    double gridMinPower = std::numeric_limits<double>::infinity();
    double gridMaxPower = -std::numeric_limits<double>::infinity();
    for (const auto& sample : gridSamples) {
        if(sample.txDomain!=PointDomain2D::EXTERIOR)++excludedTxGrid;
        if(sample.rxDomain==PointDomain2D::INTERIOR)++interiorGrid;
        else if(sample.rxDomain==PointDomain2D::BOUNDARY)++boundaryGrid;
        if(sample.txDomain==PointDomain2D::EXTERIOR && sample.rxDomain==PointDomain2D::EXTERIOR)++exteriorGrid;
        totalGridCandidates += sample.candidatePathCount;
        totalGridRetained += sample.pathCount;
        totalGridMaterialEvaluated += sample.materialEvaluatedPathCount;
        totalGridMaterialOutOfRange += sample.materialOutOfRangePathCount;
        if (sample.materialOutOfRangePathCount > 0)
            ++gridCellsWithMaterialExtrapolation;
        if (std::isfinite(sample.receivedPowerDbm)) {
            ++finiteGrid;
            gridPowerSum += sample.receivedPowerDbm;
            gridMinPower = std::min(gridMinPower, sample.receivedPowerDbm);
            gridMaxPower = std::max(gridMaxPower, sample.receivedPowerDbm);
        }
        else if(sample.txDomain==PointDomain2D::EXTERIOR && sample.rxDomain==PointDomain2D::EXTERIOR) {
            ++noPathGrid;
        }
    }
    recordScalar("gridFiniteSamples", finiteGrid);
    recordScalar("gridNoPathSamples", noPathGrid);
    recordScalar("gridExteriorSamples",exteriorGrid);
    recordScalar("gridInteriorSamples",interiorGrid);
    recordScalar("gridBoundarySamples",boundaryGrid);
    recordScalar("gridExcludedTxSamples",excludedTxGrid);
    recordScalar("txDomainCode",static_cast<int>(traceResult.txDomain));
    recordScalar("rxDomainCode",static_cast<int>(traceResult.rxDomain));
    if(exteriorGrid>0)recordScalar("gridExteriorAvailability",double(finiteGrid)/exteriorGrid);
    recordScalar("gridTotalCandidatePaths", static_cast<double>(totalGridCandidates));
    recordScalar("gridTotalRetainedPaths", static_cast<double>(totalGridRetained));
    recordScalar("gridMaterialEvaluatedPaths", static_cast<double>(totalGridMaterialEvaluated));
    recordScalar("gridMaterialOutOfRangePaths", static_cast<double>(totalGridMaterialOutOfRange));
    recordScalar("gridCellsWithMaterialExtrapolation", gridCellsWithMaterialExtrapolation);
    if (finiteGrid > 0) {
        recordScalar("gridMeanReceivedPowerDbm", gridPowerSum / finiteGrid);
        recordScalar("gridMinReceivedPowerDbm", gridMinPower);
        recordScalar("gridMaxReceivedPowerDbm", gridMaxPower);
    }

    if (!frequencySweepSamples.empty()) {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        int outOfRangeSamples = 0;
        for (const auto& sample : frequencySweepSamples) {
            if (std::isfinite(sample.receivedPowerDbm)) {
                minimum = std::min(minimum, sample.receivedPowerDbm);
                maximum = std::max(maximum, sample.receivedPowerDbm);
            }
            if (sample.materialOutOfRangePathCount > 0)
                ++outOfRangeSamples;
        }
        if (std::isfinite(minimum)) {
            recordScalar("frequencySweepMinReceivedPowerDbm", minimum);
            recordScalar("frequencySweepMaxReceivedPowerDbm", maximum);
            recordScalar("frequencySweepRippleDb", maximum - minimum);
        }
        recordScalar("frequencySweepMaterialOutOfRangeSamples", outOfRangeSamples);
    }

    for (std::size_t i = 0; i < traceResult.paths.size(); ++i) {
        const auto& p = traceResult.paths[i];
        std::string prefix = "path" + std::to_string(i) + "_";
        recordScalar((prefix + "distanceM").c_str(), p.totalDistanceM);
        recordScalar((prefix + "delayS").c_str(), p.delayS);
        recordScalar((prefix + "pathLossDb").c_str(), p.pathLossDb);
        recordScalar((prefix + "phaseRad").c_str(), p.phaseRad);
        recordScalar((prefix + "receivedPowerDbm").c_str(), p.receivedPowerDbm);
        recordScalar((prefix + "reflectionOrder").c_str(), p.reflectionOrder);
        recordScalar((prefix + "diffractionOrder").c_str(), p.diffractionOrder);
        recordScalar((prefix + "reflectionMagnitude").c_str(), p.reflectionCoefficientMagnitude);
        recordScalar((prefix + "reflectionPhaseRad").c_str(), p.reflectionCoefficientPhaseRad);
        recordScalar((prefix + "materialModelInRange").c_str(), p.materialModelInRange ? 1.0 : 0.0);
        recordScalar((prefix + "diffractionV").c_str(), p.diffractionParameterV);
        recordScalar((prefix + "diffractionLossDb").c_str(), p.diffractionLossDb);
    }
}

void RayTracing2DModule::printResults() const
{
    EV_INFO << "\n=== RayTracing2D v1.6 Thesis Release ===\n";
    EV_INFO << "TX = (" << tx.x << ", " << tx.y << ") m\n";
    EV_INFO << "RX = (" << rx.x << ", " << rx.y << ") m\n";
    EV_INFO << "Walls = " << walls.size() << ", OSM buildings = " << osmBuildingCount << "\n";
    if (!par("osmFile").stdstringValue().empty()) {
        EV_INFO << "OSM projection origin = (" << par("osmOriginLatDeg").doubleValue()
                << ", " << par("osmOriginLonDeg").doubleValue() << ") deg\n";
    }
    EV_INFO << "World view extent X = [" << view.worldMinX << ", " << view.worldMaxX
            << "] m, Y = [" << view.worldMinY << ", " << view.worldMaxY << "] m\n";
    EV_INFO << "Canvas fit = " << view.canvasWidth << " x " << view.canvasHeight
            << " units, scale = " << view.scale << " canvas-units/m, northUp="
            << (view.northUp ? "true" : "false") << "\n";
    EV_INFO << "Direct path: " << (traceResult.directBlocked ? "BLOCKED" : "LOS") << "\n";
    EV_INFO << "Candidate paths before selection: " << singleCandidatePathCount << "\n";
    EV_INFO << "Retained paths: " << traceResult.paths.size() << "\n";
    EV_INFO << "R1 candidates/valid = " << traceResult.statistics.firstOrderCandidates << "/"
            << traceResult.statistics.firstOrderValid << "\n";
    EV_INFO << "R2 candidates/valid = " << traceResult.statistics.secondOrderCandidates << "/"
            << traceResult.statistics.secondOrderValid << "\n";
    EV_INFO << "D1 candidates/valid = " << traceResult.statistics.diffractionCandidates << "/"
            << traceResult.statistics.diffractionValid << "\n\n";

    for (std::size_t i = 0; i < traceResult.paths.size(); ++i) {
        const auto& p = traceResult.paths[i];
        EV_INFO << "Path " << i << " [" << p.id << "]\n";
        EV_INFO << "  type             = " << typeName(p) << "\n";
        EV_INFO << "  reflection order = " << p.reflectionOrder << "\n";
        EV_INFO << "  diffraction order= " << p.diffractionOrder << "\n";
        EV_INFO << "  distance          = " << p.totalDistanceM << " m\n";
        EV_INFO << "  delay             = " << p.delayS * 1e6 << " us\n";
        EV_INFO << "  path loss         = " << p.pathLossDb << " dB\n";
        EV_INFO << "  phase             = " << p.phaseRad << " rad\n";
        EV_INFO << "  received power    = " << p.receivedPowerDbm << " dBm\n";
        EV_INFO << "  |Gamma product|   = " << p.reflectionCoefficientMagnitude << "\n";
        EV_INFO << "  arg(Gamma product)= " << p.reflectionCoefficientPhaseRad << " rad\n";
        EV_INFO << "  material range    = " << (p.materialModelInRange ? "IN" : "EXTRAPOLATED") << "\n";
        EV_INFO << "  diffraction v     = " << p.diffractionParameterV << "\n";
        EV_INFO << "  diffraction loss  = " << p.diffractionLossDb << " dB\n";
        for (const auto& interaction : p.interactions) {
            if (interaction.type == InteractionType::REFLECTION)
                EV_INFO << "  incidence angle   = " << interaction.incidenceAngleRad * 180.0 / PI
                        << " deg on " << interaction.objectId << " (" << interaction.material << ")\n";
        }
    }

    EV_INFO << "\nCombined received power = " << propagationSummary.totalReceivedPowerDbm << " dBm\n";
    EV_INFO << "Incoherent power         = " << propagationSummary.totalIncoherentPowerDbm << " dBm\n";
    EV_INFO << "Equivalent path loss     = " << propagationSummary.equivalentPathLossDb << " dB\n";
    EV_INFO << "RMS delay spread         = " << propagationSummary.rmsDelaySpreadS * 1e9 << " ns\n";
    if (!gridSamples.empty())
        EV_INFO << "Grid samples/time        = " << gridSamples.size() << " / "
                << gridEvaluationSeconds << " s\n";
    if (!frequencySweepSamples.empty())
        EV_INFO << "Frequency sweep samples = " << frequencySweepSamples.size() << "\n";
    EV_INFO << "==========================\n\n";
}

void RayTracing2DModule::initialize()
{
    tx = {par("txX").doubleValue(), par("txY").doubleValue()};
    rx = {par("rxX").doubleValue(), par("rxY").doubleValue()};
    walls = parseWalls(par("walls").stdstringValue());

    std::string osmFile = par("osmFile").stdstringValue();
    if (!osmFile.empty()) {
        auto imported = OsmImporter2D::load(osmFile,
                                            par("osmOriginLatDeg").doubleValue(),
                                            par("osmOriginLonDeg").doubleValue(),
                                            par("osmDefaultMaterial").stdstringValue());
        osmBuildingCount = imported.buildingCount;
        osmNodeCount = imported.sourceNodeCount;

        std::string forcedMaterial = par("osmForceMaterial").stdstringValue();
        if (!forcedMaterial.empty()) {
            // Validate the experimental override early and apply it only to
            // OSM-derived walls; manually specified walls keep their material.
            try {
                (void)Materials2D::get(forcedMaterial);
            }
            catch (const std::exception& e) {
                throw cRuntimeError("Invalid osmForceMaterial '%s': %s",
                                    forcedMaterial.c_str(), e.what());
            }
            for (auto& wall : imported.walls)
                wall.material = forcedMaterial;
        }
        walls.insert(walls.end(), imported.walls.begin(), imported.walls.end());
    }

    double tolerance = par("numericalTolerance").doubleValue();
    RayTracer2D tracer(walls, tolerance);
    auto selection = pathSelectionFromParameters();
    traceResult = tracer.trace(tx, rx,
                               par("enableLos").boolValue(),
                               par("enableFirstOrderReflections").boolValue(),
                               par("enableSecondOrderReflections").boolValue(),
                               par("enableDiffraction").boolValue(),
                               selection.maxPathDistanceM);
    const std::vector<RayPath> geometricPaths = traceResult.paths;

    PropagationConfig cfg;
    cfg.frequencyHz = par("frequency").doubleValue();
    cfg.txPowerDbm = par("txPowerDbm").doubleValue();
    cfg.reflectionLossDb = par("reflectionLossDb").doubleValue();
    cfg.reflectionPhaseShiftDeg = par("reflectionPhaseShiftDeg").doubleValue();
    std::string reflectionModel = par("reflectionModel").stdstringValue();
    if (reflectionModel != "fresnel" && reflectionModel != "fixed")
        throw cRuntimeError("Unknown reflectionModel '%s'; expected 'fresnel' or 'fixed'", reflectionModel.c_str());
    cfg.useFresnel = reflectionModel == "fresnel";
    std::string polarization = par("polarization").stdstringValue();
    if (polarization == "TE")
        cfg.polarization = Polarization2D::TE;
    else if (polarization == "TM")
        cfg.polarization = Polarization2D::TM;
    else
        throw cRuntimeError("Unknown polarization '%s'; expected 'TE' or 'TM'", polarization.c_str());
    cfg.coherentCombination = par("coherentCombination").boolValue();

    propagationSummary = Propagation2D::evaluateAndSelect(traceResult.paths, cfg, selection,
                                                          &singleCandidatePathCount);

    if (par("enableFrequencySweep").boolValue()) {
        FrequencySweepDefinition2D sweep;
        sweep.startHz = par("sweepStartFrequency").doubleValue();
        sweep.stopHz = par("sweepStopFrequency").doubleValue();
        sweep.points = par("sweepPoints").intValue();
        try {
            frequencySweepSamples = FrequencySweep2D::evaluate(geometricPaths, sweep, cfg, selection);
        }
        catch (const std::exception& e) {
            throw cRuntimeError("Invalid frequency sweep: %s", e.what());
        }
    }

    if (par("enableGrid").boolValue()) {
        GridDefinition2D grid{par("gridMinX").doubleValue(), par("gridMaxX").doubleValue(),
                              par("gridMinY").doubleValue(), par("gridMaxY").doubleValue(),
                              par("gridStepX").doubleValue(), par("gridStepY").doubleValue()};
        GridTraceOptions2D options;
        options.los = par("enableLos").boolValue();
        options.firstReflection = par("enableFirstOrderReflections").boolValue();
        options.secondReflection = par("enableSecondOrderReflections").boolValue();
        options.diffraction = par("enableDiffraction").boolValue();
        options.selection = selection;
        std::string gridMode = par("gridMode").stdstringValue();
        if (gridMode == "raytracing")
            options.mode = GridMode2D::RAY_TRACING;
        else if (gridMode == "fspl")
            options.mode = GridMode2D::FSPL_BASELINE;
        else
            throw cRuntimeError("Unknown gridMode '%s'; expected 'raytracing' or 'fspl'", gridMode.c_str());

        auto start = std::chrono::steady_clock::now();
        gridSamples = GridEvaluator2D::evaluate(tracer, tx, grid, options, cfg);
        auto stop = std::chrono::steady_clock::now();
        gridEvaluationSeconds = std::chrono::duration<double>(stop - start).count();
    }

    // Configure the drawing transform even in Cmdenv so the world extent and
    // visualization scale are reproducible in logs/scalars. This does NOT alter
    // any physical coordinate or traced path.
    configureView();

    printResults();
    recordResults();
    writeCsv(par("outputCsv").stdstringValue());
    writeGridCsv(par("gridOutputCsv").stdstringValue());
    writeFrequencySweepCsv(par("frequencySweepOutputCsv").stdstringValue());
    drawScene();
}

void RayTracing2DModule::handleMessage(cMessage *msg)
{
    delete msg;
}
