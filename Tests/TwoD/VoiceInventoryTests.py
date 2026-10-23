#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import struct
import tempfile
spec=importlib.util.spec_from_file_location('voices',Path(__file__).resolve().parents[2]/'Tools/TwoD/InspectVoices.py')
voices=importlib.util.module_from_spec(spec);spec.loader.exec_module(voices)
def wav(codec,bits,samples):
    channels=1;rate=48000;align=bits//8
    fmt=struct.pack('<HHIIHH',codec,channels,rate,rate*align,align,bits)
    return b'RIFF'+struct.pack('<I',36+len(samples))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(samples))+samples
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    (root/'line.wav').write_bytes(wav(1,16,struct.pack('<hhhh',0,4096,-4096,0)))
    (root/'music.wav').write_bytes(wav(3,32,struct.pack('<ff',.125,-.125)))
    clips=[dict(id='line',path='line.wav',subtitle_id='subtitle.line'),dict(id='music',kind='music',path='music.wav')]
    manifest=dict(schema=1,clips=clips,resident_sets=[dict(name='dialogue_and_music',ids=['line','music'])])
    report=voices.inventory(manifest,root)
    assert report['declared_coverage_complete'] and report['encoded_bytes']==104 and report['decoded_pcm_bytes']==24
    assert report['largest_declared_resident_pcm_bytes']==24 and abs(report['seconds']-6/48000)<1e-9
    clips.append(dict(id='absent',path='missing.wav'))
    report=voices.inventory(manifest,root)
    assert not report['declared_coverage_complete'] and report['missing_recordings']==['absent'] and report['missing_subtitles']==['absent']
    for bad in [dict(schema=True,clips=clips),dict(schema=1,clips=clips+[clips[0]]),
                dict(schema=1,clips=clips,resident_sets=[dict(name='bad',ids=['unknown'])]),
                dict(schema=1,clips=[dict(id='escape',path='../outside.wav')])]:
        try:voices.inventory(bad,root)
        except ValueError:pass
        else:raise AssertionError('invalid inventory admitted')
    (root/'bad.wav').write_bytes(b'bad wave bytes')
    try:voices.wave_info(root/'bad.wav')
    except ValueError:pass
    else:raise AssertionError('corrupt WAV accepted')
print('Voice inventory: PCM16/float32 sizes, duration, resident sets, missing coverage and rejection passed')
