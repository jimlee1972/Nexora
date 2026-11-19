"""Real SDK/Content updates share one published Play World; no admission hooks are used."""
import hashlib
import os
import re
import subprocess

from LinuxAuthoredPreview import screenshot
from LinuxPackagedSprites import native_pixels, require, source_mask


def run_interleaved(*, args, project, root, evidence, proof, display, window, viewport, baseline,
                    center, log, core_text, wait, reimport, observe, authored_hashes, original,
                    expected_mask, authoring_pixels):
    script = project / 'Scripts/game.zig'
    template = script.read_text()
    expected_inputs = dict(original)
    identity = None
    last_ticks = 0
    published = 0
    admission = re.compile(r'script reload admission: .*applied=(\d) generation=(\d+) ')
    record = proof['interleaved'] = {'cycles': [], 'failed_imports': []}

    def inputs():
        require(authored_hashes() == expected_inputs,
                'interleaved updates changed an uncontrolled authored input')

    def expect_file(path):
        expected_inputs[str(path.relative_to(project))] = hashlib.sha256(path.read_bytes()).hexdigest()
        inputs()

    def wanted(variant):
        result = expected_mask(viewport[2], viewport[3], 0, pivot=2 + variant)
        return bytes(0 if value == 0 else ((value - 1) ^ 1) + 1 for value in result) if variant else result

    def rendered(mask):
        wait(lambda _: source_mask(native_pixels(display, window, viewport)) == mask,
             'interleaved source/pool frame differs from independent geometry/color oracle')
        actual = native_pixels(display, window, viewport)
        require(source_mask(actual) == mask and
                all(actual[index:index + 3] == baseline[index:index + 3] for index in center),
                'interleaved updates changed source/pool or independent3D pixels')
        inputs()

    def ticks(code):
        rows = re.findall(rf'MAIN INTERLEAVED TICK code={code} ticks=(\d+)', core_text())
        return int(rows[-1]) if rows else 0

    def request(code, fault=0):
        progress = log.read_text().count('script reload progress:')
        count = len(admission.findall(log.read_text()))
        text = template.replace('fixture_generation: u32 = 0;',
                                f'fixture_generation: u32 = {code};').replace(
                                'fixture_fault: u32 = 0;', f'fixture_fault: u32 = {fault};')
        temporary = root / 'interleaved-script.pending'
        temporary.write_text(text)
        os.replace(temporary, script)
        expect_file(script)
        wait(lambda _: log.read_text().count('script reload progress:') > progress,
             'saved interleaved script did not enter the real background compiler', timeout=90)
        return count

    def still_compiling(count, candidate, old_code, before_ticks):
        wait(lambda _: ticks(old_code) > before_ticks,
             'the old module stopped dispatching during pending compilation')
        require(len(admission.findall(log.read_text())) == count and
                f'MAIN SOURCE LIVE code={candidate} ' not in core_text(),
                'fixture did not interleave source publication before candidate admission')
        return ticks(old_code)

    def live(code, accepted, mask, scene_before, minimum_ticks=0):
        nonlocal identity, last_ticks
        pattern = re.compile(rf'MAIN SOURCE LIVE code={code} reloads=(\d+) ticks=(\d+) '
                             r'scene=(\d+) ids=(\d+,\d+) pool=(\d+:\d+:\d+) phase=(\d+)')
        wait(lambda _: pattern.search(core_text()) is not None and
             admission.findall(log.read_text())[-1:] == [('1', str(accepted + 1))],
             f'interleaved SDK generation{code} not published/dispatched', timeout=90)
        match = pattern.search(core_text())
        count, current_ticks, scene, ids, pool, phase = match.groups()
        current_identity = (scene, ids, pool)
        require(int(count) == accepted and int(current_ticks) > max(last_ticks, minimum_ticks) and phase == '0',
                'interleaved module reset counters/animation phase')
        if identity is None:
            identity = current_identity
        require(current_identity == identity, 'interleaved reload changed scene/entity/pool identity')
        last_ticks = int(current_ticks)
        rendered(mask)
        require(authoring_pixels() == scene_before, 'module replacement changed authoring pixels')
        proof['reloads'].append(dict(code=code, accepted=accepted, ticks=last_ticks,
                                     scene=scene, ids=ids, pool=pool, runtime_generation=accepted + 1,
                                     minimum_previous_ticks=minimum_ticks))

    def replace_source(kind, variant):
        command = [args.fixture, '--reimport-' + kind, str(project), str(variant)]
        proof['commands'].append(command)
        subprocess.run(command, check=True, capture_output=True, timeout=10)
        path = project / ('Content/Color.nx2atlas' if kind == 'atlas' else 'Content/Source.nx2sprite')
        expect_file(path)

    def publish(kind):
        nonlocal published
        reimport(kind, published + 1)
        published += 1
        require(observe('packaged sprite ui').get('reimports') == str(published),
                'interleaved source generation was reset by module replacement')

    initial_scene = authoring_pixels()
    live(0, 0, wanted(0), initial_scene)
    for code in range(1, 5):
        variant = code % 2
        before_ticks = ticks(code - 1)
        count = request(code)
        for kind in ('atlas', 'source'):
            replace_source(kind, variant)
            publish(kind)
        mask = wanted(variant)
        rendered(mask)
        continued_ticks = still_compiling(count, code, code - 1, before_ticks)
        scene_before = authoring_pixels()
        live(code, code, mask, scene_before, continued_ticks)
        screenshot(display, window, evidence / f'interleaved-{code}.png')
        record['cycles'].append(dict(code=code, source_publications=published,
                                      old_ticks_before=before_ticks, old_ticks_after=continued_ticks,
                                      mask_sha256=hashlib.sha256(mask).hexdigest()))

    # A failed source import and a rejected hidden-pool mutation share the same pending build.
    count = request(5, fault=7)
    atlas = project / 'Content/Color.nx2atlas'
    valid_atlas = atlas.read_bytes()
    before_scene = authoring_pixels()
    atlas.write_bytes(b'corrupt Atlas')
    expect_file(atlas)
    reimport('atlas', published, failed=True)
    rendered(wanted(0))
    require(authoring_pixels() == before_scene, 'corrupt import changed last-good authoring pixels')
    record['failed_imports'].append('corrupt Atlas')
    atlas.write_bytes(valid_atlas)
    expect_file(atlas)
    publish('atlas')
    rendered(wanted(0))
    still_compiling(count, 5, 4, ticks(4))
    wait(lambda _: len(admission.findall(log.read_text())) > count,
         'interleaved hidden-pool candidate did not complete', timeout=90)
    require(admission.findall(log.read_text())[-1] == ('0', '5') and
            'MAIN SOURCE LIVE code=5 ' not in core_text() and
            'MAIN INTERLEAVED TICK code=5 ' not in core_text(),
            'invalid candidate escaped the live generation boundary')
    proof['rejected_reloads'].append(dict(code=5, fault='hidden pool during source reimport'))
    before_ticks = ticks(4)
    wait(lambda _: ticks(4) > before_ticks, 'rejected candidate stopped the old module')
    rendered(wanted(0))

    # Missing-primary rejection must retain the current Game while the next actual SDK compiles.
    count = request(6)
    before_ticks = ticks(4)
    source = project / 'Content/Source.nx2sprite'
    missing = root / 'temporarily-missing-source'
    before_scene = authoring_pixels()
    source.rename(missing)
    expected_inputs.pop('Content/Source.nx2sprite')
    try:
        reimport('source', published, failed='missing')
        rendered(wanted(0))
        require(authoring_pixels() == before_scene, 'missing import changed last-good authoring pixels')
        record['failed_imports'].append('missing Sprite source')
    finally:
        missing.rename(source)
        expect_file(source)
    for kind in ('atlas', 'source'):
        replace_source(kind, 1)
        publish(kind)
    rendered(wanted(1))
    continued_ticks = still_compiling(count, 6, 4, before_ticks)
    live(6, 5, wanted(1), authoring_pixels(), continued_ticks)
    for kind in ('atlas', 'source'):
        replace_source(kind, 0)
        publish(kind)
    rendered(wanted(0))
    require(authoring_pixels() == initial_scene, 'restored sources changed the authoring frame')
    require(all(value == original[path] for path, value in expected_inputs.items()
                if path != 'Scripts/game.zig'), 'controlled source roundtrip did not restore authored data')
    finalizers = re.findall(r'MAIN SOURCE CALLBACK code=(\d+) stage=finalizer '
                           r'entity=(-?\d+) pool=(-?\d+) world=(-?\d+)', core_text())
    require(all(any(int(row[0]) == code for row in finalizers) for code in range(6)),
            'interleaved retired/rejected library finalizers were not observed')
    screenshot(display, window, evidence / 'interleaved-recovered.png')
    record.update(successful_replacements=5, reimport_publications=published,
                  retired_library_finalizers=finalizers, identities_unchanged=True,
                  independent_source_pool_and_3d_pixels=True, old_module_continued=True)
    return expected_inputs
