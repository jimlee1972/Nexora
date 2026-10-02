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
    # Xvfb is explicitly started with a 24-bit TrueColor screen. XGetPixel removes byte-order,
    # stride and padding assumptions; the fixed visual's RGB masks are 0xff0000/0xff00/0xff.
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
    raw = bytearray()
    try:
        for y in range(height):
            raw.append(0)  # PNG filter None
            for x in range(width):
                value = x11.XGetPixel(image, x, y)
                raw.extend(((value >> 16) & 255, (value >> 8) & 255, value & 255))
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
            app = subprocess.Popen([executable, '--mode=interactive', '--backend=vulkan',
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
            deadline = time.monotonic() + 10
            while True:
                hub = screenshot(window,1280,720,output/'hub.png')
                if len(set(hub)) >= 8:  # Wait for rendered pixels, not merely the mapped black window.
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Showcase did not produce a visible frame')
                time.sleep(0.03)
            tool('key', '--window',window,'2')
            time.sleep(0.15)
            screenshot(window,1280,720,output/'rendering.png')
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
            tool('key','--window',window,'v','b','h','t','space','r')
            tool('windowsize',window,960,540)
            time.sleep(0.2)
            screenshot(window,960,540,output/'resized-hub.png')
            tool('windowclose',window)
            _, errors = app.communicate(timeout=10)
            assert app.returncode == 0, errors
            assert "Validation Error" not in errors and "SYNC-HAZARD" not in errors, errors
            evidence = json.loads(report.read_text())
            native, rooms = evidence['windowed_evidence'], evidence['runtime_rooms']
            assert native['executed'] and not native['backend_fallback']
            assert native['scene_draws'] > 10 and native['native_ui_draws'] > 10
            assert native['surface_acquires'] == native['surface_presents']
            assert native['resize_generations'] >= 1
            assert rooms['healthy'] and rooms['reloads'] == 1
            assert rooms['probe_runs'] > 13
            assert set(rooms['visited']) == {'hub','rendering','scene','input','gameplay','presentation','streaming','shipping'}
            assert set(rooms['visualized']) == set(rooms['visited'])
            assert rooms['tour']['enabled']
            assert markdown.is_file() and 'M12' in markdown.read_text()
            (output/'acceptance.json').write_text(json.dumps({
                'scope':'Linux Xvfb/lavapipe native interaction; no physical display or Windows claim',
                'room_controls':True,'screenshots':['hub.png','rendering.png','scene.png','input.png','gameplay.png','gameplay-geometry.png','presentation.png','streaming.png','shipping.png','presentation-blend.png','validation-lab.png','resized-hub.png'],
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
