from pathlib import Path
import subprocess,json,time,os
ROOT=Path(os.environ.get('RAYTRACING_REVIEW_ROOT',Path(__file__).resolve().parents[1]))
probe=ROOT/'runtime/rtprobe'
record_path=ROOT/'09_reproducibility/core_execution_records.json'
records=json.loads(record_path.read_text()) if record_path.exists() else []
def run(label,walls,receivers,out,repeats=1,mask=15):
    if out.exists():
        previous=[r for r in records if r['label']==label]
        if previous and previous[-1]['returncode']==0:
            print(label,'reuse verified completed execution',flush=True);return
        for existing in [out,out.with_suffix('.log'),Path(str(out)+'.paths.csv')]:
            if existing.exists():existing.rename(Path(str(existing)+'.failed-attempt1'))
    cmd=[str(probe),'eval',str(walls),str(receivers),str(out),str(repeats),str(mask),'500']
    started=time.monotonic()
    with out.with_suffix('.log').open('w') as log:
        p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    records.append({'label':label,'command':cmd,'returncode':p.returncode,'wall_seconds':time.monotonic()-started})
    (ROOT/'09_reproducibility/core_execution_records.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,p.returncode,round(records[-1]['wall_seconds'],3),'s',flush=True)
    if p.returncode:raise RuntimeError(label)
walls=ROOT/'09_reproducibility/walls_exact.csv'
run('probe_vs_OMNeT_M50',walls,ROOT/'09_reproducibility/m50_receivers.csv',ROOT/'09_reproducibility/probe_M50.csv')
run('external_reflection_only',walls,ROOT/'04_E5_Sionna/common_receivers.csv',ROOT/'04_E5_Sionna/rt2d_reflections.csv',mask=7)
for rx in sorted((ROOT/'06_E7_scenarios').glob('S*_receivers.csv')):
    run(rx.stem,walls,rx,rx.with_name(rx.stem.replace('_receivers','_results')+'.csv'))
for scene in sorted((ROOT/'07_E8_scalability/scenes').glob('*.csv')):
    family=scene.stem.split('_')[0]
    run(scene.stem,scene,ROOT/f'07_E8_scalability/{family}_receivers.csv',ROOT/f'07_E8_scalability/{scene.stem}_results.csv',repeats=6)
for n in [1,16,64,256]:
    run(f'receiver_count_{n}',walls,ROOT/f'07_E8_scalability/receivers_{n}.csv',ROOT/f'07_E8_scalability/receiver_count_{n}_results.csv',repeats=4)
