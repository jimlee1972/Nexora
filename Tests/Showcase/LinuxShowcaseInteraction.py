#!/usr/bin/env python3
"""Exercise native room controls and retain screenshots/reports from a real Xvfb window."""
import ctypes
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from LinuxVirtualDisplaySmoke import start_xvfb, unavailable


def screenshot(window: int, width: int, height: int, output: Path) -> bytes:
    # Xvfb is explicitly started with a 24-bit TrueColor screen. XImage metadata bounds the bulk decode;
    # native XGetPixel cross-checks it, with the original per-pixel path retained as a fallback.
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                             ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
    x11.XGetImage.restype = ctypes.c_void_p
    x11.XGetPixel.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    x11.XGetPixel.restype = ctypes.c_ulong
    x11.XDestroyImage.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError('Cannot open capture display')
    image = x11.XGetImage(display, window, 0, 0, width, height, ctypes.c_ulong(-1).value, 2)
    if not image:
        x11.XCloseDisplay(display)
        raise RuntimeError('Cannot capture Showcase window')
    class XImagePrefix(ctypes.Structure):
        _fields_ = [('width',ctypes.c_int),('height',ctypes.c_int),('xoffset',ctypes.c_int),
          ('format',ctypes.c_int),('data',ctypes.c_void_p),('byte_order',ctypes.c_int),
          ('bitmap_unit',ctypes.c_int),('bitmap_bit_order',ctypes.c_int),('bitmap_pad',ctypes.c_int),
          ('depth',ctypes.c_int),('bytes_per_line',ctypes.c_int),('bits_per_pixel',ctypes.c_int),
          ('red_mask',ctypes.c_ulong),('green_mask',ctypes.c_ulong),('blue_mask',ctypes.c_ulong)]
    raw = bytearray()
    try:
        metadata = ctypes.cast(image,ctypes.POINTER(XImagePrefix)).contents
        fast = (metadata.width == width and metadata.height == height and metadata.xoffset == 0
                and metadata.format == 2 and metadata.depth == 24 and metadata.bits_per_pixel == 32
                and metadata.byte_order in (0,1) and metadata.red_mask == 0xff0000
                and metadata.green_mask == 0xff00 and metadata.blue_mask == 0xff
                and width*4 <= metadata.bytes_per_line <= width*4+256
                and metadata.bytes_per_line*height <= 64*1024*1024 and metadata.data)
        if fast:
            data = ctypes.string_at(metadata.data,metadata.bytes_per_line*height)
            red,green,blue = (2,1,0) if metadata.byte_order == 0 else (1,2,3)
            for y in range(height):
                row=data[y*metadata.bytes_per_line:y*metadata.bytes_per_line+width*4]
                line=bytearray(width*3+1) # PNG filter None, followed by RGB.
                line[1::3]=row[red::4];line[2::3]=row[green::4];line[3::3]=row[blue::4]
                raw.extend(line)
            # Cross-check actual native XGetPixel values, independently of the bulk byte decoding.
            for y in (0,height//3,height//2,height-1):
                for x in (0,width//3,width//2,width-1):
                    value=x11.XGetPixel(image,x,y)
                    offset=y*(width*3+1)+1+x*3
                    assert raw[offset:offset+3] == bytes(((value>>16)&255,(value>>8)&255,value&255))
        else:
            for y in range(height):
                raw.append(0)
                for x in range(width):
                    value=x11.XGetPixel(image,x,y)
                    raw.extend(((value>>16)&255,(value>>8)&255,value&255))
    finally:
        x11.XDestroyImage(image)
        x11.XCloseDisplay(display)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    output.write_bytes(png)
    return bytes(raw)


def settled_screenshot(window, width, height, output, reference=None):
    # Wait for several identical presented images: changing diagnostic text must not
    # be mistaken for the first clean frame after the asynchronous F4 event.
    started=time.monotonic()
    deadline=started+15
    previous=None
    repeats=0
    while True:
        captured=screenshot(window,width,height,output)
        repeats=repeats+1 if captured==previous else 0
        if repeats>=2 and time.monotonic()-started>=2 and captured!=reference:return captured
        if time.monotonic()>=deadline:raise AssertionError(f'Native clean frame did not settle: {output.name}')
        previous=captured
        time.sleep(0.15)


def compared_screenshot(window, width, height, output, reference, equal):
    deadline = time.monotonic() + 5
    while True:
        captured = screenshot(window,width,height,output)
        if (captured == reference) == equal:
            return captured
        if time.monotonic() >= deadline:
            raise AssertionError(f'Native image did not settle within five seconds: {output.name}')
        time.sleep(0.1)


def request_window_close(window: int, display_name: str) -> None:
    """Request a normal close even when Xvfb has no window manager."""
    x11 = ctypes.CDLL('libX11.so.6')

    class ClientMessage(ctypes.Structure):
        _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
                    ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
                    ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong),
                    ('format', ctypes.c_int), ('data', ctypes.c_long * 5)]

    class Event(ctypes.Union):
        _fields_ = [('client', ClientMessage), ('padding', ctypes.c_long * 24)]

    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                               ctypes.c_long, ctypes.POINTER(Event)]
    x11.XSendEvent.restype = ctypes.c_int
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(display_name.encode())
    if not display:
        raise RuntimeError('Cannot open display for Showcase close request')
    try:
        event = Event()
        event.client.type = 33  # ClientMessage
        event.client.display = display
        event.client.window = window
        event.client.message_type = x11.XInternAtom(display, b'WM_PROTOCOLS', 0)
        event.client.format = 32
        event.client.data[0] = x11.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
        if not x11.XSendEvent(display, window, 0, 0, ctypes.byref(event)):
            raise RuntimeError('XSendEvent rejected Showcase close request')
        x11.XFlush(display)
    finally:
        x11.XCloseDisplay(display)


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--allow-unavailable-plugin"):
        raise SystemExit('usage: LinuxShowcaseInteraction.py NEXORA_SHOWCASE')
    xvfb, xdotool = shutil.which('Xvfb'), shutil.which('xdotool')
    if not xvfb or not xdotool:
        return unavailable('Xvfb or xdotool is not installed')
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='nexora-showcase-controls-') as temporary:
        output = Path(os.environ.get('NEXORA_SHOWCASE_EVIDENCE_DIR', temporary))
        output.mkdir(parents=True, exist_ok=True)
        server, display = start_xvfb(xvfb, '1280x720x24')
        app = None
        try:
            if display is None:
                return unavailable('Xvfb did not start')
            environment = os.environ.copy()
            environment['DISPLAY'] = display
            report, markdown = output / 'interactive.json', output / 'interactive.md'
            app = subprocess.Popen([executable, '--mode=interactive', '--scene=hub', '--backend=vulkan',
                '--gameplay-module=static', '--no-reload', f'--report={report}',
                f'--markdown={markdown}'], env=environment, stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE, text=True, cwd=temporary)
            def tool(*args):
                return subprocess.run([xdotool, *map(str,args)], env=environment, check=True,
                    capture_output=True, text=True, timeout=5).stdout.strip()
            deadline = time.monotonic() + 10
            window = None
            while time.monotonic() < deadline:
                if app.poll() is not None:
                    raise RuntimeError(app.stderr.read())
                result = subprocess.run([xdotool, 'search', '--name', '^Nexora Showcase$'],
                    env=environment, capture_output=True, text=True, timeout=3)
                if result.returncode == 0 and result.stdout.strip():
                    window = int(result.stdout.splitlines()[0]); break
                time.sleep(0.03)
            if window is None:
                raise RuntimeError('Showcase window was not published')
            tool('windowfocus', '--sync', window)
            # Capture runs on this Python process, so use the same isolated DISPLAY for Xlib.
            os.environ['DISPLAY'] = display
            deadline = time.monotonic() + 25
            while True:
                if app.poll() is not None:
                    raise RuntimeError(app.stderr.read())
                hub = screenshot(window,1280,720,output/'hub.png')
                if len(set(hub)) >= 8:  # Wait for rendered pixels, not merely the mapped black window.
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Showcase did not produce a visible frame')
                time.sleep(0.03)
            tool('key', '--window',window,'2')
            time.sleep(0.15)
            cubes = screenshot(window,1280,720,output/'rendering.png')
            tool('key', '--window',window,'p')
            time.sleep(0.15)
            quad = screenshot(window,1280,720,output/'rendering-quad.png')
            tool('key', '--window',window,'p')
            time.sleep(0.15)
            triangle = screenshot(window,1280,720,output/'rendering-triangle.png')
            assert cubes != quad and quad != triangle, 'Primitive controls did not change native pixels'
            tool('key', '--window',window,'p')
            tool('key', '--window',window,'3','e','u','p','F5','F3','Tab','r','F3')
            time.sleep(0.2)
            scene = screenshot(window,1280,720,output/'scene.png')
            tool('key','--window',window,'4','l')
            time.sleep(0.15)
            screenshot(window,1280,720,output/'input.png')
            tool('key','--window',window,'5')
            tool('keydown','--window',window,'d')
            time.sleep(0.2)
            tool('keyup','--window',window,'d')
            tool('key','--window',window,'c','g')
            tool('mousemove','--window',window,400,400)
            tool('mousedown',1); tool('mousemove','--window',window,460,420);tool('mouseup',1)
            time.sleep(0.2)
            gameplay = screenshot(window,1280,720,output/'gameplay.png')
            tool('key','--window',window,'F1')
            time.sleep(0.1)
            screenshot(window,1280,720,output/'gameplay-geometry.png')
            tool('key','--window',window,'F1')
            assert hub != scene and scene != gameplay, 'Room controls did not change native pixels'
            for key, room in [('6','presentation'), ('7','streaming'), ('8','shipping')]:
                tool('key','--window',window,key)
                time.sleep(0.15)
                screenshot(window,1280,720,output/f'{room}.png')
                if room == 'presentation':
                    tool('key','--window',window,'j','F1')
                    time.sleep(0.15)
                    screenshot(window,1280,720,output/'presentation-blend.png')
                    tool('key','--window',window,'F1')
            tool('key', '--window', window, '9', 'space', 'r')
            time.sleep(0.3)
            courtyard_ui = screenshot(window,1280,720,output/'courtyard-ui.png')
            tool('key', '--window', window, 'F4')
            time.sleep(0.2)
            courtyard_wide = settled_screenshot(window,1280,720,output/'courtyard-wide.png',courtyard_ui)
            assert courtyard_ui != courtyard_wide, 'Screenshot mode did not remove native UI'
            tool('key','--window',window,'k')
            time.sleep(0.2)
            assert compared_screenshot(window,1280,720,output/'courtyard-bloom-off.png',courtyard_wide,False) != courtyard_wide, 'Bloom did not change native pixels'
            tool('key','--window',window,'k')
            time.sleep(0.2)
            assert compared_screenshot(window,1280,720,output/'courtyard-bloom-restored.png',courtyard_wide,True) == courtyard_wide, 'Bloom restoration changed pixels'

            tool('key', '--window', window, 'F6')
            time.sleep(0.2)
            courtyard_unshadowed = compared_screenshot(window,1280,720,output/'courtyard-shadow-off.png',courtyard_wide,False)
            assert courtyard_unshadowed != courtyard_wide, 'Directional shadows did not change native pixels'
            tool('key', '--window', window, 'F6')
            time.sleep(0.2)
            assert compared_screenshot(window,1280,720,output/'courtyard-shadow-restored.png',courtyard_wide,True) == courtyard_wide
            tool('key', '--window', window, 'g')
            time.sleep(0.2)
            courtyard_neutral = compared_screenshot(window,1280,720,output/'courtyard-neutral.png',courtyard_wide,False)
            assert courtyard_neutral != courtyard_wide, 'Stylized tone did not change native pixels'
            tool('key', '--window', window, 'g')
            time.sleep(0.2)
            assert compared_screenshot(window,1280,720,output/'courtyard-styled-restored.png',courtyard_wide,True) == courtyard_wide
            tool('key', '--window', window, 'e')
            time.sleep(0.2)
            courtyard_exposed = compared_screenshot(window,1280,720,output/'courtyard-exposure.png',courtyard_wide,False)
            assert courtyard_exposed != courtyard_wide, 'HDR exposure did not change native pixels'
            tool('key', '--window', window, 'e')
            time.sleep(0.2)
            courtyard_exposure_restored = compared_screenshot(window,1280,720,output/'courtyard-exposure-restored.png',courtyard_wide,True)
            assert courtyard_exposure_restored == courtyard_wide, 'Exposure restoration changed the fixed shot'
            tool('key', '--window', window, 'o')
            time.sleep(0.2)
            courtyard_direct = compared_screenshot(window,1280,720,output/'courtyard-direct.png',courtyard_wide,False)
            assert courtyard_direct != courtyard_wide, 'IBL comparison did not change native pixels'
            tool('key', '--window', window, 'o')
            time.sleep(0.2)
            courtyard_ibl_restored = compared_screenshot(window,1280,720,output/'courtyard-ibl-restored.png',courtyard_wide,True)
            assert courtyard_ibl_restored == courtyard_wide, 'IBL restoration changed the fixed shot'
            tool('key', '--window', window, 'p')
            time.sleep(0.2)
            courtyard_lambert = compared_screenshot(window,1280,720,output/'courtyard-lambert.png',courtyard_wide,False)
            assert courtyard_lambert != courtyard_wide, 'Material comparison did not change native pixels'
            tool('key', '--window', window, 'p')
            time.sleep(0.2)
            restored_pbr = compared_screenshot(window,1280,720,output/'courtyard-pbr-restored.png',courtyard_wide,True)
            assert restored_pbr == courtyard_wide, 'PBR material restoration changed the fixed shot'

            for key,effect in [('n','wind'),('m','transmission'),('k','bloom'),('j','depth-of-field'),('v','planar-reflection'),('u','crystal-transparency'),('F7','atmosphere'),('F8','refraction')]:
                tool('key','--window',window,key)
                compared_screenshot(window,1280,720,output/f'courtyard-{effect}-off.png',courtyard_wide,False)
                tool('key','--window',window,key)
                compared_screenshot(window,1280,720,output/f'courtyard-{effect}-restored.png',courtyard_wide,True)
            tool('key', '--window', window, 'b')
            time.sleep(0.2)
            courtyard_material = compared_screenshot(window,1280,720,output/'courtyard-material.png',courtyard_wide,False)
            tool('key', '--window', window, 'b')
            time.sleep(0.2)
            courtyard_motion = compared_screenshot(window,1280,720,output/'courtyard-motion.png',courtyard_material,False)
            assert courtyard_wide != courtyard_material and courtyard_material != courtyard_motion
            tool('key', '--window', window, 'b')
            time.sleep(0.2)
            replay = compared_screenshot(window,1280,720,output/'courtyard-wide-replay.png',courtyard_wide,True)
            assert replay == courtyard_wide, 'Fixed wide camera did not reproduce native pixels'

            # Change actual GPU work while effects are paused, then require exact restoration.
            tool('key','--window',window,'q')
            compared_screenshot(window,1280,720,output/'courtyard-quality-high.png',courtyard_wide,False)
            tool('key','--window',window,'q')
            compared_screenshot(window,1280,720,output/'courtyard-quality-basic.png',courtyard_wide,False)
            tool('key','--window',window,'q')
            compared_screenshot(window,1280,720,output/'courtyard-quality-standard.png',courtyard_wide,True)

            tool('key','--window',window,'c')
            free_start=compared_screenshot(window,1280,720,output/'courtyard-free-camera.png',courtyard_wide,False)
            tool('keydown','--window',window,'w')
            # Prove a presented movement frame before releasing a held key. A short
            # timed press can begin and end between slow software-GPU submissions.
            try:
                compared_screenshot(window,1280,720,output/'courtyard-free-moved.png',free_start,False)
            finally:
                tool('keyup','--window',window,'w')
            settled_screenshot(window,1280,720,output/'courtyard-free-moved.png',free_start)
            tool('key','--window',window,'r')
            compared_screenshot(window,1280,720,output/'courtyard-free-restored.png',courtyard_wide,True)
            tool('key','--window',window,'Return')
            time.sleep(0.3)
            activated=compared_screenshot(window,1280,720,output/'courtyard-activated.png',courtyard_wide,False)
            assert activated != courtyard_wide, 'Activation did not draw particles'
            tool('key','--window',window,'F9')
            compared_screenshot(window,1280,720,output/'courtyard-crystal-light-off.png',activated,False)
            tool('key','--window',window,'F9')
            compared_screenshot(window,1280,720,output/'courtyard-crystal-light-restored.png',activated,True)
            time.sleep(0.3)
            assert compared_screenshot(window,1280,720,output/'courtyard-paused.png',activated,True) == activated, 'Paused animation advanced'
            tool('key','--window',window,'space')
            time.sleep(0.6)
            tool('key','--window',window,'space')
            time.sleep(0.3)
            moved=compared_screenshot(window,1280,720,output/'courtyard-animated.png',activated,False)
            assert moved != activated, 'Wind/particles did not animate'
            tool('key','--window',window,'r')
            time.sleep(0.3)
            assert compared_screenshot(window,1280,720,output/'courtyard-animation-replay.png',activated,True) == activated, 'Animation replay differs'
            tool('key','--window',window,'Return')
            time.sleep(0.3)
            assert compared_screenshot(window,1280,720,output/'courtyard-inactive.png',courtyard_wide,True) == courtyard_wide

            tool('key', '--window', window, 'F4')
            # The previous matrix interaction selected M1. Drive real M5/M6/M12 errors.
            tool('key','--window',window,'F3','Tab','Tab','Tab','Tab','i','r','i','r','Tab','i','r','x','Next')
            time.sleep(0.15)
            screenshot(window,1280,720,output/'validation-lab.png')
            exported = Path(temporary) / 'showcase-lab.json'
            lab = json.loads(exported.read_text())
            plugin = lab['integration_probes']['probes'][6]
            if len(sys.argv) == 2:
                assert plugin['status'] == 'PASS', plugin
                assert any(metric['name'] == 'output.registered' and metric['value'] == 'false'
                           for metric in plugin['metrics']), plugin
            else:
                assert plugin['status'] == 'UNSUPPORTED', plugin
            shutil.copy2(exported, output/'lab-export.json')
            shutil.copy2(Path(temporary)/'showcase-lab.md', output/'lab-export.md')
            tool('key','--window',window,'Tab','Tab','Tab','Tab','Tab','Tab','i','r','F3')
            tool('key','--window',window,'1','v','b','h','t','space','r')
            tool('windowsize',window,960,540)
            time.sleep(0.2)
            screenshot(window,960,540,output/'resized-hub.png')
            request_window_close(window, display)
            _, errors = app.communicate(timeout=10)
            assert app.returncode == 0, errors
            assert "Validation Error" not in errors and "SYNC-HAZARD" not in errors, errors
            evidence = json.loads(report.read_text())
            native, rooms = evidence['windowed_evidence'], evidence['runtime_rooms']
            assert native['executed'] and not native['backend_fallback']
            assert native['scene_draws'] > 10 and native['native_ui_draws'] > 10
            assert native['surface_acquires'] == native['surface_presents'] + native['surface_recoverable_presents'], native
            assert native['resize_generations'] >= 1
            assert rooms['healthy'] and rooms['reloads'] == 1
            assert rooms['probe_runs'] > 13
            assert set(rooms['visited']) == {'hub','rendering','scene','input','gameplay','presentation','streaming','shipping','courtyard'}
            assert set(rooms['visualized']) == set(rooms['visited'])
            assert rooms['tour']['enabled']
            assert markdown.is_file() and 'M12' in markdown.read_text()
            (output/'acceptance.json').write_text(json.dumps({
                'scope':'Linux Xvfb/lavapipe native interaction; no physical display or Windows claim',
                'courtyard_free_camera':True,'courtyard_living_replay':True,'courtyard_transparency_comparison':True,'courtyard_atmosphere_comparison':True,'courtyard_refraction_comparison':True,'courtyard_crystal_light_comparison':True,'courtyard_planar_reflection_comparison':True,'courtyard_wind_comparison':True,'courtyard_transmission_comparison':True,'courtyard_bloom_comparison':True,'courtyard_fixed_shots':True,'courtyard_screenshot_mode':True,'courtyard_ibl_comparison':True,'courtyard_hdr_exposure':True,'courtyard_shadow_comparison':True,'courtyard_tone_comparison':True,'courtyard_material_comparison':True,
                'courtyard':rooms['courtyard'],
                'quality_cycle_restores_pixels':True,'room_controls':True,'screenshots':['hub.png','rendering.png','rendering-quad.png','rendering-triangle.png','scene.png','input.png','gameplay.png','gameplay-geometry.png','presentation.png','streaming.png','shipping.png','presentation-blend.png','validation-lab.png','resized-hub.png','courtyard-ui.png','courtyard-wide.png','courtyard-bloom-off.png','courtyard-bloom-restored.png','courtyard-shadow-off.png','courtyard-shadow-restored.png','courtyard-neutral.png','courtyard-styled-restored.png','courtyard-exposure.png','courtyard-exposure-restored.png','courtyard-direct.png','courtyard-ibl-restored.png','courtyard-lambert.png','courtyard-pbr-restored.png','courtyard-material.png','courtyard-motion.png','courtyard-wide-replay.png','courtyard-activated.png','courtyard-paused.png','courtyard-animated.png','courtyard-animation-replay.png','courtyard-inactive.png','courtyard-free-camera.png','courtyard-free-moved.png','courtyard-free-restored.png','courtyard-wind-off.png','courtyard-wind-restored.png','courtyard-transmission-off.png','courtyard-transmission-restored.png'],
                'windowed_evidence':native,'build':evidence['build']},indent=2)+'\n')
            print(json.dumps({'native':native,'visited':rooms['visited'],'evidence_directory':str(output)},indent=2))
            return 0
        finally:
            if app and app.poll() is None:
                app.terminate()
                try: app.wait(timeout=3)
                except subprocess.TimeoutExpired: app.kill();app.wait(timeout=3)
            server.terminate()
            server.wait(timeout=3)

if __name__ == '__main__':
    raise SystemExit(main())
