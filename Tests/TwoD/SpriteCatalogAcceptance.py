"""Export/relocate real source files and exercise Player preload and deferred admission."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT/'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


def digest(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    for name in ('app', 'module', 'fixture', 'compiler'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-sprite-catalog-') as temporary:
        root = Path(temporary)
        source = root/'source Unicode 精靈'
        source.mkdir()
        subprocess.run([str(args.fixture), '--sprite-fixture', str(source)], check=True, timeout=30)
        shutil.copy2(args.module, source/args.module.name)
        config = dict(schema=1, id='com.nexora.sprite-catalog', title='Source admission', scene='Main',
                      module=args.module.name, audio='off', assets=[
                          dict(kind='sprite', path='source.nx2sprite'),
                          dict(kind='atlas', path='atlas.nxcook'),
                          dict(kind='sprite', path='deferred-source.nx2sprite', preload=False),
                          dict(kind='atlas', path='deferred-atlas.nxcook', preload=False),
                          dict(kind='blob', path='raw.bin')])
        descriptor = source/'game.json'
        descriptor.write_text(json.dumps(config), encoding='utf-8')
        baseline = digest(source)
        package = root/'package'
        exporter.export(descriptor, args.app, package, compiler=args.compiler, allow_development=True)
        assert digest(source) == baseline, 'export modified authored sources'
        manifest = (package/'game.nx2game').read_text(encoding='utf-8')
        assert 'sprite "assets/0000.nxcook"' in manifest
        assert 'deferred_sprite "assets/0002.nxcook"' in manifest
        before = digest(package)
        inventory = json.loads((package/'inventory.json').read_text(encoding='utf-8'))
        assert all(before[path] == value for path, value in inventory['files'].items())
        config['assets'][0]['path'] = 'source.nxcook'
        descriptor.write_text(json.dumps(config), encoding='utf-8')
        copied = root/'pre-cooked'
        exporter.export(descriptor, args.app, copied, compiler=args.compiler, allow_development=True)
        assert (copied/'assets/0000.nxcook').read_bytes() == (source/'source.nxcook').read_bytes()
        config['assets'][0]['path'] = 'source.nx2sprite'
        descriptor.write_text(json.dumps(config), encoding='utf-8')
        # A bad canonical source must fail cooking without publishing or replacing a package.
        (source/'source.nx2sprite').write_bytes(b'bad source')
        for output in (root/'rejected', package):
            try:
                exporter.export(descriptor, args.app, output, compiler=args.compiler,
                                allow_development=True)
            except (ValueError, OSError, subprocess.CalledProcessError):
                pass
            else:
                raise AssertionError('invalid source export admitted')
        assert not (root/'rejected').exists() and digest(package) == before
        # Pre-cooked envelopes are runtime-validated, including a valid hash with false metadata.
        bad_files = {name: (source/name).read_bytes() for name in
                     ('bad-id.nxcook', 'bad-dependency.nxcook', 'bad-region.nxcook')}
        relocated = root/'relocated Unicode 成品'
        package.rename(relocated)
        shutil.rmtree(source)
        env = os.environ.copy()
        env['LD_LIBRARY_PATH'] = str(relocated)+os.pathsep+env.get('LD_LIBRARY_PATH', '')

        def run(success, error=None):
            result = subprocess.run([str(relocated/args.app.name), '--run-2d-project',
                                     str(relocated/'game.nx2game'), '--frames', '12000',
                                     '--save-directory', str(root/'saves')], cwd=root, env=env,
                                    capture_output=True, text=True, encoding='utf-8', timeout=60)
            assert (result.returncode == 0) == success, (result.stdout, result.stderr)
            if success:
                assert 'SPRITE CATALOG PASS:' in result.stderr, result.stderr
                report = json.loads(result.stdout)
                assert report['allocations_balanced'] and report['project_mode']
                assert report['asset_staging_bytes'] == 0
            else:
                assert not result.stdout and 'SPRITE CATALOG PASS:' not in result.stderr
                assert error in result.stderr, result.stderr

        run(True)
        run(True)  # Process recreation consumes only immutable, relocated package bytes.
        assert digest(relocated) == before
        cooked = relocated/'assets/0000.nxcook'
        original = cooked.read_bytes()
        corrupt = original[:-1]+bytes([original[-1] ^ 1])
        for payload, error in [
                (corrupt, 'project cooked hash/type/identity'),
                (b'bad source', 'project cooked hash/type/identity'),
                (bad_files['bad-id.nxcook'], 'project Sprite source dependency'),
                (bad_files['bad-dependency.nxcook'], 'project Sprite source dependency'),
                (bad_files['bad-region.nxcook'], 'project content registration'),
                ((relocated/'assets/0001.nxcook').read_bytes(), 'project cooked hash/type/identity')]:
            cooked.write_bytes(payload)
            run(False, error)
        cooked.write_bytes(original)
        manifest_path = relocated/'game.nx2game'
        for bad, error in (
                (manifest.replace('atlas "assets/0001.nxcook"\n', ''), 'project content registration'),
                (manifest+'sprite "assets/0000.nxcook"\n', 'invalid or duplicate project asset')):
            manifest_path.write_text(bad, encoding='utf-8')
            run(False, error)
        manifest_path.write_text(manifest, encoding='utf-8')
        # A deferred corrupt source fails at request publication rather than at catalog freeze.
        deferred = relocated/'assets/0002.nxcook'
        original_deferred = deferred.read_bytes()
        deferred.write_bytes(original_deferred[:-1]+bytes([original_deferred[-1] ^ 1]))
        run(False, 'Zig variable update')
        deferred.write_bytes(original_deferred)
        run(True)
        assert digest(relocated) == before
    print('PASS Sprite catalog: canonical cook, relocated preload/deferred IO, closure, opaque compatibility and failure preservation')


if __name__ == '__main__':
    main()
