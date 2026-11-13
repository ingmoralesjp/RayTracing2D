"""Extra representation diagnostic: uniform, conforming subdivision of e30.
Same original normal direction; no physical or electromagnetic parameters fit.
"""
from pathlib import Path
import csv,json,shutil,hashlib
import numpy as np
R=Path(__file__).resolve().parents[1]
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
    with p.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
ws=read(R/'inputs/common_walls.csv');audit=[]
for count in [100,1000]:
    label=f'R_e30_refine{count}';out=R/'inputs/scenes'/label;out.mkdir(exist_ok=True);shutil.copy2(R/'inputs/common_walls.csv',out/'walls.csv');vertices=[];triangles=[];area_errors=[];norm_dots=[];plane_errors=[]
    for w in ws:
        a=np.array([float(w['x1'])-1838.748,float(w['y1'])-465]);b=np.array([float(w['x2'])-1838.748,float(w['y2'])-465]);n=count if w['id']=='osm_419317355_e30'else 1;base=len(vertices);v=[]
        for z in [-25.,25.]:
            for j in range(n+1):xy=a+(b-a)*(j/n);v.append(np.array([xy[0],xy[1],z]))
        normal=np.array([b[1]-a[1],a[0]-b[0],0.]);normal/=np.linalg.norm(normal);area=0.;edges={}
        for j in range(n):
            for face in [(j,j+1,n+1+j+1),(j,n+1+j+1,n+1+j)]:
                tri=np.array([v[k]for k in face]);cross=np.cross(tri[1]-tri[0],tri[2]-tri[0]);area+=np.linalg.norm(cross)/2;norm_dots.append(float(np.dot(cross/np.linalg.norm(cross),normal)));plane_errors.append(float(max(abs((tri-v[0])@normal))))
                for k,l in zip(face,face[1:]+face[:1]):key=tuple(sorted((k,l)));edges[key]=edges.get(key,0)+1
                triangles.append({'triangle_id':len(triangles),'wall_id':w['id'],'i0':base+face[0],'i1':base+face[1],'i2':base+face[2]})
        # The only mesh boundary is the original rectangular perimeter, now
        # split into collinear edges. Interior edges have exactly two incidences.
        assert sum(x==1 for x in edges.values())==2*n+2 and all(x in[1,2]for x in edges.values())
        for (i,j),inc in edges.items():
            if inc==1:
                vi,vj=v[i],v[j];assert (vi[2]==vj[2]and abs(vi[2])==25)or(i in[0,n]and j==i+n+1)
        area_errors.append(abs(area-np.linalg.norm(b-a)*50));vertices.extend([{'vertex_id':base+j,'x':float(p[0]),'y':float(p[1]),'z':float(p[2])}for j,p in enumerate(v)])
    assert max(area_errors)<1e-8 and max(plane_errors)<1e-10 and min(norm_dots)>1-1e-10
    write(out/'vertices.csv',vertices);write(out/'triangles.csv',triangles)
    with(out/'facades.ply').open('w')as f:
        f.write(f'ply\nformat ascii 1.0\nelement vertex {len(vertices)}\nproperty float x\nproperty float y\nproperty float z\nelement face {len(triangles)}\nproperty list uchar int vertex_indices\nend_header\n')
        for v in vertices:f.write(f"{v['x']:.17g} {v['y']:.17g} {v['z']:.17g}\n")
        for t in triangles:f.write(f"3 {t['i0']} {t['i1']} {t['i2']}\n")
    shutil.copy2(R/'inputs/scenes/A_original/scene.xml',out/'scene.xml')
    audit.append({'scene':label,'uniform_e30_tiles':count,'walls':len(ws),'triangles':len(triangles),'max_area_difference_m2':max(area_errors),'max_plane_error_m':max(plane_errors),'min_normal_dot_original':min(norm_dots),'boundary':'same rectangle; collinear edge subdivisions, no gaps/overlaps','physical_parameters_changed':False,'walls_sha256':hashlib.sha256((out/'walls.csv').read_bytes()).hexdigest()})
(R/'inputs/refinement_equivalence.json').write_text(json.dumps({'passed':True,'cases':audit},indent=2)+'\n');print(audit)
