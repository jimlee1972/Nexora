"""Real GUI PE/entrypoint, redirected CLI and Unicode-argument regression; no UI input."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Tools/Editor'))
from PackageWindowsEditor import require_gui_subsystem
from EmbeddedBrand import require_embedded_brand

editor = Path(sys.argv[1])
require_gui_subsystem(editor)
require_embedded_brand(editor)
environment = dict(os.environ, NEXORA_EDITOR_STARTUP_EVIDENCE='1')
with tempfile.TemporaryDirectory(prefix='Nexora GUI entry 陰陽 ') as temporary:
    root = Path(temporary)
    for arguments in (['--help'], ['--create-project=' + str(root / '專案'),
                                   '--project-name=GUI 陰陽 project',
                                   '--recent-projects=' + str(root / 'recent.json')]):
        result = subprocess.run([str(editor), *arguments], cwd=root, env=environment,
                                capture_output=True, timeout=30)
        assert result.returncode == 0, (result.returncode, result.stderr)
        assert b'editor GUI startup: console_attached=0' in result.stderr, result.stderr
        assert b'editor splash:' not in result.stderr, result.stderr
        if arguments[0] == '--help':
            assert b'--create-project=' in result.stdout, result.stdout
        else:
            assert json.loads(result.stdout)['project'] == 'GUI 陰陽 project', result.stdout
    failed = subprocess.run([str(editor), '--graphical', '--project=' + str(root/'missing')],
                            cwd=root,env=environment,capture_output=True,timeout=30)
    assert failed.returncode == 1, failed.stderr
    assert b'editor splash: opened' in failed.stderr and b'editor splash: closed' in failed.stderr, failed.stderr
    assert b'editor splash: stage=2' in failed.stderr, failed.stderr
    assert b'editor splash: main-frame-ready' not in failed.stderr, failed.stderr
    assert b'content indexing or project open failed:' in failed.stderr, failed.stderr

print('Windows GUI subsystem, console-free entry, redirected CLI and Unicode arguments passed')
