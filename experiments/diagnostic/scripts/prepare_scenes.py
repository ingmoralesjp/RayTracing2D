from pathlib import Path
import csv,json,hashlib,math
import numpy as np
R=Path(__file__).resolve().parents[1]
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
    with p.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
walls=read(R/'inputs/common_walls.csv');complete=read(R/'inputs/walls_complete_buildings.csv')
reflectors=['osm_556278505_e0','osm_419317355_e30'];owners={x.rsplit('_e',1)[0]for x in reflectors}
areas={}
for w in complete:
    owner=w['building_id'];areas[owner]=areas.get(owner,0.)+float(w['x1'])*float(w['y2'])-float(w['x2'])*float(w['y1'])
cases=[('A_original',walls,'original'),('B_e27_flip',walls,'e27'),('C_all_flip',walls,'all'),('D_two_surfaces',[w for w in walls if w['id']in reflectors],'original'),('E_two_buildings',[w for w in complete if w['building_id']in owners],'original'),('N_e30_reverse',walls,'reverse_e30'),('N_outward',walls,'outward')]
audits=[]
for name,ws,mode in cases:
    out=R/'inputs/scenes'/name;out.mkdir(exist_ok=True);write(out/'walls.csv',ws)
    vertices=[];triangles=[];min_dot=1.;max_area_error=0.;max_plane_error=0.;changed=0;reversed_count=0
    for i,w in enumerate(ws):
        a=np.array([float(w['x1'])-1838.748,float(w['y1'])-465,-25.]);b=np.array([float(w['x2'])-1838.748,float(w['y2'])-465,-25.]);v=np.array([a,b,b+[0,0,50],a+[0,0,50]])
        alt=mode=='all'or(mode=='e27'and w['id']=='osm_419317355_e27');changed+=alt
        fs=[(0,1,3),(1,2,3)]if alt else[(0,1,2),(0,2,3)]
        normal=np.cross(v[1]-v[0],v[2]-v[0]);normal/=np.linalg.norm(normal)
        reverse=(mode=='reverse_e30'and w['id']=='osm_419317355_e30')or(mode=='outward'and areas[w['id'].rsplit('_e',1)[0]]<0)
        if reverse:fs=[(a,c,b)for a,b,c in fs];normal=-normal;reversed_count+=1
        area=0.;edges={}
        for face in fs:
            tri=v[list(face)];cross=np.cross(tri[1]-tri[0],tri[2]-tri[0]);area+=np.linalg.norm(cross)/2;min_dot=min(min_dot,float(np.dot(cross/np.linalg.norm(cross),normal)));max_plane_error=max(max_plane_error,float(np.max(np.abs((tri-v[0])@normal))))
            for j,k in zip(face,face[1:]+face[:1]):key=tuple(sorted((j,k)));edges[key]=edges.get(key,0)+1
            triangles.append({'triangle_id':len(triangles),'wall_id':w['id'],'i0':4*i+face[0],'i1':4*i+face[1],'i2':4*i+face[2]})
        assert {e for e,c in edges.items()if c==1}=={(0,1),(1,2),(2,3),(0,3)}and sorted(edges.values())==[1,1,1,1,2]
        expected=np.linalg.norm(b-a)*50;max_area_error=max(max_area_error,abs(area-expected));assert abs(area-expected)<1e-9 and min_dot>1-1e-12 and max_plane_error<1e-11
        vertices.extend([{'vertex_id':4*i+j,'x':float(p[0]),'y':float(p[1]),'z':float(p[2])}for j,p in enumerate(v)])
    write(out/'vertices.csv',vertices);write(out/'triangles.csv',triangles)
    with(out/'facades.ply').open('w')as f:
        f.write(f'ply\nformat ascii 1.0\nelement vertex {len(vertices)}\nproperty float x\nproperty float y\nproperty float z\nelement face {len(triangles)}\nproperty list uchar int vertex_indices\nend_header\n')
        for v in vertices:f.write(f"{v['x']:.17g} {v['y']:.17g} {v['z']:.17g}\n")
        for t in triangles:f.write(f"3 {t['i0']} {t['i1']} {t['i2']}\n")
    sigma=.0462*2.4**.7822
    (out/'scene.xml').write_text(f'''<scene version="3.0.0"><bsdf type="radio-material" id="matched_concrete"><float name="relative_permittivity" value="5.24"/><float name="conductivity" value="{sigma:.17g}"/><float name="thickness" value="10"/></bsdf><shape type="ply" id="mesh-facades"><string name="filename" value="facades.ply"/><boolean name="face_normals" value="true"/><ref id="matched_concrete" name="bsdf"/></shape></scene>\n''')
    audits.append({'scene':name,'walls':len(ws),'triangles':len(triangles),'changed_diagonals':changed,'reversed_normal_facades':reversed_count,'normal_policy':mode,'normals_preserved_vs_original':reversed_count==0,'same_four_boundary_edges_per_facade':True,'no_gap_or_overlap':'two consistently oriented triangles partition each planar convex rectangle; matching perimeter and summed area','max_area_error_m2':max_area_error,'max_plane_error_m':max_plane_error,'min_normal_dot_expected_orientation':min_dot,'vertices_sha256':hashlib.sha256((out/'vertices.csv').read_bytes()).hexdigest(),'walls_sha256':hashlib.sha256((out/'walls.csv').read_bytes()).hexdigest(),'faces_sha256':hashlib.sha256((out/'triangles.csv').read_bytes()).hexdigest()})
assert len({x['vertices_sha256']for x in audits[:3]})==1 and len({x['walls_sha256']for x in audits[:3]})==1
assert [x['changed_diagonals']for x in audits[:3]]==[0,1,1218]
(R/'inputs/triangulation_equivalence.json').write_text(json.dumps({'passed':True,'units':'local Cartesian metres, z=-25..25 m, TX-centered XY','precision':'float64 verification; PLY loaded as float32 by Sionna','cases':audits},indent=2)+'\n')
print('Equivalent physical surfaces verified:',[(x['scene'],x['changed_diagonals'])for x in audits])
