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
        assert "NEXORA_SCRIPT_TESTS_PASSED" in result.stdout + result.stderr, result.stdout + result.stderr
        assert list((output / "module").rglob("*ScriptFixture*"))
        assert before == {p.name: p.read_bytes() for p in source.iterdir()}, "SDK modified authored project"
        script_path = source / "game.zig"
        original_script = script_path.read_bytes()
        migrations = [
            ('const nx = @import("nexora");\npub const Game = struct {\n'
             'pub fn saveState(_: *const @This(), _: *nx.game.StateWriter) !void {}\n};\n',
             "both saveState and loadState"),
            ('const nx = @import("nexora");\npub const Game = struct {\n'
             'pub const state_version: u32 = 0;\n'
             'pub fn saveState(_: *const @This(), _: *nx.game.StateWriter) !void {}\n'
             'pub fn loadState(_: *@This(), _: *nx.game.StateReader) !void {}\n};\n',
             "nonzero u32 state_version"),
            ('const nx = @import("nexora");\npub const Game = struct {\n'
             'pub const state_version: u32 = 1;\n'
             'pub fn saveState(_: *const @This(), w: *nx.game.StateWriter) !void {\n'
             'try w.int(*const u8, undefined);\n}\n'
             'pub fn loadState(_: *@This(), _: *nx.game.StateReader) !void {}\n};\n',
             "never native pointers"),
        ]
        for invalid_script, reason in migrations:
            script_path.write_text(invalid_script, encoding="utf-8")
            rejected = run([args.cmake, "--build", str(output), "--target", "game", "--config", "Release"],
                           expected_success=False)
            assert reason in rejected.stdout + rejected.stderr, rejected.stdout + rejected.stderr
        script_path.write_bytes(original_script)
        with (source / "game.zig").open("a", encoding="utf-8") as script:
            script.write('\ntest "deliberate authored failure" { try std.testing.expect(false); }\n')
        run([args.cmake, "--build", str(output), "--target", "game", "--config", "Release"], expected_success=False)
        manifest["script"] = "../outside.zig"
        (source / "project.json").write_text(json.dumps(manifest), encoding="utf-8")
        rejected = run(configure, expected_success=False)
        assert "project-relative" in rejected.stdout + rejected.stderr
    print("Script-only project: Unicode path, SDK build, entry/script tests, unchanged sources, "
          "paired/versioned migration and pointer-encoding rejection, traversal rejection passed")


if __name__ == "__main__":
    main()
