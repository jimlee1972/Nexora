#!/usr/bin/env python3
"""Inventory declared local WAV/subtitle coverage and decoded PCM resident sets (metadata only)."""
import argparse
import json
from pathlib import Path
import struct
MAX=64*1024*1024

def unique(pairs):
    out={}
    for key,value in pairs:
        if key in out: raise ValueError('duplicate JSON field')
        out[key]=value
    return out

def wave_info(path):
    if not path.is_file() or not 12<=path.stat().st_size<=MAX: raise ValueError('WAV file budget')
    data=path.read_bytes()
    if data[:4]!=b'RIFF' or data[8:12]!=b'WAVE' or struct.unpack_from('<I',data,4)[0]!=len(data)-8:
        raise ValueError('invalid RIFF/WAVE')
    offset=12;fmt=None;payload=None
    while offset<len(data):
        if len(data)-offset<8: raise ValueError('truncated chunk')
        tag=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0];offset+=8
        if size>len(data)-offset: raise ValueError('truncated chunk payload')
        if tag==b'fmt ':
            if fmt is not None: raise ValueError('duplicate fmt')
            fmt=data[offset:offset+size]
        if tag==b'data':
            if payload is not None: raise ValueError('duplicate data')
            payload=size
        offset+=size+(size&1)
    if offset!=len(data) or fmt is None or len(fmt)<16 or not payload: raise ValueError('missing WAV chunks')
    codec,channels,rate,byte_rate,alignment,bits=struct.unpack_from('<HHIIHH',fmt)
    if channels not in (1,2) or not 8000<=rate<=192000 or (codec,bits) not in ((1,16),(3,32)):
        raise ValueError('unsupported WAV format')
    if alignment!=channels*bits//8 or byte_rate!=rate*alignment or payload%alignment:
        raise ValueError('invalid WAV alignment')
    frames=payload//alignment
    return dict(encoded_bytes=len(data),decoded_pcm_bytes=frames*channels*4,frames=frames,
                seconds=frames/rate,sample_rate=rate,channels=channels,codec='pcm16' if codec==1 else 'float32')

def inventory(manifest,root):
    root=root.resolve(strict=True)
    if set(manifest)-{'schema','clips','resident_sets'} or type(manifest.get('schema')) is not int or manifest['schema']!=1:
        raise ValueError('invalid inventory schema')
    clips=manifest.get('clips');sets=manifest.get('resident_sets',[])
    if not isinstance(clips,list) or not 1<=len(clips)<=4096 or not isinstance(sets,list) or len(sets)>256:
        raise ValueError('inventory count budget')
    entries={};paths=set()
    for clip in clips:
        if not isinstance(clip,dict) or set(clip)-{'id','path','kind','subtitle_id'}: raise ValueError('invalid clip fields')
        identity=clip.get('id');kind=clip.get('kind','voice');subtitle=clip.get('subtitle_id')
        if not isinstance(identity,str) or not 1<=len(identity)<=256 or identity in entries or kind not in ('voice','music','sfx'):
            raise ValueError('invalid or duplicate clip ID/kind')
        if subtitle is not None and (not isinstance(subtitle,str) or not subtitle): raise ValueError('invalid subtitle ID')
        result=dict(id=identity,kind=kind,subtitle_id=subtitle,status='missing')
        source=clip.get('path')
        if source is not None:
            if not isinstance(source,str) or not source or Path(source).is_absolute(): raise ValueError('invalid relative WAV path')
            path=(root/source).resolve()
            if not path.is_relative_to(root): raise ValueError('WAV path escapes manifest root')
            if path in paths: raise ValueError('duplicate source path; reuse its clip ID')
            paths.add(path)
            if path.exists():result.update(status='present',**wave_info(path))
        entries[identity]=result
    resident=[];names=set()
    for group in sets:
        if not isinstance(group,dict) or set(group)!={'name','ids'}:raise ValueError('invalid resident set')
        name=group['name'];ids=group['ids']
        if not isinstance(name,str) or not name or name in names or not isinstance(ids,list) or len(ids)>4096:
            raise ValueError('invalid resident set name/count')
        if any(not isinstance(i,str) or i not in entries for i in ids) or len(set(ids))!=len(ids):
            raise ValueError('unknown/duplicate resident clip')
        names.add(name);missing=[i for i in ids if entries[i]['status']!='present']
        resident.append(dict(name=name,assets=len(ids),missing_ids=missing,
                             decoded_pcm_bytes=sum(entries[i].get('decoded_pcm_bytes',0) for i in ids)))
    values=list(entries.values());missing=[e['id'] for e in values if e['status']!='present']
    subtitles=[e['id'] for e in values if e['kind']=='voice' and not e['subtitle_id']]
    return dict(validation='container_metadata_only; engine still validates samples',clips=values,
                declared_coverage_complete=not missing and not subtitles,missing_recordings=missing,missing_subtitles=subtitles,
                encoded_bytes=sum(e.get('encoded_bytes',0) for e in values),
                decoded_pcm_bytes=sum(e.get('decoded_pcm_bytes',0) for e in values),
                seconds=sum(e.get('seconds',0) for e in values),resident_sets=resident,
                largest_declared_resident_pcm_bytes=max((g['decoded_pcm_bytes'] for g in resident),default=0))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.manifest.stat().st_size>MAX: raise ValueError('manifest budget')
    result=inventory(json.loads(args.manifest.read_text(encoding='utf-8'),object_pairs_hook=unique),args.manifest.resolve().parent)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
