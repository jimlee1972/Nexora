"""Create and reopen a project through the real native graphical Editor CLI."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

from LinuxDisplayAcceptance import collect_output, start_xvfb


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="nexora-native-project-cli-") as temporary:
        root = Path(temporary)
        project = root / "陰陽 CLI project"
        recent = root / "recent-projects"
        server, display = start_xvfb(args.xvfb, "1600x900x24")
        child = None
        try:
            if display is None:
                raise RuntimeError("Xvfb did not become ready")
            environment = dict(os.environ, DISPLAY=display)
            for operation in ("create", "open"):
                command = [args.editor, "--graphical", "--frames=8", "--recent-projects=" + str(recent)]
                command += (["--create-project=" + str(project), "--project-name=陰陽 CLI"]
                            if operation == "create" else ["--project=" + str(project)])
                child = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE, text=True, encoding="utf-8")
                stdout, stderr = collect_output(child, 30)
                evidence = stdout + "\n" + stderr
                if (child.returncode or not re.search(r"\bpresented=8\b", evidence)
                        or "access=read-write" not in evidence):
                    raise RuntimeError(f"{operation} graphical CLI failed: exit={child.returncode}; "
                                       f"stdout={stdout[:2048]!r}; stderr={stderr[:2048]!r}")
                child = None
                descriptor = (project / "project.nexora").read_bytes()
                if operation == "create":
                    original = descriptor
                elif descriptor != original:
                    raise RuntimeError("reopen replaced the created project identity")
        finally:
            if child is not None and child.poll() is None:
                child.kill()
                child.communicate(timeout=5)
            server.terminate()
            server.communicate(timeout=5)
    print("Native graphical CLI creation and reopen passed; physical Windows NOT VERIFIED")


if __name__ == "__main__":
    main()
