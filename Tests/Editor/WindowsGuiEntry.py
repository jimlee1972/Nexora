"""Real GUI PE/entrypoint, redirected CLI and Unicode-argument regression; no UI input."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Tools/Editor'))
from PackageWindowsEditor import require_gui_subsystem

editor = Path(sys.argv[1])
require_gui_subsystem(editor)
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
        if arguments[0] == '--help':
            assert b'--create-project=' in result.stdout, result.stdout
        else:
            assert json.loads(result.stdout)['project'] == 'GUI 陰陽 project', result.stdout
print('Windows GUI subsystem, console-free entry, redirected CLI and Unicode arguments passed')
