"""Separate nominal coplanar geometry from the float32 triangle representation."""
from pathlib import Path
import csv,json,math
import numpy as np
R=Path(__file__).resolve().parents[1]
def read(p):return list(csv.DictReader(p.open()))
def normal(v):
    n=np.cross(v[1]-v[0],v[2]-v[0]);return n/np.linalg.norm(n)
def mirror(p,v,n):return p-2*np.dot(p-v[0],n)*n
def hit(p,q,v,n):return p+(q-p)*(np.dot(v[0]-p,n)/np.dot(q-p,n))
def bary(p,v):
    a,b=v[1]-v[0],v[2]-v[0];x=np.linalg.solve([[np.dot(a,a),np.dot(a,b)],[np.dot(a,b),np.dot(b,b)]],[np.dot(p-v[0],a),np.dot(p-v[0],b)]);return [1-sum(x),*x]
def write(p,rows):
    with p.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
rx={}
for file in (R/'inputs').glob('receivers_*.csv'):rx.update({r['rx_id']:np.array([float(r['rx_x'])-1838.748,float(r['rx_y'])-465,1.5])for r in read(file)})
rows=[];cache={};origin=np.array([1838.748,465,0.]);tx=np.array([0.,0.,1.5])
for folder in sorted((R/'results/sionna').iterdir()):
    if not folder.is_dir()or not(folder/'paths.json').exists():continue
    meta=json.loads((folder/'metadata.json').read_text());scene=meta['scene']
    if scene not in cache:
        root=R/'inputs/scenes'/scene;v=np.array([[float(r[k])for k in ['x','y','z']]for r in read(root/'vertices.csv')]);tri=np.array([[int(r[k])for k in ['i0','i1','i2']]for r in read(root/'triangles.csv')]);cache[scene]=(v,v.astype(np.float32).astype(float),tri)
    v64,v32,tri=cache[scene]
    for path in json.loads((folder/'paths.json').read_text()):
        if not path['target']:continue
        points=np.vstack([tx,np.array(path['interaction_positions'])-origin,rx[path['rx_id']]]);ids=path['primitive_ids'];faces=[v32[tri[k]]for k in ids];norms=[normal(f)for f in faces]
        image1=mirror(tx,faces[0],norms[0]);image2=mirror(image1,faces[1],norms[1]);q2=hit(rx[path['rx_id']],image2,faces[1],norms[1]);q1=hit(q2,image1,faces[0],norms[0]);q=[q1,q2]
        residual=[];angles=[];barys=[]
        for k in range(2):
            vin=points[k+1]-points[k];vin/=np.linalg.norm(vin);vout=points[k+2]-points[k+1];vout/=np.linalg.norm(vout);pred=vin-2*np.dot(vin,norms[k])*norms[k];residual.append(np.linalg.norm(pred-vout));n64=normal(v64[tri[ids[k]]]);angles.append(math.acos(np.clip(abs(np.dot(norms[k],n64)),0,1)));barys.extend(bary(q[k],faces[k]))
        rows.append({'scene':scene,'batch':Path(meta['rx_file']).stem,'seed':meta['seed'],'rx_id':path['rx_id'],'path_index':path['path_index'],'first_primitive':ids[0],'second_primitive':ids[1],'max_API_vs_quantized_exact_reflection_m':max(np.linalg.norm(points[k+1]-q[k])for k in range(2)),'max_specular_residual_quantized_planes':max(residual),'max_triangle_normal_change_rad':max(angles),'min_quantized_exact_reflection_barycentric':min(barys),'both_quantized_exact_reflections_on_finite_triangles':bool(min(barys)>=-1e-7),'nominal_vs_float32_vertex_max_m':max(np.max(np.linalg.norm(v64[tri[id]]-v32[tri[id]],axis=1))for id in ids)})
write(R/'independent_audit/sionna_quantized_triangle_audit.csv',rows)
central=[r for r in rows if r['rx_id']=='M50_053'and r['scene']=='R_e30_refine1000'];print('Central refined representation:');print(json.dumps(central,indent=2))
