#!/usr/bin/env python3
"""Real FreeType raster/cmap, corpus accounting and failure publication checks."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
try:
    from fontTools.fontBuilder import FontBuilder
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    import PIL
except ImportError:
    print('SKIP: install Tools/TwoD/font-requirements.txt for portable font bake tests')
    sys.exit(77)
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('font_bake',ROOT/'Tools/TwoD/BakeFont.py')
baker=importlib.util.module_from_spec(spec);spec.loader.exec_module(baker)
with tempfile.TemporaryDirectory(prefix='nexora-font-bake-') as temporary:
    root=Path(temporary);font=root/'fixture.ttf';corpus=root/'corpus.txt'
    builder=FontBuilder(1000,isTTF=True)
    codes=[32,65,0x4e2d,0x1f680]
    names=['.notdef']+[f'uni{c:X}' for c in codes]
    builder.setupGlyphOrder(names);builder.setupCharacterMap(dict(zip(codes,names[1:])))
    glyphs={}
    for name in names:
        pen=TTGlyphPen(None)
        if name not in ('.notdef','uni20'):
            pen.moveTo((50,0));pen.lineTo((500,0));pen.lineTo((500,700));pen.lineTo((50,700));pen.closePath()
        glyphs[name]=pen.glyph()
    builder.setupGlyf(glyphs);builder.setupHorizontalMetrics({n:(600,0) for n in names})
    builder.setupHorizontalHeader(ascent=800,descent=-200)
    builder.setupNameTable(dict(familyName='Nexora Raster Test',styleName='Regular',uniqueFontIdentifier='NexoraRasterTest',fullName='Nexora Raster Test',psName='NexoraRasterTest'))
    builder.setupOS2(sTypoAscender=800,sTypoDescender=-200,usWinAscent=800,usWinDescent=200)
    builder.setupPost();builder.save(font)
    corpus.write_text('A中🚀\n A',encoding='utf-8')
    output=root/'font.nxfont';report=root/'report.json'
    result=baker.bake(font,[corpus],output,report,32,128)
    data=output.read_bytes();magic,w,h,pixels,line,count=struct.unpack_from('<8sIIffI',data)
    assert magic==b'NXFONT1\0' and count==4 and result['missing']==[]
    assert len(data)==28+40*count+w*h*4 and line>0
    assert any(data[28+40*count+3::4]),'no alpha raster'
    assert [struct.unpack_from('<I',data,28+i*40)[0] for i in range(count)]==codes
    try:baker.bake(font,[corpus],output,root/'again.json',32,128)
    except ValueError:pass
    else:raise AssertionError('overwrote font')
    assert output.read_bytes()==data
    corpus.write_text('雪',encoding='utf-8')
    try:baker.bake(font,[corpus],root/'missing.nxfont',root/'missing.json',32,128)
    except ValueError:pass
    else:raise AssertionError('missing glyph admitted')
    assert not (root/'missing.nxfont').exists() and 'U+96EA' in (root/'missing.json').read_text()
    corpus.write_bytes(b'\xff')
    try:baker.bake(font,[corpus],root/'invalid.nxfont',root/'invalid.json',32,128)
    except UnicodeDecodeError:pass
    else:raise AssertionError('invalid UTF-8 admitted')
    assert not (root/'invalid.nxfont').exists()
    corpus.write_text('A中🚀',encoding='utf-8')
    a=baker.bake(font,[corpus],root/'audit.nxfont',root/'audit.json',32,128,True)
    assert a['status']=='COVERED' and not (root/'audit.nxfont').exists()
print('PASS: portable real raster, non-BMP cmap, full corpus, budgets and rejected publication')
