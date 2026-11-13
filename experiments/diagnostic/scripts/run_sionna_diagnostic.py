from pathlib import Path
import argparse,csv,json,time,importlib.metadata
import numpy as np
import mitsuba as mi
mi.set_variant('llvm_ad_mono_polarized')
import drjit as dr
from sionna.rt import load_scene,PlanarArray,Transmitter,Receiver,PathSolver
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--receivers',default='inputs/receivers_initial.csv');p.add_argument('--scenes',default='A_original,B_e27_flip,C_all_flip,D_two_surfaces,E_two_buildings');p.add_argument('--seed',type=int,default=42);p.add_argument('--samples',type=int,default=1000000);args=p.parse_args()
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
    with p.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
rxfile=Path(args.receivers);rxfile=rxfile if rxfile.is_absolute()else R/rxfile;receivers=read(rxfile);n=len(receivers)
for name in args.scenes.split(','):
    inputs=R/'inputs/scenes'/name;out=R/'results/sionna'/f'{name}_{rxfile.stem}_seed{args.seed}';out.mkdir(parents=True,exist_ok=True)
    tris=read(inputs/'triangles.csv');scene=load_scene(str(inputs/'scene.xml'),merge_shapes=False);scene.frequency=2.4e9
    scene.tx_array=PlanarArray(num_rows=1,num_cols=1,vertical_spacing=.5,horizontal_spacing=.5,pattern='iso',polarization='V');scene.rx_array=PlanarArray(num_rows=1,num_cols=1,vertical_spacing=.5,horizontal_spacing=.5,pattern='iso',polarization='V');scene.add(Transmitter('TX',position=[0,0,1.5]))
    for i,r in enumerate(receivers):scene.add(Receiver('RX'+str(i),position=[float(r['rx_x'])-1838.748,float(r['rx_y'])-465,1.5]))
    start=time.monotonic();paths=PathSolver()(scene,max_depth=2,max_num_paths_per_src=1000000,samples_per_src=args.samples,los=True,specular_reflection=True,diffuse_reflection=False,refraction=False,diffraction=False,edge_diffraction=False,seed=args.seed)
    dr.eval(paths.a,paths.tau,paths.valid,paths.vertices,paths.primitives,paths.objects,paths.interactions);dr.sync_thread();elapsed=time.monotonic()-start
    re,im=paths.a;a=re.numpy().reshape(n,-1).astype(float)+1j*im.numpy().reshape(n,-1).astype(float);tau=paths.tau.numpy().reshape(n,-1).astype(float);valid=paths.valid.numpy().reshape(n,-1)
    m=a.shape[1];depth=paths.interactions.shape[0];types=paths.interactions.numpy().reshape(depth,n,m);vertices=paths.vertices.numpy().reshape(depth,n,m,3).astype(float);primitives=paths.primitives.numpy().reshape(depth,n,m);objects=paths.objects.numpy().reshape(depth,n,m)
    raw=[];summary=[]
    for i,r in enumerate(receivers):
        mask=valid[i]&(tau[i]>=0)&(tau[i]*299792458<=500+1e-4)&(np.abs(a[i])>0);ind=np.flatnonzero(mask);target=[]
        for j in ind:
            ii=np.flatnonzero(types[:,i,j]);seq=[];locs=[];prim=[];obj=[]
            for q in ii:
                k=int(primitives[q,i,j]);assert 0<=k<len(tris),(k,len(tris));seq.append(tris[k]['wall_id']);locs.append((vertices[q,i,j]+[1838.748,465,0]).tolist());prim.append(k);obj.append(int(objects[q,i,j]))
            is_target=seq==['osm_556278505_e0','osm_419317355_e30'];power=14+20*np.log10(abs(a[i,j]));distance=float(tau[i,j]*299792458)
            if is_target:target.append((distance,float(power)))
            raw.append({'rx_id':r['rx_id'],'path_index':int(j),'distance_m':distance,'path_loss_db':float(-20*np.log10(abs(a[i,j]))),'power_dbm':float(power),'phase_rad':float(np.angle(a[i,j]*np.exp(-2j*np.pi*2.4e9*tau[i,j]))),'surface_sequence':seq,'interaction_positions':locs,'interaction_types':[int(types[q,i,j])for q in ii],'primitive_ids':prim,'object_ids':obj,'target':is_target})
        ai,ti=a[i,mask],tau[i,mask]
        if len(ai):
            weights=np.abs(ai)**2;mean=np.sum(weights*ti)/sum(weights);coherent=14+20*np.log10(abs(sum(ai*np.exp(-2j*np.pi*2.4e9*ti))));inc=14+10*np.log10(sum(weights));rms=1e9*np.sqrt(sum(weights*(ti-mean)**2)/sum(weights))
        else:coherent=inc=rms=float('nan')
        summary.append({'rx_id':r['rx_id'],'rx_x':r['rx_x'],'rx_y':r['rx_y'],'num_paths':len(ind),'target_count':len(target),'target_distance_m':target[0][0]if target else float('nan'),'target_individual_dbm':target[0][1]if target else float('nan'),'coherent_dbm':float(coherent),'incoherent_dbm':float(inc),'rms_ns':float(rms)})
    write(out/'summary.csv',summary);(out/'paths.json').write_text(json.dumps(raw,indent=2)+'\n')
    meta={'engine':'Sionna','scene':name,'receivers':n,'rx_file':str(rxfile),'versions':{k:importlib.metadata.version(k)for k in ['sionna-rt','mitsuba','drjit','numpy']},'variant':mi.variant(),'seconds':elapsed,'samples_per_src':args.samples,'seed':args.seed,'API':'Paths.vertices/primitives/objects/interactions; primitive -> archived triangle -> wall_id','tensor_shapes':{'a':list(paths.a[0].shape),'vertices':list(paths.vertices.shape),'primitives':list(paths.primitives.shape)},'frequency_hz':2.4e9,'tx_dbm':14,'polarization':'V/TE','eps_r':5.24,'sigma_Sm':.0462*2.4**.7822,'thickness_halfspace_m':10,'max_path_m':500,'max_depth':2,'diffraction':False,'refraction':False}
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n');print(name,'RX',n,'target present',sum(int(r['target_count'])>0 for r in summary),flush=True)
