#pragma once

#include <omnetpp.h>

#include "Geometry2D.h"
#include "RayTracer2D.h"
#include "Propagation2D.h"
#include "OsmImporter2D.h"
#include "GridEvaluator2D.h"
#include "FrequencySweep2D.h"

#include <vector>

class RayTracing2DModule : public omnetpp::cSimpleModule {
  protected:
    virtual void initialize() override;
    virtual void handleMessage(omnetpp::cMessage *msg) override;

  private:
    struct ViewTransform2D {
        bool valid = false;
        double worldMinX = 0.0;
        double worldMaxX = 1.0;
        double worldMinY = 0.0;
        double worldMaxY = 1.0;
        double canvasWidth = 1200.0;
        double canvasHeight = 800.0;
        double margin = 35.0;
        double scale = 1.0;
        double left = 0.0;
        double top = 0.0;
        bool northUp = true;
    };

    Point2D tx;
    Point2D rx;
    std::vector<Wall2D> walls;
    TraceResult traceResult;
    PropagationSummary propagationSummary;
    int singleCandidatePathCount = 0;
    int osmBuildingCount = 0;
    int osmNodeCount = 0;
    std::vector<GridSample2D> gridSamples;
    std::vector<FrequencySweepSample2D> frequencySweepSamples;
    double gridEvaluationSeconds = 0.0;
    ViewTransform2D view;

    std::vector<Wall2D> parseWalls(const std::string& specification) const;
    PathSelectionConfig2D pathSelectionFromParameters() const;
    void drawScene();
    void writeCsv(const std::string& fileName) const;
    void writeGridCsv(const std::string& fileName) const;
    void writeFrequencySweepCsv(const std::string& fileName) const;
    void drawHeatmap();
    void drawBuildingsAndWalls();
    void drawMapDecorations();
    void recordResults();
    void printResults() const;

    void configureView();
    omnetpp::cFigure::Point toCanvasPoint(const Point2D& p) const;
    omnetpp::cFigure::Rectangle toCanvasRectangle(const Point2D& center,
                                                  double widthWorld,
                                                  double heightWorld) const;
    bool worldBoxIntersectsView(double minX, double minY, double maxX, double maxY) const;

    omnetpp::cCanvas *sceneCanvas();
    void drawMarker(const Point2D& p, const char *label, const omnetpp::cFigure::Color& color);
};
