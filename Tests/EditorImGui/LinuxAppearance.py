#!/usr/bin/env python3
"""Verify actual Editor appearance pixels, explicit persistence, and read-only project reopen."""
import argparse
from collections import Counter
import os
from pathlib import Path
import subprocess
import tempfile

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import native_point, rendered_pixels
from NativeSceneInput import NativeSceneInput


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-native-appearance-') as temporary:
        state = Path(temporary)
        root = state / 'Project'
        (root / 'Content').mkdir(parents=True)
        (root / '.nexora').mkdir()
        descriptor = root / 'project.nexora'
        descriptor.write_text('schema=2\nuuid=aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee\nname=Appearance\n')
        (root / '.nexora/workspace').write_text('schema=1\n')
        authored = descriptor.read_bytes()
        preference = state / 'appearance'
        server, display = start_xvfb(args.xvfb, '1280x900x24')
        driver = None
        try:
            if display is None:
                raise RuntimeError('Xvfb did not become ready')
            env = os.environ.copy()
            env.update(DISPLAY=display, NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')
            for number in range(2):
                process = launch(args.editor, root, state / 'recent-projects', env,
                                 read_only=number == 1, binary=True)
                window = wait_for_window(args.xdotool, env)
                driver = NativeSceneInput(process, args.xdotool, env, window, state, number)
                driver.send('windowfocus', '--sync', window)
                driver.wait(lambda value: 'appearance_settings' in value, 'Appearance menu absent')

                def pixels(light):
                    # The docked Inspector at the right contains a broad untextured panel surface.
                    geometry = subprocess.check_output([args.xdotool, 'getwindowgeometry', '--shell', window], env=env, text=True)
                    size = dict(line.split('=', 1) for line in geometry.splitlines() if '=' in line)
                    width, height = int(size['WIDTH']), int(size['HEIGHT'])
                    values = rendered_pixels(display, window, (width - 90, height // 3, 60, 100))
                    neutral = Counter(r for r, g, b in values if r == g == b and 20 < r < 245)
                    if not neutral:
                        return False
                    dominant = neutral.most_common(1)[0][0]
                    # Palette swatches are decoded for RenderUi; the sRGB attachment presents
                    # the intended Dark .22 (56) / Light .76 (194), rather than brightening them.
                    return dominant > 180 if light else dominant < 90

                driver.wait(lambda _: pixels(number == 1), 'Saved/default theme pixels not presented')
                if number == 0 and preference.exists():
                    raise RuntimeError('Reading the absent preference created a file')
                driver.click(native_point(driver, 'appearance_settings'))
                driver.pointer_move(*native_point(driver, 'appearance_menu'))
                target = 'appearance_light' if number == 0 else 'appearance_dark'
                driver.click(native_point(driver, target))
                expected = 'schema=1\ntheme=' + ('light' if number == 0 else 'dark') + '\n'
                driver.wait(lambda _: preference.is_file() and preference.read_text() == expected,
                            'Explicit theme selection was not persisted')
                driver.wait(lambda _: pixels(number == 0), 'Selected theme pixels did not change')
                request_window_close(window, env)
                # A project without a saved scene starts with an unsaved Scene Root. Discard it;
                # appearance must never require saving or modifying authored scene content.
                driver.click(native_point(driver, 'close_discard'))
                driver.finish()
                driver = None
                if descriptor.read_bytes() != authored or list((root / 'Content').glob('*.scene')):
                    raise RuntimeError('Appearance changed authored project data')
            print('Native default Dark, Light pixels/save/reopen and read-only Dark switch passed')
        finally:
            if driver is not None:
                driver.abort()
            server.terminate()
            server.communicate(timeout=10)


if __name__ == '__main__':
    main()
