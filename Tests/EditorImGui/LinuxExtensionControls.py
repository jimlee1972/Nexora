"""Read actual test-build widget positions; all input travels through native X11."""
import os
import re
import select
import time


class ExtensionControls:
    def __init__(self, process, send, window, captured=None):
        self.process = process
        self.send = send
        self.window = window
        self.captured = captured if captured is not None else bytearray()

    def pump(self):
        if select.select([self.process.stderr], [], [], .1)[0]:
            self.captured.extend(os.read(self.process.stderr.fileno(), 65536))

    def wait(self, predicate, label):
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError("Editor exited during " + label)
            self.pump()
            result = predicate()
            if result:
                return result
        raise RuntimeError("Actual native operation missing: " + label + "; output=" +
                           self.captured.decode('utf-8', errors='replace'))

    def point(self, key):
        def current():
            lines = re.findall(r'extensions dpi=([\d.]+) revision=\d+ controls=([^\n]*)',
                               self.captured.decode('utf-8', errors='replace'))
            if not lines:
                return None
            dpi, controls = lines[-1]
            match = re.search(r'(?:^|;)' + re.escape(key) + r':([-\d.]+),([-\d.]+);', controls)
            return [round(float(match[1]) * float(dpi)),
                    round(float(match[2]) * float(dpi))] if match else None
        return self.wait(current, "rendered extension control " + key)

    def click(self, key):
        x, y = self.point(key)
        self.send('mousemove', '--window', str(self.window), str(x), str(y))
        self.send('mousedown', '1')
        self.send('mouseup', '1')

    def type(self, key, text):
        self.click(key)
        self.send('key', '--clearmodifiers', '--delay', '100', 'ctrl+a')
        self.send('type', '--clearmodifiers', '--delay', '15', '--', text)

    def action(self, key, action):
        start = len(self.captured)
        self.click(key)
        def completed_and_drawn():
            output = self.captured[start:].decode('utf-8', errors='replace')
            result = re.search(rf'extensions action={action} applied=1 revision=(\d+)', output)
            return result and re.search(r'extensions dpi=[\d.]+ revision=' + result[1] +
                                        r' controls=', output)
        self.wait(completed_and_drawn, "accepted and rendered extension action " + str(action))
