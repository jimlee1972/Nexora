"""Copied native GUI observations; this driver never invokes authoring APIs."""
import json
import os
import re
import select
import subprocess
import time

from LinuxDisplayAcceptance import collect_output


class NativeSceneInput:
    physical = {'ctrl': ('Control_L', 0), 'alt': ('Alt_L', 1),
                'shift': ('Shift_L', 2), 'n': ('n', 3), 's': ('s', 4),
                'd': ('d', 5), 'a': ('a', 6), 'Return': ('Return', 7), 'z': ('z', 8)}

    def __init__(self, process, xdotool, environment, window, state, launch, minimum_wait=.2, phase_wait=None, ack_pointer=False):
        self.process, self.xdotool, self.environment = process, xdotool, environment
        self.window, self.state, self.launch = str(window), state, launch
        self.minimum_wait = minimum_wait
        self.phase_wait = minimum_wait if phase_wait is None else phase_wait
        self.ack_pointer = ack_pointer
        self.stderr, self.stdout = bytearray(), bytearray()
        self.completed = False
        self.retain()

    def drain(self, timeout=0):
        for pipe in select.select([self.process.stdout, self.process.stderr], [], [], timeout)[0]:
            data = os.read(pipe.fileno(), 65536)
            error = pipe is self.process.stderr
            (self.stderr if error else self.stdout).extend(data)
            if data:
                stream = "stderr" if error else "stdout"
                with (self.state / f"editor-{self.launch}.{stream}.log").open("ab") as log:
                    log.write(data)

    def observation(self):
        records = re.findall(rb'^scene merge ui ([^\n]*)\n', self.stderr, re.M)
        return dict(part.split('=', 1) for part in records[-1].decode().split()) if records else {}

    def wait(self, predicate, message, deadline=None):
        deadline = time.monotonic() + 8 if deadline is None else deadline
        while time.monotonic() < deadline:
            value = self.observation()
            if value and predicate(value) and time.monotonic() < deadline:
                return value
            if self.process.poll() is not None:
                # Exit actions may emit their final observation before terminating.
                # Drain once more before deciding that its acknowledgement is absent.
                self.drain()
                value = self.observation()
                if value and predicate(value) and time.monotonic() < deadline:
                    return value
                raise RuntimeError('Editor exited awaiting scene GUI input')
            self.drain(min(.05, max(0, deadline - time.monotonic())))
        raise RuntimeError(f'{message}; actual GUI: {self.observation()}')

    def raw(self, *arguments, deadline=None, minimum_wait=None):
        with (self.state / 'input.jsonl').open('a', encoding='utf-8') as log:
            log.write(json.dumps({'time': time.monotonic(), 'launch': self.launch,
                                  'input': arguments}) + '\n')
        remaining = None if deadline is None else deadline - time.monotonic()
        if remaining is not None and remaining <= 0:
            raise RuntimeError("Scene input deadline expired before sending the next phase")
        subprocess.run([self.xdotool, *map(str, arguments)], env=self.environment, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=remaining)
        until = time.monotonic() + (self.minimum_wait if minimum_wait is None else minimum_wait)
        if deadline is not None:
            until = min(until, deadline)
        while time.monotonic() < until:
            self.drain(max(0, until - time.monotonic()))

    def chord(self, chord, deadline=None):
        deadline = time.monotonic() + 8 if deadline is None else deadline
        parts = [self.physical[part] for part in chord.split('+')]
        value = self.wait(lambda v: v['focused'] == '1' and v['keys'] == '000000000',
                          'Scene chord was not released', deadline)
        history = chord == 'ctrl+z'
        if chord == 'ctrl+alt+d' or history:
            gate = 'history_admitted' if history else 'comparison_admitted'
            value = self.wait(lambda v: v[gate] == '1', 'Scene shortcut is not admitted', deadline)
        before = tuple(map(int, value['undo'].split(','))) if history else None
        for name, index in parts:
            self.raw('keydown', name, deadline=deadline, minimum_wait=self.phase_wait)
            self.wait(lambda v: v['keys'][index] == '1', f'GUI did not observe {name} down', deadline)
        for name, index in reversed(parts):
            self.raw('keyup', name, deadline=deadline, minimum_wait=self.phase_wait)
            value = self.wait(lambda v: v['keys'][index] == '0',
                              f'GUI did not observe {name} up', deadline)
        if history:
            after = tuple(map(int, value['undo'].split(',')))
            if after != (before[0] + 1, before[1] + 1):
                raise RuntimeError(f'One scene Undo was not applied exactly once: {before} -> {after}')

    def play_key(self, key, deadline=None):
        """Acknowledge one real Play shortcut; runtime success remains the caller's oracle."""
        deadline = time.monotonic() + 8 if deadline is None else deadline
        index = {'F5': 0, 'F6': 1, 'F10': 2}[key]
        value = self.wait(lambda v: v['focused'] == '1' and v['play_keys'] == '000'
                          and v['play_admitted'] == '1',
                          'Play shortcut is not released/focused/admitted', deadline)
        before = int(value['play_requests'])
        self.raw('keydown', key, deadline=deadline, minimum_wait=self.phase_wait)
        self.wait(lambda v: v['play_keys'][index] == '1',
                  f'GUI did not observe {key} down', deadline)
        self.raw('keyup', key, deadline=deadline, minimum_wait=self.phase_wait)
        value = self.wait(lambda v: v['play_keys'][index] == '0',
                          f'GUI did not observe {key} up', deadline)
        if int(value['play_requests']) != before + 1:
            raise RuntimeError(f'One Play shortcut request was not issued exactly once: '
                               f'{before} -> {value}')

    def file_open(self, deadline=None):
        """Send one Ctrl+O gesture; copied request count acknowledges actual UI admission."""
        deadline = time.monotonic() + 8 if deadline is None else deadline
        value = self.wait(lambda v: v['focused'] == '1' and v['keys'] == '000000000'
                          and v['file_keys'] == '0' and v['file_open_admitted'] == '1',
                          'Open Scene shortcut is not released/focused/admitted', deadline)
        before = int(value['file_open_requests'])
        for name, field, index in (('Control_L', 'keys', 0), ('o', 'file_keys', 0)):
            self.raw('keydown', name, deadline=deadline, minimum_wait=self.phase_wait)
            self.wait(lambda v: v[field][index] == '1',
                      f'GUI did not observe Open Scene {name} down', deadline)
        for name, field, index in (('o', 'file_keys', 0), ('Control_L', 'keys', 0)):
            self.raw('keyup', name, deadline=deadline, minimum_wait=self.phase_wait)
            value = self.wait(lambda v: v[field][index] == '0',
                              f'GUI did not observe Open Scene {name} up', deadline)
        if int(value['file_open_requests']) != before + 1:
            raise RuntimeError(f'One Open Scene request was not issued exactly once: '
                               f'{before} -> {value}')

    def send(self, *arguments):
        if self.ack_pointer and arguments:
            if arguments[0] == 'mousemove' and 'click' in arguments:
                split = arguments.index('click')
                button = int(arguments[split + 1])
                if button in (1, 2, 3) and len(arguments) == split + 2:
                    deadline = time.monotonic() + 8
                    self.raw(*arguments[:split], deadline=deadline)
                    target = tuple(map(float, arguments[split - 2:split]))
                    self.wait(lambda v: tuple(map(float, v['pointer'].split(','))) == target,
                              'GUI did not observe pointer motion', deadline)
                    self.pointer_button(button, True, deadline)
                    self.pointer_button(button, False, deadline)
                    return
                # Wheel events have no held-button state in the three-button observer.
                self.raw(*arguments)
                return
            if arguments[0] == 'mousemove':
                deadline = time.monotonic() + 8
                self.raw(*arguments, deadline=deadline)
                target = tuple(map(float, arguments[-2:]))
                self.wait(lambda v: tuple(map(float, v['pointer'].split(','))) == target,
                          'GUI did not observe pointer motion', deadline)
                return
            if arguments[0] == 'click' and len(arguments) == 2 and int(arguments[1]) in (1, 2, 3):
                deadline = time.monotonic() + 8
                self.pointer_button(int(arguments[1]), True, deadline)
                self.pointer_button(int(arguments[1]), False, deadline)
                return
            if arguments[0] in ('mousedown', 'mouseup') and len(arguments) == 2:
                self.pointer_button(int(arguments[1]), arguments[0] == 'mousedown')
                return
        if arguments and arguments[0] in ('keydown', 'keyup') and len(arguments) == 2:
            physical = next((part for alias, part in self.physical.items()
                             if arguments[1] in (alias, part[0])), None)
            if physical is not None:
                deadline = time.monotonic() + 8
                self.raw(*arguments, deadline=deadline, minimum_wait=self.phase_wait)
                expected = '1' if arguments[0] == 'keydown' else '0'
                self.wait(lambda v: v['keys'][physical[1]] == expected,
                          'GUI did not observe direct key phase', deadline)
                return
        if arguments and arguments[0] == 'key':
            chords = []
            index = 1
            while index < len(arguments):
                argument = str(arguments[index])
                if argument == '--clearmodifiers':
                    index += 1
                    continue
                if argument == '--delay':
                    index += 2
                    continue
                chords.append(argument)
                index += 1
            if chords and all(all(part in self.physical for part in chord.split('+')) for chord in chords):
                for chord in chords:
                    self.chord(chord)
                return
        self.raw(*arguments)

    def pointer_move(self, x, y, deadline=None):
        deadline = time.monotonic() + 8 if deadline is None else deadline
        self.raw('mousemove', '--window', self.window, x, y, deadline=deadline)
        self.wait(lambda v: tuple(map(float, v['pointer'].split(','))) == (x, y),
                  'GUI did not observe pointer target', deadline)

    def pointer_button(self, button, down, deadline=None):
        deadline = time.monotonic() + 8 if deadline is None else deadline
        self.raw('mousedown' if down else 'mouseup', str(button), deadline=deadline)
        index = {1: 0, 2: 2, 3: 1}[button]
        self.wait(lambda v: v['buttons'][index] == ('1' if down else '0'),
                  'GUI did not observe pointer button phase', deadline)

    def click(self, point, hold=.25):
        deadline = time.monotonic() + 8
        x, y = map(round, point)
        self.pointer_move(x, y, deadline)
        self.wait(lambda v: v['down'] == '0', 'Scene pointer was not released', deadline)
        self.pointer_button(1, True, deadline)
        time.sleep(hold)
        self.pointer_button(1, False, deadline)

    def settled_viewport(self, viewport):
        stable_since = time.monotonic()
        while time.monotonic() - stable_since < .4:
            self.drain(.05)
            records = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)', self.stderr)
            updated = tuple(map(int, records[-1])) if records else viewport
            if updated != viewport:
                viewport, stable_since = updated, time.monotonic()
        return viewport

    def retain(self):
        (self.state / f'editor-{self.launch}.stderr.log').write_bytes(self.stderr)
        (self.state / f'editor-{self.launch}.stdout.log').write_bytes(self.stdout)

    def finish(self, timeout=15):
        out, error = collect_output(self.process, timeout, stdout_prefix=bytes(self.stdout),
                                    stderr_prefix=bytes(self.stderr))
        self.stdout[:] = out.encode('utf-8')
        self.stderr[:] = error.encode('utf-8')
        self.completed = True
        self.retain()
        return out, error

    def abort(self):
        if self.completed:
            self.retain()
            return
        if self.process.poll() is None:
            self.process.kill()
        out, error = self.process.communicate(timeout=5)
        self.stdout.extend(out)
        self.stderr.extend(error)
        self.retain()
