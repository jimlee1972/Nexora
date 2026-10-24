"""Build a script-only external project through the SDK's public CMake entry."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(arguments, expected_success=True):
    result = subprocess.run(arguments, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=180)
    assert (result.returncode == 0) == expected_success, result.stdout + result.stderr
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--zig", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="nexora-script-") as temporary:
        root = Path(temporary)
        source = root / "authored project 遊戲"
        source.mkdir()
        shutil.copy2(ROOT / "Tests/TwoD/GameScriptFixture.zig", source / "game.zig")
        manifest = dict(schema=1, module_name="ScriptFixture", script="game.zig")
        (source / "project.json").write_text(json.dumps(manifest), encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\nproject(ScriptFixture NONE)\n'
            f'include("{ROOT.as_posix()}/CMake/Modules/NexoraGameProject.cmake")\n'
            'nexora_add_game_module(game PROJECT "${CMAKE_SOURCE_DIR}/project.json" '
            'OUTPUT "${CMAKE_BINARY_DIR}/module" ZIG "${ZIG}")\n', encoding="utf-8")
        before = {p.name: p.read_bytes() for p in source.iterdir()}
        output = root / "build"
        configure = [args.cmake, "-S", str(source), "-B", str(output), f"-DZIG={Path(args.zig).as_posix()}"]
        run(configure)
        result = run([args.cmake, "--build", str(output), "--target", "game", "--config", "Release"])
        assert "2/2 tests passed" in result.stdout + result.stderr, result.stdout + result.stderr
        assert list((output / "module").rglob("*ScriptFixture*"))
        assert before == {p.name: p.read_bytes() for p in source.iterdir()}, "SDK modified authored project"
        manifest["script"] = "../outside.zig"
        (source / "project.json").write_text(json.dumps(manifest), encoding="utf-8")
        rejected = run(configure, expected_success=False)
        assert "project-relative" in rejected.stdout + rejected.stderr
    print("Script-only project: Unicode path, SDK build, entry/script tests, unchanged sources and traversal rejection passed")


if __name__ == "__main__":
    main()
