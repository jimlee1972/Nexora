import datetime, hashlib, json, os, pathlib, subprocess, sys
root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(__file__).with_name(sys.argv[1]); out.mkdir(exist_ok=False)
exe = root/'build/windows-showcase-shipping/bin/Shipping/NexoraTwoDTailProfile.exe'
identity = dict(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(), utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), workload='DIAGNOSTIC ONLY; mixed preserves load, render-only omits CPU updates/reload', images=[2,3], source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'Engine/Presentation/src/VulkanSurface.cpp',root/'Tools/TwoD/Benchmark.cpp',pathlib.Path(__file__).with_name('BenchmarkProfile.cpp'),pathlib.Path(__file__).with_name('VulkanProfile.cpp')]})
(out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
identity['acquire_sync'] = os.environ.get('NEXORA_TRACE_ACQUIRE_SYNC')
(out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
for render_only in ((False,) if len(sys.argv)>2 else (False,True)):
    for images in (2,3):
        stem=('render' if render_only else 'mixed')+f'-{images}'
        env=os.environ.copy();env['NEXORA_TRACE_IMAGES']=str(images)
        if render_only:env['NEXORA_TRACE_RENDER_ONLY']='1'
        else:env.pop('NEXORA_TRACE_RENDER_ONLY',None)
        args=[str(exe),'--native','--backend','vulkan','--frames','600','--report',str(out/f'{stem}.csv')]
        print(f'Start {stem}',flush=True)
        with (out/f'{stem}.stdout').open('wb') as stdout,(out/f'{stem}.trace').open('wb') as stderr:
            result=subprocess.run(args,cwd=root,env=env,stdout=stdout,stderr=stderr,timeout=600)
        print(f'{stem}: exit {result.returncode}',flush=True)
        if result.returncode:raise RuntimeError(stem)
print('Diagnostic comparison finished; these are not acceptance runs.',flush=True)
