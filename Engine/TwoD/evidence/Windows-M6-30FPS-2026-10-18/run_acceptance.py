import csv
import ctypes
from ctypes import wintypes
import datetime
import hashlib
import json
import pathlib
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'Engine/TwoD/evidence/Windows-M6-30FPS-2026-10-18'
EXE = ROOT / 'build/windows-showcase-shipping/bin/Shipping/NexoraTwoDBenchmark.exe'

def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

class Memory(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [(n, ctypes.c_size_t) for n in ('PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage')]

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Memory), wintypes.DWORD]

OUT.mkdir(parents=True, exist_ok=False)
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
plan = dict(source=revision, declared_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), target_fps=30, frame_budget_ms=dict(p95=1000/30, p99=1000/30, maximum=2000/30), repeats=2, backends=['dx12', 'vulkan'], frames_per_case=600, warmup_frames=4, viewport=[640,480], present_mode='VSync', simulation_step_seconds=1/60, cases=['100 mixed','1000 mixed','5000 mixed','10000 mixed','1000 overlap','10000 sparse'], scope='Current physical developer-PC candidate screening; minimum product specification remains undeclared', load='Unmodified mixed benchmark: animation, movement, spatial rebuild/query/sweep, extraction/culling/sorting, instanced draws, full pool recycle and immutable atlas retirement/reload at measured midpoint', exclusions=['integrated UI/VFX product load','allocator/VRAM accounting','long-duration soak','per-frame trace (benchmark emits aggregate CSV)'], run_conditions='Sequential runs; no builds or other Nexora fixtures during timing; normal desktop applications remain open', failure_policy='All rows and both repeats must satisfy all three predeclared limits. Do not alter budgets or omit failed repeats.', executable_sha256=sha(EXE), executable=str(EXE), source_clean=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()=='')
save('plan.json', plan)
summary=[]
for repeat in (1,2):
    for backend in plan['backends']:
        stem=f'{backend}-{repeat}'
        report=OUT/f'{stem}.csv'
        args=[str(EXE),'--native','--backend',backend,'--frames','600','--report',str(report)]
        started=time.monotonic()
        samples=[]
        with (OUT/f'{stem}.stdout').open('wb') as stdout, (OUT/f'{stem}.stderr').open('wb') as stderr:
            process=subprocess.Popen(args,cwd=ROOT,stdout=stdout,stderr=stderr)
            handle=kernel.OpenProcess(0x410,False,process.pid)
            try:
                while process.poll() is None:
                    memory=Memory(); memory.cb=ctypes.sizeof(memory)
                    if handle and psapi.GetProcessMemoryInfo(handle,ctypes.byref(memory),memory.cb):
                        samples.append(dict(seconds=round(time.monotonic()-started,3),working_set_bytes=memory.WorkingSetSize,peak_working_set_bytes=memory.PeakWorkingSetSize,private_commit_bytes=memory.PrivateUsage))
                    if time.monotonic()-started>600:
                        process.kill(); process.wait()
                        raise RuntimeError(f'{stem} exceeded 600-second process timeout')
                    time.sleep(0.1)
            finally:
                if handle: kernel.CloseHandle(handle)
        rows=list(csv.DictReader(report.open(encoding='utf-8'))) if report.exists() else []
        checks=[]
        for row in rows:
            checks.append(dict(objects=int(row['objects']),sparse=row['sparse'],overlap=row['overlap'],p95_ms=float(row['frame_p95_ms']),p99_ms=float(row['frame_p99_ms']),max_ms=float(row['frame_max_ms']),pass_budget=all(float(row[key])<=plan['frame_budget_ms'][budget] for key,budget in [('frame_p95_ms','p95'),('frame_p99_ms','p99'),('frame_max_ms','maximum')]),hardware_nvidia=row['vendor_id']=='4318' and row['device_id']=='5121',frames_valid=row['frames']=='600' and row['backend_requested']==backend))
        save(f'{stem}-memory.json',samples)
        entry=dict(backend=backend,repeat=repeat,args=args,exit_code=process.returncode,elapsed_seconds=round(time.monotonic()-started,3),memory_samples=len(samples),peak_working_set_bytes=max((x['peak_working_set_bytes'] for x in samples),default=None),peak_private_commit_bytes=max((x['private_commit_bytes'] for x in samples),default=None),rows=checks,correctness_pass=process.returncode==0 and len(rows)==6 and all(x['hardware_nvidia'] and x['frames_valid'] for x in checks),performance_pass=len(rows)==6 and all(x['pass_budget'] for x in checks))
        summary.append(entry); save('summary.json',summary)
        print(f'{stem}: exit={process.returncode}, rows={len(rows)}, within_budget={sum(x["pass_budget"] for x in checks)}/6',flush=True)
        if process.returncode: break
save('identity.json',dict(source=revision,executable_sha256_before=plan['executable_sha256'],executable_sha256_after=sha(EXE),plan_sha256=sha(OUT/'plan.json'),csv_sha256={p.name:sha(p) for p in OUT.glob('*.csv')}))
print('All measured runs retained; full M6 acceptance requires documented scope review.',flush=True)
