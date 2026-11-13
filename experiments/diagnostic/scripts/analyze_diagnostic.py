from pathlib import Path
from decimal import Decimal as D
import csv,json,math,os,hashlib,difflib
import numpy as np
from independent_geometry import audit
R=Path(__file__).resolve().parents[1]
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
    with p.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
positions={}
expected={}
for p in sorted((R/'inputs').glob('receivers_*.csv')):
    for r in read(p):assert r['rx_id']not in positions;positions[r['rx_id']]=r
    for r in json.loads((R/'independent_audit'/f'{p.stem}.json').read_text())['positions']:expected[r['rx_id']]=r
joined=[];allpaths=[];allints=[];apiaudits=[];runs=[]
for engine in ['rt2d','matlab','sionna']:
    for folder in sorted((R/'results'/engine).iterdir()):
        if not folder.is_dir()or not(folder/'summary.csv').exists():continue
        meta=json.loads((folder/'metadata.json').read_text());scene=meta.get('scene','RT2D_physical');seed=meta.get('seed','');batch=Path(meta.get('rx_file',meta.get('command',['','','','unknown'])[3])).stem;paths=json.loads((folder/'paths.json').read_text());paths=paths if isinstance(paths,list)else[paths]
        summaries=read(folder/'summary.csv');runs.append({'engine':engine,'scene':scene,'batch':batch,'seed':seed,'receivers':len(summaries),'metadata':str((folder/'metadata.json').relative_to(R)),'evidence':'new_execution'})
        for r in summaries:
            q=positions[r['rx_id']];ex=expected[r['rx_id']];joined.append({'engine':engine,'scene':scene,'batch':batch,'seed':seed,'rx_id':r['rx_id'],'axis':q['axis'],'offset_m':q['offset_m'],'rx_x':r['rx_x'],'rx_y':r['rx_y'],'model_independently_valid':ex['independently_valid'],'model_clearance_e27_m':ex['clearance_e27_m'],'model_clearance_e28_m':ex['clearance_e28_m'],'model_distance_m':ex['distance_m'],'model_individual_dbm':ex['individual_power_dbm'],'num_paths':r['num_paths'],'target_count':r['target_count'],'target_present':int(r['target_count'])>0,'target_distance_m':r['target_distance_m'],'target_individual_dbm':r['target_individual_dbm'],'coherent_total_dbm':r['coherent_dbm'],'incoherent_dbm':r['incoherent_dbm'],'rms_ns':r['rms_ns']})
        for i,p in enumerate(paths):
            seq=p['surface_sequence'];seq=[seq]if isinstance(seq,str)else seq;locs=p['interaction_positions'];locs=[locs]if len(seq)==1 and locs and not isinstance(locs[0],list)else locs
            allpaths.append({'engine':engine,'scene':scene,'batch':batch,'seed':seed,'rx_id':p['rx_id'],'path_index':i,'API_path_index':p.get('path_index',p.get('path_id','')),'surface_sequence':';'.join(seq),'target':p['target'],'distance_m':p['distance_m'],'individual_dbm':p['power_dbm'],'phase_rad':p['phase_rad']})
            for k,loc in enumerate(locs):
                allints.append({'engine':engine,'scene':scene,'batch':batch,'seed':seed,'rx_id':p['rx_id'],'path_index':i,'interaction_index':k,'API_interaction_type':p['interaction_types'][k],'wall_id':seq[k],'x_m':loc[0],'y_m':loc[1],'z_m':loc[2],'primitive_id':p.get('primitive_ids',['']*len(seq))[k],'mapping_min_distance_m':p.get('mapping_min_distance_m',['']*len(seq))[k]if isinstance(p.get('mapping_min_distance_m',[]),list)else p.get('mapping_min_distance_m','')})
            if p['target']:
                q=positions[p['rx_id']];ex=expected[p['rx_id']];inter=[(D(str(v[0])),D(str(v[1])))for v in locs];result=audit((D(q['tx_x']),D(q['tx_y'])),(D(q['rx_x']),D(q['rx_y'])),inter,api_roundoff=engine!='rt2d')
                pointdelta=max(math.dist(ex['candidate_points'][k+1],v[:2])for k,v in enumerate(locs));apiaudits.append({'engine':engine,'scene':scene,'batch':batch,'seed':seed,'rx_id':p['rx_id'],'path_index':i,'model_route_valid':ex['independently_valid'],'API_polyline_clear_with_declared_contact_tolerance':result['independently_valid'],'API_max_reflection_coordinate_difference_m':pointdelta,'API_max_finite_facade_distance_m':max(v['on_line_distance_m']for v in result['reflections']),'API_max_specular_unit_vector_residual':max(v['specular_unit_vector_residual']for v in result['reflections']),'API_vs_reported_distance_m':result['distance_m']-float(p['distance_m']),'API_clearance_e27_m':result['clearance_e27_m'],'API_clearance_e28_m':result['clearance_e28_m'],'API_nonendpoint_facade_hits':len(result['nonendpoint_facade_hits']),'API_interior_intervals':len(result['interior_intervals']),'API_roundoff_contacts':len(result['API_contact_roundoff_facade_hits']),'API_contact_tolerance_m':result['API_contact_tolerance_m'],'distance_difference_vs_model_m':float(p['distance_m'])-ex['distance_m'],'individual_power_difference_vs_model_db':float(p['power_dbm'])-ex['individual_power_dbm']})
write(R/'report/positions_triangulations_engines.csv',joined);write(R/'report/all_paths_identity.csv',allpaths);write(R/'report/all_interactions.csv',allints);write(R/'independent_audit/engine_API_path_audits.csv',apiaudits)
(R/'report/new_execution_inventory.json').write_text(json.dumps(runs,indent=2)+'\n')
central=[r for r in joined if r['rx_id']=='M50_053'];write(R/'report/central_comparison.csv',central)
# Matrices include every receiver, so missing routes are not silently dropped.
matrix=[]
for id,q in positions.items():
    row={'rx_id':id,'axis':q['axis'],'offset_m':q['offset_m'],'rx_x':q['rx_x'],'rx_y':q['rx_y'],'independent_valid':expected[id]['independently_valid'],'clearance_e27_m':expected[id]['clearance_e27_m']}
    for engine,scene,seed in [('rt2d','RT2D_physical',''),('matlab','A_original',''),('matlab','B_e27_flip',''),('matlab','C_all_flip',''),('matlab','R_e30_refine1000',''),('sionna','A_original',42),('sionna','B_e27_flip',42),('sionna','C_all_flip',42),('sionna','R_e30_refine1000',42)]:
        rows=[r for r in joined if r['rx_id']==id and r['engine']==engine and r['scene']==scene and str(r['seed'])==str(seed)];assert len(rows)<=1,(id,engine,scene);row[engine+'_'+scene]=int(rows[0]['target_present'])if rows else''
    matrix.append(row)
write(R/'report/presence_matrix.csv',matrix)
# Numerical comparisons on finite identity-matched paths, with all-RX counts.
stats=[]
for engine in ['rt2d','matlab','sionna']:
    scenes=sorted({r['scene']for r in joined if r['engine']==engine})
    for scene in scenes:
        rows=[r for r in joined if r['engine']==engine and r['scene']==scene and str(r['seed'])in['','42']];good=[r for r in rows if r['target_present']];auds=[r for r in apiaudits if r['engine']==engine and r['scene']==scene and str(r['seed'])in['','42']]
        stats.append({'engine':engine,'scene':scene,'all_positions':len(rows),'model_valid_positions':sum(r['model_independently_valid']for r in rows),'target_present_positions':len(good),'target_present_on_model_obstructed_positions':sum(not r['model_independently_valid']for r in good),'max_distance_difference_m':max((abs(float(r['target_distance_m'])-r['model_distance_m'])for r in good),default=None),'max_individual_power_difference_db':max((abs(float(r['target_individual_dbm'])-r['model_individual_dbm'])for r in good),default=None),'max_API_reflection_coordinate_difference_m':max((r['API_max_reflection_coordinate_difference_m']for r in auds),default=None)})
write(R/'report/identity_matched_statistics.csv',stats)
# Scientific source files remain byte-identical to v2.
V=R.parent/'raytracing-final-correction-20261112';source_checks=[]
archived={x['path']:x['sha256']for x in json.loads((R/'tests/scientific_source_identity.json').read_text())['files']}if (R/'tests/scientific_source_identity.json').exists()else{}
for p in sorted((R/'source/src').glob('*')):
    if p.suffix in ['.cc','.h','.ned']:
        digest=hashlib.sha256(p.read_bytes()).hexdigest();same=p.read_bytes()==(V/'source/src'/p.name).read_bytes()if V.exists()else digest==archived.get('src/'+p.name);assert same,p.name;source_checks.append({'path':'src/'+p.name,'sha256':digest,'identical_to_F_v2':same})
(R/'tests/scientific_source_identity.json').write_text(json.dumps({'algorithm_changed':False,'files':source_checks,'framework_VERSION':(R/'source/VERSION').read_text().strip()},indent=2)+'\n')
if V.exists():
    patch=[]
    for file in [R/'source/Makefile',R/'source/tests/m50_053_test.cpp']:
        rel=file.relative_to(R/'source');old=V/'source'/rel;patch.extend(difflib.unified_diff(old.read_text().splitlines(True)if old.exists()else[],file.read_text().splitlines(True),fromfile='a/'+str(rel)if old.exists()else'/dev/null',tofile='b/'+str(rel)))
    (R/'tests/F_M50_regression.patch').write_text(''.join(patch))
os.environ['MPLCONFIGDIR']=str(R/'runtime/matplotlib')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
def save(name,fig):
    for ext in ['pdf','png']:fig.savefig(R/'figures'/f'{name}.{ext}',bbox_inches='tight',dpi=180)
    plt.close(fig)
initial_ids={r['rx_id']for r in read(R/'inputs/receivers_initial.csv')}
fig,axs=plt.subplots(1,3,figsize=(16,4),layout="constrained")
for ax,axis,title in zip(axs,['x','y','y'],['Initial x offsets','Initial y offsets','Physical y tangency (µm from threshold)']):
    band=[r for r in matrix if ((r['rx_id']in initial_ids and r['axis']in[axis,'center'])if ax is not axs[2]else(r['axis']==axis and .05925<float(r['offset_m'])<.05934))];band.sort(key=lambda r:float(r['offset_m']));keys=['independent_valid','rt2d_RT2D_physical','sionna_A_original','matlab_A_original']
    data=np.array([[int(r[k])for r in band]for k in keys]);im=ax.imshow(data,aspect='auto',vmin=0,vmax=1,cmap='RdYlGn',interpolation='nearest');ax.set_yticks(range(4),['Independent','RT2D','Sionna','MATLAB original']);ax.set_xticks(range(len(band)),[f"{float(r['offset_m']):+.3f}"if ax is not axs[2]else f"{(float(r['offset_m'])-.0592901892524201)*1e6:+.1f}"for r in band],rotation=60);ax.set_title(title);ax.set_xlabel('RX displacement (m)'if ax is not axs[2]else'Offset relative to analytic tangency (µm)')
fig.suptitle('R2 presence: green present/valid, red absent/obstructed; exact facade sequence');save('presence_sweep',fig)
fig,axs=plt.subplots(1,2,figsize=(11,4))
for ax,key,title in zip(axs,['target_distance_m','target_individual_dbm'],['Identity-matched R2 length (m)','Individual R2 power (dBm)']):
    for engine,scene in [('rt2d','RT2D_physical'),('sionna','A_original'),('matlab','R_e30_refine1000')]:
        rr=sorted([r for r in joined if r['engine']==engine and r['scene']==scene and r['axis']=='y'and abs(float(r['offset_m']))<=.100001 and r['target_present']and str(r['seed'])in['','42']],key=lambda r:float(r['offset_m']));ax.plot([float(r['offset_m'])for r in rr],[float(r[key])for r in rr],'o-',label=engine+' '+scene)
    ax.set_title(title);ax.set_xlabel('y displacement (m)');ax.legend(fontsize=6);ax.grid(alpha=.2)
save('path_length_and_power',fig)
pipeline=json.loads((R/'results/matlab_visibility/pipeline_diagnostics.json').read_text());vis=json.loads((R/'results/matlab_visibility/facet_visibility.json').read_text());ex=expected['M50_053'];points=np.array(ex['candidate_points']);walls=read(R/'inputs/walls_complete_buildings.csv');fig,axs=plt.subplots(1,2,figsize=(12,5))
for ax in axs:
    for w in walls:
        if w['building_id']in['osm_556278505','osm_419317355']:ax.plot([float(w['x1']),float(w['x2'])],[float(w['y1']),float(w['y2'])],color='.5',lw=.7)
    ax.plot(points[:,0],points[:,1],'r-o',ms=3,label='Target R2');ax.set_aspect('equal');ax.set_xlabel('East (m)');ax.set_ylabel('North (m)')
for row in vis:
    if row['scene']=='A_original'and row['contains_P2']and row['sample_kind']in['incenter','vertex1','vertex2','vertex3']:
        p=row['sample_xyz'];axs[0].plot([1950,p[0]],[500,p[1]],'--',lw=.7,color='blue');axs[0].plot(p[0],p[1],'bx',ms=4)
axs[0].set_xlim(1825,1965);axs[0].set_ylim(445,525);axs[0].set_title('Real R2 visible; coarse-facet visibility probes blocked');axs[0].legend(fontsize=7)
vx,vy=1905.8410179701343,493.73951476082743;axs[1].set_xlim(vx-.12,vx+.12);axs[1].set_ylim(vy-.12,vy+.12);axs[1].ticklabel_format(useOffset=False);axs[1].plot([vx],[vy],'ko');axs[1].set_title('Central clearance: 0.0380401 m')
fig.text(.06,.01,'Map data: OpenStreetMap contributors (ODbL). No physical geometry or material parameters fitted.',fontsize=8);save('geometry_and_visibility',fig)
print('Positions',len(positions),'engine rows',len(joined),'paths',len(allpaths),'interactions',len(allints),'API audits',len(apiaudits));print('Central',[(r['engine'],r['scene'],r['target_count'])for r in central])
