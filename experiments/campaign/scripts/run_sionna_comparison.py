from pathlib import Path
import argparse,csv,json,time,os,importlib.metadata
import numpy as np
import mitsuba as mi
mi.set_variant('llvm_ad_mono_polarized')
import drjit as dr
from sionna.rt import load_scene,PlanarArray,Transmitter,Receiver,PathSolver

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--samples',type=int,default=1000000);parser.add_argument('--seed',type=int,default=42)
parser.add_argument('--directory',default='04_E5_Sionna')
args=parser.parse_args();out=ROOT/args.directory;tag=f's{args.samples}_seed{args.seed}'
def read(p):
    with p.open()as f:return list(csv.DictReader(f))
def write(p,rows):
    with p.open('w',newline='')as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
walls=read(out/'common_walls.csv');receivers=read(out/'common_receivers.csv')
mesh=out/'facades.ply'
with mesh.open('w')as f:
    f.write(f'ply\nformat ascii 1.0\nelement vertex {len(walls)*4}\nproperty float x\nproperty float y\nproperty float z\nelement face {len(walls)*2}\nproperty list uchar int vertex_indices\nend_header\n')
    for w in walls:
        ax,ay,bx,by=[float(w[k])-offset for k,offset in [('x1',1838.748),('y1',465),('x2',1838.748),('y2',465)]]
        for x,y,z in [(ax,ay,-25),(bx,by,-25),(bx,by,25),(ax,ay,25)]:f.write(f'{x:.17g} {y:.17g} {z}\n')
    for i in range(len(walls)):
        j=i*4;f.write(f'3 {j} {j+1} {j+2}\n3 {j} {j+2} {j+3}\n')
sigma=0.0462*2.4**0.7822
xml=out/'common_scene.xml'
xml.write_text(f'''<scene version="3.0.0">
<bsdf type="radio-material" id="matched_concrete">
<float name="relative_permittivity" value="5.24"/>
<float name="conductivity" value="{sigma:.17g}"/>
<float name="thickness" value="10"/>
</bsdf>
<shape type="ply" id="mesh-facades"><string name="filename" value="facades.ply"/>
<boolean name="face_normals" value="true"/><ref id="matched_concrete" name="bsdf"/></shape>
</scene>''')
scene=load_scene(str(xml),merge_shapes=False)
scene.frequency=2.4e9
scene.tx_array=PlanarArray(num_rows=1,num_cols=1,vertical_spacing=.5,horizontal_spacing=.5,pattern='iso',polarization='V')
scene.rx_array=PlanarArray(num_rows=1,num_cols=1,vertical_spacing=.5,horizontal_spacing=.5,pattern='iso',polarization='V')
scene.add(Transmitter('TX',position=[0,0,1.5]))
for i,r in enumerate(receivers):scene.add(Receiver('RX'+str(i),position=[float(r['rx_x'])-1838.748,float(r['rx_y'])-465,1.5]))
start=time.monotonic()
paths=PathSolver()(scene,max_depth=2,max_num_paths_per_src=1000000,
                   samples_per_src=args.samples,los=True,specular_reflection=True,
                   diffuse_reflection=False,refraction=False,diffraction=False,
                   edge_diffraction=False,seed=args.seed)
dr.eval(paths.a,paths.tau,paths.valid);dr.sync_thread()
elapsed=time.monotonic()-start
n=len(receivers)
real,imag=paths.a
a=real.numpy().reshape(n,-1).astype(np.float64)+1j*imag.numpy().reshape(n,-1).astype(np.float64)
tau=paths.tau.numpy().reshape(n,-1).astype(np.float64)
valid=paths.valid.numpy().reshape(n,-1)
interactions=paths.interactions.numpy()
orders=np.count_nonzero(interactions,axis=0).reshape(n,-1)
summary=[];raw=[]
for i,r in enumerate(receivers):
    mask=valid[i]&(tau[i]>=0)&(tau[i]*299792458<=500+1e-4)&(np.abs(a[i])>0)
    ai,ti=a[i,mask],tau[i,mask];order=orders[i,mask]
    if len(ai):
        # Paths.a excludes propagation phase; reconstruct it in float64.
        coherent=ai*np.exp(-2j*np.pi*2.4e9*ti)
        weights=np.abs(ai)**2;mean=np.sum(weights*ti)/np.sum(weights)
        p=14+20*np.log10(abs(np.sum(coherent)));inc=14+10*np.log10(np.sum(weights));rms=np.sqrt(np.sum(weights*(ti-mean)**2)/np.sum(weights))*1e9
    else:p=inc=float('nan');rms=0
    summary.append({'rx_id':r['rx_id'],'num_paths':len(ai),'power_dbm':float(p),'incoherent_dbm':float(inc),'rms_ns':float(rms),'r1_paths':int((order==1).sum()),'r2_paths':int((order==2).sum())})
    for j,(coefficient,delay,reflections) in enumerate(zip(ai,ti,order)):
        raw.append({'rx_id':r['rx_id'],'distance_m':float(delay*299792458),'delay_s':float(delay),
                    'path_loss_db':float(-20*np.log10(abs(coefficient))),
                    'passband_real':float(coefficient.real),'passband_imag':float(coefficient.imag),
                    'phase_baseband_rad':float(np.angle(coefficient*np.exp(-2j*np.pi*2.4e9*delay))),
                    'reflections':int(reflections)})
write(out/f'sionna_{tag}_results.csv',summary)
if raw:write(out/f'sionna_{tag}_paths.csv',raw)
metadata={'versions':{pkg:importlib.metadata.version(pkg) for pkg in ['sionna-rt','mitsuba','drjit','numpy']},
          'variant':mi.variant(),'samples_per_src':args.samples,'seed':args.seed,'batch_seconds':elapsed,
          'max_depth':2,'refraction':False,'diffraction':False,'wall_thickness_for_halfspace_limit_m':10,
          'permittivity':5.24,'conductivity_sm':sigma,'receivers':n,'walls':len(walls),
          'note':'Float32 tracing; common vertical-facade scene, no ground/roofs, matched V/TE polarization. Timing includes first JIT compilation.'}
(out/f'sionna_{tag}_metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
print(json.dumps(metadata,indent=2));print(summary[:2])
