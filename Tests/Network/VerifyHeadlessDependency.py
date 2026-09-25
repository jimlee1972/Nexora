import json
import pathlib
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
for forbidden in ("Renderer", "RHI", "Presentation", "Window", "Editor", "EditorImGui"):
    if forbidden in seen:
        raise SystemExit(f"headless dependency reaches {forbidden}")
