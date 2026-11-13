#include "RayTracer2D.h"
#include "Propagation2D.h"
#include "Materials2D.h"
#include "GridEvaluator2D.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
std::vector<Wall2D> polygon(const std::vector<Point2D>& points,const std::string& id="solid") {
    std::vector<Wall2D>w;
    for(std::size_t i=0;i<points.size();++i)
        w.push_back({id+std::to_string(i),points[i],points[(i+1)%points.size()],"concrete",id});
    return w;
}
}
int main() {
    // Independent two-ray analytical solution, no production Fresnel or FSPL
    // used to obtain the expected field. exp(+jwt), epsilon-j*sigma/(w*eps0).
    const double pi=std::acos(-1.0),c=299792458.0,f=2.4e9,lambda=c/f;
    const double d0=16,d1=std::sqrt(292.0),cosTheta=6/d1;
    const std::complex<double> epsilon(5.24,-0.0462*std::pow(2.4,0.7822)/(2*pi*f*8.854187817e-12));
    auto q=std::sqrt(epsilon-(1-cosTheta*cosTheta));
    auto gamma=(cosTheta-q)/(cosTheta+q);
    auto field=lambda/(4*pi)*(std::polar(1.0/d0,-2*pi*d0/lambda)+gamma*std::polar(1.0/d1,-2*pi*d1/lambda));
    const double expected=14+10*std::log10(std::norm(field));
    require(std::abs(expected-(-47.717029647))<1e-8,"independent analytic fixture");
    RayTracer2D twoRay({{"screen",{1836.748,462},{1856.748,462},"concrete"}});
    auto paths=twoRay.trace({1838.748,465},{1854.748,465},true,true,false,false).paths;
    PropagationConfig cfg;cfg.frequencyHz=f;cfg.txPowerDbm=14;cfg.polarization=Polarization2D::TE;cfg.coherentCombination=true;
    auto sum=Propagation2D::evaluate(paths,cfg);
    require(paths.size()==2,"two-ray path count");
    require(std::abs(sum.totalReceivedPowerDbm-expected)<1e-8,"lossy TE coherent phase regression");
    require(std::abs(sum.totalReceivedPowerDbm-(-47.547608211))>0.1,"reject historical phase");
    require(std::abs(std::norm(std::conj(field))-std::norm(field))<1e-20,"whole-field conjugation preserves power");
    std::cout<<"PASS 1 independent lossy TE phase regression\n";

    auto square=polygon({{0,0},{10,0},{10,10},{0,10}});RayTracer2D solid(square);
    auto untagged=square;for(auto&w:untagged)w.buildingId.clear();
    RayTracer2D inferred(untagged);
    require(inferred.getBuildings().size()==1 && inferred.trace({-5,5},{5,5},true,true,true,true).paths.empty(),"infer opaque square in legacy wall-list API");
    auto interior=solid.trace({-5,5},{5,5},true,true,true,true);
    require(interior.paths.empty()&&interior.rxDomain==PointDomain2D::INTERIOR,"interior RX must be excluded");
    auto insideTx=solid.trace({5,5},{-5,5},true,true,true,true);
    require(insideTx.paths.empty()&&insideTx.txDomain==PointDomain2D::INTERIOR,"interior TX must be excluded");
    std::cout<<"PASS 2 interior RX; PASS 3 interior TX\n";
    auto exterior=solid.trace({-5,5},{12,12},true,true,true,true);
    int diffracted=0;for(const auto&p:exterior.paths)diffracted+=p.diffractionOrder==1;
    require(diffracted==1,"preserve valid exterior D1, reject inward leg");
    require(solid.isBlocked({0,0},{10,10},"solid0"),"corner-to-corner interior crossing");
    require(solid.isBlocked({-1,5},{11,5}),"exterior endpoints crossing solid");
    require(!solid.segmentEntersBuilding({-1,11},{11,11}),"exterior clear segment");
    std::cout<<"PASS 4 exterior D1; PASS 5 crossing interiors\n";
    auto grazing=solid.trace({-5,5},{5,15},true,true,true,true);int count=0;
    for(const auto&p:grazing.paths)count+=p.diffractionOrder==1;
    require(count==1,"shared building corner must not duplicate D1");
    std::reverse(square.begin(),square.end());RayTracer2D reordered(square);
    require(reordered.classifyPoint({5,5})==PointDomain2D::INTERIOR,"ownership independent of wall ordering");
    std::cout<<"PASS 6 unique corners and wall ordering\n";
    const double tol=1e-8;
    require(solid.classifyPoint({0,5})==PointDomain2D::BOUNDARY,"facade boundary");
    require(solid.classifyPoint({0,0})==PointDomain2D::BOUNDARY,"vertex boundary");
    require(solid.classifyPoint({-tol/2,5})==PointDomain2D::BOUNDARY,"absolute metric tolerance band");
    require(solid.classifyPoint({-2*tol,5})==PointDomain2D::EXTERIOR,"just outside tolerance");
    require(solid.classifyPoint({2*tol,5})==PointDomain2D::INTERIOR,"just inside tolerance");
    require(solid.trace({-5,5},{0,0}).paths.empty(),"boundary receiver excluded");
    auto shifted=polygon({{1e6,1e6},{1e6+10,1e6},{1e6+10,1e6+10},{1e6,1e6+10}});
    RayTracer2D translated(shifted);
    require(translated.classifyPoint({1e6-2*tol,1e6+5})==PointDomain2D::EXTERIOR,"domain origin invariance");
    std::cout<<"PASS 7 facade/vertex tolerances and translation\n";
    RayTracer2D concave(polygon({{0,0},{10,0},{10,4},{4,4},{4,10},{0,10}}));
    require(concave.classifyPoint({6,6})==PointDomain2D::EXTERIOR,"concave notch exterior");
    require(concave.classifyPoint({2,6})==PointDomain2D::INTERIOR,"concave arm interior");
    require(concave.segmentEntersBuilding({0,10},{10,0}),"concave corner chord crosses solid");
    require(!concave.segmentEntersBuilding({4,10},{10,4}),"concave notch chord exterior");
    std::cout<<"PASS 8 concave polygon domain and chords\n";
    RayTracer2D knife({{"knife",{0,-2},{0,2},"concrete"}});
    require(knife.getBuildings().empty(),"isolated wall is not solid polygon");
    require(knife.trace({-5,0},{5,0},true,false,false,true).paths.size()==2,"preserve ideal knife-edge screen");
    std::cout<<"PASS 9 isolated knife-edge compatibility\n";
    bool rejected=false;try{RayTracer2D bad(polygon({{0,0},{10,10},{0,10},{10,0}}));}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"reject self-intersecting solid");
    square.pop_back();rejected=false;try{RayTracer2D bad(square);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"reject open ownership ring");
    std::cout<<"PASS 10 unsupported polygon failures explicit\n";
    GridTraceOptions2D options;options.diffraction=true;options.secondReflection=true;
    auto grid=GridEvaluator2D::evaluate(solid,{-5,5},{-5,5,6,6,5,1},options,cfg);
    require(grid.size()==3,"grid size");
    require(grid[2].rxDomain==PointDomain2D::INTERIOR&&grid[2].pathCount==0&&std::isnan(grid[2].receivedPowerDbm),"excluded grid NaN");
    require(grid[1].rxDomain==PointDomain2D::BOUNDARY&&std::isnan(grid[1].receivedPowerDbm),"boundary grid NaN");
    require(grid[0].rxDomain==PointDomain2D::EXTERIOR,"exterior domain classified");
    std::cout<<"PASS 11 explicit grid domain masks\nALL 11 correction test groups passed\n";
}
