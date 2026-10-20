#!/usr/bin/env python3
"""Retain public loader evidence after a failed mandatory Linux sanitizer gate."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def diagnose(root: Path, build: Path, output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    original = build / "Testing/Temporary/LastTest.log"
    if original.is_file():
        shutil.copyfile(original, output / "original-ctest.log")

    # Read only public Vulkan manifest identity fields, never the process environment.
    manifests = []
    for directory in (Path("/etc/vulkan"), Path("/usr/share/vulkan"),
                      Path("/usr/local/share/vulkan")):
        for kind in ("icd.d", "implicit_layer.d", "explicit_layer.d"):
            for path in sorted((directory / kind).glob("*.json")):
                entry = {"path": str(path)}
                try:
                    data = json.loads(path.read_text(encoding="utf-8"))
                    entry["file_format_version"] = data.get("file_format_version")
                    records = ([data["ICD"]] if "ICD" in data else
                               data.get("layers", [data.get("layer", {})]))
                    entry["identities"] = [
                        {key: item[key] for key in ("name", "library_path", "api_version")
                         if key in item} for item in records]
                except (OSError, ValueError, TypeError, AttributeError) as error:
                    entry["error"] = str(error)
                manifests.append(entry)
    (output / "manifests.json").write_text(json.dumps(manifests, indent=2) + "\n",
                                           encoding="utf-8")
    packages = ("libvulkan1", "libvulkan-dev", "mesa-vulkan-drivers", "libgl1-mesa-dri",
                "libasan8", "libubsan1", "libc6", "libgcc-s1", "libstdc++6", "libx11-6")
    with (output / "packages.log").open("w", encoding="utf-8") as log:
        try:
            run = subprocess.run(["dpkg-query", "-W", "-f=${Package} ${Version}\n", *packages],
                                 stdout=log, stderr=subprocess.STDOUT, timeout=10, check=False)
            log.write(f"package_query_exit={run.returncode}\n")
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write(f"package_query_error={error}\n")

    # Instrument only the native child, leaving Xvfb and the original helper untouched.
    # Do not select an ICD/layer or change ASan/LSan settings. LD_DEBUG records mapping
    # bases before dlclose, allowing leaked PCs to be attributed after driver unload.
    wrapper = output / "trace-native.sh"
    binary = build / "Tests/WindowPresentation/NexoraVulkanSceneTests"
    wrapper.write_text("#!/bin/sh\nexec env " + shlex.join([
        "LD_DEBUG=files", f"LD_DEBUG_OUTPUT={output / 'loader'}",
        "VK_LOADER_DEBUG=error,warn,driver,layer", str(binary)]) + ' "$@"\n',
        encoding="utf-8")
    wrapper.chmod(0o700)
    command = [sys.executable, str(root / "Tests/Showcase/LinuxVulkanSceneAcceptance.py"),
               "--native-test", str(wrapper), "--evidence-dir", str(output / "native")]
    with (output / "diagnostic.log").open("w", encoding="utf-8") as log:
        try:
            run = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT,
                                 timeout=45, check=False)
            code = run.returncode
        except subprocess.TimeoutExpired:
            code = 124
        except OSError as error:
            log.write(f"diagnostic_error={error}\n")
            code = 1
    (output / "result.json").write_text(json.dumps({
        "diagnostic_exit_code": code, "original_gate_replaced": False,
        "scope": "One additional native scene invocation; evidence only, not acceptance",
    }, indent=2) + "\n", encoding="utf-8")
    # Keep attribution usable from job logs as well as the full artifact. Never print
    # the original whole-suite log or environment; only this bounded native probe.
    for name in ("manifests.json", "packages.log", "diagnostic.log"):
        print(f"Vulkan diagnostic {name}:\n{(output / name).read_text(encoding='utf-8')}")
    for trace in sorted(output.glob("loader.*")):
        print(f"Vulkan module mappings ({trace.name}):")
        for line in trace.read_text(encoding="utf-8", errors="replace").splitlines():
            if any(marker in line for marker in ("generating link map", "base:",
                                                  "calling fini:", "destroying link map")):
                print(line)
    print(f"Vulkan diagnostic exit={code}; evidence={output}; original gate remains failed.")
    return code if code >= 0 else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    return diagnose(Path(__file__).resolve().parents[2], args.build_dir.resolve(),
                    args.output_dir.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
