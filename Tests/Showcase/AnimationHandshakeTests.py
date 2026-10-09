"""Replay delayed native input/presentation without an X server or real-time sleeps."""
from pathlib import Path
from unittest import TestCase, main
from unittest.mock import patch
import LinuxShowcaseInteraction as interaction


class DelayedRenderer:
    def __init__(self, visible_motion=True):
        self.now = 0.0
        self.next_frame = 2.0
        self.running = False
        self.toggles = 0
        self.image = b"paused original"
        self.visible_motion = visible_motion
        self.events = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds

    def tool(self, *arguments):
        if arguments != ('key', '--window', 1, 'space'):
            raise RuntimeError('unexpected input')
        self.events.append((self.now, self.image))
        self.toggles += 1

    def screenshot(self, *_arguments):
        if self.now >= self.next_frame:
            self.running ^= self.toggles % 2 == 1
            self.toggles = 0
            self.next_frame = self.now + 2.0
            if self.running and self.visible_motion:
                self.image = f"animated at {self.now}".encode()
        return self.image


class HandshakeTests(TestCase):
    def install(self, renderer):
        clock = type('Clock', (), {'monotonic': renderer.monotonic, 'sleep': renderer.sleep})
        return patch.multiple(interaction, time=clock, screenshot=renderer.screenshot)

    def test_original_sleep_queues_both_toggles_before_a_frame(self):
        renderer = DelayedRenderer()
        with self.install(renderer):
            renderer.tool('key', '--window', 1, 'space')
            renderer.sleep(0.6)
            renderer.tool('key', '--window', 1, 'space')
            renderer.sleep(0.3)
            with self.assertRaisesRegex(AssertionError, 'five seconds'):
                interaction.compared_screenshot(1, 1, 1, Path('original'), b'paused original', False)
        self.assertFalse(renderer.running)
        self.assertEqual(renderer.image, b'paused original')

    def test_motion_is_presented_before_pause_and_pause_reaches_presentation(self):
        renderer = DelayedRenderer()
        with self.install(renderer):
            changed = interaction.animate_and_pause(1, 1, 1, Path('.'), b'paused original', renderer.tool)
        self.assertNotEqual(changed, b'paused original')
        self.assertEqual(len(renderer.events), 2)
        self.assertNotEqual(renderer.events[1][1], b'paused original')
        self.assertFalse(renderer.running)
        self.assertEqual(renderer.toggles, 0)
        self.assertGreaterEqual(renderer.now - renderer.events[1][0], 2)
        renderer.sleep(4)
        self.assertEqual(renderer.screenshot(), changed)

    def test_never_presented_motion_fails_at_original_deadline_and_queues_pause(self):
        renderer = DelayedRenderer(visible_motion=False)
        with self.install(renderer):
            with self.assertRaisesRegex(AssertionError, 'five seconds'):
                interaction.animate_and_pause(1, 1, 1, Path('.'), b'paused original', renderer.tool)
        self.assertEqual(len(renderer.events), 2)
        self.assertGreaterEqual(renderer.now, 5)
        self.assertLess(renderer.now, 5.2)
        renderer.sleep(2)
        renderer.screenshot()
        self.assertFalse(renderer.running)

    def test_capture_error_still_queues_pause(self):
        renderer = DelayedRenderer()
        with self.install(renderer), patch.object(interaction, 'screenshot', side_effect=RuntimeError('capture lost')):
            with self.assertRaisesRegex(RuntimeError, 'capture lost'):
                interaction.animate_and_pause(1, 1, 1, Path('.'), b'paused original', renderer.tool)
        self.assertEqual(len(renderer.events), 2)


if __name__ == '__main__':
    main()
