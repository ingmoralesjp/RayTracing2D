#include "OsmImporter2D.h"
#include "RayTracer2D.h"
#include "Propagation2D.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
double length(Point2D a){return std::hypot(a.x,a.y);}
Point2D sub(Point2D a,Point2D b){return {a.x-b.x,a.y-b.y};}
Point2D unit(Point2D a){auto d=length(a);return {a.x/d,a.y/d};}
double pointSegment(Point2D p,Point2D a,Point2D b){auto d=sub(b,a);auto ap=sub(p,a);double t=std::clamp((ap.x*d.x+ap.y*d.y)/(d.x*d.x+d.y*d.y),0.0,1.0);return std::hypot(p.x-a.x-t*d.x,p.y-a.y-t*d.y);}
bool target(const RayPath& p){return p.reflectionOrder==2&&p.interactions.size()==2&&p.interactions[0].objectId=="osm_556278505_e0"&&p.interactions[1].objectId=="osm_419317355_e30";}
}
int main(){
    auto map=OsmImporter2D::load("simulations/data/student/zacatecas_student.osm",22.75695,-102.60025,"concrete");
    require(map.walls.size()==14182&&map.buildingCount==2721,"archived full OSM fixture; no open-screen reduction");
    RayTracer2D tracer(map.walls,1e-9);Point2D tx{1838.748,465},rx{1950,500};
    require(tracer.getBuildings().size()==2721,"all archived closed building owners retained");
    require(tracer.classifyPoint(tx)==PointDomain2D::EXTERIOR&&tracer.classifyPoint(rx)==PointDomain2D::EXTERIOR,"central TX/RX exterior");
    auto trace=tracer.trace(tx,rx,true,true,true,false,500);
    PropagationConfig cfg;cfg.frequencyHz=2.4e9;cfg.txPowerDbm=14;cfg.polarization=Polarization2D::TE;
    PathSelectionConfig2D guards;guards.maxPathDistanceM=500;guards.minReceivedPowerDbm=-300;
    Propagation2D::evaluateAndSelect(trace.paths,cfg,guards);
    const RayPath* path=nullptr;int matches=0;for(const auto&p:trace.paths)if(target(p)){path=&p;++matches;}
    require(matches==1,"unique R2 with exact ordered facade identities");
    require(std::abs(path->totalDistanceM-126.752526332356)<1e-8,"independent archived R2 length, 10 nm numeric regression tolerance");
    require(std::abs(path->receivedPowerDbm-(-77.16083589641877))<1e-7,"independent TE per-path power, 1e-7 dB tolerance");
    require(path->points.size()==4,"two interaction vertices");
    const Point2D expected[]={{1859.1589629365651,470.2723324142413},{1862.9203201245375,487.5788101234553}};
    double reconstructed=0;
    for(std::size_t i=0;i<3;++i){
        reconstructed+=length(sub(path->points[i+1],path->points[i]));
        require(!tracer.segmentEntersBuilding(path->points[i],path->points[i+1]),"no leg enters any opaque building");
    }
    require(std::abs(reconstructed-path->totalDistanceM)<1e-10,"reconstructed total path distance");
    for(int i=0;i<2;++i){
        const auto&interaction=path->interactions[i];const auto p=path->points[i+1];
        require(interaction.type==InteractionType::REFLECTION&&length(sub(p,interaction.position))<1e-10,"reflection API position");
        require(length(sub(p,expected[i]))<1e-8,"independently computed reflection coordinate");
        const auto wall=std::find_if(map.walls.begin(),map.walls.end(),[&](const Wall2D&w){return w.id==interaction.objectId;});require(wall!=map.walls.end(),"facade exists");
        const auto tangent=sub(wall->b,wall->a),offset=sub(p,wall->a);double u=(offset.x*tangent.x+offset.y*tangent.y)/(tangent.x*tangent.x+tangent.y*tangent.y);
        require(u>0&&u<1&&pointSegment(p,wall->a,wall->b)<1e-8,"reflection inside finite facade");
        const auto incoming=unit(sub(p,path->points[i])),outgoing=unit(sub(path->points[i+2],p)),normal=unit({-tangent.y,tangent.x});double dot=incoming.x*normal.x+incoming.y*normal.y;
        require(std::hypot(outgoing.x-incoming.x+2*dot*normal.x,outgoing.y-incoming.y+2*dot*normal.y)<1e-11,"independent specular-law check");
    }
    Point2D critical{1905.8410179701343,493.73951476082743};
    require(std::abs(pointSegment(critical,path->points[2],rx)-0.0380401078780554)<1e-8,"38 mm clearance preserved, not a visibility crossing");
    auto shiftedValid=tracer.trace(tx,{1950,499.9},true,true,true,false,500);require(std::count_if(shiftedValid.paths.begin(),shiftedValid.paths.end(),target)==1,"valid negative y perturbation");
    auto shiftedBlocked=tracer.trace(tx,{1950,500.1},true,true,true,false,500);require(std::none_of(shiftedBlocked.paths.begin(),shiftedBlocked.paths.end(),target),"positive y obstruction must remove target R2");
    std::cout<<std::setprecision(17)<<"PASS M50_053: full closed-building map, exterior TX/RX, exact facade sequence, valid specular legs, length="<<path->totalDistanceM<<" m, power="<<path->receivedPowerDbm<<" dBm; +/-0.1 m y controls passed\n";
}
