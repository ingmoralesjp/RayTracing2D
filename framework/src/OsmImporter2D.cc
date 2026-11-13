#include "OsmImporter2D.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double EARTH_RADIUS_M = 6371008.8;

std::string attribute(const std::string& tag, const std::string& name)
{
    std::regex expression(name + R"re(\s*=\s*"([^"]*)")re");
    std::smatch match;
    return std::regex_search(tag, match, expression) ? match[1].str() : "";
}

Point2D project(double latDeg, double lonDeg, double originLatDeg, double originLonDeg)
{
    double lat0 = originLatDeg * PI / 180.0;
    return {EARTH_RADIUS_M * std::cos(lat0) * (lonDeg - originLonDeg) * PI / 180.0,
            EARTH_RADIUS_M * (latDeg - originLatDeg) * PI / 180.0};
}

std::string normalizedMaterial(const std::string& raw, const std::string& fallback)
{
    auto normalize = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (char& c : value)
            if (c == '-' || c == ' ') c = '_';
        return value;
    };

    std::string value = normalize(raw.empty() ? fallback : raw);
    if (value == "concrete" || value == "cement" || value == "cement_block" ||
        value == "concrete_block" || value == "reinforced_concrete")
        return "concrete";
    if (value == "brick" || value == "bricks" || value == "masonry" || value == "stone")
        return "brick";
    if (value == "glass" || value == "glass_panels")
        return "glass";
    if (value == "wood" || value == "timber")
        return "wood";
    if (value == "metal" || value == "steel" || value == "aluminium" || value == "aluminum")
        return "metal";
    if (value == "plasterboard" || value == "drywall" || value == "gypsum")
        return "plasterboard";
    if (value == "generic" || value == "air")
        return value;

    // OSM material vocabulary is open-ended. Unknown tags fall back explicitly
    // instead of making the propagation stage fail on an unrecognized string.
    std::string normalizedFallback = normalize(fallback);
    if (normalizedFallback == value)
        return "generic";
    return normalizedMaterial(normalizedFallback, "generic");
}
}

OsmImportResult2D OsmImporter2D::load(const std::string& fileName,
                                      double originLatDeg, double originLonDeg,
                                      const std::string& defaultMaterial)
{
    if (!std::isfinite(originLatDeg) || !std::isfinite(originLonDeg))
        throw std::invalid_argument("OSM projection origin must be finite");
    std::ifstream input(fileName);
    if (!input)
        throw std::runtime_error("Cannot open OSM file: " + fileName);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string xml = buffer.str();

    std::unordered_map<std::string, Point2D> nodes;
    std::regex nodePattern(R"re(<node\b[^>]*>)re");
    for (std::sregex_iterator it(xml.begin(), xml.end(), nodePattern), end; it != end; ++it) {
        std::string tag = it->str();
        std::string id = attribute(tag, "id");
        std::string lat = attribute(tag, "lat");
        std::string lon = attribute(tag, "lon");
        if (id.empty() || lat.empty() || lon.empty())
            throw std::runtime_error("Malformed OSM node");
        nodes[id] = project(std::stod(lat), std::stod(lon), originLatDeg, originLonDeg);
    }

    // Bound relation bodies with string searches: recursive std::regex over a
    // large transport relation can exhaust the stack even when it is not a building.
    std::size_t relationOffset=0;
    while ((relationOffset=xml.find("<relation ",relationOffset))!=std::string::npos) {
        auto relationEnd=xml.find("</relation>",relationOffset);
        if(relationEnd==std::string::npos)throw std::invalid_argument("Unclosed OSM relation");
        const std::string body=xml.substr(relationOffset,relationEnd-relationOffset);
        std::regex relationTag(R"re(<tag\b[^>]*>)re");
        for(std::sregex_iterator it(body.begin(),body.end(),relationTag),end;it!=end;++it) {
            if(attribute(it->str(),"k")=="building" && !attribute(it->str(),"v").empty() && attribute(it->str(),"v")!="no")
                throw std::invalid_argument("OSM building relations/multipolygons are unsupported; supply simple closed ways");
        }
        relationOffset=relationEnd+11;
    }
    OsmImportResult2D result;
    result.sourceNodeCount = static_cast<int>(nodes.size());
    std::regex wayPattern(R"re(<way\b([^>]*)>([\s\S]*?)</way>)re");
    std::regex ndPattern(R"re(<nd\b[^>]*>)re");
    std::regex tagPattern(R"re(<tag\b[^>]*>)re");
    for (std::sregex_iterator it(xml.begin(), xml.end(), wayPattern), end; it != end; ++it) {
        std::string opening = (*it)[1].str();
        std::string body = (*it)[2].str();
        std::string wayId = attribute(opening, "id");
        bool building = false;
        std::string material = normalizedMaterial(defaultMaterial, "generic");
        for (std::sregex_iterator jt(body.begin(), body.end(), tagPattern), tagEnd; jt != tagEnd; ++jt) {
            std::string tag = jt->str();
            std::string key = attribute(tag, "k");
            std::string value = attribute(tag, "v");
            if (key == "building" && !value.empty() && value != "no")
                building = true;
            if (key == "building:material" && !value.empty())
                material = normalizedMaterial(value, defaultMaterial);
        }
        if (!building)
            continue;

        std::vector<std::string> refs;
        for (std::sregex_iterator jt(body.begin(), body.end(), ndPattern), ndEnd; jt != ndEnd; ++jt)
            refs.push_back(attribute(jt->str(), "ref"));
        if (refs.size() < 4 || refs.front() != refs.back())
            throw std::runtime_error("OSM building way " + wayId + " must be a closed polygon");
        for (std::size_t i = 1; i < refs.size(); ++i) {
            auto first = nodes.find(refs[i - 1]);
            auto second = nodes.find(refs[i]);
            if (first == nodes.end() || second == nodes.end())
                throw std::runtime_error("OSM way " + wayId + " references a missing node");
            result.walls.push_back({"osm_" + wayId + "_e" + std::to_string(i - 1),
                                    first->second, second->second, material, "osm_" + wayId});
        }
        ++result.buildingCount;
    }
    return result;
}
