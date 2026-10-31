#!/usr/bin/env python3
"""Run the real Zig diagnostic in an exported generic Player; preserve every attempt."""
import argparse
import array
import ctypes as C
import ctypes.util
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import queue
import re
import shutil
import struct
import subprocess
import sys
import threading
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'Tools/TwoD'))
import StageHD2D as stager
spec = importlib.util.spec_from_file_location('export_game', ROOT/'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class XImage(C.Structure):
    _fields_ = [('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int), ('format', C.c_int),
                ('data', C.c_void_p), ('byte_order', C.c_int), ('bitmap_unit', C.c_int),
                ('bitmap_bit_order', C.c_int), ('bitmap_pad', C.c_int), ('depth', C.c_int),
                ('bytes_per_line', C.c_int), ('bits_per_pixel', C.c_int),
                ('red_mask', C.c_ulong), ('green_mask', C.c_ulong), ('blue_mask', C.c_ulong)]


class NativeWindow:
    def __init__(self, display):
        self.x = C.CDLL(ctypes.util.find_library('X11'))
        self.x.XOpenDisplay.argtypes = [C.c_char_p]; self.x.XOpenDisplay.restype = C.c_void_p
        self.display = self.x.XOpenDisplay(display.encode())
        if not self.display: raise RuntimeError('X11 capture connection unavailable')
        self.x.XDefaultRootWindow.argtypes = [C.c_void_p]; self.x.XDefaultRootWindow.restype = C.c_ulong
        self.root = self.x.XDefaultRootWindow(self.display)
        self.x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
        self.x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
        self.x.XFree.argtypes = [C.c_void_p]
        self.x.XSync.argtypes = [C.c_void_p, C.c_int]
        self.x.XGetGeometry.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint)]
        self.x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
        self.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_ulong, C.c_int]
        self.x.XGetImage.restype = C.POINTER(XImage)
        self.x.XDestroyImage.argtypes = [C.POINTER(XImage)]
        self.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
        self.x.XCloseDisplay.argtypes = [C.c_void_p]
        self.window = 0

    def find(self):
        root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
        if not self.x.XQueryTree(self.display, self.root, C.byref(root), C.byref(parent), C.byref(children), C.byref(count)):
            raise RuntimeError('X11 window tree query failed')
        try:
            for index in range(count.value):
                name = C.c_void_p()
                if self.x.XFetchName(self.display, children[index], C.byref(name)) and name.value:
                    try:
                        if C.string_at(name).decode('utf-8', 'replace') == 'DEV ONLY HD2D Zig':
                            self.window = children[index]; return
                    finally: self.x.XFree(name)
        finally:
            if children: self.x.XFree(children)
        raise RuntimeError('native diagnostic window was not found')

    def geometry(self):
        root, x, y = C.c_ulong(), C.c_int(), C.c_int()
        width, height, border, depth = C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
        if not self.x.XGetGeometry(self.display, self.window, C.byref(root), C.byref(x), C.byref(y), C.byref(width), C.byref(height), C.byref(border), C.byref(depth)):
            raise RuntimeError('native diagnostic geometry unavailable')
        child = C.c_ulong()
        self.x.XTranslateCoordinates(self.display, self.window, self.root, 0, 0, C.byref(x), C.byref(y), C.byref(child))
        return width.value, height.value, x.value, y.value

    def click_ui_scale(self, dpi):
        library = ctypes.util.find_library('Xtst')
        if not library: raise RuntimeError('XTest is required for the synthetic native input case')
        xt = C.CDLL(library)
        xt.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_ulong]
        xt.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        width, _, x, y = self.geometry()
        self.x.XSetInputFocus(self.display, self.window, 2, 0)
        assert xt.XTestFakeMotionEvent(self.display, -1, x + round(width * 0.625 - 3.5 * dpi), y + round(111 * dpi), 0)
        self.x.XSync(self.display, 0)
        # Focus transitions deliberately invalidate input in the Player. Admit a neutral
        # focused interval before pressing, rather than weakening that lifecycle protection.
        time.sleep(0.12)
        assert xt.XTestFakeButtonEvent(self.display, 1, 1, 0)
        self.x.XSync(self.display, 0)
        time.sleep(0.08)
        assert xt.XTestFakeButtonEvent(self.display, 1, 0, 0)
        self.x.XSync(self.display, 0)

    def capture(self, path):
        width, height, _, _ = self.geometry()
        self.x.XSync(self.display, 0)
        image = self.x.XGetImage(self.display, self.window, 0, 0, width, height, C.c_ulong(-1), 2)
        if not image: raise RuntimeError('actual native window readback failed')
        try:
            value = image.contents
            if value.bits_per_pixel != 32: raise RuntimeError('unsupported X11 readback pixel packing')
            words = array.array('I'); words.frombytes(C.string_at(value.data, value.bytes_per_line * height))
            if value.byte_order != (0 if sys.byteorder == 'little' else 1): words.byteswap()
            masks = [value.red_mask, value.green_mask, value.blue_mask]
            shifts = [(mask & -mask).bit_length() - 1 for mask in masks]
            rgb = bytearray(width * height * 3)
            stride = value.bytes_per_line // 4
            for y in range(height):
                for x in range(width):
                    pixel = words[y * stride + x]
                    for c in range(3): rgb[(y * width + x) * 3 + c] = (pixel & masks[c]) >> shifts[c]
        finally: self.x.XDestroyImage(image)
        def chunk(kind, data): return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        rows = b''.join(b'\0' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
        path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                         + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
        world = rgb[(height // 3) * width * 3:]
        triples = zip(world[0::3], world[1::3], world[2::3])
        gray = warm = blue = 0
        for r, g, b in triples:
            gray += max(r, g, b) - min(r, g, b) <= 2
            warm += r > g * 1.2 and r > b * 1.4 and r > 60
            blue += b > r * 1.3 and b > g * 1.1 and b > 60
        return dict(width=width, height=height, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                    grayscale_fraction=gray/(len(world)//3), warm_pixels=warm, blue_pixels=blue,
                    dynamic_range=max(world[0::3])-min(world[0::3]))

    def close(self): self.x.XCloseDisplay(self.display)


def run_case(package, app, native, directory, capture_display=None):
    directory.mkdir()
    environment = os.environ.copy()
    if sys.platform.startswith('linux'): environment['LD_LIBRARY_PATH'] = str(package) + os.pathsep + environment.get('LD_LIBRARY_PATH', '')
    backend = 'vulkan' if sys.platform.startswith('linux') else 'dx12' if sys.platform == 'win32' else 'metal'
    command = [str(package/app.name), '--run-2d-project', str(package/'game.nx2game'), '--frames', '144',
               '--save-directory', str(directory/'saves')]
    if native: command += ['--native', '--gpu-timing', '--backend', backend]
    stderr_lines, events, captures = [], queue.Queue(), {}
    process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
    def read_errors():
        for line in process.stderr:
            stderr_lines.append(line)
            if 'HD2D UI READY' in line or 'HD2D CAPTURE ' in line: events.put(line)
    reader = threading.Thread(target=read_errors); reader.start()
    window = NativeWindow(capture_display) if capture_display else None
    deadline = time.monotonic() + 90
    try:
        while process.poll() is None:
            if time.monotonic() > deadline: raise TimeoutError('bounded Zig diagnostic deadline')
            try: event = events.get(timeout=0.05)
            except queue.Empty: continue
            if not window: continue
            window.find()
            if 'HD2D UI READY' in event:
                dpi = float(re.search(r'dpi=([0-9.]+)', event).group(1))
                window.click_ui_scale(dpi)
            else:
                kind = 'map' if 'CAPTURE map' in event else 'scene'
                captures[kind] = window.capture(directory/f'{kind}.png')
        stdout = process.stdout.read(); reader.join(timeout=3)
        stderr = ''.join(stderr_lines)
        (directory/'stdout.log').write_text(stdout, encoding='utf-8')
        (directory/'stderr.log').write_text(stderr, encoding='utf-8')
        (directory/'command.json').write_text(json.dumps(command, indent=2)+'\n', encoding='utf-8')
        assert process.returncode == 0, f'Player exit {process.returncode}: {stderr}'
        report = json.loads(stdout)
        assert report['status'] == 'PASS' and report['project_mode'] and report['allocations_balanced']
        assert report['ui_frames'] == 144 and report['asset_active_bytes'] == report['asset_staging_bytes'] == 0
        assert 'HD2D STOP PASS phase_mask=4095 reloads=2' in stderr and 'FAIL' not in stderr
        assert 'Validation Error' not in stderr
        for phase in range(12): assert f'HD2D PHASE {phase} ' in stderr
        if native:
            assert report['native_frames'] == 144 and report['effects_completed_frame'] > 0 and report['effects_peak_bytes'] > 0
            assert report['effects_scene_bytes'] == report['effects_shadow_bytes'] == report['effects_bloom_bytes'] == report['effects_upload_capacity_bytes'] == 0
            assert 'features=7' in stderr
            if report['effects_timing_source']:
                assert 'timing_mask=15 features=7' in stderr
                assert report['effects_timing_mask'] == 15
            else:
                assert report['effects_timing_status'] in (3, 4)
        else:
            assert report['native_frames'] == report['effects_timing_source'] == 0 and 'features=0' in stderr
            assert report['effects_world_ms'] is None
        if window:
            assert stderr.count('HD2D UI ACTION 6 camera_unchanged=1') == 1, 'native click or UI-scale camera isolation failed'
            assert set(captures) == {'scene', 'map'}, 'both actual Zig window captures are required'
            assert captures['scene']['warm_pixels'] > 500 and captures['scene']['blue_pixels'] > 500
            assert captures['map']['grayscale_fraction'] > 0.98 and captures['map']['dynamic_range'] > 80
            assert captures['scene']['sha256'] != captures['map']['sha256']
        evidence = dict(status='PASS', native=native, backend=backend if native else 'none', report=report,
                        captures=captures, input='synthetic XTest PASS' if window else 'NOT_RUN',
                        physical_display='NOT_RUN', target_pc_performance='NOT_RUN', capture_holds_are_not_fps_workload=True)
        (directory/'acceptance.json').write_text(json.dumps(evidence, indent=2)+'\n', encoding='utf-8')
        return evidence
    finally:
        if process.poll() is None: process.kill(); process.wait(timeout=3)
        reader.join(timeout=3)
        if window: window.close()
        # Retain logs even when capture/assertion/timeout fails.
        if not (directory/'stderr.log').exists():
            (directory/'stderr.log').write_text(''.join(stderr_lines), encoding='utf-8')
            (directory/'stdout.log').write_text(process.stdout.read(), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    args.app, args.module = args.app.resolve(), args.module.resolve()
    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    attempt = None
    for index in range(1, 10000):
        candidate = args.evidence_dir/f'attempt-{index:04d}'
        try: candidate.mkdir(); attempt = candidate; break
        except FileExistsError: pass
    if attempt is None: raise RuntimeError('attempt evidence capacity exhausted')
    server = display = None
    if args.native and sys.platform.startswith('linux'):
        sys.path.insert(0, str(ROOT/'Tests/Showcase'))
        from LinuxVirtualDisplaySmoke import start_xvfb, unavailable
        xvfb = shutil.which('Xvfb')
        if not xvfb: return unavailable('Xvfb unavailable for actual Zig HD2D rendering')
        server, display = start_xvfb(xvfb, '3200x1440x24')
        if not display: server.terminate(); server.wait(timeout=3); return unavailable('Xvfb could not establish a display')
        os.environ['DISPLAY'] = display
    try:
        sizes = [(1920, 1080), (2520, 1080), (800, 600), (961, 541)] if args.native else [(1920, 1080)]
        results = []
        for index, (width, height) in enumerate(sizes):
            package = attempt/f'package-{index}'
            stager.stage(args.app, args.module, package, width, height, development=True)
            inventory = json.loads((package/'inventory.json').read_text(encoding='utf-8'))
            assert 'Licenses/Noto-CJK-OFL.txt' in inventory['files']
            for name, digest in inventory['files'].items():
                assert hashlib.sha256((package/name).read_bytes()).hexdigest() == digest
            results.append(run_case(package, args.app, args.native, attempt/f'run-{index}', display if index == 0 else None))
        (attempt/'summary.json').write_text(json.dumps(dict(status='PASS', cases=len(results), native=args.native,
            physical_pc='NOT_RUN', engineering_acceptance_requires_current_head_ci=True), indent=2)+'\n')
        print(f'PASS real Zig HD2D Player cases={len(results)} native={args.native} evidence={attempt}')
        return 0
    finally:
        if server: server.terminate(); server.wait(timeout=3)


if __name__ == '__main__':
    raise SystemExit(main())
