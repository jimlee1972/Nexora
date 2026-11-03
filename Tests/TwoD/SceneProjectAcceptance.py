"""Build real Zig against the public SDK and export/load multiple authored documents."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(*args, cwd=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=240)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--zig', required=True)
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    if args.native and sys.platform.startswith('linux') and not shutil.which('xvfb-run'):
        print('SKIP: native scene acceptance needs Xvfb')
        raise SystemExit(77)
    with tempfile.TemporaryDirectory(prefix='nexora-scene-project-') as temporary:
        root = Path(temporary) / 'authored 陰陽'
        root.mkdir()
        (root/'game.zig').write_text('''const nx = @import("nexora");
pub const Game = struct {
    state: nx.scene_diagnostic.State = .{},
    pub fn start(self: *Game, ctx: *const nx.game.Context) !void { try self.state.start(ctx); }
    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void { try self.state.update(ctx); }
    pub fn stop(self: *Game, ctx: *const nx.game.Context) void { self.state.stop(ctx); }
};
''', encoding='utf-8')
        (root/'project.json').write_text(json.dumps(dict(schema=1, module_name='SceneFixture', script='game.zig')), encoding='utf-8')
        (root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\nproject(SceneFixture NONE)\n'
            f'include("{ROOT.as_posix()}/CMake/Modules/NexoraGameProject.cmake")\n'
            'nexora_add_game_module(game PROJECT "${CMAKE_SOURCE_DIR}/project.json" OUTPUT "${CMAKE_BINARY_DIR}/module" ZIG "${ZIG}")\n', encoding='utf-8')
        run(args.cmake, '-S', root, '-B', root/'build', '-DZIG='+args.zig)
        run(args.cmake, '--build', root/'build', '--target', 'game')
        suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'
        built = list((root/'build/module').rglob('*SceneFixture'+suffix))
        assert len(built) == 1, built
        shutil.copy2(built[0], root/('module'+suffix))
        (root/'empty.txt').write_text('scene', encoding='utf-8')
        for name in ('Start', 'Main'):
            run(args.compiler, 'scene-empty', root/'empty.txt', root/(name+'.scene'))
        run(args.compiler, 'scene-sprite', ROOT/'Bindings/Zig/diagnostic/base.nx2atlas', root/'Title.scene')
        config = dict(schema=1, id='com.nexora.scene-fixture', title='Scene Fixture', scene='Start.scene',
                      module='module'+suffix, audio='off', assets=[],
                      scenes=[dict(id=[501, i], path=name+'.scene') for i, name in enumerate(('Start','Title','Main'), 1)])
        descriptor = root/'game.json'
        descriptor.write_text(json.dumps(config), encoding='utf-8')
        run(sys.executable, ROOT/'Tools/TwoD/ExportGame.py', descriptor, '--runtime', args.app,
            '--compiler', args.compiler, '--output', root/'package', '--allow-development')
        # Remove sources so the generic player can only consume the packaged catalog.
        for name in ('Start', 'Title', 'Main'):
            (root/(name+'.scene')).unlink()
        command = [root/'package'/args.app.name, '--run-2d-project', root/'package/game.nx2game',
                   '--frames', '60', '--report', root/'report.json', '--save-directory', root/'saves']
        if args.native:
            command += ['--native', '--backend', 'vulkan' if sys.platform.startswith('linux') else 'dx12']
            if sys.platform.startswith('linux'):
                command = ['xvfb-run', '-a'] + command
        result = run(*command, cwd=root)
        logs = result.stdout + result.stderr
        assert 'SCENE DIAGNOSTIC PASS' in logs and 'SCENE DIAGNOSTIC FAIL' not in logs, logs
        report = json.loads((root/'report.json').read_text(encoding='utf-8'))
        assert report['status'] == 'PASS' and report['allocations_balanced'], report
        if args.native:
            assert report['native_frames'] == 60, report
        # Invalid scene bytes must reject before publication; existing package stays intact.
        (root/'Start.scene').write_bytes(b'bad scene')
        for name in ('Title', 'Main'):
            shutil.copy2(root/'package/scenes/0000.scene', root/(name+'.scene'))
        rejected = subprocess.run([sys.executable, str(ROOT/'Tools/TwoD/ExportGame.py'), str(descriptor),
            '--runtime', str(args.app), '--compiler', str(args.compiler), '--output', str(root/'bad'),
            '--allow-development'], capture_output=True, timeout=60)
        assert rejected.returncode and not (root/'bad').exists()
    print('PASS packaged Zig scene replacement/additive/unload, source isolation and atomic export')


if __name__ == '__main__':
    main()
