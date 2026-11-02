"""Kill real child processes and verify evidence survives without async logger shutdown."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

fixture, module = sys.argv[1:]

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

with tempfile.TemporaryDirectory(prefix="nexora-crash-證據-") as temporary:
    root = Path(temporary)
    modes = ("normal", "rotation", "exception", "unknown-exception", "terminate", "abort",
                 "worker-crash", "update-crash", "fixed-crash", "create-crash", "stop-crash", "start-crash", "destroy-crash",
                 "update-error", "fixed-error", "job-exception", "job-crash")
    if os.name != "nt":
        modes += ("stack-overflow",)
    for mode in modes:
        destination = root / mode
        environment = dict(os.environ, NEXORA_LOG_DIR=str(destination))
        result = subprocess.run([fixture, mode, module], env=environment, capture_output=True, timeout=30)
        sessions = list(destination.glob("run-*"))
        require(len(sessions) == 1, f"{mode}: missing or duplicate session: {result.stderr!r}")
        session = sessions[0]
        log = (session / "game.log").read_text(errors="replace")
        crash = (session / "crash.log").read_text(errors="replace")
        require("build=" in crash and "platform=" in crash, f"{mode}: missing metadata")
        require((session / "modules.txt").stat().st_size > 0, f"{mode}: missing module map")
        if mode != "rotation":
            require("accepted immediately before crash" in log, f"{mode}: lost accepted log")
        context = (session / "context.txt").read_text()
        require("game.primary_scene=42" in context and "game.primary_scene=41" not in context, f"{mode}: stale context snapshot")
        if mode in ("normal", "rotation", "update-error", "fixed-error", "job-exception"):
            require(result.returncode == 0, f"{mode}: unexpected failure {result.stderr!r}")
            require("session=closed" in log, f"{mode}: shutdown did not drain")
            require("fatal kind=" not in crash, f"{mode}: spurious native crash")
        else:
            require(result.returncode != 0, f"{mode}: crash swallowed")
        if mode == "normal":
            require(log.count("[Concurrent]") == 64, "concurrent log loss")
            require(r"first\x0aforged\x00tail" in log, "log injection/NUL not escaped")
            require("recoverable game failure" in crash, "game failure not persisted")
            require("invalid gameplay log rejected" in log, "invalid wire length was accepted")
        elif mode == "rotation":
            require((session / "game.1.log").exists(), "rotation did not occur")
            require(all(p.stat().st_size <= 4 * 1024 * 1024 for p in session.glob("game*.log")), "unbounded log file")
        elif mode in ("exception", "unknown-exception"):
            text = "game exception details" if mode == "exception" else "unhandled non-standard exception"
            require(text in crash, f"{mode}: missing exception record")
        elif mode == "job-exception":
            require("game job exception" in crash, "captured game job exception not persisted")
        elif mode in ("update-error", "fixed-error"):
            callback = "update" if mode == "update-error" else "fixed_update"
            require(f"[Gameplay.{callback}] callback failed result=-3 generation=1" in crash, f"{mode}: missing callback failure")
        else:
            require("fatal kind=" in crash, f"{mode}: missing native crash record")
            if os.name == "nt" and "minidump=ready" in crash:
                require((session / "crash.dmp").read_bytes().startswith(b"MDMP"), f"{mode}: missing native minidump")
            if mode == "worker-crash":
                require("context=Gameplay.worker-遊戲" in crash, "Unicode fault context was corrupted")
            if mode == "stack-overflow":
                require("context=Gameplay.stack" in crash, "alternate signal stack did not capture exhausted owner stack")
            elif mode == "job-crash":
                require("context=JobSystem.job" in crash, "missing job crash context")
            elif mode.endswith("-crash"):
                callback = {"worker-crash": "worker", "update-crash": "update", "fixed-crash": "fixed_update",
                            "create-crash": "create", "stop-crash": "on_stop", "start-crash": "on_start", "destroy-crash": "destroy"}[mode]
                require(f"context=Gameplay.{callback}" in crash, f"{mode}: missing faulting callback: {crash}")
            if mode in ("update-crash", "fixed-crash"):
                require("gameplay breadcrumb before failure" in log, "lost gameplay log before native crash")
                require(Path(module).name in (session / "modules.txt").read_text(errors="replace"), "missing dynamic gameplay module map")
        print(f"PASS {mode}")
    # A bad explicit destination must fail visibly, never silently claim active diagnostics.
    bad = root / "not-a-directory"
    bad.write_text("file")
    result = subprocess.run([fixture, "normal", module], env=dict(os.environ, NEXORA_LOG_DIR=str(bad)),
                            capture_output=True, timeout=30)
    require(result.returncode != 0 and b"diagnostics startup failed" in result.stderr, "bad output path was hidden")
print("PASS diagnostics persistence and native gameplay crash capture")
