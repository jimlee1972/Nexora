#!/usr/bin/env python3
"""Offline >96 MiB WAV package; no final recordings or audible-speaker claim."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import wave
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('export_game',ROOT/'Tools/TwoD/ExportGame.py')
exporter=importlib.util.module_from_spec(spec);spec.loader.exec_module(exporter)
inventory_spec=importlib.util.spec_from_file_location('voice_inventory',ROOT/'Tools/TwoD/InspectVoices.py')
voice_inventory=importlib.util.module_from_spec(inventory_spec);inventory_spec.loader.exec_module(voice_inventory)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--module',type=Path,required=True)
    parser.add_argument('--shipping',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-voice-') as temporary:
        root=Path(temporary);shutil.copy2(args.module,root/args.module.name)
        payload=struct.pack('<h',4096)*(4*1024*1024) # 87.38-second mono line, decoded 16 MiB
        for index in range(12):
            with wave.open(str(root/f'line{index}.wav'),'wb') as f:
                f.setnchannels(1);f.setsampwidth(2);f.setframerate(48000);f.writeframes(payload)
        del payload
        report=voice_inventory.inventory(dict(schema=1,
            clips=[dict(id=f'line{i}',path=f'line{i}.wav',subtitle_id=f'subtitle.line{i}') for i in range(12)],
            resident_sets=[dict(name=f'one_line_{i}',ids=[f'line{i}']) for i in range(12)]),root)
        assert report['encoded_bytes']>96*1024**2 and report['decoded_pcm_bytes']==192*1024**2
        assert report['largest_declared_resident_pcm_bytes']==16*1024**2 and report['declared_coverage_complete']
        print('Fixture inventory: '+json.dumps({k:report[k] for k in ('encoded_bytes','decoded_pcm_bytes','seconds','largest_declared_resident_pcm_bytes')}))
        (root/'corrupt.wav').write_bytes(b'not a WAV')
        config=dict(schema=1,id='com.nexora.voice',title='Zig Offline Voice',scene='Main',module=args.module.name,
                    audio='off',audio_capacity=dict(assets=4,active_bytes=32*1024**2,live_bytes=48*1024**2),
                    assets=[dict(kind='blob',path=f'line{i}.wav') for i in range(12)]+[dict(kind='blob',path='corrupt.wav')])
        descriptor=root/'game.json'
        modes=['off']+(['required'] if sys.platform.startswith('linux') else [])
        null=root/'alsa-null.conf';null.write_text('pcm.!default { type null }\n')
        for mode in modes:
            config['audio']=mode;descriptor.write_text(json.dumps(config),encoding='utf-8')
            output=root/f'package-{mode}'
            exporter.export(descriptor,args.app,output,allow_development=not args.shipping)
            assert sum(p.stat().st_size for p in (output/'assets').iterdir())>96*1024**2
            assert 'audio_capacity 4 33554432 50331648' in (output/'game.nx2game').read_text()
            assert json.loads((output/'inventory.json').read_text())['development_test_copy']==(not args.shipping)
            env=os.environ.copy()
            if sys.platform.startswith('linux'):
                env['LD_LIBRARY_PATH']=str(output)+os.pathsep+env.get('LD_LIBRARY_PATH','')
                env['ALSA_CONFIG_PATH']=str(null)
            process=subprocess.Popen([str(output/args.app.name),'--run-2d-project',str(output/'game.nx2game'),'--frames','2000'],
                                     stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',env=env)
            observed_peak=0;deadline=time.monotonic()+110
            while process.poll() is None:
                if time.monotonic()>deadline:
                    process.kill();process.communicate();raise AssertionError('voice Player timed out')
                if sys.platform.startswith('linux'):
                    try:
                        status=Path(f'/proc/{process.pid}/status').read_text()
                        rss=next(int(line.split()[1])*1024 for line in status.splitlines() if line.startswith('VmRSS:'))
                        observed_peak=max(observed_peak,rss)
                    except (OSError,StopIteration):pass
                time.sleep(.005)
            stdout,stderr=process.communicate(timeout=5)
            result=subprocess.CompletedProcess(process.args,process.returncode,stdout,stderr)
            assert result.returncode==0,(result.stdout,result.stderr)
            assert 'SP-05 PASS:' in result.stderr,result.stderr
            report=json.loads(result.stdout)
            assert report['allocations_balanced'],report
            assert report['audio_active_bytes']==report['audio_retired_bytes']==report['asset_staging_bytes']==0,report
            assert report['audio_peak_live_bytes']==16*1024**2,report
            if mode=='required': assert report['audio_submitted_frames']>0,report
            print(f'observed_process_peak_rss={observed_peak if observed_peak else "UNAVAILABLE"}')
            print(f'PASS: {mode} output, 12 x 87-second WAV lines, >96 MiB offline package, 16 MiB peak PCM, zero final residency')
        # Invalid descriptor capacity must fail before publishing a package.
        for i,bad in enumerate([None,{},dict(assets=0,active_bytes=1,live_bytes=1),dict(assets=True,active_bytes=1,live_bytes=1),
                                dict(assets=4,active_bytes=10,live_bytes=9),dict(assets=4,active_bytes=1,live_bytes=1,extra=1)]):
            config['audio_capacity']=bad;descriptor.write_text(json.dumps(config))
            out=root/f'rejected-{i}'
            try: exporter.export(descriptor,args.app,out,allow_development=True)
            except ValueError: pass
            else: raise AssertionError(f'invalid audio capacity admitted: {bad}')
            assert not out.exists()
    print('SP-05 independent Zig/Player voice lifecycle and capacity rejection passed; physical audibility NOT VERIFIED')
if __name__=='__main__':main()
