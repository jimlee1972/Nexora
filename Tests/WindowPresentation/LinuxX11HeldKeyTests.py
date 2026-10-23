#!/usr/bin/env python3
"""Real X server repeat: one physical transition pair, repeated committed text."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import shutil
import subprocess
import tempfile
import time


class KeyboardState(ctypes.Structure):
    _fields_ = [('key_click_percent', ctypes.c_int), ('bell_percent', ctypes.c_int),
                ('bell_pitch', ctypes.c_uint), ('bell_duration', ctypes.c_uint),
                ('led_mask', ctypes.c_ulong), ('global_auto_repeat', ctypes.c_int),
                ('auto_repeats', ctypes.c_char * 32)]


class ClientData(ctypes.Union):
    _fields_ = [('b', ctypes.c_char * 20), ('s', ctypes.c_short * 10),
                ('l', ctypes.c_long * 5)]


class ClientMessage(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong), ('send_event', ctypes.c_int),
                ('display', ctypes.c_void_p), ('window', ctypes.c_ulong),
                ('message_type', ctypes.c_ulong), ('format', ctypes.c_int), ('data', ClientData)]


class XEvent(ctypes.Union):
    _fields_ = [('client', ClientMessage), ('padding', ctypes.c_long * 24)]


def run_mode(args, evidence, mode):
    state = evidence / mode
    state.mkdir()
    server = child = None
    connection = None
    report = dict(mode=mode, injected_unavailable=(mode == 'legacy'),
                  probe_sha256=hashlib.sha256(Path(args.probe).read_bytes()).hexdigest(),
                  control_sha256=hashlib.sha256(Path(args.control).read_bytes()).hexdigest())
    streams = set()
    captured = {'stdout': bytearray(), 'stderr': bytearray()}
    records = []
    consumed = 0
    try:
        read_fd, write_fd = os.pipe()
        server_log = (state / 'xvfb.log').open('wb')
        try:
            server = subprocess.Popen([args.xvfb, '-displayfd', str(write_fd), '-screen', '0',
                                       '640x480x24', '-nolisten', 'tcp', '-noreset'], pass_fds=(write_fd,),
                                      stdout=server_log, stderr=server_log)
        finally:
            os.close(write_fd)
            server_log.close()
        try:
            if not select.select([read_fd], [], [], 5)[0]:
                raise RuntimeError('Owned Xvfb did not publish its display within five seconds')
            number = os.read(read_fd, 128).decode().strip()
            if not number.isdecimal():
                raise RuntimeError(f'Invalid owned Xvfb display record: {number!r}')
        finally:
            os.close(read_fd)
        environment = os.environ.copy()
        environment['DISPLAY'] = ':' + number
        # This environment variable is read only by the test interposer, never the backend.
        environment['NEXORA_X11_REPEAT_TEST_MODE'] = mode
        inherited_preload = environment.get('LD_PRELOAD', '')
        environment['LD_PRELOAD'] = str(Path(args.control).resolve()) + (
            ':' + inherited_preload if inherited_preload else '')
        x11 = ctypes.CDLL('libX11.so.6')
        x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x11.XOpenDisplay.restype = ctypes.c_void_p
        x11.XGetKeyboardControl.argtypes = [ctypes.c_void_p, ctypes.POINTER(KeyboardState)]
        x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        x11.XkbSetAutoRepeatRate.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                            ctypes.c_uint, ctypes.c_uint]
        x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        x11.XInternAtom.restype = ctypes.c_ulong
        x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                                   ctypes.c_long, ctypes.POINTER(XEvent)]
        x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        connection = x11.XOpenDisplay(environment['DISPLAY'].encode())
        if not connection:
            raise RuntimeError('Could not open the owned Xvfb display')

        def global_repeat():
            value = KeyboardState()
            x11.XGetKeyboardControl(connection, ctypes.byref(value))
            return value.global_auto_repeat

        # The control connection reads the server's key mapping only; it never changes
        # the backend client's detectable-repeat state.
        native_n_keycode = x11.XKeysymToKeycode(connection, ord('n'))
        if not native_n_keycode:
            raise RuntimeError('The owned server has no native keycode for n')
        native_b_keycode = x11.XKeysymToKeycode(connection, ord('b'))
        if not native_b_keycode:
            raise RuntimeError('The owned server has no native keycode for b')
        report['native_b_keycode'] = native_b_keycode
        report['native_n_keycode'] = native_n_keycode
        report['global_before'] = global_repeat()
        if report['global_before'] != 1:
            raise RuntimeError('The owned server must begin with global auto-repeat enabled')
        # Configure rate on this test-owned server, keeping global repeat enabled throughout.
        if not x11.XkbSetAutoRepeatRate(connection, 0x0100, 60, 30):
            raise RuntimeError('Could not configure the owned server repeat rate')
        x11.XSync(connection, 0)
        report.update(repeat_delay_ms=60, repeat_interval_ms=30)
        child = subprocess.Popen([args.probe], env=environment,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        streams = {child.stdout, child.stderr}

        def drain(timeout=.01):
            nonlocal consumed
            for pipe in select.select(list(streams), [], [], timeout)[0]:
                data = os.read(pipe.fileno(), 65536)
                name = 'stdout' if pipe is child.stdout else 'stderr'
                if not data:
                    streams.discard(pipe)
                    continue
                captured[name].extend(data)
                with (state / (name + '.log')).open('ab') as output:
                    output.write(data)
                if len(captured[name]) > 1024 * 1024:
                    raise RuntimeError('Native held-key probe exceeded its one MiB stream limit')
            while True:
                end = captured['stdout'].find(b'\n', consumed)
                if end < 0:
                    break
                records.append(json.loads(captured['stdout'][consumed:end]))
                consumed = end + 1

        def wait(predicate, label, seconds=2):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if predicate():
                    return
                drain(min(.02, max(0, deadline - time.monotonic())))
                if child.poll() is not None:
                    drain(0)
                    if predicate():
                        return
                    raise RuntimeError(f'Native probe exited awaiting {label}')
            raise RuntimeError(f'Native probe timed out awaiting {label}')

        def tool(*arguments):
            command = [args.xdotool, *map(str, arguments)]
            with (state / 'input.jsonl').open('a') as log:
                log.write(json.dumps(dict(time=time.monotonic(), argv=command)) + '\n')
            # No test interposer is needed in the unrelated xdotool client.
            tool_environment = dict(environment)
            tool_environment.pop('LD_PRELOAD', None)
            tool_environment.pop('NEXORA_X11_REPEAT_TEST_MODE', None)
            subprocess.run(command, env=tool_environment, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=2)

        wait(lambda: any(record.get('kind') == 'ready' for record in records), 'native window')
        ready = next(record for record in records if record.get('kind') == 'ready')
        main_handle = ready['handle']
        # Loader diagnostics are supplemental. A missing /proc view must not change
        # the functional input oracle or substitute for actual native event counts.
        try:
            paths = set()
            for line in Path(f'/proc/{child.pid}/maps').read_text().splitlines():
                columns = line.split(None, 5)
                if len(columns) == 6 and 'libNexoraWindow' in Path(columns[5]).name:
                    paths.add(columns[5])
            loaded = []
            for path in sorted(paths):
                entry = dict(path=path)
                try:
                    entry['sha256'] = hashlib.sha256(Path(path).read_bytes()).hexdigest()
                except OSError as error:
                    entry['unavailable'] = type(error).__name__
                loaded.append(entry)
            report['window_library_maps'] = loaded or [dict(unavailable='NoWindowLibraryMapping')]
        except OSError as error:
            report['window_library_maps'] = [dict(unavailable=type(error).__name__)]

        def count(kind, value=None, since=0):
            return sum(record.get('kind') == 'event' and record.get('window') == main_handle and
                       record.get('type') == kind and
                       (value is None or record.get('value0') == value)
                       for record in records[since:])

        tool('windowfocus', '--sync', ready['window'])
        wait(lambda: count(4, 1) >= 1, 'initial focus')
        tool('keydown', 'n')
        held_started = time.monotonic()
        held_until = held_started + .75
        while time.monotonic() < held_until:
            drain(min(.01, max(0, held_until - time.monotonic())))
        report['held_seconds'] = time.monotonic() - held_started
        def key_event(window, key, down, since):
            return any(record.get('kind') == 'event' and record.get('window') == window and
                       record.get('type') == 5 and record.get('value0') == key and
                       record.get('value1') == int(down) for record in records[since:])

        offset = len(records)
        tool('keyup', 'n')
        wait(lambda: key_event(main_handle, ready['key_n'], False, offset), 'physical N release')
        if mode == 'normal':
            # Detectable repeat has no legacy fake release. Queue genuine physical taps
            # while the consumer is stopped, so equal server timestamps cannot erase edges.
            xtest = ctypes.CDLL('libXtst.so.6')
            xtest.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                               ctypes.c_int, ctypes.c_ulong]
            offset = len(records)
            child.send_signal(signal.SIGSTOP)
            stop_confirmed = False
            try:
                stop_deadline = time.monotonic() + 2
                while time.monotonic() < stop_deadline:
                    stopped_pid, status = os.waitpid(child.pid, os.WNOHANG | os.WUNTRACED)
                    if stopped_pid:
                        if not os.WIFSTOPPED(status) or os.WSTOPSIG(status) != signal.SIGSTOP:
                            raise RuntimeError('Native tap consumer exited instead of stopping')
                        stop_confirmed = True
                        break
                    drain(min(.005, max(0, stop_deadline - time.monotonic())))
                if not stop_confirmed:
                    raise RuntimeError('Native tap consumer did not stop within two seconds')
                with (state / 'input.jsonl').open('a') as log:
                    log.write(json.dumps(dict(time=time.monotonic(), native='XTestFakeKeyEvent',
                                              keycode=native_b_keycode, delay_ms=0,
                                              transitions=[1, 0] * 20,
                                              consumer_stop_confirmed=True)) + '\n')
                for _ in range(20):
                    for down in (1, 0):
                        if not xtest.XTestFakeKeyEvent(connection, native_b_keycode, down, 0):
                            raise RuntimeError('Could not queue one genuine native B phase')
                # Finish server processing before allowing the consumer to drain its queue.
                x11.XSync(connection, 0)
                report['rapid_burst_stop_confirmed'] = stop_confirmed
            finally:
                # A failed assertion or injection must never leave the owned child stopped.
                if child.poll() is None:
                    child.send_signal(signal.SIGCONT)
            wait(lambda: count(6, ord('b'), offset) == 20, 'all genuine B tap text')
        # A still-held M must be forgotten on A's focus loss. Its physical release goes to B;
        # after focus returns, A must admit the next M press rather than deduplicate it.
        offset = len(records)
        tool('keydown', 'm')
        wait(lambda: key_event(main_handle, ready['key_m'], True, offset), 'first M down on A')
        offset = len(records)
        tool('windowfocus', '--sync', ready['other'])
        wait(lambda: count(4, 0, offset) >= 1, 'focus loss while M is held')
        offset = len(records)
        tool('keyup', 'm')
        wait(lambda: key_event(ready['other_handle'], ready['key_m'], False, offset), 'M release on B')
        offset = len(records)
        tool('windowfocus', '--sync', ready['window'])
        wait(lambda: count(4, 1, offset) >= 1, 'focus regain')
        offset = len(records)
        tool('keydown', 'm')
        # Text is emitted even when a stale held map incorrectly suppresses the physical edge.
        # Acknowledge native processing through text, then finish the one down/up gesture and
        # report exact M counts so that the missing second down remains an explicit failure.
        wait(lambda: any(record.get('kind') == 'event' and record.get('window') == main_handle and
                         record.get('type') == 6 and record.get('value0') == ord('m')
                         for record in records[offset:]), 'second M processing on A')
        offset = len(records)
        tool('keyup', 'm')
        wait(lambda: key_event(main_handle, ready['key_m'], False, offset), 'final M release on A')
        message = XEvent()
        message.client.type = 33
        message.client.display = connection
        message.client.window = ready['window']
        message.client.message_type = x11.XInternAtom(connection, b'WM_PROTOCOLS', 0)
        message.client.format = 32
        message.client.data.l[0] = x11.XInternAtom(connection, b'WM_DELETE_WINDOW', 0)
        with (state / 'input.jsonl').open('a') as log:
            log.write(json.dumps(dict(time=time.monotonic(), close='WM_DELETE_WINDOW',
                                      window=ready['window'])) + '\n')
        if not x11.XSendEvent(connection, ready['window'], 0, 0, ctypes.byref(message)):
            raise RuntimeError('Could not send the ordinary close request')
        x11.XSync(connection, 0)
        wait(lambda: any(record.get('kind') == 'summary' for record in records), 'close summary')
        child.wait(timeout=2)
        while streams:
            drain(.01)
        summary = next(record for record in records if record.get('kind') == 'summary')
        report['summary'] = summary
        report['exit'] = child.returncode
        report['global_after'] = global_repeat()
        capabilities = [json.loads(line) for line in captured['stderr'].splitlines()
                        if line.startswith(b'{"kind":"repeat_capability"')]
        report['capabilities'] = capabilities
        native_keys = [json.loads(line) for line in captured['stderr'].splitlines()
                       if line.startswith(b'{"kind":"native_key_event"')]
        native_n = [event for event in native_keys
                    if event.get('window') == ready['window'] and
                    event.get('keycode') == native_n_keycode]
        native_press = sum(event.get('type') == 2 and event.get('send_event') == 0
                           for event in native_n)
        native_release = sum(event.get('type') == 3 and event.get('send_event') == 0
                             for event in native_n)
        report['native_n'] = dict(press=native_press, release=native_release,
                                  synthetic=sum(event.get('send_event') != 0 for event in native_n))
        if mode == 'normal':
            native_b = [event for event in native_keys
                        if event.get('window') == ready['window'] and
                        event.get('keycode') == native_b_keycode]
            burst_press = sum(event.get('type') == 2 for event in native_b)
            burst_release = sum(event.get('type') == 3 for event in native_b)
            burst_synthetic = sum(event.get('send_event') != 0 for event in native_b)
            equal_pairs = sum(
                before.get('type') == 3 and after.get('type') == 2 and
                before.get('time') == after.get('time') and
                before.get('window') == after.get('window') and
                before.get('keycode') == after.get('keycode') and
                before.get('send_event') == after.get('send_event') == 0
                for before, after in zip(native_b, native_b[1:]))
            report['rapid_burst'] = dict(applicable=True, press=burst_press,
                                         release=burst_release, synthetic=burst_synthetic,
                                         equal_release_press_pairs=equal_pairs,
                                         down=summary['b_down'], up=summary['b_up'],
                                         text=summary['b_text'])
            report['rapid_burst_ok'] = (
                report.get('rapid_burst_stop_confirmed') is True and
                burst_press == burst_release == 20 and burst_synthetic == 0 and
                equal_pairs >= 1 and
                summary['b_down'] == summary['b_up'] == summary['b_text'] == 20)
        else:
            # Without detectable repeat, equal-time physical taps remain ambiguous.
            report['rapid_burst'] = dict(applicable=False, reason='Legacy heuristic limitation')
            report['rapid_burst_ok'] = summary['b_down'] == summary['b_up'] == summary['b_text'] == 0
        report['text_preserved_ok'] = native_press == summary['text'] and native_press >= 2
        report['native_repeat_ok'] = (bool(native_n) and
                                      all(event.get('send_event') == 0 for event in native_n) and
                                      (native_release == 1 if mode == 'normal' else native_release >= 2))
        report['counts_ok'] = summary['down'] == 1 and summary['up'] == 1 and summary['text'] >= 2
        report['focus_reset_ok'] = summary['m_down'] == 2 and summary['m_up'] == 1
        report['lifecycle_ok'] = (child.returncode == 0 and summary['focus'] >= 2 and
                                  summary['blur'] >= 1 and summary['close'] == 1 and
                                  summary['destroyed_event'] == 0 and summary['alive_at_close'] == 1 and
                                  summary['destroy_ok'] == 1)
        report['capability_ok'] = any(
            observation.get('requested') == 1 and
            observation.get('injected_denial') == int(mode == 'legacy') and
            observation.get('active') == int(mode == 'normal') and
            observation.get('query_supported') == 1 and
            observation.get('global_before') == observation.get('global_after') == 1
            for observation in capabilities)
        report['passed'] = (report['counts_ok'] and report['text_preserved_ok'] and report['native_repeat_ok'] and
                            report['focus_reset_ok'] and report['lifecycle_ok'] and report['rapid_burst_ok'] and
                            report['capability_ok'] and report['global_before'] == report['global_after'] == 1)
        if not report['passed']:
            report['failure'] = 'Physical/text counts, native repeat/text preservation, held-focus reset, lifecycle, detectable tap edges, same-client capability or global repeat invariant failed'
    except Exception as error:
        report.update(passed=False, failure=str(error))
    finally:
        if child is not None:
            if child.poll() is None:
                child.kill()
            stdout, stderr = child.communicate(timeout=2)
            for name, tail in (('stdout', stdout), ('stderr', stderr)):
                if tail:
                    with (state / (name + '.log')).open('ab') as log:
                        log.write(tail)
        if connection:
            x11.XCloseDisplay(connection)
        if server is not None:
            server.terminate()
            server.wait(timeout=2)
        (state / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--control', required=True)
    parser.add_argument('--xvfb', default='Xvfb')
    parser.add_argument('--xdotool', default='xdotool')
    parser.add_argument('--evidence-dir', type=Path, required=True)
    args = parser.parse_args()
    for name in ('xvfb', 'xdotool'):
        if not shutil.which(getattr(args, name)):
            print(f'Native X11 held-key test unavailable: {name} not installed')
            return 77
    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='attempt-', dir=args.evidence_dir))
    reports = [run_mode(args, evidence, mode) for mode in ('normal', 'legacy')]
    print(json.dumps(dict(evidence=str(evidence), reports=reports), sort_keys=True))
    return 0 if all(report['passed'] for report in reports) else 1


if __name__ == '__main__':
    raise SystemExit(main())
