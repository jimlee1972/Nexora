"""Native Play camera/mesh pixels, pause/step/stop, and unchanged Editor scene."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window
from LinuxNativeScenePreview import scene_region_pixels


def main():
    parser = argparse.ArgumentParser()
    for argument in ('editor', 'xvfb', 'xdotool'):
        parser.add_argument('--' + argument, required=True)
    parser.add_argument('--module')
    parser.add_argument('--input-routing', action='store_true')
    parser.add_argument('--project-bindings', action='store_true')
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='nexora-game-view-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-game-state-'))
    (root / 'Content').mkdir()
    (root / '.nexora/scenes').mkdir(parents=True)
    (root / 'project.nexora').write_text('schema=1\nname=Game View Acceptance\n')
    (root / '.nexora/workspace').write_text('schema=1\n')
    if args.project_bindings:
        if not args.input_routing:
            parser.error('--project-bindings requires --input-routing')
        (root / '.nexora/play-input.ini').write_text(
            'schema=1\nleft=A,Left\nright=B,Right\nforward=W,Up\nbackward=S,Down\n'
            'action=Space,None\nprimary=MouseLeft,None\nsecondary=MouseRight,None\n'
            'sprint=LeftShift,RightShift\nmodifier=LeftControl,RightControl\n')
    (root / 'Content/Triangle.obj').write_text(
        'v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 1 1\nf 1//1 2//1 3//1\n')
    (root / 'Content/Triangle.obj.meta').write_text(
        'schema=1\nuuid=12345678-9abc-def0-fedc-ba9876543210\ntype=.obj\n')
    scene = root / '.nexora/scenes/Main.scene'
    scene.write_text('NEXORA_EDITOR_SCENE 2\nnode 10 0 Camera\nnode 20 0 Triangle\nworld\n'
        'NEXORA_SCENE 3 "Game scene" 0 2\n'
        '10 0 0 0 5 0 0 0 1 1 1 1 1 0 0 60 0.1 1000 1 0 0\n'
        '20 0 0 0 0 0 0 0 1 1 1 1 0 0 1 60 0.1 1000 1 12751791000609863510 0\n')
    library_argument = []
    if args.module:
        filename = Path(args.module).name
        shutil.copy2(args.module, root / 'Content' / filename)
        library_argument = [f'--gameplay-library=Content/{filename}']
    baseline = scene.read_bytes()
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    editor = None
    captured = b''
    try:
        if display is None:
            raise RuntimeError('Xvfb startup failed')
        env = os.environ.copy()
        env['DISPLAY'] = display
        env['XDG_STATE_HOME'] = str(state)
        editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
            '--native-scene-preview', '--frames=10000', f'--recent-projects={state / "recent"}', *library_argument],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        window = int(wait_for_window(args.xdotool, env))

        def send(*arguments):
            subprocess.run([args.xdotool, *map(str, arguments)], env=env, check=True)

        def wait_view(label):
            nonlocal captured
            deadline = time.monotonic() + 12
            expression = label + rb': (\d+) (\d+) (\d+) (\d+)'
            while time.monotonic() < deadline:
                if select.select([editor.stderr], [], [], 0.1)[0]:
                    captured += os.read(editor.stderr.fileno(), 4096)
                matches = list(re.finditer(expression, captured))
                if matches:
                    return tuple(map(int, matches[-1].groups()))
            raise RuntimeError(f'No {label!r}: {captured!r}')

        wait_view(b'native scene viewport')
        send('windowfocus', window)
        send('key', '--delay', '80', 'F5')
        viewport = wait_view(b'native game viewport')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            pixels = scene_region_pixels(display, window, viewport)
            # Bright warm Lambertian geometry, surrounded by the dark Game canvas.
            bright = sum(r > 100 and g > 90 and b > 75 and r > b + 10
                         for r, g, b in zip(pixels[::3], pixels[1::3], pixels[2::3]))
            if bright > 20:
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('Play camera did not rasterize imported OBJ pixels')
        if args.module and not args.input_routing:
            first = scene_region_pixels(display, window, viewport)
            time.sleep(0.6)
            if scene_region_pixels(display, window, viewport) == first:
                raise RuntimeError('Loaded gameplay fixed callback did not move the native mesh')
        if args.input_routing:
            movement_key = 'b' if args.project_bindings else 'd'
            first = scene_region_pixels(display, window, viewport)
            send('keydown', movement_key)
            time.sleep(0.3)
            send('keyup', movement_key)
            if scene_region_pixels(display, window, viewport) != first:
                raise RuntimeError('Uncaptured Game input reached the module')
            x, y, width, height = viewport
            send('mousemove', '--window', window, x + width // 2, y + height // 2)
            send('click', 1)
            time.sleep(0.2)
            if args.project_bindings:
                send('keydown', 'd')
                time.sleep(0.25)
                send('keyup', 'd')
                if scene_region_pixels(display, window, viewport) != first:
                    raise RuntimeError('Persisted bindings still admitted old D movement')
            send('keydown', movement_key)
            time.sleep(0.45)
            if scene_region_pixels(display, window, viewport) == first:
                raise RuntimeError('Captured bound movement did not drive the native mesh')
            send('key', '--delay', '80', 'Escape')
            time.sleep(0.2)
            released = scene_region_pixels(display, window, viewport)
            time.sleep(0.3)
            if scene_region_pixels(display, window, viewport) != released:
                raise RuntimeError('Escape retained a held movement key')
            send('keyup', movement_key)
            send('click', 1)
            time.sleep(0.2)
        send('key', '--delay', '80', 'F6')
        time.sleep(0.3)
        paused = scene_region_pixels(display, window, viewport)
        time.sleep(0.2)
        if scene_region_pixels(display, window, viewport) != paused:
            raise RuntimeError('Paused Game View was not stable')
        send('key', '--delay', '80', 'F10')
        time.sleep(0.3)
        send('key', '--delay', '80', 'F5')
        time.sleep(0.3)
        if scene.read_bytes() != baseline:
            raise RuntimeError('Play commands modified the authored scene')
        request_window_close(str(window), env)
        _, stderr = editor.communicate(timeout=15)
        captured += stderr
        if editor.returncode != 0 or b'pie_steps=1' not in captured:
            raise RuntimeError(f'Game View shutdown/step failed: {captured!r}')
        editor = None
        if args.project_bindings:
            bindings_file = root / '.nexora/play-input.ini'
            retained = bindings_file.read_bytes()
            captured = b''
            editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
                '--read-only', '--native-scene-preview', '--frames=10000',
                f'--recent-projects={state / "recent"}'], env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            window = int(wait_for_window(args.xdotool, env))
            wait_view(b'native scene viewport')
            send('windowfocus', window)
            send('key', '--delay', '80', 'F5')
            viewport = wait_view(b'native game viewport')
            time.sleep(0.25)
            first = scene_region_pixels(display, window, viewport)
            x, y, width, height = viewport
            send('mousemove', '--window', window, x + width // 2, y + height // 2)
            send('click', 1)
            time.sleep(0.2)
            send('keydown', 'd')
            time.sleep(0.25)
            send('keyup', 'd')
            if scene_region_pixels(display, window, viewport) != first:
                raise RuntimeError('Read-only reopen restored old D instead of project bindings')
            send('keydown', 'b')
            time.sleep(0.45)
            if scene_region_pixels(display, window, viewport) == first:
                raise RuntimeError('Read-only reopen did not load B movement and saved gameplay module')
            send('keyup', 'b')
            send('key', '--delay', '80', 'Escape')
            send('key', '--delay', '80', 'F5')
            request_window_close(str(window), env)
            _, stderr = editor.communicate(timeout=15)
            if editor.returncode != 0 or b'ignored input settings' in captured + stderr:
                raise RuntimeError(f'Reopened input project failed: {stderr!r}')
            editor = None
            if bindings_file.read_bytes() != retained or scene.read_bytes() != baseline:
                raise RuntimeError('Read-only input reopen changed settings or authored scene')
        if args.module and not args.input_routing:
            expected = f'schema=1\nlibrary=Content/{filename}\n'
            if (root / '.nexora/gameplay-library.ini').read_text() != expected:
                raise RuntimeError('Gameplay library path was not persisted')
            captured = b''
            editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
                '--native-scene-preview', '--frames=10000',
                f'--recent-projects={state / "recent"}'], env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            window = int(wait_for_window(args.xdotool, env))
            time.sleep(0.3)
            send('windowfocus', window)
            send('key', '--delay', '80', 'F5')
            viewport = wait_view(b'native game viewport')
            time.sleep(0.2)
            first = scene_region_pixels(display, window, viewport)
            time.sleep(0.6)
            if scene_region_pixels(display, window, viewport) == first:
                raise RuntimeError('Reopened project did not load its persisted gameplay library')
            send('key', '--delay', '80', 'F5')
            time.sleep(0.2)
            request_window_close(str(window), env)
            _, stderr = editor.communicate(timeout=15)
            if editor.returncode != 0 or scene.read_bytes() != baseline:
                raise RuntimeError(f'Reopened gameplay project failed: {stderr!r}')
            editor = None
            settings = root / '.nexora/gameplay-library.ini'
            corrupt = 'schema=999\nlibrary=Content/old.so\n'
            settings.write_text(corrupt)
            checked = subprocess.run([args.editor, f'--project={root}', '--graphical',
                '--frames=4', f'--recent-projects={state / "recent"}'], env=env,
                capture_output=True, timeout=15)
            if checked.returncode != 0 or settings.read_text() != corrupt:
                raise RuntimeError('Opening corrupt settings overwrote the saved file')
            settings.write_text(expected)
            checked = subprocess.run([args.editor, f'--project={root}', '--graphical',
                '--read-only', '--gameplay-library=', '--frames=4',
                f'--recent-projects={state / "recent"}'], env=env,
                capture_output=True, timeout=15)
            if checked.returncode != 0 or settings.read_text() != expected:
                raise RuntimeError('Read-only CLI override wrote project settings')
        return 0
    finally:
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.wait(timeout=5)
        xvfb.terminate()
        xvfb.wait(timeout=5)
        shutil.rmtree(root, ignore_errors=True)
        shutil.rmtree(state, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
