#!/usr/bin/env python3
"""Exercise actual native prefab create/edit/save/variant/reopen and source isolation."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import struct
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window


def require(value, message):
    if not value:
        raise RuntimeError(message)


def asset(path):
    raw = path.read_bytes()
    require(raw[:8] == b'NXPFAB1\n' and len(raw) >= 72, 'Actual wrapped prefab header missing')
    count, nested, size = struct.unpack_from('<IIQ', raw, 56)
    cursor = 72
    identities = []
    for _ in range(count):
        identity = raw[cursor:cursor + 24]
        fields = struct.unpack_from('<I', raw, cursor + 24)[0]
        cursor += 28
        properties = []
        for _ in range(fields):
            key = raw[cursor:cursor + 16]
            length = struct.unpack_from('<I', raw, cursor + 16)[0]
            cursor += 20
            properties.append((key, raw[cursor:cursor + length]))
            cursor += length
        identities.append((identity, properties))
    cursor += nested * 56
    require(cursor + size == len(raw), 'Wrapped prefab metadata/source framing changed')
    return raw, struct.unpack_from('<Q', raw, 24)[0], identities, raw[cursor:]


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-prefab-'))
    root = scratch / 'project'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True)
    source = root / '.nexora/scenes/Main.scene'
    original = source.read_bytes()
    node = re.search(rb'^node (\d+) 0 ', original, re.M)[1]
    opaque = re.search(rb'^opaque .+$', original, re.M)[0]
    base_id = '00000000-0000-02bd-0000-000000000001'
    variant_id = '00000000-0000-02bd-0000-000000000002'
    base = root / '.nexora/prefabs' / (base_id + '.nxprefab')
    variant = root / '.nexora/prefabs' / (variant_id + '.nxprefab')
    xvfb, display = start_xvfb(args.xvfb, '1600x1200x24')
    process = None
    captured = bytearray()
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env['DISPLAY'] = display
        env['XDG_STATE_HOME'] = str(scratch / 'state')
        env['MESA_SHADER_CACHE_DIR'] = str(scratch / 'shader-cache')

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def key(*keys):
            send('key', '--clearmodifiers', '--delay', '80', *keys)

        def click(window, x, y):
            send('mousemove', '--window', window, str(x), str(y))
            send('mousedown', '1')
            send('mouseup', '1')

        def text(window, x, y, value):
            click(window, x, y)
            key('ctrl+a')
            send('type', '--clearmodifiers', '--delay', '5', '--', value)

        def wait_log(pattern, start=0):
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Editor exited during prefab controls')
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured[start:].decode('utf-8', errors='replace')):
                    return
            raise RuntimeError(f'Missing prefab action {pattern}: {captured!r}')

        def open_host(read_only=False):
            nonlocal process, captured
            captured = bytearray()
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            window = wait_for_window(args.xdotool, env)
            send('windowsize', '--sync', window, '1600', '1200')
            send('windowfocus', '--sync', window)
            time.sleep(.8)
            key('ctrl+alt+p')
            return window

        def close_host(window):
            nonlocal process
            request_window_close(window, env)
            _, diagnostics = collect_output(process, 15)
            diagnostics = captured.decode('utf-8', errors='replace') + diagnostics
            require(process.returncode == 0, 'Native prefab host close failed')
            require(not re.search(r'\b(?:VUID-|SYNC-)[A-Za-z0-9_-]*', diagnostics),
                    'Native prefab host emitted Vulkan validation errors')
            process = None

        window = open_host()
        text(window, 180, 113, base_id)
        click(window, 130, 137)
        wait_log(r'prefab action=0 applied=1 .*dirty=1')
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=1 dirty=0')
        first = asset(base)
        require(first[1] == 1 and first[3] == original, 'Initial actual prefab capture changed source')
        click(window, 140, 227)
        text(window, 180, 427, 'Isolated source')
        key('Return')
        text(window, 180, 451, '3.5')
        key('Return')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=2 dirty=0', start)
        edited = asset(base)
        require(edited[1] == 2 and edited[2] == first[2] and opaque in edited[3] and
                b'node ' + node + b' 0 Isolated source\n' in edited[3] and
                re.search(rb'^' + node + rb' 0 3\.5 ', edited[3], re.M),
                'Actual prefab fields/save changed stable IDs or unknown data')
        click(window, 88, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=3 dirty=0', start)
        undone = asset(base)
        require(re.search(rb'^' + node + rb' 0 0 ', undone[3], re.M) and
                b'node ' + node + b' 0 Isolated source\n' in undone[3],
                'Actual field Undo was lost by publication')
        click(window, 140, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=4 dirty=0', start)
        restored = asset(base)
        require(restored[3] == edited[3] and restored[2] == first[2], 'Actual field Redo failed')
        text(window, 180, 131, variant_id)
        start = len(captured)
        click(window, 310, 154)
        wait_log(r'prefab action=2 applied=1 .*revision=0 dirty=1', start)
        request_window_close(window, env)
        time.sleep(.4)
        key('Escape')
        require(process.poll() is None and not variant.exists() and asset(base)[0] == restored[0],
                'Actual dirty-prefab window close discarded work or published without consent')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=1 dirty=0', start)
        child_asset = asset(variant)
        require(child_asset[3] == restored[3] and child_asset[2] == restored[2] and
                child_asset[0][32:48] == restored[0][8:24] and
                struct.unpack_from('<Q', child_asset[0], 48)[0] == 4,
                'Actual variant lost its exact base/identity/source')
        require(source.read_bytes() == original and asset(base)[0] == restored[0],
                'Prefab workflow changed its original scene or base')
        before_review = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        click(window, 140, 227)
        text(window, 180, 427, 'Variant override')
        key('Return')
        text(window, 180, 451, '9')
        key('Return')
        start = len(captured)
        click(window, 310, 177)
        wait_log(r'prefab action=5 applied=1 .*dirty=1', start)
        click(window, 435, 177)
        time.sleep(.3)
        start = len(captured)
        click(window, 165, 216)
        wait_log(r'prefab action=6 applied=1 .*dirty=0', start)
        require(before_review == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Review/revert published source or changed project files')
        click(window, 88, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=2 dirty=0', start)
        undo_revert = asset(variant)
        require(b'node ' + node + b' 0 Variant override\n' in undo_revert[3] and
                re.search(rb'^' + node + rb' 0 9 ', undo_revert[3], re.M) and
                undo_revert[2] == child_asset[2] and opaque in undo_revert[3],
                'One native Undo did not restore all reverted properties/identities')
        click(window, 140, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=3 dirty=0', start)
        require(asset(variant)[3] == child_asset[3] and asset(variant)[2] == child_asset[2] and
                source.read_bytes() == original and asset(base)[0] == restored[0],
                'Native Redo lost exact source properties or changed original/base')
        # Select one stable field in the actual differences table; preserve the other edit.
        root_fields = next(properties for identity, properties in child_asset[2]
                           if struct.unpack_from('<Q', identity, 16)[0] == int(node))
        fields = {name: struct.unpack('<QQ', identity) for identity, name in root_fields}
        selected_name = fields[b'name'] < fields[b'transform.position']
        click(window, 140, 227)
        text(window, 180, 427, 'Selected override')
        key('Return')
        text(window, 180, 451, '7')
        key('Return')
        unchanged_files = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        start = len(captured)
        click(window, 310, 177)
        wait_log(r'prefab action=5 applied=1 .*dirty=1', start)
        click(window, 90, 580)
        start = len(captured)
        click(window, 565, 177)
        wait_log(r'prefab action=7 applied=1 .*dirty=1', start)
        click(window, 435, 177)
        start = len(captured)
        click(window, 165, 216)
        wait_log(r'prefab action=6 applied=1 .*dirty=1', start)
        require(unchanged_files == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Selected review/revert wrote project files before explicit Save')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=4 dirty=0', start)
        partial = asset(variant)
        expected_name = b'Isolated source' if selected_name else b'Selected override'
        expected_position = rb'7' if selected_name else rb'3\.5'
        require(b'node ' + node + b' 0 ' + expected_name + b'\n' in partial[3] and
                re.search(rb'^' + node + rb' 0 ' + expected_position + rb' ', partial[3], re.M) and
                partial[2] == child_asset[2] and opaque in partial[3],
                'Actual selected revert changed an unselected value, stable identity or unknown bytes')
        click(window, 88, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=5 dirty=0', start)
        selected_undo = asset(variant)
        require(b'node ' + node + b' 0 Selected override\n' in selected_undo[3] and
                re.search(rb'^' + node + rb' 0 7 ', selected_undo[3], re.M),
                'One selected native Undo failed to restore both edits')
        click(window, 140, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=6 dirty=0', start)
        require(asset(variant)[3] == partial[3] and asset(variant)[2] == partial[2],
                'One selected native Redo failed to retain unselected properties')
        start = len(captured)
        click(window, 310, 177)
        wait_log(r'prefab action=5 applied=1 .*dirty=0', start)
        click(window, 435, 177)
        start = len(captured)
        click(window, 165, 216)
        wait_log(r'prefab action=6 applied=1 .*dirty=1', start)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=7 dirty=0', start)
        require(asset(variant)[3] == child_asset[3] and asset(variant)[2] == child_asset[2] and
                source.read_bytes() == original and asset(base)[0] == restored[0],
                'Full review after selected workflow failed to restore exact source properties')
        # Explicit selected publication advances only the exact current base source.
        click(window, 140, 227)
        text(window, 180, 427, 'Source applied')
        key('Return')
        text(window, 180, 451, '11')
        key('Return')
        before_apply = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        start = len(captured)
        click(window, 310, 177)
        wait_log(r'prefab action=5 applied=1 .*dirty=1', start)
        click(window, 90, 580)
        start = len(captured)
        click(window, 565, 177)
        wait_log(r'prefab action=7 applied=1 .*dirty=1', start)
        click(window, 430, 154)
        require(before_apply == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Source application review/confirmation published before explicit consent')
        start = len(captured)
        click(window, 140, 247)
        wait_log(r'prefab action=8 applied=1 .*revision=7 dirty=1', start)
        applied_source = asset(base)
        source_name = b'Source applied' if selected_name else b'Isolated source'
        source_position = rb'3\.5' if selected_name else rb'11'
        require(applied_source[1] == 5 and applied_source[2] == restored[2] and
                b'node ' + node + b' 0 ' + source_name + b'\n' in applied_source[3] and
                re.search(rb'^' + node + rb' 0 ' + source_position + rb' ', applied_source[3], re.M) and
                opaque in applied_source[3] and asset(variant)[1] == 7 and
                asset(variant)[3] == child_asset[3] and source.read_bytes() == original,
                'Selected source publication changed unselected values, variant file, IDs or original scene')
        retained_base = root / '.nexora/prefabs/revisions' / base_id / '4.nxprefab'
        require(retained_base.read_bytes() == restored[0], 'Source apply lost exact prior base revision')
        after_apply = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        changed = {path for path in before_apply.keys() | after_apply.keys()
                   if before_apply.get(path) != after_apply.get(path)}
        require(changed == {base.relative_to(root), retained_base.relative_to(root)},
                'Source apply changed unrelated project files')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=8 dirty=0', start)
        after_local_save = asset(variant)
        require(b'node ' + node + b' 0 Source applied\n' in after_local_save[3] and
                re.search(rb'^' + node + rb' 0 11 ', after_local_save[3], re.M) and
                struct.unpack_from('<Q', after_local_save[0], 48)[0] == 4,
                'Source publication acknowledged/rewrote local edits or implicitly rebased the variant')
        click(window, 88, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=9 dirty=0', start)
        require(b'node ' + node + b' 0 Source applied\n' in asset(variant)[3] and
                re.search(rb'^' + node + rb' 0 3\.5 ', asset(variant)[3], re.M),
                'Source apply occupied/erased local Undo history')
        click(window, 140, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=10 dirty=0', start)
        require(asset(variant)[3] == after_local_save[3] and asset(variant)[2] == child_asset[2] and
                asset(base)[0] == applied_source[0] and source.read_bytes() == original,
                'Source apply changed local Redo, stable identities or published base after Save')
        # Create two real three-way conflicts against the retained v4 reference.
        text(window, 180, 131, base_id)
        start = len(captured)
        click(window, 230, 154)
        wait_log(r'prefab action=1 applied=1 .*revision=5 dirty=0', start)
        click(window, 140, 227)
        text(window, 180, 427, 'New source')
        key('Return')
        text(window, 180, 451, '19')
        key('Return')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=6 dirty=0', start)
        rebase_source = asset(base)
        text(window, 180, 131, variant_id)
        start = len(captured)
        click(window, 230, 154)
        wait_log(r'prefab action=1 applied=1 .*revision=10 dirty=0', start)
        click(window, 140, 227)
        before_rebase_variant = asset(variant)
        rebase_before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        start = len(captured)
        click(window, 552, 154)
        wait_log(r'prefab action=9 applied=1 .*revision=10 dirty=0', start)
        click(window, 614, 154)
        require(rebase_before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Unresolved native rebase wrote project files')
        # Stable field UUID ordering defines the two actual clipped table rows.
        name_y = 580 if selected_name else 603
        position_y = 603 if selected_name else 580
        click(window, 618, name_y)
        click(window, 615, name_y + 40)  # Take source name.
        click(window, 618, position_y)
        click(window, 615, position_y + 23)  # Keep local complete position group.
        start = len(captured)
        click(window, 660, 177)
        wait_log(r'prefab action=10 applied=1 .*revision=10 dirty=0', start)
        click(window, 614, 154)
        require(rebase_before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Native rebase preparation/confirmation published without explicit Save')
        start = len(captured)
        click(window, 122, 216)
        wait_log(r'prefab action=11 applied=1 .*revision=10 dirty=1', start)
        require(rebase_before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Native rebase wrote sources before explicit Save')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=11 dirty=0', start)
        rebased = asset(variant)
        require(b'node ' + node + b' 0 New source\n' in rebased[3] and
                re.search(rb'^' + node + rb' 0 11 ', rebased[3], re.M) and
                rebased[2] == child_asset[2] and opaque in rebased[3] and
                struct.unpack_from('<Q', rebased[0], 48)[0] == 6 and
                asset(base)[0] == rebase_source[0] and source.read_bytes() == original,
                'Native rebase choices lost whole-group overrides, exact reference, IDs or source isolation')
        retained_variant = root / '.nexora/prefabs/revisions' / variant_id / '10.nxprefab'
        require(retained_variant.read_bytes() == before_rebase_variant[0],
                'Native rebase publication lost exact previous variant')
        click(window, 88, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=12 dirty=0', start)
        unrebased = asset(variant)
        require(unrebased[3] == after_local_save[3] and unrebased[2] == child_asset[2] and
                struct.unpack_from('<Q', unrebased[0], 48)[0] == 4,
                'One native Undo failed to restore properties plus exact retained reference')
        click(window, 140, 200)
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=13 dirty=0', start)
        require(asset(variant)[3] == rebased[3] and asset(variant)[2] == rebased[2] and
                struct.unpack_from('<Q', asset(variant)[0], 48)[0] == 6 and
                asset(base)[0] == rebase_source[0],
                'One native Redo failed properties/reference history or mutated the source')
        # Advance source again through actual controls for read-only latest-source review.
        text(window, 180, 131, base_id)
        start = len(captured)
        click(window, 230, 154)
        wait_log(r'prefab action=1 applied=1 .*revision=6 dirty=0', start)
        click(window, 140, 227)
        text(window, 180, 427, 'Readonly newer source')
        key('Return')
        start = len(captured)
        click(window, 110, 177)
        wait_log(r'prefab action=3 applied=1 .*revision=7 dirty=0', start)
        close_host(window)
        before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        window = open_host(True)
        text(window, 180, 113, variant_id)
        click(window, 230, 137)
        wait_log(r'prefab action=1 applied=1 .*revision=13 dirty=0')
        start = len(captured)
        click(window, 310, 177)
        wait_log(r'prefab action=5 applied=1 .*dirty=0', start)
        click(window, 435, 177)
        require(b'prefab action=6' not in captured, 'Read-only review enabled property revert')
        start = len(captured)
        click(window, 552, 154)
        wait_log(r'prefab action=9 applied=1 .*revision=13 dirty=0', start)
        start = len(captured)
        click(window, 660, 177)
        wait_log(r'prefab action=10 applied=1 .*revision=13 dirty=0', start)
        click(window, 614, 154)
        require(b'prefab action=11' not in captured, 'Read-only latest-source review enabled rebase')
        click(window, 140, 227)
        text(window, 180, 427, 'Forbidden readonly edit')
        key('Return')
        click(window, 110, 177)
        close_host(window)
        require(before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Read-only reopened prefab controls changed project files')
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
