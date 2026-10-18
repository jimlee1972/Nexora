import datetime,hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2]
work=pathlib.Path(__file__).resolve().parent
out=work/'diagnostic-dx12-tail';out.mkdir(exist_ok=False)
exe=root/'build/windows-showcase-shipping/bin/Shipping/NexoraTwoDTailProfile.exe'
identity=dict(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='DIAGNOSTIC ONLY; original mixed load, stage clocks and buffered per-frame trace',source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'Engine/Presentation/src/VulkanSurface.cpp',root/'Engine/Presentation/src/Dx12Surface.cpp',work/'VulkanProfile.cpp',work/'BenchmarkProfile.cpp']})
(out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
for repeat in (1,2):
    stem=f'dx12-{repeat}'
    args=[str(exe),'--native','--backend','dx12','--frames','600','--report',str(out/f'{stem}.csv')]
    with (out/f'{stem}.stdout').open('wb') as stdout,(out/f'{stem}.trace').open('wb') as stderr:
        result=subprocess.run(args,cwd=root,stdout=stdout,stderr=stderr,timeout=600)
    print(f'{stem}: exit {result.returncode}',flush=True)
    if result.returncode:raise RuntimeError(stem)
