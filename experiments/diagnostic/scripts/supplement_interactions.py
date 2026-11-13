"""Rank numerical representatives by interaction coordinates; never deduplicate sums."""
from pathlib import Path
import csv,json,math,collections,os,hashlib
R=Path(__file__).resolve().parents[1]
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
    with p.open('w',newline='')as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def key(r):return tuple(str(r[k])for k in ['engine','scene','batch','seed','rx_id'])
paths=read(R/'report/all_paths_identity.csv');ints=read(R/'report/all_interactions.csv')
audits=read(R/'independent_audit/engine_API_path_audits.csv');summaries=read(R/'report/positions_triangulations_engines.csv')
indexed={key(r)+(r['path_index'],):r for r in audits};grouped=collections.defaultdict(list)
for r in paths:
    if r['target']=='True':grouped[key(r)].append(r)
ranked=[];canonical=[]
for row in summaries:
    kk=key(row);candidates=grouped[kk]
    candidates.sort(key=lambda p:float(indexed[kk+(p['path_index'],)]['API_max_reflection_coordinate_difference_m']))
    for rank,p in enumerate(candidates,1):
        au=indexed[kk+(p['path_index'],)];interaction_rows=[q for q in ints if key(q)==kk and q['path_index']==p['path_index']]
        ranked.append({**p,'geometry_rank':rank,'primitive_sequence':';'.join(q['primitive_id']for q in interaction_rows),'max_reflection_difference_m':au['API_max_reflection_coordinate_difference_m'],'specular_vector_residual':au['API_max_specular_unit_vector_residual'],'API_geometry_passed':au['API_polyline_clear_with_declared_contact_tolerance'],'model_route_valid':au['model_route_valid']})
    canonical.append({**{k:row[k]for k in ['engine','scene','batch','seed','rx_id','offset_m']},'raw_same_facade_sequence_count':len(candidates),'geometry_closest_path_index':candidates[0]['path_index']if candidates else'','geometry_closest_max_point_difference_m':indexed[kk+(candidates[0]['path_index'],)]['API_max_reflection_coordinate_difference_m']if candidates else'','geometry_closest_length_m':candidates[0]['distance_m']if candidates else'','geometry_closest_individual_dbm':candidates[0]['individual_dbm']if candidates else'','raw_coherent_total_dbm':row['coherent_total_dbm'],'raw_incoherent_total_dbm':row['incoherent_dbm'],'selection_rule':'minimum maximum P1/P2 XY error to independent nominal specular solution; no output or sum removed'})
write(R/'report/interaction_ranked_candidates.csv',ranked);write(R/'report/geometry_closest_representative.csv',canonical)
seeds=[]
for scene in ['A_original','R_e30_refine1000']:
    rr={seed:{r['rx_id']:r for r in summaries if r['engine']=='sionna'and r['scene']==scene and r['batch']=='receivers_transition'and r['seed']==seed}for seed in ['42','73']}
    for rx,a in rr['42'].items():
        b=rr['73'][rx];seeds.append({'scene':scene,'rx_id':rx,'count_seed42':a['target_count'],'count_seed73':b['target_count'],'coherent_seed42_dbm':a['coherent_total_dbm'],'coherent_seed73_dbm':b['coherent_total_dbm'],'delta_total_db':float(b['coherent_total_dbm'])-float(a['coherent_total_dbm']),'max_point_error_seed42_m':max((float(p['max_reflection_difference_m'])for p in ranked if p['engine']=='sionna'and p['scene']==scene and p['rx_id']==rx and p['seed']=='42'),default=0),'max_point_error_seed73_m':max((float(p['max_reflection_difference_m'])for p in ranked if p['engine']=='sionna'and p['scene']==scene and p['rx_id']==rx and p['seed']=='73'),default=0)})
write(R/'report/sionna_seed_comparison.csv',seeds)
# Identify, but do not redistribute, the installed proprietary implementation.
vendor=[]
for symbol in json.loads((R/'results/matlab_visibility/installed_API_sources.json').read_text()):
    p=Path(symbol['installed_path'])
    if not p.exists():continue
    vendor.append({**symbol,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'lines':len(p.read_text().splitlines()),'redistributed':False})
if vendor:(R/'report/installed_MATLAB_source_fingerprints.json').write_text(json.dumps(vendor,indent=2)+'\n')
os.environ['MPLCONFIGDIR']=str(R/'runtime/matplotlib')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
central=[r for r in summaries if r['rx_id']=='M50_053']
scenes=['A_original','B_e27_flip','C_all_flip','R_e30_refine100','R_e30_refine1000']
fig,axs=plt.subplots(1,2,figsize=(12,4.5))
for engine,marker in [('matlab','s'),('sionna','o')]:
    rows=[next(r for r in central if r['engine']==engine and r['scene']==sc)for sc in scenes]
    axs[0].plot(range(5),[int(r['target_count'])for r in rows],marker+'-',label=engine)
    axs[1].plot(range(5),[float(r['coherent_total_dbm'])for r in rows],marker+'-',label=engine+' raw coherent total')
axs[0].axhline(1,color='black',ls='--',label='RT2D (unchanged physical geometry)')
axs[1].axhline(-77.16083589641877,color='black',ls='--',label='RT2D individual = total')
axs[0].set_ylabel('Number of API contributions on the same facade sequence');axs[1].set_ylabel('Power (dBm)')
for ax in axs:
    ax.set_xticks(range(5),['A original','B e27 flip','C all flip','e30 100 tiles','e30 1000 tiles'],rotation=25);ax.grid(alpha=.2);ax.legend(fontsize=7)
axs[1].annotate('Four numerical representatives\nsame primitive pair; no deduplication',xy=(4,-65.11973409),xytext=(1,-69),arrowprops={'arrowstyle':'->'},fontsize=8)
fig.suptitle('Central M50_053: physically equivalent mesh representations; missing MATLAB paths omitted from power plot')
for ext in ['pdf','png']:fig.savefig(R/'figures'/f'central_representation_and_total_power.{ext}',bbox_inches='tight',dpi=180)
plt.close(fig)
# Separate physical grazing from the much wider candidate-visibility transition.
matrix=read(R/'report/presence_matrix.csv')
band=sorted([r for r in matrix if r['axis']=='y'and -1.31<float(r['offset_m'])<-1.30],key=lambda r:float(r['offset_m']))
keys=['independent_valid','rt2d_RT2D_physical','sionna_A_original','matlab_A_original','matlab_R_e30_refine1000']
fig,ax=plt.subplots(figsize=(8,3.5));ax.imshow([[int(r[k]=='True')if k=='independent_valid'else int(r[k])for r in band]for k in keys],aspect='auto',vmin=0,vmax=1,cmap='RdYlGn')
ax.set_yticks(range(5),['Independent geometry','RT2D','Sionna original','MATLAB original','MATLAB e30 1000 tiles']);ax.set_xticks(range(len(band)),[f"{float(r['offset_m']):.7f}"for r in band],rotation=35);ax.set_xlabel('RX y displacement (m)');ax.set_title('Candidate visibility transition: physical R2 remains clear (~0.877 m)')
for ext in ['pdf','png']:fig.savefig(R/'figures'/f'candidate_visibility_transition.{ext}',bbox_inches='tight',dpi=180)
print('Ranked',len(ranked),'numerical contributions,',len(canonical),'receiver/scene/engine rows; seed controls',len(seeds))
