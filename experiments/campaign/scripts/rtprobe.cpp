#include "OsmImporter2D.h"
#include "Propagation2D.h"
#include "RayTracer2D.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <limits>

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v; std::stringstream ss(s); std::string item;
    while(std::getline(ss,item,',')) {if(!item.empty()&&item.back()=='\r')item.pop_back();v.push_back(item);}
    return v;
}
std::ifstream input(const std::string& name) {
    std::ifstream f(name); if(!f) throw std::runtime_error("Cannot read "+name); return f;
}
int main(int argc,char**argv) {
 try {
    if(argc>=2 && std::string(argv[1])=="dump") {
        if(argc!=6) throw std::runtime_error("dump OSM LAT LON OUT");
        auto osm=OsmImporter2D::load(argv[2],std::stod(argv[3]),std::stod(argv[4]),"concrete");
        std::ofstream out(argv[5]); out<<std::setprecision(17);
        out<<"id,x1,y1,x2,y2,material,building_id\n";
        for(auto&w:osm.walls) out<<w.id<<','<<w.a.x<<','<<w.a.y<<','<<w.b.x<<','<<w.b.y<<','<<w.material<<','<<w.buildingId<<'\n';
        std::cout<<"buildings="<<osm.buildingCount<<" walls="<<osm.walls.size()<<'\n'; return 0;
    }
    if(argc!=8) throw std::runtime_error("eval WALLS RECEIVERS OUT REPEATS MASK MAXDIST");
    std::vector<Wall2D>walls; auto in=input(argv[2]);std::string line;std::getline(in,line);
    while(std::getline(in,line)) {if(line.empty())continue;auto v=split(line);walls.push_back({v.at(0),{std::stod(v.at(1)),std::stod(v.at(2))},{std::stod(v.at(3)),std::stod(v.at(4))},v.at(5),v.size()>6?v.at(6):""});}
    RayTracer2D tracer(walls,1e-9);
    auto receivers=input(argv[3]); std::getline(receivers,line);
    std::ofstream out(argv[4]); out<<std::setprecision(17);
    std::ofstream paths(std::string(argv[4])+".paths.csv");paths<<std::setprecision(17);
    paths<<"rx_id,path_id,reflection_order,diffraction_order,distance_m,delay_s,path_loss_db,phase_rad,received_power_dbm,point_index,point_x,point_y,object_ids\n";
    out<<"rx_id,repeat,tx_x,tx_y,rx_x,rx_y,direct_blocked,num_paths,los_paths,r1_paths,r2_paths,d1_paths,power_dbm,incoherent_dbm,rms_ns,first_theoretical,first_evaluated,second_theoretical,second_evaluated,diffraction_candidates,seconds,tx_domain,rx_domain\n";
    int repeats=std::stoi(argv[5]),mask=std::stoi(argv[6]);double maxdist=std::stod(argv[7]);
    PropagationConfig config;config.frequencyHz=2.4e9;config.txPowerDbm=14;config.polarization=Polarization2D::TE;config.coherentCombination=true;
    PathSelectionConfig2D selection;selection.maxPathDistanceM=maxdist;selection.minReceivedPowerDbm=-300;
    while(std::getline(receivers,line)) {
        if(line.empty())continue;auto v=split(line);Point2D tx{std::stod(v.at(1)),std::stod(v.at(2))},rx{std::stod(v.at(3)),std::stod(v.at(4))};
        for(int rep=0;rep<repeats;++rep) {
            auto start=std::chrono::steady_clock::now();
            auto trace=tracer.trace(tx,rx,mask&1,mask&2,mask&4,mask&8,maxdist);
            auto sum=Propagation2D::evaluateAndSelect(trace.paths,config,selection);
            double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            if(trace.paths.empty()) { sum.totalReceivedPowerDbm=sum.totalIncoherentPowerDbm=sum.rmsDelaySpreadS=std::numeric_limits<double>::quiet_NaN(); }
            int los=0,r1=0,r2=0,d1=0;
            for(auto&p:trace.paths) {los+=p.isDirect;r1+=p.reflectionOrder==1;r2+=p.reflectionOrder==2;d1+=p.diffractionOrder==1;
                if(rep==0) for(std::size_t pi=0;pi<p.points.size();++pi) {
                    paths<<v[0]<<','<<p.id<<','<<p.reflectionOrder<<','<<p.diffractionOrder<<','<<p.totalDistanceM<<','<<p.delayS<<','<<p.pathLossDb<<','<<p.phaseRad<<','<<p.receivedPowerDbm<<','<<pi<<','<<p.points[pi].x<<','<<p.points[pi].y<<',';
                    for(const auto&i:p.interactions)paths<<i.objectId<<';';paths<<'\n';
                }}
            auto&s=trace.statistics;
            out<<v[0]<<','<<rep<<','<<tx.x<<','<<tx.y<<','<<rx.x<<','<<rx.y<<','<<trace.directBlocked<<','<<trace.paths.size()<<','<<los<<','<<r1<<','<<r2<<','<<d1<<','<<sum.totalReceivedPowerDbm<<','<<sum.totalIncoherentPowerDbm<<','<<sum.rmsDelaySpreadS*1e9<<','<<s.firstOrderTheoretical<<','<<s.firstOrderCandidates<<','<<s.secondOrderTheoretical<<','<<s.secondOrderCandidates<<','<<s.diffractionCandidates<<','<<seconds<<','<<domainName(trace.txDomain)<<','<<domainName(trace.rxDomain)<<'\n';
        }
        out.flush();
    }
    return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
