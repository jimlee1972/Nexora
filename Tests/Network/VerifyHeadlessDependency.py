import json
import pathlib
import subprocess
import sys

modules = {item["name"]: item for item in json.loads(pathlib.Path(sys.argv[1]).read_text())["modules"]}
seen = set()


def visit(name):
    if name in seen:
        return
    seen.add(name)
    for dependency in modules[name].get("dependencies", []):
        visit(dependency)


visit("DedicatedServer")
expected = {"API", "Foundation", "Core", "Network", "DedicatedServer"}
if seen != expected:
    raise SystemExit(f"unexpected dedicated-server dependency closure: {sorted(seen)}")

for forbidden in ("Renderer", "RHI", "Presentation", "Window", "Runtime", "Editor", "EditorImGui"):
    if forbidden in seen:
        raise SystemExit(f"headless dependency reaches {forbidden}")

if sys.argv[3] == "1":
    targets = subprocess.run(
        ["cmake", "--build", sys.argv[2], "--target", "help"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    for forbidden in (
        "NexoraRHI",
        "NexoraRenderer",
        "NexoraRuntime",
        "NexoraPresentation",
        "NexoraWindow",
        "NexoraEditor",
    ):
        if forbidden in targets:
            raise SystemExit(f"headless build exposes forbidden target {forbidden}")
