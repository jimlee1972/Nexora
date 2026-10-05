"""Generate original bounded crystal and detail-map authoring payloads; no downloaded art claim."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PrepareCourtyardEnvironment import golden_sky
from CourtyardImageCook import decode_png, area_filter, leaf_card

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / 'Content/Showcase/Courtyard/Hero'

def generate():
    source = json.loads((CONTENT / 'source.json').read_text())
    source_hashes = {}
    authored = {}
    for role, entry in source['authored_images'].items():
        data = (CONTENT / entry['path']).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry['sha256']:
            raise ValueError(f'Authored source hash mismatch: {role}')
        source_hashes[entry['path']] = digest
        authored[role] = decode_png(data)
    sides, rings = source['sides'], source['rings']
    vertices = []
    def point(ring, side):
        radius, y = ring
        x, z = sides[side % len(sides)]
        return (radius*x, y, radius*z)
    def triangle(points):
        a,b,c = points
        u,v = tuple(b[i]-a[i] for i in range(3)),tuple(c[i]-a[i] for i in range(3))
        n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        length=math.sqrt(sum(x*x for x in n))
        if length < 1e-8: return
        n=tuple(round(x/length,6) for x in n)
        for p,uv in zip(points,((0,0),(1,0),(0.5,1))): vertices.append(p+n+uv)
    for band in range(len(rings)-1):
        for side in range(len(sides)):
            a,b,c,d=point(rings[band],side),point(rings[band],side+1),point(rings[band+1],side+1),point(rings[band+1],side)
            triangle((a,c,b));triangle((a,d,c))
    mesh=f'nexora.showcase.mesh.v1 {len(vertices)} {len(vertices)}\n'
    mesh+=''.join(' '.join(format(x,'.9g') for x in row)+'\n' for row in vertices)
    mesh+=' '.join(map(str,range(len(vertices))))+'\n'
    outputs={'crystal.showcase':mesh.encode()}
    def noise(x,y):
        value=((x*73856093)^(y*19349663)^source['seed'])&0xffffffff
        value=((value^(value>>13))*1274126177)&0xffffffff
        return (value^(value>>16))&255
    size=source['detail_size']
    assert size==256
    stone_color = area_filter(authored['stone_color'], size, size)
    stone_height = area_filter(authored['stone_height'], size, size)
    def height(x,y):
        x%=size;y%=size
        return stone_height[(y*size+x)*4]
    for material in ('stone','bronze'):
        color,normal,orm=bytearray(),bytearray(),bytearray()
        for y in range(size):
            for x in range(size):
                n=noise(x,y);h=height(x,y)
                if material=='stone':
                    offset=(y*size+x)*4
                    rgb=tuple(stone_color[offset:offset+3])
                    ao,rough,metal=215+h*40//255,220+n//16,0
                else:
                    patina=(noise(x//4,y//4)<60)
                    rgb=(55+n//12,107+n//16,88+n//16) if patina else (155+n//9,100+n//12,38+n//14)
                    ao,rough,metal=230,(190 if patina else 130)+n//8,255
                color.extend((*rgb,255));orm.extend((ao,min(255,rough),metal,255))
                dx=(height(x+1,y)-height(x-1,y))*0.006 if material=='stone' else (noise(x+1,y)-noise(x-1,y))*0.0003
                dy=(height(x,y+1)-height(x,y-1))*0.006 if material=='stone' else (noise(x,y+1)-noise(x,y-1))*0.0003
                length=math.sqrt(dx*dx+dy*dy+1)
                normal.extend((round(127.5-127.5*dx/length),round(127.5-127.5*dy/length),round(127.5+127.5/length),255))
        for name,data in [('color',color),('normal',normal),('orm',orm)]:outputs[f'{material}-{name}.rgba']=bytes(data)
    size=source['texture_size']
    assert size==64
    outputs['leaf.rgba']=bytes(leaf_card(authored['leaf'],source['leaf_size']))
    for name in ('mote',):
        data=bytearray()
        for y in range(size):
            for x in range(size):
                u=(2*x+1-size)/size;v=(2*y+1-size)/size
                mask=(abs(u)<0.72*(1-v*v) and abs(v)<0.94) if name=='leaf' else u*u+v*v<0.6
                rgb=(65+noise(x,y)//12,135+noise(x,y)//8,30+noise(x,y)//16) if name=='leaf' else (255,255,255)
                data.extend((*rgb,255 if mask else 0))
        outputs[name+'.rgba']=bytes(data)
    # Six-face atlas; evaluate one continuous directional sky across all face boundaries.
    normals=((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1))
    rights=((0,0,-1),(0,0,1),(1,0,0),(1,0,0),(1,0,0),(-1,0,0))
    ups=((0,1,0),(0,1,0),(0,0,-1),(0,0,1),(0,1,0),(0,1,0))
    sky=bytearray()
    for y in range(256):
        for x in range(384):
            face=(y//128)*3+x//128
            u,v=2*(x%128)/127-1,2*(y%128)/127-1
            direction=[normals[face][i]+u*rights[face][i]+v*ups[face][i] for i in range(3)]
            length=math.sqrt(sum(d*d for d in direction)); dx,dy,dz=[d/length for d in direction]
            linear=golden_sky((dx,dy,dz),source['sky'])
            rgb=[round(255*(12.92*c if c<=0.0031308 else 1.055*c**(1/2.4)-0.055)) for c in linear]
            sky.extend((*rgb,255))
    outputs['sky.rgba']=bytes(sky)
    metadata={'schema' :'nexora.courtyard.hero-manifest.v1','source':'source.json','author':source['author'],'license':source['license'],'source_sha256':hashlib.sha256((CONTENT/'source.json').read_bytes()).hexdigest(),'license_sha256':hashlib.sha256((ROOT/'LICENSE').read_bytes()).hexdigest(),'converter_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'sky_converter_sha256':hashlib.sha256((ROOT/'Tools/Build/PrepareCourtyardEnvironment.py').read_bytes()).hexdigest(),'derived':{name:hashlib.sha256(data).hexdigest() for name,data in outputs.items()}}
    metadata['authored_sources']=source_hashes
    metadata['image_cook_sha256']=hashlib.sha256((ROOT/'Tools/Build/CourtyardImageCook.py').read_bytes()).hexdigest()
    outputs['manifest.json']=(json.dumps(metadata,indent=2)+'\n').encode()
    header='// Generated original courtyard art by PrepareCourtyardHero.py.\n// clang-format off\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace nexora::showcase::courtyard_hero {\n'
    for name,key in [('sun_direction','sun_direction'),('sun_radiance','sun_radiance'),('key_radiance','key_radiance')]:
        values=','.join(f'{float(c):.6f}F' for c in source['sky'][key])
        header+=f'inline constexpr std::array<float,3> {name}{{{values}}};\n'
    header+='inline constexpr char mesh[] = R"NEXORA_ART('+mesh+')NEXORA_ART";\n'
    header+='inline constexpr char metadata[] = R"NEXORA_ART('+outputs['manifest.json'].decode()+')NEXORA_ART";\n'
    for name in ('stone-color','stone-normal','stone-orm','bronze-color','bronze-normal','bronze-orm','leaf','mote','sky'):
        data=outputs[name+'.rgba'];header+=f'inline constexpr std::array<std::uint8_t, {len(data)}> {name.replace("-","_")}{{\n'
        for start in range(0,len(data),32):header+='  '+','.join(str(x) for x in data[start:start+32])+',\n'
        header+='};\n'
    header+='}\n// clang-format on\n'
    return outputs,header

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    outputs,header=generate();files={CONTENT/name:data for name,data in outputs.items()};files[ROOT/'Apps/Showcase/CourtyardHero.h']=header.encode()
    for path,data in files.items():
        if args.check:
            if not path.is_file() or path.read_bytes()!=data:raise SystemExit(f'Original courtyard artifact differs: {path}')
        else:path.write_bytes(data)
