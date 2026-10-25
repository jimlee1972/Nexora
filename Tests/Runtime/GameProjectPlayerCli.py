#!/usr/bin/env python3
"""Consume a cooked two-scene catalog without any authoring scene/source tree."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

fixture, player = (str(Path(path).resolve()) for path in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="nexora-game-catalog-") as temporary:
    root = Path(temporary)
    package = root / "Game.nxgame"
    subprocess.run([fixture, "--write", str(package)], check=True, cwd=root)
    result = subprocess.run([player, "--verify-game-package", str(package)], check=True,
                            cwd=root, text=True, capture_output=True)
    data = json.loads(result.stdout)
    assert data["status"] == "VERIFIED_GAME_PACKAGE", data
    assert data["scene_count"] == 2 and data["asset_count"] == 4, data
    assert data["start_scene"].replace("-", "").endswith("0000000000000002"), data
    assert not data["native_rendering"] and not data["gameplay_loaded"], data
    assert not list(root.glob("*.scene")), "consumer must need no authored scene files"
    legacy = subprocess.run([player, "--verify-package", str(package)], cwd=root,
                            capture_output=True)
    assert legacy.returncode == 1, legacy
    package.write_bytes(package.read_bytes()[:-1])
    broken = subprocess.run([player, "--verify-game-package", str(package)], cwd=root,
                            capture_output=True)
    assert broken.returncode == 1, broken
print("Runtime-only source-free multi-scene verification passed")
