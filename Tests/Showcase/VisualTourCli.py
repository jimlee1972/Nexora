"""Verify the full deterministic CLI timeline; this headless gate is not native visual proof."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='nexora-visual-tour-cli-') as temporary:
    report=Path(temporary)/'tour.json'
    result=subprocess.run([sys.argv[1],'--headless','--tour=visual','--frames=6100',
                           '--no-reload','--gameplay-module=static',f'--report={report}'],
                          capture_output=True,text=True,timeout=30)
    if result.returncode: raise RuntimeError(result.stderr)
    data=json.loads(report.read_text())
    tour=data['runtime_rooms']['tour']
    assert data['status']=='PASS' and data['scene']=='courtyard'
    assert tour['kind']=='visual' and tour['duration_seconds']==100
    assert tour['seconds']==100 and tour['paused'] and tour['step']==4
    assert data['runtime_rooms']['courtyard']['device_active']
    assert data['headless_evidence']['executed'] and not data['windowed_evidence']['executed']
    assert 5999<=data['lifecycle']['frames']<=6002
    print('PASS: 100-second visual timeline completed, finale activated, automatic stop and lifecycle verified (headless).')
