from pathlib import Path
import subprocess,os,csv,json,time,argparse,hashlib
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--receivers',default='inputs/receivers_initial.csv');args=p.parse_args();rx=Path(args.receivers);rx=rx if rx.is_absolute()else R/rx
def read(p):return list(csv.DictReader(p.open()))
probe=R/'runtime/rtprobe'
probe.parent.mkdir(parents=True,exist_ok=True)
if not probe.exists():
    cmd=['clang++','-std=c++17','-O3','-DNDEBUG=1','-I'+str(R/'source/src'),str(R/'scripts/rtprobe.cpp')]+[str(R/'source/src'/s)for s in ['Geometry2D.cc','RayTracer2D.cc','Propagation2D.cc','Materials2D.cc','OsmImporter2D.cc']]+['-o',str(probe)];subprocess.run(cmd,check=True)
out=R/'results/rt2d'/rx.stem;out.mkdir(exist_ok=True);cmd=[str(probe),'eval',str(R/'inputs/walls_complete_buildings.csv'),str(rx),str(out/'probe.csv'),'1','7','500'];start=time.monotonic();subprocess.run(cmd,check=True);elapsed=time.monotonic()-start
groups={}
for row in read(out/'probe.csv.paths.csv'):groups.setdefault((row['rx_id'],row['path_id']),[]).append(row)
raw=[]
for (id,key),rows in groups.items():
    rows.sort(key=lambda r:int(r['point_index']));head=rows[0];seq=[s for s in head['object_ids'].split(';')if s];locs=[[float(r['point_x']),float(r['point_y']),1.5]for r in rows[1:-1]]
    raw.append({'rx_id':id,'path_id':key,'distance_m':float(head['distance_m']),'path_loss_db':float(head['path_loss_db']),'power_dbm':float(head['received_power_dbm']),'phase_rad':float(head['phase_rad']),'surface_sequence':seq,'interaction_positions':locs,'interaction_types':['Reflection']*len(seq),'target':seq==['osm_556278505_e0','osm_419317355_e30']})
summary=[]
for row in read(out/'probe.csv'):
    targets=[p for p in raw if p['rx_id']==row['rx_id']and p['target']]
    summary.append({'rx_id':row['rx_id'],'rx_x':row['rx_x'],'rx_y':row['rx_y'],'num_paths':int(row['num_paths']),'target_count':len(targets),'target_distance_m':targets[0]['distance_m']if targets else float('nan'),'target_individual_dbm':targets[0]['power_dbm']if targets else float('nan'),'coherent_dbm':float(row['power_dbm']),'incoherent_dbm':float(row['incoherent_dbm']),'rms_ns':float(row['rms_ns']),'tx_domain':row['tx_domain'],'rx_domain':row['rx_domain']})
with(out/'summary.csv').open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
(out/'paths.json').write_text(json.dumps(raw,indent=2)+'\n');(out/'metadata.json').write_text(json.dumps({'engine':'RT2D','version':(R/'source/VERSION').read_text().strip(),'command':cmd,'seconds':elapsed,'walls':14182,'building_polygons':2721,'triangulation':'not applicable, original physical 2D segments unchanged','receivers':len(summary),'probe_sha256':hashlib.sha256(probe.read_bytes()).hexdigest(),'scientific_source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in (R/'source/src').glob('*')if p.suffix in ['.cc','.h']},'evidence':'new execution with full closed-building map, not reduced open screens'},indent=2)+'\n')
print('RT2D target present',sum(r['target_count']>0 for r in summary),'/',len(summary))
