"""Verify actual configured target/source graphs for optional PoseSearch profiles."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def verify(root: Path, cmake: str, directory: Path, options: list[str], enabled: bool) -> None:
    query = directory / ".cmake/api/v1/query"
    query.mkdir(parents=True)
    (query / "codemodel-v2").touch()
    result = subprocess.run(
        [cmake, "-S", str(root), "-B", str(directory), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Development", "-DBUILD_TESTING=OFF",
         "-DNEXORA_ENABLE_NATIVE_BACKENDS=OFF", "-DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF",
         "-DNEXORA_ENABLE_EDITOR=OFF", "-DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF",
         "-DNEXORA_ENABLE_SLANG=OFF", "-DNEXORA_ENABLE_MIMALLOC=OFF",
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", *options],
        capture_output=True, text=True, check=False,
    )
    if result.returncode:
        raise RuntimeError(f"{directory.name}: configure failed\n{result.stdout}\n{result.stderr}")
    reply = directory / ".cmake/api/v1/reply"
    index = json.loads(next(reply.glob("index-*.json")).read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    targets = model["configurations"][0]["targets"]
    by_name = {entry["name"]: entry for entry in targets}
    source_entries = json.loads((directory / "compile_commands.json").read_text())
    pose_sources = [entry for entry in source_entries if "/Engine/PoseSearch/" in entry["file"].replace("\\", "/")]
    if enabled:
        if "NexoraPoseSearch" not in by_name or len(pose_sources) != 1:
            raise RuntimeError(f"{directory.name}: enabled PoseSearch target/source missing")
        target = json.loads((reply / by_name["NexoraPoseSearch"]["jsonFile"]).read_text())
        dependencies = {entry["id"] for entry in target.get("dependencies", [])}
        if dependencies != {by_name["NexoraFoundation"]["id"]}:
            raise RuntimeError(f"{directory.name}: PoseSearch exceeded its Foundation dependency boundary")
        expected_type = "STATIC_LIBRARY" if "-DNEXORA_LINK_MODE=Monolithic" in options else "SHARED_LIBRARY"
        if target["type"] != expected_type:
            raise RuntimeError(f"{directory.name}: incorrect linkage type {target['type']}")
    elif "NexoraPoseSearch" in by_name or pose_sources:
        raise RuntimeError(f"{directory.name}: disabled PoseSearch target or source leaked into build")
    print(f"{directory.name}: {'enabled with Foundation only' if enabled else 'target and source stripped'}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()
    cases = [
        ("development", [], True),
        ("disabled", ["-DNEXORA_ENABLE_POSE_SEARCH=OFF"], False),
        ("shipping-minimal", ["-DCMAKE_BUILD_TYPE=Shipping", "-DNEXORA_SHIPPING_PROFILE=Minimal"], False),
        ("shipping-full", ["-DCMAKE_BUILD_TYPE=Shipping", "-DNEXORA_SHIPPING_PROFILE=Full", "-DNEXORA_LINK_MODE=Monolithic"], True),
        ("shipping-dedicated", ["-DCMAKE_BUILD_TYPE=Shipping", "-DNEXORA_SHIPPING_PROFILE=Dedicated"], False),
        ("headless", ["-DNEXORA_HEADLESS=ON"], False),
    ]
    with tempfile.TemporaryDirectory(prefix="nexora-pose-search-") as temporary:
        for name, options, enabled in cases:
            verify(args.root.resolve(), args.cmake, Path(temporary) / name, options, enabled)


if __name__ == "__main__":
    main()
