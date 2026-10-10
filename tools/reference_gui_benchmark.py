"""Compare original MMD 9.32 and this port through the normal PMM/playback UI.

The original layout is tied to SHA-256 07516f...a1. Port offsets match the
2026-09-24 app layout; refresh them if MMDAppState changes. No process memory
is written. Only the child process launched here is controlled or terminated.
"""
import argparse, ctypes as c, ctypes.wintypes as w, csv, hashlib, importlib.util
import json, os, pathlib, statistics, subprocess, time
spec = importlib.util.spec_from_file_location('mdump', pathlib.Path(__file__).with_name('mdump2.py'))
d = importlib.util.module_from_spec(spec); spec.loader.exec_module(d)
k, u = d.k32, d.u32
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
k.CloseHandle.argtypes = [w.HANDLE]
k.CreateToolhelp32Snapshot.argtypes = [w.DWORD,w.DWORD]; k.CreateToolhelp32Snapshot.restype = w.HANDLE
k.Module32FirstW.argtypes = [w.HANDLE,c.POINTER(d.MODULEENTRY32W)]
k.Module32NextW.argtypes = [w.HANDLE,c.POINTER(d.MODULEENTRY32W)]
k.ReadProcessMemory.argtypes = [w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
k.VirtualQueryEx.argtypes = [w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t]; k.VirtualQueryEx.restype=c.c_size_t
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.SetWindowTextW.argtypes=[w.HWND,w.LPCWSTR]
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
u.ShowWindow.argtypes=[w.HWND,c.c_int]
u.ClientToScreen.argtypes=[w.HWND,c.POINTER(w.POINT)]
u.WindowFromPoint.argtypes=[w.POINT];u.WindowFromPoint.restype=w.HWND
u.GetAncestor.argtypes=[w.HWND,w.UINT];u.GetAncestor.restype=w.HWND
u.GetForegroundWindow.restype=w.HWND
OFF={"viewOffsetX":832,"viewOffsetY":836,"cameraPitch":840,"cameraYaw":844,"cameraRoll":848,
"cameraDistance":661760,"cameraPosX":884,"cameraPosY":888,"cameraPosZ":896,"playbackActive":877,
"playbackLoopEnabled":901,"currentFrame":5200,"fpsLimit":661764,"fpsOverlayFrameCount":864,
"framesPerSecond":868,"fpsOverlayEnabled":854,"renderW":661752,"renderH":661756,"hideRight":663020,
"sidebarWidth":661224,"renderer":661216,"hwnd":661192,"groundGridEnabled":853,"slotIdx":5088,"modelSlots":3048}
import struct
class BasicLimits(c.Structure):
    _fields_=[('ProcessUserTime',c.c_int64),('JobUserTime',c.c_int64),('LimitFlags',w.DWORD),
              ('MinWorkingSet',c.c_size_t),('MaxWorkingSet',c.c_size_t),('ActiveProcesses',w.DWORD),
              ('Affinity',c.c_size_t),('PriorityClass',w.DWORD),('SchedulingClass',w.DWORD)]
class ExtendedLimits(c.Structure):
    _fields_=[('Basic',BasicLimits),('IoCounters',c.c_uint64*6),('ProcessMemory',c.c_size_t),
              ('JobMemory',c.c_size_t),('PeakProcessMemory',c.c_size_t),('PeakJobMemory',c.c_size_t)]
k.CreateJobObjectW.argtypes=[c.c_void_p,w.LPCWSTR];k.CreateJobObjectW.restype=w.HANDLE
k.SetInformationJobObject.argtypes=[w.HANDLE,c.c_int,c.c_void_p,w.DWORD]
k.AssignProcessToJobObject.argtypes=[w.HANDLE,w.HANDLE]
def child_job(proc):
    job=k.CreateJobObjectW(None,None);limits=ExtendedLimits();limits.Basic.LimitFlags=0x2000
    if not job or not k.SetInformationJobObject(job,9,c.byref(limits),c.sizeof(limits)) or not k.AssignProcessToJobObject(job,int(proc._handle)):
        error=c.get_last_error();proc.terminate();proc.wait(timeout=10)
        if job:k.CloseHandle(job)
        raise OSError(error,'cannot contain the test application in a process job')
    return job
def valid_app(h,app):
    if not 0x10000 < app < 0x7fffffffffff:return False
    return d.valid_model(h,d.qword(h,app+3048)) and bool(d.qword(h,app+661192))
d.valid_app=valid_app
def models(h,app):
    raw=d.rpm(h,app+3048,255*8)
    if not raw:return []
    return [m for m in struct.unpack('<255Q',raw) if m]
def val(h,app,field,fmt='I'):
    size=struct.calcsize(fmt);raw=d.rpm(h,app+OFF[field],size)
    if raw is None or len(raw)!=size:raise RuntimeError('unreadable '+field)
    return struct.unpack('<'+fmt,raw)[0]
def windows(pid):
    found=[]
    @c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
    def cb(hwnd,lp):
        wp=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(wp))
        if wp.value==pid:
            title=c.create_unicode_buffer(512);cls=c.create_unicode_buffer(100)
            u.GetWindowTextW(hwnd,title,512);u.GetClassNameW(hwnd,cls,100)
            found.append((hwnd,cls.value,title.value))
        return True
    u.EnumWindows(cb,0);return found
def snapshot(h,app):
    fields={f:val(h,app,f,'f') for f in ['viewOffsetX','viewOffsetY','cameraPitch','cameraYaw','cameraRoll',
        'cameraDistance','cameraPosX','cameraPosY','cameraPosZ','fpsLimit']}
    fields.update({f:val(h,app,f,'B') for f in ['playbackActive','groundGridEnabled','slotIdx']})
    fields.update({f:val(h,app,f) for f in ['renderW','renderH','currentFrame','framesPerSecond']})
    fields['viewport']=struct.unpack('<4i',d.rpm(h,app+OFF['hideRight'],16))
    fields['models']=[]
    for model in models(h,app):
        bc=d.dword(h,model+d.M_BONECOUNT);bt=d.qword(h,model+d.M_BONETABLE)
        bones=d.dump_bones(h,bt,bc)
        fields['models'].append({'bone_count':bc,'bones':bones})
    return fields
# Topmost without activation: SWP_NOACTIVATE (0x10) keeps keyboard focus where it
# was, SWP_NOMOVE|SWP_NOSIZE (0x2|0x1) keep the geometry the resize loop settled
# on. SWP_NOZORDER (0x4) must stay out: it cancels the topmost placement and the
# call silently does nothing.
WP_TOPMOST=0x13
GA_ROOT=2
# Composite probes cover the viewport: centre plus the four inset corners, so a
# chat window overlapping one edge still shows up.
INSET=8
def probe_points(vr):
    x0,y0,x1,y1=vr
    return [('centre',(x0+x1)//2,(y0+y1)//2),('top-left',x0+INSET,y0+INSET),
        ('top-right',x1-1-INSET,y0+INSET),('bottom-left',x0+INSET,y1-1-INSET),
        ('bottom-right',x1-1-INSET,y1-1-INSET)]
def probe_viewport(hwnd,vr):
    points=[]
    for name,cx,cy in probe_points(vr):
        point=w.POINT(cx,cy)
        if not u.ClientToScreen(hwnd,c.byref(point)):
            points.append({'point':name,'x':cx,'y':cy,'error':'ClientToScreen failed','ours':False});continue
        at=u.WindowFromPoint(point)
        root=u.GetAncestor(at,GA_ROOT) if at else 0
        points.append({'point':name,'client':[cx,cy],'screen':[point.x,point.y],'hwnd':hex(at) if at else None,
            'root':hex(root) if root else None,'ours':bool(at) and root==hwnd})
    return points
def check_visible(result,hwnd,vr,stage):
    points=probe_viewport(hwnd,vr)
    entry={'stage':stage,'viewport':list(vr),'target':hex(hwnd),'points':points,
        'foreground':hex(u.GetForegroundWindow() or 0),
        'foreground_unchanged':(u.GetForegroundWindow() or 0)==result['foreground_before'],
        'occluded':[p['point'] for p in points if not p.get('ours')],
        'all_ours':all(p.get('ours') for p in points)}
    result.setdefault('visibility',[]).append(entry)
    print('VISIBILITY',stage,json.dumps(entry),flush=True)
    if not entry['all_ours']:
        raise RuntimeError(stage+' occluded: '+repr(entry['occluded'])+' hit '+repr(points))
    return entry
def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',type=pathlib.Path,required=True)
    p.add_argument('--scene',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True)
    p.add_argument('--original',action='store_true');p.add_argument('--copies',type=int,required=True)
    p.add_argument('--seconds',type=int,default=30);p.add_argument('--no-capture',action='store_true')
    p.add_argument('--counter-only',action='store_true')
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    if a.original:
        if hashlib.sha256(a.exe.read_bytes()).hexdigest()!='07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1':
            raise RuntimeError('original binary does not match the validated 9.32 build')
        OFF.update(cameraPosX=876,cameraPosY=880,cameraPosZ=884,playbackActive=872,
                   playbackLoopEnabled=889,fpsOverlayFrameCount=860,framesPerSecond=864)
    d.F_PARENT=56
    u.SetProcessDPIAware()
    if (a.out/'result.json').exists():raise RuntimeError('preserve previous run; use a new directory')
    si=subprocess.STARTUPINFO();si.dwFlags=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
    env=dict(os.environ);env['__COMPAT_LAYER']='HIGHDPIAWARE'
    proc=subprocess.Popen([str(a.exe),str(a.scene)],cwd=a.exe.parent,startupinfo=si,env=env)
    job=child_job(proc) # Closing the runner also closes the test application.
    h=k.OpenProcess(0x0410,False,proc.pid);pm=None
    result={'exe':str(a.exe),'exe_sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),
        'scene':str(a.scene),'scene_sha256':hashlib.sha256(a.scene.read_bytes()).hexdigest(),'pid':proc.pid,
        'window_topmost':True}
    result['foreground_before']=u.GetForegroundWindow() or 0
    print('FOREGROUND BEFORE',hex(result['foreground_before']),flush=True)
    print('PID',proc.pid,flush=True)
    try:
        deadline=time.monotonic()+90;app=0;hwnd=0;stable=0
        while time.monotonic()<deadline:
            if proc.poll() is not None:raise RuntimeError('application exited '+str(proc.returncode))
            ws=windows(proc.pid)
            dialogs=[(win,title,d.enum_children(win)) for win,cls,title in ws if cls=='#32770']
            if dialogs:raise RuntimeError('unexpected dialog '+repr(dialogs))
            base,size=d.main_module(h,proc.pid)
            if base:app=app or (d.find_app_orig(h,base) if a.original else d.find_app_port(h,base,size))
            if app and d.valid_app(h,app):
                count=len(models(h,app))
                if count==a.copies:
                    stable+=1
                    if stable>=3:break
            time.sleep(.4)
        if not app or not d.valid_app(h,app) or stable<3:raise RuntimeError('model count failed to settle; windows='+repr(windows(proc.pid)))
        hwnd=val(h,app,'hwnd','Q');print('LOADED',hex(app),hex(hwnd),flush=True)
        # Resize the normal window until the scene rectangle is 1280 x 720.
        # Show without taking keyboard focus: hidden windows can be throttled.
        u.ShowWindow(hwnd,4)
        u.SetWindowPos(hwnd,None,40,30,1630,1000,0x14)
        result['resize_attempts']=[]
        for _ in range(20):
            time.sleep(.5);vr=struct.unpack('<4i',d.rpm(h,app+OFF['hideRight'],16))
            ww=w.RECT();u.GetWindowRect(hwnd,c.byref(ww))
            vw,vh=vr[2]-vr[0],vr[3]-vr[1]
            result['resize_attempts'].append({'viewport':vr,'window':[ww.left,ww.top,ww.right,ww.bottom]})
            if (vw,vh)==(1280,720):break
            u.SetWindowPos(hwnd,None,0,0,ww.right-ww.left+1280-vw,ww.bottom-ww.top+720-vh,0x416)
        # Raise once, after the geometry has settled: repeated calls would keep
        # fighting any window the user opens in front. The child is terminated in
        # the finally block, so the topmost flag needs no undoing here.
        if not u.SetWindowPos(hwnd,-1,0,0,0,0,WP_TOPMOST):
            raise RuntimeError('cannot make the test window topmost; error '+str(c.get_last_error()))
        result['window_topmost_call']={'hwnd':hex(hwnd),'flags':hex(WP_TOPMOST),
            'previous_foreground':hex(u.GetForegroundWindow() or 0)}
        result['loaded']=snapshot(h,app)
        print('VIEWPORT',result['loaded']['viewport'],flush=True)
        vr=result['loaded']['viewport']
        if (vr[2]-vr[0],vr[3]-vr[1])!=(1280,720):raise RuntimeError('viewport mismatch')
        u.PostMessageW(hwnd,0x111,234,0) # native UI command: remove FPS cap
        end=u.GetDlgItem(hwnd,0x19A)
        if not end:raise RuntimeError('play-end edit missing')
        u.SetWindowTextW(end,'300000')
        u.PostMessageW(hwnd,0x111,408,u.GetDlgItem(hwnd,408)) # button notification, with control HWND
        time.sleep(3)
        result['playing']=snapshot(h,app)
        if result['playing']['playbackActive']!=1 or result['playing']['fpsLimit']!=1000:
            raise RuntimeError('normal playback or uncapped rate failed')
        from PIL import ImageGrab
        vr=result['playing']['viewport'];point=w.POINT(vr[0],vr[1]);u.ClientToScreen(hwnd,c.byref(point))
        check_visible(result,hwnd,vr,'before-capture')
        ImageGrab.grab(bbox=(point.x,point.y,point.x+1280,point.y+720)).save(a.out/'viewport.png')
        time.sleep(1)
        if not a.no_capture:
            pmexe=r'C:\Program Files\NVIDIA Corporation\FrameViewSDK\bin\PresentMon_x64.exe'
            cmd=[pmexe,'--process_id',str(proc.pid),'--output_file',str(a.out/'frames.csv'),
                '--no_console_stats','--v1_metrics','--timed',str(a.seconds),'--terminate_after_timed',
                '--session_name','MMDAlpha_'+str(proc.pid),'--no_track_input']
            if not a.counter_only:
                with (a.out/'presentmon.log').open('wb') as log:
                    pm=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
                    pm.wait(timeout=a.seconds+20)
            if a.counter_only or pm.returncode:
                result['presentmon_error']=('counter-only requested after ETW startup failed in smoke tests'
                    if a.counter_only else 'PresentMon exited '+str(pm.returncode)+'; see presentmon.log')
                # Read the same native one-second FPS display on both versions.
                # These are rounded application counters, not ETW present times.
                values=[];start=time.monotonic()
                while time.monotonic()-start<a.seconds:
                    check_visible(result,hwnd,vr,'fps-sample-'+str(len(values)))
                    values.append({'seconds':time.monotonic()-start,'fps':val(h,app,'framesPerSecond'),
                                   'frame':val(h,app,'currentFrame')})
                    time.sleep(1.05)
                result['native_fps']={'samples':values,'median_fps':statistics.median(v['fps'] for v in values),
                    'min_fps':min(v['fps'] for v in values),'max_fps':max(v['fps'] for v in values),
                    'note':'Read-only samples of the native one-second FPS display. No frame-time P95 or ETW claim.'}
                result['after']=snapshot(h,app)
                print(json.dumps(result['native_fps']),flush=True)
                return
            result['after']=snapshot(h,app)
            rows=list(csv.DictReader((a.out/'frames.csv').open(newline='',encoding='utf-8-sig')))
            eligible=[r for r in rows if int(r['ProcessID'])==proc.pid and float(r.get('MsBetweenPresents') or 0)>0]
            if len(eligible)<360:raise RuntimeError('fewer than 360 presents: '+str(len(eligible)))
            sample=eligible[60:360]
            dt=[float(r['MsBetweenPresents']) for r in sample]
            result['presentmon']={'frames':len(eligible),'warmup':60,'samples':300,'mean_ms':statistics.mean(dt),
                'p95_ms':sorted(dt)[284],'present_fps':1000/statistics.mean(dt),
                'swapchains':sorted(set(r['SwapChainAddress'] for r in sample)),
                'note':'Present intervals in the normal window/playback loop; separate from GPU-drained benchmark.'}
        print(json.dumps(result.get('presentmon',{})),flush=True)
    except Exception as error:
        result['error']=str(error);raise
    finally:
        (a.out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        if pm and pm.poll() is None:pm.terminate();pm.wait(timeout=5)
        if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
        if h:k.CloseHandle(h)
        k.CloseHandle(job)
if __name__=='__main__':main()
