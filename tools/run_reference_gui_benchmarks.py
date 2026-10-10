"""Run baseline/candidate GUI FPS comparisons sequentially in ABBA order.

Every round runs four launches in the order A, B, B, A. Variant A is the
baseline and variant B is the candidate. Which executable plays each variant
depends on which of the two mutually exclusive baseline options is given:

  --original       A runs that exe as original MMD 9.32, using the original
                   offset layout and the --original flag, so the candidate is
                   compared against the original MMD.
  --baseline-port  A runs that ported exe with the ported offset layout and
                   no --original flag, so the candidate is compared against
                   another port build.

The metric is the native application FPS counter, sampled roughly every 1.05
seconds for --seconds seconds in every run. Only --copies and --layout vary
between summaries; every other setting is shared by both variants.
"""
import argparse, json, pathlib, statistics, subprocess, sys

def signature(result):
    state=result['playing']
    fields=['viewOffsetX','viewOffsetY','cameraPitch','cameraYaw','cameraRoll','cameraDistance',
            'cameraPosX','cameraPosY','cameraPosZ','fpsLimit','renderW','renderH','viewport','groundGridEnabled']
    return {'camera':{key:state[key] for key in fields},
            'pose':[[bone['matInit'] for bone in model['bones']] for model in state['models']]}

def variant_modes(args):
    """Map variant letter to (label, executable, original-offset flag)."""
    if args.baseline_port is None:
        return {'A':('original_mmd',args.original,True),'B':('candidate_port',args.candidate,False)}
    return {'A':('baseline_port',args.baseline_port,False),'B':('candidate_port',args.candidate,False)}

def parse_args():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--candidate',type=pathlib.Path,required=True)
    p.add_argument('--scenes',type=pathlib.Path,required=True)
    p.add_argument('--out',type=pathlib.Path,required=True)
    baseline=p.add_mutually_exclusive_group(required=True)
    baseline.add_argument('--original',type=pathlib.Path,
                          help='original MMD 9.32 exe for variant A; runs with the original offset layout '
                               'and the --original flag')
    baseline.add_argument('--baseline-port',type=pathlib.Path,
                          help='ported baseline exe for variant A; runs with the ported offset layout and '
                               'without --original')
    p.add_argument('--copies',type=int,nargs='+',default=[1,2,4,8])
    p.add_argument('--layouts',nargs='+',default=['spread','overlap'])
    p.add_argument('--seconds',type=int,default=12)
    return p.parse_args()

def main():
    a=parse_args();a.out.mkdir(parents=True,exist_ok=True)
    if (a.out/'runs.json').exists():raise RuntimeError('use a new output directory')
    modes=variant_modes(a)
    script=pathlib.Path(__file__).with_name('reference_gui_benchmark.py')
    runs=[];summaries=[]
    for label,(_,exe,_) in sorted(modes.items()):
        if not exe.is_file():raise RuntimeError(f'variant {label} executable missing: {exe}')
    for layout in a.layouts:
        for count in a.copies:
            values={'A':[],'B':[]};expected=None
            for order,variant in enumerate('ABBA'):
                role,exe,original_flag=modes[variant]
                name=f'{layout}-{count}-{order+1}{variant}';directory=a.out/name
                cmd=[sys.executable,str(script),'--exe',str(exe),
                     '--scene',str(a.scenes/f'grass-{count}-{layout}.pmm'),'--out',str(directory),
                     '--copies',str(count),'--seconds',str(a.seconds),'--counter-only']
                if original_flag:cmd.append('--original')
                print('START',name,role,flush=True)
                directory.mkdir()
                with (directory/'run.log').open('wb') as log:
                    run=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
                record={'name':name,'variant':variant,'role':role,'exe':str(exe),
                        'original_offsets':original_flag,'returncode':run.returncode,'command':cmd}
                runs.append(record);(a.out/'runs.json').write_text(json.dumps(runs,indent=2),encoding='utf-8')
                if run.returncode:raise RuntimeError(f'{name} failed: see run.log')
                data=json.loads((directory/'result.json').read_text(encoding='utf-8'))
                sig=signature(data)
                if expected is not None and sig!=expected:raise RuntimeError(f'{name}: scene/pose differs')
                expected=sig
                if len(data['playing']['models'])!=count or data['after']['playbackActive']!=1:
                    raise RuntimeError(f'{name}: scene/playback invalid')
                for stage in ['loaded','after']:
                    if data[stage]['models']!=data['playing']['models']:
                        raise RuntimeError(f'{name}: static pose changed at {stage}')
                fps=data['native_fps']['median_fps'];values[variant].append(fps)
                print('DONE',name,fps,'FPS',flush=True)
            summaries.append({'copies':count,'layout':layout,'seconds':a.seconds,
                'baseline':{'variant':'A','role':modes['A'][0],'exe':str(modes['A'][1]),
                            'median_fps':statistics.median(values['A']),'raw_median_fps':values['A']},
                'candidate':{'variant':'B','role':modes['B'][0],'exe':str(modes['B'][1]),
                            'median_fps':statistics.median(values['B']),'raw_median_fps':values['B']},
                'metric':f'Native rounded one-second FPS counter sampled over {a.seconds} seconds per run; '
                         'median of per-run medians across one ABBA round; not ETW frame timing.'})
            (a.out/'summary.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8')
            print(json.dumps(summaries[-1]),flush=True)

if __name__=='__main__':main()
