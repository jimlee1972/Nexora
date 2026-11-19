"""Native input and independent pixels for the same authored source used by Build."""
import hashlib
import re
import time

from LinuxSpriteSourceInspector import references
from LinuxSceneSprites import project as project_point


def require(value, message):
    if not value:
        raise RuntimeError(message)


def srgb8(linear):
    """Independent IEC61966-2-1 output oracle for the shared linear RGBA8 contract."""
    value = linear / 255
    encoded = 12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055
    return round(255 * encoded)


class SpriteSourceWorkflow:
    def __init__(self, root, scene, proof, driver, point, state, probe, capture, pixels):
        self.root, self.scene, self.proof = root, scene, proof
        self.driver, self.point, self.state = driver, point, state
        self.probe, self.capture, self.pixels = probe, capture, pixels
        self.original = scene.read_bytes()
        self.reference = references(self.original)
        require(len(self.reference) == 1, 'workflow fixture requires one source-bound entity')
        target = re.search(rb'^node (\d+) 0 First$', self.original, re.M)
        require(target is not None, 'workflow source entity absent')
        self.target = target[1].decode() + ','
        proof['same_project_workflow'] = {'scene': 'Authored.scene', 'play': [], 'reimports': []}
        self.record = proof['same_project_workflow']

    def observation(self, prefix):
        rows = re.findall(rb'^' + prefix.encode() + rb' (.*)$', self.driver().stderr, re.M)
        return dict(re.findall(r'(\w+)=([^ ]*)', rows[-1].decode())) if rows else {}

    def wait(self, predicate, message):
        self.driver().wait(lambda _: predicate(), message)

    def open_scene(self):
        driver = self.driver()
        deadline = time.monotonic() + 10
        driver.file_open(deadline)
        value = driver.wait(lambda v: v.get('file_dialog') == '1' and
                            v.get('file_path_ready') == '1' and 'file_path_point' in v,
                            'workflow Open Scene modal absent', deadline)
        driver.click(tuple(round(float(x)) for x in value['file_path_point'].split(',')))
        driver.wait(lambda v: v.get('file_path_active') == '1' and v.get('text_input') == '1',
                    'workflow scene field not focused', deadline)
        driver.chord('ctrl+a', deadline)
        driver.raw('type', '--clearmodifiers', '--delay', '2', 'Authored.scene', deadline=deadline)
        driver.wait(lambda v: v.get('file_path_hex') == b'Authored.scene'.hex(),
                    'workflow scene path not acknowledged', deadline)
        driver.chord('Return', deadline)
        driver.wait(lambda v: v.get('file_dialog') == '0', 'workflow scene open did not finish', deadline)

    def save(self, expected):
        self.driver().chord('ctrl+s')
        self.wait(lambda: self.scene.read_bytes() == expected and self.state().get('dirty') == '0',
                  'GUI Save did not publish the enrolled scene bytes')

    def select_source(self):
        self.probe()
        state = self.state()
        point = project_point((0, 3, 0), tuple(map(float, state['camera'].split(','))),
                              tuple(map(int, state['viewport'].split(','))))
        self.driver().click(point)
        self.wait(lambda: self.state().get('selected') == self.target,
                  'native picking did not select the source entity')

    def assign(self):
        self.select_source()
        self.point('sprite_source_combo')
        self.driver().click(self.point('sprite_source_clear'))
        self.driver().chord('ctrl+s')
        self.wait(lambda: not references(self.scene.read_bytes()) and self.state().get('dirty') == '0',
                  'source Clear did not reach the Build-enrolled scene')
        cleared = self.scene.read_bytes()
        require(cleared != self.original, 'Clear did not remove the original source')
        self.driver().chord('ctrl+z')
        self.save(self.original)
        self.driver().chord('ctrl+shift+z')
        self.save(cleared)
        self.driver().click(self.point('sprite_source_combo'))
        self.driver().click(self.point('sprite_source_first'))
        self.save(self.original)
        self.driver().chord('ctrl+z')
        self.save(cleared)
        self.driver().chord('ctrl+shift+z')
        self.save(self.original)
        self.capture('workflow-assigned')
        self.record.update(assignment_exact=True, undo_exact=True, redo_exact=True,
                           assigned_scene_sha256=hashlib.sha256(self.scene.read_bytes()).hexdigest(),
                           reference_hex=self.reference[0].decode())

    def reopened(self):
        require(self.scene.read_bytes() == self.original and references(self.scene.read_bytes()) ==
                self.reference, 'same scene lost its GUI-saved source reference after reopen')
        self.capture('workflow-reopened')
        self.record['same_scene_reopened'] = True

    def play(self, expected):
        linear = tuple(expected)
        expected = tuple(srgb8(channel) for channel in linear)
        driver = self.driver()
        scene_pixel = self.probe()
        before_scene = self.scene.read_bytes()
        driver.play_key('F5')
        # A saved SceneOnly configuration uses the owning packaged Play provider. Its source
        # snapshot must publish a native frame; the separate module-free provider is not involved.
        driver.raw('key', 'ctrl+2')
        self.wait(lambda: self.observation('packaged sprite ui').get('state') == '1' and
                  self.observation('packaged sprite ui').get('module') == '0' and
                  self.observation('packaged sprite ui').get('textures') == '1' and
                  self.observation('packaged sprite ui').get('visible') == '1' and
                  int(self.observation('packaged sprite ui').get('commits', '0')) >= 1 and
                  'viewport' in self.observation('packaged sprite ui'),
                  'same-project configured Play did not publish its native source frame')
        driver.play_key('F6')
        self.wait(lambda: self.observation('packaged sprite ui').get('state') == '2', 'Play did not pause')
        viewport = tuple(map(int, self.observation('packaged sprite ui')['viewport'].split(',')))
        width, height = viewport[2:]
        samples = []
        def check_pixels():
            data = self.pixels(viewport)
            samples.clear()
            # Public default 2D camera: vertical size20, source bounds[-2,2]x[1,5].
            for x, y in [(-1, 2), (-1, 4), (0, 3), (1, 2), (1, 4)]:
                px, py = int(width / 2 + x * height / 20), int(height / 2 - y * height / 20)
                offset = (py * width + px) * 3
                actual = tuple(data[offset:offset + 3])
                samples.append({'world': [x, y], 'rgb': actual})
            self.record['last_play_probe'] = {'viewport': viewport, 'expected_rgb': expected,
                                               'samples': list(samples)}
            return all(len(sample['rgb']) == 3 and all(abs(a-b) <= 1 for a, b in
                       zip(sample['rgb'], expected)) for sample in samples)
        try:
            self.wait(check_pixels, 'GUI-assigned source differs from independent native Game pixels')
        except RuntimeError:
            self.capture('workflow-play-failure')
            raise
        steps = self.observation('packaged sprite ui')['steps']
        driver.play_key('F10')
        self.wait(lambda: int(self.observation('packaged sprite ui')['steps']) == int(steps) + 1,
                  'same-project paused Step was not applied once')
        self.wait(check_pixels, 'paused Step changed the source pixels')
        self.capture('workflow-play-' + str(len(self.record['play']) + 1))
        self.record['play'].append({'viewport': viewport, 'expected_linear_rgb': linear,
                                    'expected_rgb': expected,
                                    'samples': list(samples), 'provider': 'configured-scene-only', 'module': False, 'step_exact': True})
        driver.play_key('F5')
        self.wait(lambda: self.observation('packaged sprite ui').get('state') == '0' and
                  self.observation('packaged sprite ui').get('textures') == '0' and
                  self.observation('packaged sprite ui').get('commits') == '0',
                  'Stop retained same-project Play assets')
        require(self.scene.read_bytes() == before_scene and self.probe() == scene_pixel,
                'isolated Play changed the saved scene or authoring pixels')

    def reimport(self, data, dominant, failure=None):
        atlas = self.root / 'Content/Atlas.nx2atlas'
        before_pixel = self.probe()
        if data is not None:
            atlas.write_bytes(data)
        driver = self.driver()
        before = self.observation('project asset workflow')
        point = self.point('atlaspoint', self.state)
        driver.send('mousemove', '--window', driver.window, *point, 'click', '3')
        # The popup can move up from the window bottom after its first hidden sizing frame.
        # Settle the actual current menu geometry before one acknowledged native click.
        driver.click(self.point('reimport', self.state))
        if failure:
            # Missing primaries are rejected by admission before a reimport job exists.
            error = ('asset source is unavailable or is a symlink' if failure == 'missing'
                     else 'Invalid 2D Atlas source.').encode().hex()
            self.wait(lambda: self.observation('project asset workflow').get('busy') == '0' and
                      self.observation('project asset workflow').get('error_hex') == error and
                      (failure == 'missing' or
                       self.observation('project asset workflow').get('reimport_state') == '6'),
                      failure + ' dependency did not fail through actual Reimport')
        else:
            self.wait(lambda: self.observation('project asset workflow').get('busy') == '0' and
                      self.observation('project asset workflow').get('reimport_state') == '4' and
                      int(self.observation('project asset workflow').get('revision', '0')) ==
                      int(before['revision']) + 1,
                      'Reimport did not publish exactly one new Content revision')
        def admitted_pixels():
            value = self.probe()
            return value[dominant] > value[(dominant + 1) % 3] * 2
        self.wait(admitted_pixels, 'reimport did not retain/admit expected native pixels')
        after = self.observation('project asset workflow')
        require(after['generation'] == before['generation'], 'Reimport replaced the project')
        require(self.state().get('selected') == self.target, 'Reimport changed source selection')
        if failure:
            require(after['revision'] == before['revision'], 'failed Reimport published a revision')
            require(self.probe() == before_pixel, 'failed Reimport changed last-good Scene pixels')
        require(self.scene.read_bytes() == self.original, 'reimport changed GUI-saved source refs')
        if failure:
            self.capture('workflow-rejected-' + failure)
        self.record['reimports'].append({'failure': failure, 'scene_sha256':
            hashlib.sha256(self.scene.read_bytes()).hexdigest(), 'dominant_channel': dominant,
            'revision_before': before['revision'], 'revision_after': after['revision'],
            'project_generation': after['generation'], 'selection': self.state()['selected'],
            'error_hex': after.get('error_hex', '')})

    def reimport_roundtrip(self, original_atlas, edited_atlas):
        self.select_source()
        self.reimport(original_atlas, 0)
        self.play((240, 20, 20))
        self.reimport(edited_atlas, 1)
        self.play((20, 240, 20))
        self.reimport(b'corrupt Atlas', 1, 'corrupt')
        self.reimport(edited_atlas, 1)
        atlas = self.root / 'Content/Atlas.nx2atlas'
        missing = atlas.with_suffix('.missing')
        atlas.rename(missing)
        try:
            self.reimport(None, 1, 'missing')
        finally:
            missing.rename(atlas)
        self.reimport(edited_atlas, 1)
        self.capture('workflow-reimported')
