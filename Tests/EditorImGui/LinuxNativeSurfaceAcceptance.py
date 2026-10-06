#!/usr/bin/env python3
"""Run the native Editor resource-lifetime contract on its own Xvfb display."""
import argparse
import os
import subprocess

from LinuxDisplayAcceptance import collect_output, start_xvfb


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-test", required=True)
    parser.add_argument("--xvfb", required=True)
    args = parser.parse_args()
    server, display = start_xvfb(args.xvfb, "1280x720x24")
    try:
        if display is None:
            raise RuntimeError("native Editor test display did not start")
        environment = os.environ.copy()
        environment["DISPLAY"] = display
        with subprocess.Popen([args.native_test], env=environment, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True) as process:
            try:
                collect_output(process, 30)
                if process.returncode != 0:
                    raise RuntimeError(f"native Editor lifetime test failed: {process.returncode}")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)
        return 0
    finally:
        server.terminate()
        server.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
