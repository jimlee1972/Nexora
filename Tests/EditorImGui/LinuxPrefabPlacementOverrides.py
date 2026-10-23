#!/usr/bin/env python3
"""Review and explicitly revert a persisted instance through actual native controls."""
import argparse
import difflib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput
from LinuxNativeScenePreview import scene_pixels, scene_region_pixels


def require(value, message):
    if not value:
        raise RuntimeError(message)


def files(root):
    return {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--selected', action='store_true')
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-instance-overrides-'))
    root, expected_file = scratch / 'project', scratch / 'retained.scene'
    subprocess.run([args.fixture, '--write-native-selected-fixture' if args.selected else '--write-native-fixture',
                    str(root), str(expected_file)], check=True)
    selected_fields = []
    selected_properties = None
    if args.selected:
        selected_fields = [int(value) for value in Path(str(expected_file) + '.rows').read_text().splitlines()]
        selected_properties = Path(str(expected_file) + '.properties').read_bytes()
        require(len(selected_fields) == 3 and len(set(selected_fields)) == 3,
                'Independent field-row oracle is incomplete')
    scene = root / '.nexora/scenes/Main.scene'
    original, retained = scene.read_bytes(), expected_file.read_bytes()
    require(original != retained and b'prefab-placement ' in original,
            'Fixture did not persist actual property overrides and instance bindings')
    runtime_header = next(line for line in original.splitlines() if line.startswith(b'NEXORA_SCENE 3 '))
    require(runtime_header.split()[-1] == b'1', 'Named Scene marker fixture must have exactly one entity')
    sources = {p: data for p, data in files(root).items() if 'prefabs' in p.parts}
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    passed = False
    case_deadline = time.monotonic() + 90
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / 'state'),
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')

        def deadline(seconds=10):
            return min(case_deadline, time.monotonic() + seconds)

        def wait(predicate, message, expected=None):
            until = deadline()
            while time.monotonic() < until:
                require(process.poll() is None, 'Native host exited before ' + message)
                if predicate():
                    return
                driver.drain(min(.05, max(0, until - time.monotonic())))
            print(bytes(driver.stderr).decode(errors='replace'), flush=True)
            if expected is not None:
                print(''.join(difflib.unified_diff(expected.decode().splitlines(keepends=True),
                      scene.read_text().splitlines(keepends=True), fromfile='expected fixture',
                      tofile='saved fixture')), flush=True)
            capture = os.environ.get('NEXORA_NATIVE_ACCEPTANCE_FAILURE_IMAGE')
            if capture and shutil.which('import'):
                subprocess.run(['import', '-display', display, '-window', window, capture],
                               env=env, check=False)
            raise RuntimeError(message)

        def click(name):
            # Refresh positioning only; send one physical down/up at the actual item.
            until = deadline()
            value = driver.wait(lambda v: v.get('focused') == '1' and
                                          v.get('buttons') == '000' and name in v,
                                'Named override control was not focused/rendered/released: ' + name, until)
            for _ in range(3):
                point = tuple(map(round, map(float, value[name].split(','))))
                driver.pointer_move(*point, until)
                value = driver.wait(lambda v: v.get('focused') == '1' and
                                              v.get('buttons') == '000' and name in v,
                                    'Named override control disappeared before one click: ' + name, until)
                if tuple(map(round, map(float, value[name].split(',')))) == point:
                    break
            else:
                raise RuntimeError('Named override geometry did not settle before one click: ' + name)
            driver.pointer_button(1, True, until)
            driver.pointer_button(1, False, until)

        def key(chord):
            until = deadline()
            if chord in ('ctrl+s', 'ctrl+z'):
                driver.chord(chord, until)
                return
            # Redo and modal Escape already have copied physical key observations.
            parts = chord.split('+')
            driver.wait(lambda v: v.get('focused') == '1' and v.get('keys') == '000000000'
                                  and v.get('rebase_keys') == '00',
                        'Override shortcut was not focused/released', until)
            for down, sequence in ((True, parts), (False, reversed(parts))):
                for part in sequence:
                    action = 'keydown' if down else 'keyup'
                    if part == 'ctrl':
                        driver.raw(action, 'Control_L', deadline=until, minimum_wait=.2)
                        driver.wait(lambda v: v.get('keys', '')[:1] == str(int(down)),
                                    'GUI did not observe override Control phase', until)
                    else:
                        index = {'Escape': 0, 'y': 1}[part]
                        driver.raw(action, part, deadline=until, minimum_wait=.2)
                        driver.wait(lambda v: v.get('rebase_keys', '')[index:index + 1] == str(int(down)),
                                    'GUI did not observe override key phase: ' + part, until)

        event_offset = 0

        def pending_events():
            return list(re.finditer(rb'^prefab instance [^\n]*\n', driver.stderr[event_offset:], re.M))

        def take_event(message):
            nonlocal event_offset
            until = deadline()
            driver.wait(lambda _: bool(pending_events()), message, until)
            events = pending_events()
            require(len(events) == 1, 'One native control emitted multiple instance results')
            event = events[0]
            event_offset += event.end()
            return event.group().decode().strip()

        def modal_confirm(window):
            # The fixed-size confirmation is centered in the initial1280x720 viewport.
            # Observe Confirm's left padding below the title bar; title blue must not count
            # as button geometry, and underlying Scene gizmos must not count as consent.
            points = []

            def presented():
                pixels = scene_region_pixels(display, int(window), (412, 310, 1, 120))
                ys = [310 + index * 3 for index, (red, green, blue) in
                      enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3]))
                      if blue >= 105 and blue >= red + 35 and blue >= green + 10]
                if len(ys) >= 3:
                    points[:] = [412, (min(ys) + max(ys)) // 2]
                    return True
                return False
            wait(presented, 'Explicit revert confirmation was not presented')
            return points

        whole_y = None

        def selected_row_y(window, ordinal):
            # Observe the first checkbox's dark-blue frame below the action buttons.
            # Neutral table borders/header text cannot satisfy this color admission.
            points = []

            def presented():
                groups, run = [], []
                for top in range(240, 640, 100):
                    pixels = scene_region_pixels(display, int(window), (1018, top, 1, 100))
                    for offset, (red, green, blue) in enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3])):
                        y = top + offset * 3
                        if y > whole_y + 45 and blue >= 45 and blue >= red + 20 and blue >= green + 10:
                            run.append(y)
                        elif run:
                            if len(run) >= 3:
                                groups.append(run[len(run) // 2])
                            run = []
                if run and len(run) >= 3:
                    groups.append(run[len(run) // 2])
                if len(groups) > ordinal:
                    points[:] = [groups[ordinal]]
                    return True
                return False
            wait(presented, 'Captured property checkbox was not presented')
            print(f'Property checkbox{ordinal} presented at y{points[0]}', flush=True)
            return points[0]

        def select_row(ordinal):
            nonlocal whole_y
            value = driver.wait(lambda v: 'override_revert' in v,
                                'Whole-revert control was not rendered above property rows', deadline())
            whole_y = float(value['override_revert'].split(',')[1])
            selected_row_y(window, ordinal)
            click('override_row_' + str(ordinal))

        for launch_number, read_only in enumerate((False, True), 1):
            before_launch = files(root)
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only, binary=True)
            driver = NativeSceneInput(process, args.xdotool, env, '', scratch, launch_number,
                                      minimum_wait=.2, phase_wait=.2)
            event_offset = 0
            window = wait_for_window(args.xdotool, env)
            driver.window = str(window)
            driver.raw('windowfocus', '--sync', window, deadline=deadline())
            selection_y = 199 if read_only else 182
            wait(lambda: scene_pixels(display, int(window), (518, selection_y, 4, 4))[0],
                 'Scene selection control was not ready')
            # The saved fixture has one entity; source_node names its actual visible marker.
            click('source_node')
            driver.wait(lambda v: v.get('source_selected') == '1' and
                                  v.get('source_selection_count') == '1' and 'override_review' in v,
                        'Single fixture entity was not selected for override review', deadline())
            before_review = files(root)
            click('override_review')
            event = take_event('Actual review did not emit an owning result')
            require(re.fullmatch(r'prefab instance reviewed rows=[1-9][0-9]* retained=1 published=2', event),
                    'Review lost retained/current source identity: ' + event)
            require(files(root) == before_review, 'Read-only review wrote project/source files')
            if read_only:
                if args.selected:
                    select_row(selected_fields[0])
                click('override_selected' if args.selected else 'override_revert')
                require(not pending_events() and scene.read_bytes() == original,
                        'Read-only control authorized a revert')
            else:
                if args.selected:
                    # The fixture owns a public Core row oracle; the checkbox is observed in the native image.
                    select_row(selected_fields[0])
                click('override_selected' if args.selected else 'override_revert')
                key('Escape')
                require(not pending_events() and files(root) == before_review,
                        'Cancel mutated files or authorized revert')
                click('override_selected' if args.selected else 'override_revert')
                modal_confirm(window)
                click('override_confirm')
                event = take_event('Confirmed actual control did not revert')
                expected_event = ('prefab instance selected revert retained=1 rows=1' if args.selected
                                  else 'prefab instance reverted retained=1')
                require(event == expected_event and files(root) == before_review,
                        'Revert changed files before explicit Scene Save or used current source')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'Revert Scene Save lost exact retained bytes')
                key('ctrl+z')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'One Undo did not restore all local properties')
                key('ctrl+y')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'One Redo did not restore the entire revert')
                key('ctrl+z')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'Final Undo did not restore readonly fixture')
                if args.selected:
                    before_properties = files(root)
                    click('override_review')
                    event = take_event('Second actual property review did not complete')
                    require(re.fullmatch(r'prefab instance reviewed rows=[1-9][0-9]* retained=1 published=2', event)
                            and files(root) == before_properties,
                            'Refresh changed files or retained/current identity')
                    for ordinal in sorted(selected_fields[1:]):
                        select_row(ordinal)
                    click('override_selected')
                    modal_confirm(window)
                    click('override_confirm')
                    event = take_event('Selected rotation/opaque consent did not complete')
                    require(event == 'prefab instance selected revert retained=1 rows=2' and
                            files(root) == before_properties,
                            'Selected property groups changed files before explicit Save')
                    key('ctrl+s')
                    wait(lambda: scene.read_bytes() == selected_properties,
                         'Selected rotation/opaque changed unselected Name/position or exact stored values',
                         expected=selected_properties)
                    key('ctrl+z')
                    key('ctrl+s')
                    wait(lambda: scene.read_bytes() == original,
                         'Selected property groups were not one complete Undo')
                    key('ctrl+y')
                    key('ctrl+s')
                    wait(lambda: scene.read_bytes() == selected_properties,
                         'Selected property groups were not one exact Redo')
                    key('ctrl+z')
                    key('ctrl+s')
                    wait(lambda: scene.read_bytes() == original,
                         'Selected property final Undo did not restore readonly original')

            request_window_close(window, env)
            until = deadline(15)
            while process.poll() is None and time.monotonic() < until:
                driver.drain(min(.05, max(0, until - time.monotonic())))
            require(process.poll() is not None, 'Native instance did not close within 15 seconds')
            _, captured = driver.finish(5)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native instance workflow failed or reported Vulkan errors')
            require(scene.read_bytes() == original and
                    {p: data for p, data in files(root).items() if 'prefabs' in p.parts} == sources,
                    'Instance workflow changed source archives or original scene')
            if read_only:
                require(files(root) == before_launch, 'Read-only reopen wrote project files')
            process = driver = None
        passed = True
        print('Native scoped override review, explicit revert, atomic Undo/Redo, Save and readonly passed')
    finally:
        if process is not None:
            if driver is not None:
                driver.abort()
            elif process.poll() is None:
                process.kill()
                process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        print(f'Native override fixtures retained (passed={passed}): {scratch}', flush=True)


if __name__ == '__main__':
    main()
