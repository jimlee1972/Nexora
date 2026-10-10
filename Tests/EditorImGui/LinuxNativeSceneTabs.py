#!/usr/bin/env python3
"""Exercise actual additive document ownership, Save All and reference gates in the native Editor."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from LinuxSceneFiles import wait_until


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--editor', required=True)
    parser.add_argument('--xvfb', required=True)
    parser.add_argument('--xdotool', required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='nexora-native-scene-tabs-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-native-tabs-state-'))
    (root / 'Content').mkdir()
    (root / '.nexora').mkdir()
    (root / 'project.nexora').write_text('schema=1\nname=Additive Tabs Acceptance\n')
    (root / '.nexora/workspace').write_text('schema=1\n')
    server, display = start_xvfb(args.xvfb, '1280x900x24')
    process = None
    passed = False
    try:
        if display is None:
            raise RuntimeError('Xvfb did not become ready')
        env = os.environ.copy()
        env['DISPLAY'] = display
        env['XDG_STATE_HOME'] = str(state)

        def send(*arguments):
            if arguments[0] == 'key':
                arguments = ('key', '--delay', '100', *arguments[1:])
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def key(chord):
            send('key', '--clearmodifiers', chord)

        def save_as(relative):
            key('ctrl+shift+s')
            send('mousemove', '--window', window, '600', '375', 'click', '1')
            key('ctrl+a')
            send('type', '--clearmodifiers', '--delay', '2', relative)
            key('Return')
            wait_until(lambda: (root / relative).is_file(), 'Additive Save As did not publish', process)

        def restore(file, expected):
            key('ctrl+z')  # Exactly one Undo; retries below may only repeat Save.
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                key('ctrl+s')
                if file.read_bytes() == expected:
                    return
            raise RuntimeError('A document switch lost its independent Undo or save destination')

        process = launch(args.editor, root, state / 'recent', env)
        window = wait_for_window(args.xdotool, env)
        send('windowfocus', '--sync', window)
        time.sleep(.8)
        send('mousemove', '--window', window, '500', '220', 'click', '1')
        primary = root / '.nexora/scenes/Main.scene'
        key('ctrl+s')
        wait_until(primary.is_file, 'Primary scene did not save', process)
        primary_original = primary.read_bytes()
        key('ctrl+alt+n')
        key('ctrl+shift+n')
        save_as('Content/Second.scene')
        second = root / 'Content/Second.scene'
        second_original = second.read_bytes()
        if second_original.count(b'node ') != 1 or primary.read_bytes() != primary_original:
            raise RuntimeError('New additive scene replaced or inherited primary source contents')
        key('ctrl+shift+n')
        key('ctrl+alt+Prior')
        key('ctrl+shift+n')
        if primary.read_bytes() != primary_original or second.read_bytes() != second_original:
            raise RuntimeError('Tab switching saved source files implicitly')
        key('ctrl+alt+s')
        wait_until(lambda: primary.read_bytes() != primary_original and
                   second.read_bytes() != second_original, 'Save All omitted an owned document', process)
        if (primary.read_bytes().count(b'node ') != primary_original.count(b'node ') + 1 or
                second.read_bytes().count(b'node ') != second_original.count(b'node ') + 1):
            raise RuntimeError('Save All cross-wrote document contents')
        restore(primary, primary_original)
        key('ctrl+alt+Next')
        restore(second, second_original)
        # Create a third unique identity, close its owner, then reopen it as a reference.
        key('ctrl+alt+n')
        key('ctrl+shift+n')
        save_as('Content/Reference.scene')
        reference = root / 'Content/Reference.scene'
        reference_original = reference.read_bytes()
        key('ctrl+alt+w')
        key('ctrl+alt+shift+o')
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '2', 'Content/Reference.scene')
        key('Return')
        key('ctrl+shift+n')
        key('ctrl+s')
        key('ctrl+z')
        if reference.read_bytes() != reference_original:
            raise RuntimeError('Reference view authored or saved its source')
        # Dirty the primary, return to the reference, and request native window close.
        key('ctrl+alt+Next')
        key('ctrl+shift+n')
        key('ctrl+alt+Prior')
        request_window_close(window, env)
        time.sleep(.4)
        if process.poll() is not None or primary.read_bytes() != primary_original:
            raise RuntimeError('Clean active reference hid another owned document dirty state')
        key('Escape')
        key('ctrl+alt+s')
        wait_until(lambda: primary.read_bytes() != primary_original,
                   'Save All from an active reference omitted another owned document', process)
        if second.read_bytes() != second_original or reference.read_bytes() != reference_original:
            raise RuntimeError('Save All changed the clean scene or read-only reference')
        # Inspection is available on the actual active reference, without acquiring a writer.
        key('ctrl+alt+d')
        time.sleep(1)
        request_window_close(window, env)
        output, error = collect_output(process, 15)
        if (process.returncode != 0 or 'scene_documents=3' not in error or
                'scene_references=1' not in error or 'scene_nodes=1' not in error or 'ui_draws=' not in error or
                'scene comparison ready fields=0 conflicts=0' not in error):
            raise RuntimeError(f'Actual additive host ownership/reference acceptance failed: {output}\n{error}')
        process = None
        metadata = root / '.nexora/scene-composition.ini'
        if not metadata.is_file():
            raise RuntimeError('Native shutdown did not persist the named scene set')
        saved_metadata = metadata.read_bytes()
        saved_primary = primary.read_bytes()
        # Both access roles reopen the saved membership and active reference, independent of
        # the old single-scene startup setting. No source or metadata version changes on reopen.
        for read_only in (True, False):
            process = launch(args.editor, root, state / 'recent', env, frames=8, read_only=read_only)
            output, error = collect_output(process, 20)
            if (process.returncode != 0 or 'scene_documents=3' not in error or
                    'scene_references=1' not in error or 'scene_nodes=1' not in error or
                    primary.read_bytes() != saved_primary or second.read_bytes() != second_original or
                    reference.read_bytes() != reference_original or metadata.read_bytes() != saved_metadata):
                raise RuntimeError(f'Native composition reopen lost roles/order/active source: {output}\n{error}')
            process = None
        # A corrupt last source rejects the entire candidate. The host keeps the bootstrap
        # and freezes source authoring, leaving both foreign bytes and saved metadata intact.
        reference.write_bytes(b'foreign invalid scene source')
        process = launch(args.editor, root, state / 'recent', env)
        window = wait_for_window(args.xdotool, env)
        send('windowfocus', '--sync', window)
        time.sleep(.8)
        send('mousemove', '--window', window, '500', '220', 'click', '1')
        key('ctrl+shift+n')
        key('ctrl+s')
        request_window_close(window, env)
        output, error = collect_output(process, 15)
        if (process.returncode != 0 or 'scene_documents=1' not in error or
                'saved scene set could not be restored:' not in error or
                primary.read_bytes() != saved_primary or second.read_bytes() != second_original or
                reference.read_bytes() != b'foreign invalid scene source' or
                metadata.read_bytes() != saved_metadata):
            raise RuntimeError(f'Failed native restore changed source/metadata versions: {output}\n{error}')
        process = None
        reference.write_bytes(reference_original)
        process = launch(args.editor, root, state / 'recent', env, frames=8)
        output, error = collect_output(process, 20)
        if process.returncode != 0 or 'scene_documents=3' not in error or 'scene_references=1' not in error:
            raise RuntimeError(f'Repaired native scene set could not reopen: {output}\n{error}')
        process = None
        # Closing back to one document must replace the existing composition, preventing old
        # additive members from returning on restart. No source is deleted or saved implicitly.
        process = launch(args.editor, root, state / 'recent', env)
        window = wait_for_window(args.xdotool, env)
        send('windowfocus', '--sync', window)
        time.sleep(.8)
        send('mousemove', '--window', window, '500', '220', 'click', '1')
        key('ctrl+alt+w')  # Active reference closes; primary becomes active.
        key('ctrl+alt+Next')
        key('ctrl+alt+w')  # Close the second owned document.
        request_window_close(window, env)
        output, error = collect_output(process, 15)
        if (process.returncode != 0 or 'scene_documents=1' not in error or
                'scene_references=0' not in error or
                metadata.read_bytes().splitlines()[2] != b'1 0'):
            raise RuntimeError(f'Closing additive members lost the remaining composition: {output}\n{error}')
        process = None
        single_metadata = metadata.read_bytes()
        process = launch(args.editor, root, state / 'recent', env, frames=8)
        output, error = collect_output(process, 20)
        if (process.returncode != 0 or 'scene_documents=1' not in error or
                'scene_references=0' not in error or metadata.read_bytes() != single_metadata or
                primary.read_bytes() != saved_primary or second.read_bytes() != second_original or
                reference.read_bytes() != reference_original):
            raise RuntimeError(f'Closed additive members returned or changed sources: {output}\n{error}')
        process = None
        passed = True
        print('Actual native additive ownership, independent Undo, Save All and reference/close gates passed.')
    finally:
        if process is not None:
            process.kill()
            process.communicate(timeout=10)
        server.terminate()
        server.communicate(timeout=10)
        if passed:
            shutil.rmtree(root)
            shutil.rmtree(state)
        else:
            print(f'Failure fixture retained: {root}; user state: {state}')


if __name__ == '__main__':
    main()
