#!/usr/bin/env python3
"""Same-process native Play capture/pause/step/teardown and warmed RSS envelope."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import (physical_key_arguments, request_window_close,
                                    start_xvfb, wait_for_window)
from LinuxNativeScenePreview import scene_region_pixels

CYCLES = 32
WARMUP = 4
RSS_GROWTH_LIMIT = 32 * 1024 * 1024
OUTPUT_LIMIT = 4 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'module', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='nexora-play-soak-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-play-soak-state-'))
    (root / 'Content').mkdir()
    (root / '.nexora/scenes').mkdir(parents=True)
    (root / 'project.nexora').write_text('schema=1\nname=Native Play Soak\n')
    (root / '.nexora/workspace').write_text('schema=1\n')
    (root / 'Content/Triangle.obj').write_text(
        'v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 1 1\nf 1//1 2//1 3//1\n')
    (root / 'Content/Triangle.obj.meta').write_text(
        'schema=1\nuuid=12345678-9abc-def0-fedc-ba9876543210\ntype=.obj\n')
    scene = root / '.nexora/scenes/Main.scene'
    scene.write_text('NEXORA_EDITOR_SCENE 2\nnode 10 0 Camera\nnode 20 0 Triangle\nworld\n'
        'NEXORA_SCENE 3 "Game scene" 0 2\n'
        '10 0 0 0 5 0 0 0 1 1 1 1 1 0 0 60 0.1 1000 1 0 0\n'
        '20 0 0 0 0 0 0 0 1 1 1 1 0 0 1 60 0.1 1000 1 12751791000609863510 0\n')
    baseline = {p: p.read_bytes() for p in (scene, root / 'Content/Triangle.obj',
                                           root / 'Content/Triangle.obj.meta')}
    layout = 'schema=1\n'
    for name, panel in [('Project', 'project'), ('Hierarchy', 'hierarchy'),
                        ('Inspector', 'inspector'), ('Console', 'console'),
                        ('Content', 'content'), ('Scene', 'scene'), ('Game', 'game'),
                        ('Profiler', 'profiler'), ('Build', 'build')]:
        layout += f'[Window][{name}###nexora.{panel}]\n'
        layout += ('Pos=640,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'game' else
                   'Pos=0,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'scene' else
                   'Pos=0,700\nSize=100,20\nCollapsed=1\n\n')
    (root / '.nexora/editor-layout.ini').write_text(layout)
    module = Path(args.module).name
    shutil.copy2(args.module, root / 'Content' / module)
    server, display = start_xvfb(args.xvfb, '1280x720x24')
    editor = None
    captured = bytearray()
    rss = []
    started = time.monotonic()
    cycle = 0
    passed = False
    collected = False
    source_baseline = None
    source_viewport = None
    last_source_pixels = None
    failure = None
    stdout = tempfile.TemporaryFile()
    try:
        if display is None:
            raise RuntimeError('Xvfb did not start')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(state),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')
        editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
            '--native-scene-preview', '--frames=100000', f'--recent-projects={state / "recent"}',
            f'--gameplay-library=Content/{module}'], env=env, stdout=stdout, stderr=subprocess.PIPE)
        window = int(wait_for_window(args.xdotool, env))

        def pump(duration=.02):
            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                if select.select([editor.stderr], [], [], max(0, deadline - time.monotonic()))[0]:
                    data = os.read(editor.stderr.fileno(), 65536)
                    if not data:
                        break
                    captured.extend(data)
                    if len(captured) > OUTPUT_LIMIT:
                        raise RuntimeError('Native soak output exceeded its 4 MiB budget')
                else:
                    break
            if b'soak lifecycle stop neutral=0' in captured or b'soak lifecycle destroy neutral=0' in captured:
                raise RuntimeError('Teardown lost the provider/World or retained captured input')
            if b'pages=0' in captured:
                raise RuntimeError('Touched host allocation was corrupted before destruction')
            if editor.poll() is not None:
                raise RuntimeError(f'Editor exited during cycle {cycle}: {editor.returncode}')

        def wait(predicate, message, timeout=3, deadline=None):
            deadline = time.monotonic() + timeout if deadline is None else deadline
            while time.monotonic() < deadline:
                pump()
                if predicate() and time.monotonic() < deadline:
                    return
            raise RuntimeError(message)

        def send(*arguments, deadline=None):
            requested = arguments
            arguments = physical_key_arguments(arguments, env)
            with (state / 'input.jsonl').open('a', encoding='utf-8') as log:
                log.write(json.dumps(dict(time=time.monotonic(), cycle=cycle,
                                          input=requested, native_input=arguments)) + '\n')
            remaining = 5 if deadline is None else deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError('Native soak input deadline expired before sending')
            subprocess.run([args.xdotool, *map(str, arguments)], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=remaining)

        def fixed():
            return re.findall(rb'soak fixed tick=(\d+) neutral=([01])', captured)

        def pixels():
            views = re.findall(rb'native game viewport: (\d+) (\d+) (\d+) (\d+)', captured)
            if not views:
                raise RuntimeError('Native Game viewport unavailable')
            return scene_region_pixels(display, window, tuple(map(int, views[-1])))

        def source_pixels():
            nonlocal source_viewport, last_source_pixels
            views = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)\r?\n', captured)
            if not views:
                raise RuntimeError('Native Scene complete viewport record unavailable')
            source_viewport = tuple(map(int, views[-1]))
            last_source_pixels = scene_region_pixels(display, window, source_viewport)
            return last_source_pixels

        def unchanged(reference, label):
            pump(.1)
            if pixels() != reference:
                raise RuntimeError(label)

        def gui():
            records = re.findall(rb'^scene merge ui ([^\n]*)\n', captured, re.M)
            return dict(part.split('=', 1) for part in records[-1].decode().split()) if records else {}

        def pointer_at(value, x, y):
            return tuple(map(float, value.get('pointer', 'nan,nan').split(','))) == (x, y)

        def acquire(cx, cy, deadline):
            # One physical acquisition, with its native pointer/button/seed phases observed by
            # the real GUI. These copies do not claim provider delivery; the original module
            # applied/pixel and fixed-neutral assertions below remain the result oracles.
            send('mousemove', '--window', window, cx, cy, deadline=deadline)
            wait(lambda: pointer_at(gui(), cx, cy) and gui().get('buttons') == '000'
                 and gui().get('game_hovered') == '1'
                 and gui().get('game_capture_admitted') == '1',
                 'Game canvas did not admit acquisition at its actual hover target', deadline=deadline)
            previous = int(gui()['game_capture_requests'])
            send('mousedown', 1, deadline=deadline)
            wait(lambda: gui().get('buttons') == '100' and gui().get('game_focused') == '1'
                 and int(gui().get('game_capture_requests', '-1')) == previous + 1,
                 'One Game canvas mouse-down was not admitted exactly once', deadline=deadline)
            send('mouseup', 1, deadline=deadline)
            wait(lambda: gui().get('buttons') == '000' and gui().get('game_clicked') == '0'
                 and gui().get('game_focused') == '1'
                 and int(gui().get('game_capture_requests', '-1')) == previous + 1,
                 'Game canvas mouse-up lost capture or repeated admission', deadline=deadline)
            released_frame = int(gui()['frame'])
            send('mousemove', '--window', window, cx + 1, cy + 1, deadline=deadline)
            wait(lambda: pointer_at(gui(), cx + 1, cy + 1) and gui().get('game_focused') == '1'
                 and int(gui().get('game_capture_requests', '-1')) == previous + 1
                 and int(gui().get('frame', '-1')) > released_frame,
                 'A separate post-acquisition seed frame was not observed', deadline=deadline)
            return previous + 1

        wait(lambda: re.search(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)\r?\n', captured),
             'Native scene startup was not ready', 12)
        # A viewport log precedes presentation. Focus/neutral input acknowledgement and
        # actual readback stability share the original three-second readiness bound.
        startup_deadline = time.monotonic() + 3
        send('windowfocus', window, deadline=startup_deadline)
        send('mousemove', '--sync', '--window', window, 630, 10, deadline=startup_deadline)
        stable_source = None
        stable_since = None

        def source_ready():
            nonlocal stable_source, stable_since, source_baseline
            observations = re.findall(rb'^scene merge ui ([^\n]*)\n', captured, re.M)
            observation = dict(part.split('=', 1) for part in observations[-1].decode().split()) \
                if observations else {}
            if (observation.get('focused') != '1' or observation.get('buttons') != '000' or
                    tuple(map(float, observation.get('pointer', 'nan,nan').split(','))) != (630, 10)):
                stable_source = stable_since = None
                return False
            actual = source_pixels()
            blue = sum(b >= 105 and b >= r + 35 and b >= g + 10
                       for r, g, b in zip(actual[::3], actual[1::3], actual[2::3]))
            current = (source_viewport, actual)
            now = time.monotonic()
            if blue < 20:
                stable_source = stable_since = None
            elif current != stable_source:
                stable_source, stable_since = current, now
            elif now - stable_since >= .3:
                # The same readback supplies readiness and the exact pre-Start baseline.
                source_baseline = actual
                return True
            return False

        wait(source_ready, 'Native source Scene focus/neutral input or rasterization was not stable',
             deadline=startup_deadline)
        baseline_viewport = source_viewport
        fresh_pixels = None
        for cycle in range(1, CYCLES + 1):
            send('key', '--delay', '30', 'F5')
            wait(lambda: captured.count(b'soak lifecycle start fresh=1 neutral=1') == cycle,
                 'Fresh neutral module startup was not observed')
            wait(lambda: len(re.findall(rb'native game viewport:', captured)) >= cycle,
                 'Native Game viewport did not return')
            if fresh_pixels is None:
                def fresh_ready():
                    nonlocal fresh_pixels
                    actual = pixels()
                    if sum(r > 100 and g > 90 and b > 75 and r > b + 10
                           for r, g, b in zip(actual[::3], actual[1::3], actual[2::3])) > 20:
                        fresh_pixels = actual
                        return True
                    return False

                wait(fresh_ready, 'Fresh Play geometry was not rasterized')
            else:
                wait(lambda: pixels() == fresh_pixels, 'New Play clone retained an old rendered pose')
            if source_pixels() != source_baseline or source_viewport != baseline_viewport:
                raise RuntimeError('Play startup changed native authoring Scene pixels or viewport')
            view = re.findall(rb'native game viewport: (\d+) (\d+) (\d+) (\d+)', captured)[-1]
            x, y, width, height = map(int, view)
            cx, cy = x + width // 2, y + height // 2
            send('mousemove', '--window', window, cx - 20, cy + 20)
            send('click', 4)
            unchanged(fresh_pixels, 'Uncaptured input changed Game pixels')
            motion_deadline = time.monotonic() + 3
            capture_request = acquire(cx, cy, motion_deadline)
            before = pixels()
            applied = captured.count(b'soak applied ')
            send('mousemove', '--window', window, cx + 41, cy - 19, deadline=motion_deadline)
            wait(lambda: pointer_at(gui(), cx + 41, cy - 19)
                 and gui().get('game_focused') == '1'
                 and int(gui().get('game_capture_requests', '-1')) == capture_request,
                 'Captured motion did not reach the actual focused Game canvas', deadline=motion_deadline)
            wait(lambda: captured.count(b'soak applied ') > applied,
                 'Captured motion was not copied', deadline=motion_deadline)
            wait(lambda: pixels() != before, 'Captured motion did not change native Game pixels',
                 deadline=motion_deadline)
            moved = pixels()
            unchanged(moved, 'An empty input batch replayed displacement')
            if source_pixels() != source_baseline:
                raise RuntimeError('Play motion changed native authoring Scene pixels')
            send('key', '--delay', '30', 'Escape')
            pump(.08)
            send('mousemove', '--window', window, cx - 20, cy + 20)
            send('click', 4)
            unchanged(moved, 'Escape did not revoke motion/wheel capture')
            send('key', '--delay', '30', 'F6')
            pump(.1)
            paused_ticks = len(fixed())
            pump(.1)
            if len(fixed()) != paused_ticks:
                raise RuntimeError('Pause did not stop fixed callbacks')
            paused = pixels()
            send('click', 1)
            send('click', 4)
            unchanged(paused, 'Paused capture admitted pointer input')
            send('key', '--delay', '30', 'F10')
            wait(lambda: len(fixed()) >= paused_ticks + 1, 'Manual Step did not invoke Fixed')
            pump(.1)
            if len(fixed()) != paused_ticks + 1 or fixed()[-1][1] != b'1' or pixels() != paused:
                raise RuntimeError('Manual Step was duplicated, non-neutral, or changed pixels')
            send('key', '--delay', '30', 'F6')
            wait(lambda: len(fixed()) > paused_ticks + 1, 'Resume did not restart fixed callbacks')
            unchanged(paused, 'Resume replayed captured input')
            stop_deadline = time.monotonic() + 3
            acquire(cx, cy, stop_deadline)
            send('mousedown', 8, deadline=stop_deadline)
            wait(lambda: fixed()[-1][1] == b'0', 'Stop fixture did not hold captured input',
                 deadline=stop_deadline)
            send('key', '--delay', '30', 'F5')
            wait(lambda: captured.count(b'soak lifecycle destroy neutral=1 pages=1') == cycle,
                 'Stop/Destroy did not complete while the provider and World were alive')
            send('mouseup', 8)
            pump(.1)
            if source_pixels() != source_baseline:
                raise RuntimeError('Stop changed native authoring Scene pixels')
            if any(path.read_bytes() != value for path, value in baseline.items()):
                raise RuntimeError('A Play lifecycle changed authoring bytes')
            status = Path(f'/proc/{editor.pid}/status').read_text()
            match = re.search(r'^VmRSS:\s+(\d+) kB$', status, re.MULTILINE)
            if not match:
                raise RuntimeError('Process RSS is unavailable')
            rss.append(int(match[1]) * 1024)
            if cycle > WARMUP and max(rss[WARMUP:]) - max(rss[WARMUP - 2:WARMUP]) > RSS_GROWTH_LIMIT:
                raise RuntimeError(f'Warmed RSS growth exceeded 32 MiB: {rss}')
        lifecycle = re.findall(rb'soak lifecycle (create|start|stop|destroy)', captured)
        if lifecycle != [stage for _ in range(CYCLES) for stage in (b'create', b'start', b'stop', b'destroy')]:
            raise RuntimeError(f'Unbalanced module lifecycle order: {lifecycle}')
        request_window_close(str(window), env)
        _, tail = editor.communicate(timeout=15)
        captured.extend(tail)
        collected = True
        if len(captured) > OUTPUT_LIMIT:
            raise RuntimeError('Native soak output exceeded its 4 MiB budget')
        stdout.seek(0)
        output = stdout.read(OUTPUT_LIMIT + 1)
        if len(output) > OUTPUT_LIMIT:
            raise RuntimeError('Native stdout exceeded its budget')
        if editor.returncode or b'pie_steps=1' not in captured:
            raise RuntimeError('Editor shutdown or final-session Step accounting failed')
        if re.search(rb'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+', output + captured):
            raise RuntimeError('Vulkan validation failed during soak')
        report = dict(cycles=CYCLES, warmup_cycles=WARMUP, module_touched_bytes=4 * 1024 * 1024,
            rss_scope='current_process_resident_set', rss_unit='bytes', rss_after_stop=rss,
            warm_baseline=max(rss[WARMUP - 2:WARMUP]), warmed_peak=max(rss[WARMUP:]),
            warmed_end=rss[-1], growth_limit=RSS_GROWTH_LIMIT,
            elapsed_seconds=round(time.monotonic() - started, 3), lifecycle_records=len(lifecycle),
            pixels_input_pause_step_discard='passed', source_pixels='unchanged', source_bytes='unchanged', validation_errors=0)
        print('Native Play soak evidence: ' + json.dumps(report, sort_keys=True))
        passed = True
        return 0
    except Exception as error:
        failure = str(error)
        raise RuntimeError(f'Native Play soak cycle {cycle}: {error}; tail={bytes(captured[-4096:])!r}') from error
    finally:
        if editor is not None and not collected:
            if editor.poll() is None:
                editor.kill()
            _, tail = editor.communicate(timeout=5)
            captured.extend(tail)
        (state / 'editor.stderr.log').write_bytes(captured)
        stdout.flush()
        if not passed:
            stdout.seek(0)
            with (state / 'editor.stdout.log').open('wb') as retained_stdout:
                shutil.copyfileobj(stdout, retained_stdout)
        stdout.close()
        server.terminate()
        server.communicate(timeout=5)
        if passed:
            shutil.rmtree(root, ignore_errors=True)
            shutil.rmtree(state, ignore_errors=True)
        else:
            evidence = dict(cycle=cycle, failure=failure, elapsed=time.monotonic() - started,
                            source_viewport=source_viewport, rss_after_stop=rss,
                            gui_observation=(re.findall(rb'^scene merge ui ([^\n]*)\n',
                                captured, re.M)[-1:] or [b''])[0].decode(errors='replace'),
                            reference_viewport=locals().get('baseline_viewport'),
                            pixel_format='RGB, center region, stride=3')
            if source_baseline is not None:
                (state / 'source-reference.rgb').write_bytes(bytes(source_baseline))
            if last_source_pixels is not None:
                (state / 'source-actual.rgb').write_bytes(bytes(last_source_pixels))
            if source_baseline is not None and last_source_pixels is not None:
                reference, actual = source_baseline, last_source_pixels
                changed = [index // 3 for index in range(0, min(len(reference), len(actual)), 3)
                           if reference[index:index + 3] != actual[index:index + 3]]
                evidence.update(reference_rgb_bytes=len(reference), actual_rgb_bytes=len(actual),
                                different_sample_pixels=len(changed) + abs(len(reference) - len(actual)) // 3,
                                sample_bbox=None)
                if source_viewport is not None:
                    x, y, width, height = source_viewport
                    left, top = max(x, x + width // 2 - 90), max(y, y + height // 2 - 110)
                    columns = len(range(left, min(x + width, x + width // 2 + 90), 3))
                    if changed and columns:
                        xs = [left + (index % columns) * 3 for index in changed]
                        ys = [top + (index // columns) * 3 for index in changed]
                        evidence['sample_bbox'] = [min(xs), min(ys), max(xs), max(ys)]
            (state / 'failure.json').write_text(json.dumps(evidence, indent=2) + '\n')
            print(f'Native Play soak fixtures retained: {root} {state}', flush=True)


if __name__ == '__main__':
    raise SystemExit(main())
