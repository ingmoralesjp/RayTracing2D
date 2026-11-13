"""Decimal image construction, exact segment determinants and winding audit.
No use of RT2D geometry functions or outputs to construct the expected route.
"""
from pathlib import Path
from decimal import Decimal as D, getcontext
from collections import defaultdict
import csv,json,math,argparse,cmath
getcontext().prec=60
R=Path(__file__).resolve().parents[1];TARGET=['osm_556278505_e0','osm_419317355_e30'];TOL=D('1e-8')
def read(p):return list(csv.DictReader(p.open()))
def sub(a,b):return(a[0]-b[0],a[1]-b[1])
def add(a,b):return(a[0]+b[0],a[1]+b[1])
def mul(a,t):return(a[0]*t,a[1]*t)
def dot(a,b):return a[0]*b[0]+a[1]*b[1]
def cross(a,b):return a[0]*b[1]-a[1]*b[0]
def length(a):return dot(a,a).sqrt()
def dist(a,b):return length(sub(a,b))
def pt_seg(p,a,b):
    v=sub(b,a);t=max(D(0),min(D(1),dot(sub(p,a),v)/dot(v,v)));return dist(p,add(a,mul(v,t)))
def intersect(a,b,c,e):
    v,w=sub(b,a),sub(e,c);den=cross(v,w);off=sub(c,a)
    if den==0:
        if cross(off,v)!=0:return None
        lo,hi=sorted([dot(sub(c,a),v)/dot(v,v),dot(sub(e,a),v)/dot(v,v)]);lo=max(lo,D(0));hi=min(hi,D(1))
        if hi<lo:return None
        return ('overlap'if (hi-lo)*length(v)>TOL else'touch',(lo+hi)/2,None)
    t,u=cross(off,w)/den,cross(off,v)/den
    if 0<=t<=1 and 0<=u<=1:return('point',t,u)
    return None
def mirror(p,a,b):
    v=sub(b,a);foot=add(a,mul(v,dot(sub(p,a),v)/dot(v,v)));return sub(mul(foot,D(2)),p)
def infinite_hit(a,b,c,e):
    v,w=sub(b,a),sub(e,c);den=cross(v,w);assert den!=0;return add(a,mul(v,cross(sub(c,a),w)/den))
W=read(R/'inputs/walls_complete_buildings.csv');walls={};groups=defaultdict(list)
for row in W:
    a,b=(D(row['x1']),D(row['y1'])),(D(row['x2']),D(row['y2']));walls[row['id']]=(a,b);groups[row['building_id']].append((row['id'],a,b))
polys=[]
for owner,edges in groups.items():
    for i,edge in enumerate(edges):assert dist(edge[2],edges[(i+1)%len(edges)][1])<D('1e-10'),owner
    pts=[e[1]for e in edges];polys.append((owner,pts,(min(p[0]for p in pts),max(p[0]for p in pts),min(p[1]for p in pts),max(p[1]for p in pts))))
def locate(p):
    boundary=False
    for owner,pts,(xmin,xmax,ymin,ymax)in polys:
        if not xmin-TOL<=p[0]<=xmax+TOL or not ymin-TOL<=p[1]<=ymax+TOL:continue
        wn=0;on=False
        for a,b in zip(pts,pts[1:]+pts[:1]):
            on|=pt_seg(p,a,b)<=TOL;side=cross(sub(b,a),sub(p,a))
            if a[1]<=p[1]<b[1]and side>0:wn+=1
            elif b[1]<=p[1]<a[1]and side<0:wn-=1
        if wn and not on:return'interior'
        boundary|=on
    return'boundary'if boundary else'exterior'
def audit(tx,rx,interaction_points=None,api_roundoff=False):
    a,b=walls[TARGET[0]];c,e=walls[TARGET[1]];image1=mirror(tx,a,b);image2=mirror(image1,c,e);p2=infinite_hit(rx,image2,c,e);p1=infinite_hit(p2,image1,a,b)
    points=[tx,p1,p2,rx]if interaction_points is None else[tx]+interaction_points+[rx]
    hits=[];inside=[];cuts_by_leg=[];roundoff_contacts=[];roundoff_intervals=[]
    contact_tol=D('1e-4')if api_roundoff else TOL
    for li,(s,t)in enumerate(zip(points,points[1:])):
        cuts=[D(0),D(1)]
        for id,(v,w)in walls.items():
            if max(s[0],t[0])<min(v[0],w[0])or min(s[0],t[0])>max(v[0],w[0])or max(s[1],t[1])<min(v[1],w[1])or min(s[1],t[1])>max(v[1],w[1]):continue
            hit=intersect(s,t,v,w)
            if hit is None:
                # Same declared metric boundary policy as point classification:
                # nonendpoint contact within 1e-8 m is conservatively grazing.
                for vertex in [v,w]:
                    if pt_seg(vertex,s,t)<=TOL and dist(vertex,s)>TOL and dist(vertex,t)>TOL:
                        hits.append({'leg':li,'wall':id,'kind':'within_metric_boundary_band','position':[float(x)for x in vertex]});break
                continue
            kind,frac,u=hit;cuts.append(frac);position=add(s,mul(sub(t,s),frac));end=dist(position,s)<=TOL or dist(position,t)<=TOL
            # Only contact at a path endpoint is allowed; a non-endpoint
            # grazing vertex is explicitly recorded and rejected conservatively.
            own_contact=(li>0 and id==TARGET[li-1]and dist(position,s)<=contact_tol)or(li<2 and id==TARGET[li]and dist(position,t)<=contact_tol)
            entry={'leg':li,'wall':id,'kind':kind,'position':[float(x)for x in position]}
            if api_roundoff and not end and own_contact and kind!='overlap':roundoff_contacts.append(entry)
            elif kind=='overlap'or not end:hits.append(entry)
        cuts=sorted(set(cuts));cuts_by_leg.append(len(cuts))
        for t0,t1 in zip(cuts,cuts[1:]):
            if (t1-t0)*dist(s,t)<=2*TOL:continue
            mid=add(s,mul(sub(t,s),(t0+t1)/2))
            if locate(mid)=='interior':
                entry={'leg':li,'sample':[float(x)for x in mid],'interval_parameter':[float(t0),float(t1)]}
                if api_roundoff and min(dist(mid,points[1]),dist(mid,points[2]))<=2*contact_tol:roundoff_intervals.append(entry)
                else:inside.append(entry)
    reflection=[];gamma=1+0j
    for i,id in enumerate(TARGET,1):
        a,b=walls[id];v=sub(b,a);u=dot(sub(points[i],a),v)/dot(v,v);vin=mul(sub(points[i],points[i-1]),1/dist(points[i],points[i-1]));vout=mul(sub(points[i+1],points[i]),1/dist(points[i+1],points[i]));normal=(-v[1]/length(v),v[0]/length(v));projection=dot(vin,normal);predicted=sub(vin,mul(normal,2*projection));res=length(sub(predicted,vout));cosine=float(abs(projection));eps=complex(5.24,-(.0462*2.4**.7822)/(2*math.pi*2.4e9*8.854187817e-12));q=cmath.sqrt(eps-(1-cosine*cosine));gamma*=(cosine-q)/(cosine+q)
        reflection.append({'wall_id':id,'point':[float(x)for x in points[i]],'u':float(u),'finite_facade':bool(0<u<1),'on_line_distance_m':float(pt_seg(points[i],a,b)),'specular_unit_vector_residual':float(res)})
    clearance={}
    for id in ['osm_419317355_e27','osm_419317355_e28']:
        c,e=walls[id];values=[]
        for s,t in zip(points,points[1:]):
            hit=intersect(s,t,c,e);values.append(D(0)if hit else min(pt_seg(s,c,e),pt_seg(t,c,e),pt_seg(c,s,t),pt_seg(e,s,t)))
        clearance[id]=float(min(values))
    distance=sum(dist(s,t)for s,t in zip(points,points[1:]));power=14+20*math.log10(299792458/(2.4e9*4*math.pi*float(distance))*abs(gamma));spec_tol=1e-5 if api_roundoff else 1e-10 if interaction_points is not None else 1e-40
    valid=locate(tx)==locate(rx)=='exterior'and not hits and not inside and all(x['finite_facade']and x['specular_unit_vector_residual']<spec_tol and x['on_line_distance_m']<=float(contact_tol)for x in reflection)
    return {'target_surface_sequence':TARGET,'candidate_points':[[float(x)for x in p]for p in points],'tx_domain':locate(tx),'rx_domain':locate(rx),'independently_valid':valid,'distance_m':float(distance),'distance_decimal_m':str(distance),'individual_power_dbm':power,'reflections':reflection,'nonendpoint_facade_hits':hits,'interior_intervals':inside,'boundary_partition_counts':cuts_by_leg,'clearance_e27_m':clearance['osm_419317355_e27'],'clearance_e28_m':clearance['osm_419317355_e28'],'API_contact_roundoff_facade_hits':roundoff_contacts,'API_contact_roundoff_interior_intervals':roundoff_intervals,'API_contact_tolerance_m':float(contact_tol),'specular_tolerance_unit_vector':spec_tol}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--receivers',default='inputs/receivers_initial.csv');args=p.parse_args();file=Path(args.receivers);file=file if file.is_absolute()else R/file;results=[];summary=[]
    for r in read(file):
        result={'rx_id':r['rx_id'],'rx_x':float(r['rx_x']),'rx_y':float(r['rx_y']),**audit((D(r['tx_x']),D(r['tx_y'])),(D(r['rx_x']),D(r['rx_y'])))};results.append(result);summary.append({k:v for k,v in result.items()if not isinstance(v,(dict,list))});print(r['rx_id'],result['independently_valid'],result['clearance_e27_m'],flush=True)
    (R/'independent_audit'/f'{file.stem}.json').write_text(json.dumps({'precision_decimal_digits':60,'geometry_input':'archived metric endpoints as exact 17-digit decimal coordinates; no RT2D predicates','tolerance_m':str(TOL),'power_method':'independent TE Fresnel magnitudes and free-space spherical spreading','positions':results},indent=2)+'\n')
    with(R/'independent_audit'/f'{file.stem}.csv').open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
