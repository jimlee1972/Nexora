#!/usr/bin/env python3
"""Replay the retained CI failure through the capture gate without an X server."""
from pathlib import Path
import struct
import sys
import zlib

root=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(root/'Tests/Showcase'))
import LinuxShowcaseInteraction as gate


def raw(name):
    png=(Path(__file__).parent/(name+'.png')).read_bytes()
    offset=8
    data=b''
    while offset<len(png):
        size=struct.unpack('>I',png[offset:offset+4])[0]
        if png[offset+4:offset+8]==b'IDAT':data+=png[offset+8:offset+8+size]
        offset+=size+12
    image=zlib.decompress(data)
    assert len(image)==(1280*3+1)*720
    assert all(image[y*(1280*3+1)]==0 for y in range(720))
    return image


ui,stale,off,clean=map(raw,['courtyard-ui','courtyard-wide','courtyard-bloom-off','courtyard-bloom-restored'])
assert stale!=ui and off==clean
assert gate.navigation_overlay_present(stale,1280,720,ui)
assert not gate.navigation_overlay_present(clean,1280,720,ui)
original=(gate.screenshot,gate.time.monotonic,gate.time.sleep)
try:
    clock=[0.]
    gate.time.monotonic=lambda:clock[0]
    gate.time.sleep=lambda duration:clock.__setitem__(0,clock[0]+duration)
    gate.screenshot=lambda *args:stale if clock[0]<3 else clean
    accepted=gate.settled_screenshot(0,1280,720,Path('replay.png'),ui,ui_reference=ui)
    assert accepted==clean and clock[0]>=3.3
    print('PASS: stable changed UI rejected; repeated clean frame accepted')
    clock[0]=0
    gate.screenshot=lambda *args:stale
    try:
        gate.settled_screenshot(0,1280,720,Path('never-clean.png'),ui,ui_reference=ui)
        raise AssertionError('Stable UI was accepted')
    except AssertionError as error:
        assert 'Native clean frame did not settle' in str(error)
        assert 15<=clock[0]<15.2
    print('PASS: UI that never disappears fails at the original 15-second deadline')
finally:
    gate.screenshot,gate.time.monotonic,gate.time.sleep=original
