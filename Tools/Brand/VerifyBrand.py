"""Stdlib production admission of RGBA edges, actual ICO payloads, and ZIP hashes."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile
import zlib
SIZES=(16,24,32,48,64,128,256)
def require(ok,message):
    if not ok:raise ValueError(message)
def png(data):
    require(data[:8]==b'\x89PNG\r\n\x1a\n','Expected PNG')
    pos,compressed=8,bytearray()
    while pos<len(data):
        n=struct.unpack_from('>I',data,pos)[0];kind=data[pos+4:pos+8];body=data[pos+8:pos+8+n]
        require(zlib.crc32(kind+body)&0xffffffff==struct.unpack_from('>I',data,pos+8+n)[0],'PNG CRC')
        if kind==b'IHDR':
            w,h,bits,color,_,_,interlace=struct.unpack('>IIBBBBB',body)
            require((bits,color,interlace)==(8,6,0),'Expected straight RGBA8')
        if kind==b'IDAT':compressed.extend(body)
        pos+=n+12
    raw=zlib.decompress(compressed);stride=w*4;previous=bytearray(stride);rows=[]
    require(len(raw)==(stride+1)*h,'PNG length')
    for y in range(h):
        method=raw[y*(stride+1)];row=bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
        for x in range(stride):
            a=row[x-4] if x>=4 else 0;b=previous[x];c=previous[x-4] if x>=4 else 0
            if method==0:add=0
            elif method==1:add=a
            elif method==2:add=b
            elif method==3:add=(a+b)//2
            elif method==4:
                p=a+b-c;d=(abs(p-a),abs(p-b),abs(p-c));add=(a,b,c)[d.index(min(d))]
            else:raise ValueError('PNG filter')
            row[x]=(row[x]+add)&255
        rows.append(row);previous=row
    return w,h,b''.join(rows)
def alpha_check(data,icon=False):
    w,h,rgba=png(data);alpha=rgba[3::4]
    require(min(alpha)==0 and max(alpha)==255 and any(0<a<255 for a in alpha),'Missing transparency/AA')
    border=alpha[:w]+alpha[-w:]+alpha[::w]+alpha[w-1::w]
    require(not any(border),'Foreground touches bounds')
    if icon:
        for i,a in enumerate(alpha):
            if a>=32:
                require(all(abs(r-t)<=10 for r,t in zip(rgba[i*4:i*4+3],(71,217,242))),'Foreign color/black fringe')
        require(alpha[int(h*.8)*w+int(w*.5)]==0,'Closed n counter')
        require(sum(a>127 for a in alpha)<w*h*.45,'Opaque board used as icon')
    return w,h
def verify(brand):
    out=brand/'Export';alpha_check((out/'nexora-wordmark.png').read_bytes())
    alpha_check((out/'nexora-editor-icon.png').read_bytes(),True)
    ico=(out/'nexora-editor.ico').read_bytes()
    require(struct.unpack_from('<HHH',ico)==(0,1,len(SIZES)),'ICO header/count')
    end=6+16*len(SIZES)
    for i,size in enumerate(SIZES):
        w,h,_,_,planes,bits,n,offset=struct.unpack_from('<BBBBHHII',ico,6+i*16)
        require((w or 256,h or 256,planes,bits)==(size,size,1,32),'ICO directory')
        require(end<=offset and offset+n<=len(ico),'ICO payload range/overlap');end=offset+n
        require(alpha_check(ico[offset:end],True)==(size,size),'ICO actual dimensions')
        require(alpha_check((out/f'nexora-editor-icon-{size}.png').read_bytes(),True)==(size,size),'PNG size')
    with zipfile.ZipFile(out/'Nexora-Brand.zip') as archive:
        require(archive.testzip() is None,'ZIP damaged')
        for name,digest in json.loads((out/'manifest.json').read_text()).items():
            data=(brand/name).read_bytes()
            require(hashlib.sha256(data).hexdigest()==digest,'Asset hash')
            require(archive.read(name)==data,'Stale ZIP')
    print('PASS: transparent PNG, clean RGBA edges, seven actual ICO images, ZIP hashes')
if __name__=='__main__':verify(Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'Assets/Brand')
