"""Rasterize the maintained approved SVG paths; authoring-only Pillow/CairoSVG dependencies."""
from pathlib import Path
import hashlib
import json
import struct
import zipfile
import cairosvg
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[2]
SIZES = (16, 24, 32, 48, 64, 128, 256)

def main():
    brand = ROOT/'Assets/Brand'
    source, out = brand/'Source', brand/'Export'
    out.mkdir(parents=True, exist_ok=True)
    for name,w,h in [('nexora-wordmark',2320,500),('nexora-editor-icon',1024,1024)]:
        cairosvg.svg2png(url=str(source/(name+'.svg')),write_to=str(out/(name+'.png')),output_width=w,output_height=h)
    payloads, entries, offset = [], [], 6+16*len(SIZES)
    for size in SIZES:
        target=out/f'nexora-editor-icon-{size}.png'
        cairosvg.svg2png(url=str(source/'nexora-editor-icon.svg'),write_to=str(target),output_width=size,output_height=size)
        data=target.read_bytes()
        entries.append(struct.pack('<BBBBHHII',size%256,size%256,0,0,1,32,len(data),offset))
        payloads.append(data)
        offset+=len(data)
    (out/'nexora-editor.ico').write_bytes(struct.pack('<HHH',0,1,len(SIZES))+b''.join(entries+payloads))
    sheet=Image.new('RGB',(1200,520),'black')
    logo=Image.open(out/'nexora-wordmark.png').convert('RGBA')
    logo.thumbnail((1040,230),Image.Resampling.LANCZOS)
    sheet.paste(logo,((1200-logo.width)//2,70),logo)
    icon=Image.open(out/'nexora-editor-icon.png').convert('RGBA').resize((160,160),Image.Resampling.LANCZOS)
    draw=ImageDraw.Draw(sheet)
    for x,color in [(370,'white'),(650,'#242424')]:
        draw.rectangle((x,330,x+180,510),fill=color)
        sheet.paste(icon,(x+10,340),icon)
    sheet.save(out/'nexora-brand-review.png')
    files=sorted([*source.glob('*.svg'),*out.glob('*.png'),*out.glob('*.ico')])
    (out/'manifest.json').write_text(json.dumps({str(p.relative_to(brand)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
    with zipfile.ZipFile(out/'Nexora-Brand.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in [*files,out/'manifest.json']:z.write(p,str(p.relative_to(brand)))
if __name__=='__main__':main()
