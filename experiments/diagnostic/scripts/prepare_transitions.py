from pathlib import Path
from decimal import Decimal as D
import csv,json
from independent_geometry import walls,mirror,infinite_hit,cross,sub
R=Path(__file__).resolve().parents[1];tx=(D('1838.748'),D('465'));a,b=walls['osm_556278505_e0'];c,e=walls['osm_419317355_e30'];im1=mirror(tx,a,b);im2=mirror(im1,c,e);vertex=min(walls['osm_419317355_e27'],key=lambda p:p[0])
def value(axis,offset):
    rx=(D(1950)+(offset if axis=='x'else D(0)),D(500)+(offset if axis=='y'else D(0)));p2=infinite_hit(rx,im2,c,e);return cross(sub(rx,p2),sub(vertex,p2))
roots={}
for axis,lo,hi in [('y',D('.05'),D('.1')),('x',D('-.5'),D(0))]:
    left=value(axis,lo)
    for _ in range(70):
        mid=(lo+hi)/2
        if value(axis,mid)*left>0:lo=mid
        else:hi=mid
    roots[axis]=str((lo+hi)/2)
visibility=json.loads((R/'results/matlab_visibility/facet_visibility.json').read_text());center=next(r['sample_xyz']for r in visibility if r['scene']=='A_original'and r['sample_kind']=='incenter'and r['contains_P2']);vx,vy=map(float,vertex);x,y,z=center;stage_offset=y+(vy-y)*(1950-x)/(vx-x)-500
rows=[];existing=set()
for name in ['receivers_initial.csv','receivers_directed.csv']:
    for r in csv.DictReader((R/'inputs'/name).open()):existing.add((r['axis'],round(float(r['offset_m']),10)))
for axis,values in [('y',[float(roots['y'])+delta for delta in [-.00003,-.000015,-.000005,0,.000005,.000015,.00003]]),('x',[float(roots['x'])+delta for delta in [-.0001,-.00002,-.000005,.000005,.00002,.0001]]),('y',[stage_offset+delta for delta in [-.003,-.001,-.0002,-.00005,0,.00005,.0002,.001,.003]])]:
    for delta in values:
        key=(axis,round(delta,10))
        if key in existing:continue
        existing.add(key);rows.append({'rx_id':f'{axis}_{delta:+.10f}','tx_x':1838.748,'tx_y':465,'rx_x':1950+(delta if axis=='x'else 0),'rx_y':500+(delta if axis=='y'else 0),'axis':axis,'offset_m':delta})
with(R/'inputs/receivers_transition.csv').open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(R/'independent_audit/transition_predictions.json').write_text(json.dumps({'archived_model_physical_vertex_tangency_offsets_m':roots,'MATLAB_relevant_triangle_incenter_vertex_tangency_y_offset_m':stage_offset,'physical_vertex_xy':[float(x)for x in vertex],'candidate_incenter_xyz':center,'precision':'60-digit Decimal physical roots; float64 incenter prediction; not survey accuracy or an assumed vendor tolerance','new_positions':len(rows)},indent=2)+'\n')
print('Transition positions',len(rows),'roots',roots,'viewshed prediction',stage_offset)
