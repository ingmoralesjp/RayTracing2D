#pragma once

#include "Geometry2D.h"

#include <string>
#include <vector>

struct OsmImportResult2D {
    std::vector<Wall2D> walls;
    int buildingCount = 0;
    int sourceNodeCount = 0;
};

class OsmImporter2D {
  public:
    static OsmImportResult2D load(const std::string& fileName,
                                  double originLatDeg,
                                  double originLonDeg,
                                  const std::string& defaultMaterial = "concrete");
};
