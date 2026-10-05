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

    for tier, name in enumerate(['basic','standard','high']):
        quality_report=Path(temporary)/f'{name}.json'
        completed=subprocess.run([sys.argv[1],'--headless','--scene=courtyard','--frames=1',
            f'--quality={name}','--pause-animation','--activate-device','--no-reload',
            '--gameplay-module=static',f'--report={quality_report}'],capture_output=True,text=True,timeout=15)
        assert completed.returncode==0,completed.stderr
        quality=json.loads(quality_report.read_text())
        settings=quality['runtime_rooms']['courtyard']
        assert quality['render_settings']['quality']==settings['quality']==name
        assert settings['shadow_resolution']==512*(2**tier)
        assert settings['particle_budget']==24*(2**tier)
        # Headless timeline checks do not construct or submit native courtyard geometry.
        assert settings['foliage_quad_count']==0 and settings['geometry_vertex_count']==0
        assert settings['animation_paused'] and settings['animation_seconds']==0
        assert settings['bloom_enabled']==(tier!=0)
    rejected=subprocess.run([sys.argv[1],'--headless','--quality=unknown'],capture_output=True,text=True)
    assert rejected.returncode!=0 and '--quality must be' in rejected.stderr
    print('PASS: quality CLI selects actual workload settings and rejects unknown tiers.')
