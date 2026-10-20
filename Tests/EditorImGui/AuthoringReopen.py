"""Actual ImGui authoring and a fresh-process reopen; no physical display claim."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable, scale = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="nexora-authoring-reopen-") as temporary:
        project = Path(temporary) / "編輯 authoring µ"
        for phase in ("write", "read"):
            completed = subprocess.run([executable, phase, str(project), scale], capture_output=True,
                                       text=True, encoding="utf-8", errors="backslashreplace", timeout=60)
            if completed.returncode:
                raise RuntimeError(f"{phase} phase exited {completed.returncode}: "
                                   f"stdout={completed.stdout[-4096:]!r}; "
                                   f"stderr={completed.stderr[-4096:]!r}")
        print(f"Actual ImGui multi-edit/Undo/Redo/save and fresh-process reopen passed at {scale}x")


if __name__ == "__main__":
    main()
