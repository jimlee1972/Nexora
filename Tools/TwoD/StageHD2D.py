#!/usr/bin/env python3
"""Stage a persistent DEV ONLY Zig HD2D Player package, including font notices/inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import ExportGame as exporter

ROOT = Path(__file__).resolve().parents[2]


def stage(app, module, output, width=1920, height=1080, development=False):
    app, module = app.resolve(), module.resolve()
    with tempfile.TemporaryDirectory(prefix='nexora-hd2d-stage-') as temporary:
        source = Path(temporary)
        shutil.copyfile(module, source/module.name)
        descriptor = dict(schema=1, id='com.nexora.hd2d.diagnostic', title='DEV ONLY HD2D Zig', scene='Main',
            module=module.name, audio='off', display=dict(width=width, height=height, mode='windowed', resizable=True))
        (source/'game.json').write_text(json.dumps(descriptor), encoding='utf-8')
        exporter.export(source/'game.json', app, output, allow_development=development)
    inventory_path = output/'inventory.json'
    inventory = json.loads(inventory_path.read_text(encoding='utf-8'))
    # Extend the trusted freshly generated inventory; never discard or rewrite existing entries.
    for name, digest in inventory['files'].items():
        if hashlib.sha256((output/name).read_bytes()).hexdigest() != digest:
            raise ValueError('export inventory changed before notice publication')
    for original, name in [('OFL.txt', 'Noto-CJK-OFL.txt'), ('NOTICE.md', 'Noto-CJK-NOTICE.md')]:
        target = output/'Licenses'/name
        data = (ROOT/'Bindings/Zig/diagnostic'/original).read_bytes()
        target.parent.mkdir(exist_ok=True)
        if target.exists() and target.read_bytes() != data:
            raise ValueError('diagnostic font notice conflicts with existing package bytes')
        target.write_bytes(data)
        inventory['files'][target.relative_to(output).as_posix()] = hashlib.sha256(data).hexdigest()
    inventory_path.write_text(json.dumps(inventory, indent=2, sort_keys=True)+'\n', encoding='utf-8')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--allow-development', action='store_true')
    args = parser.parse_args()
    stage(args.app, args.module, args.output.resolve(), development=args.allow_development)
    print(f'DEV ONLY diagnostic package: {args.output}')


if __name__ == '__main__':
    main()
