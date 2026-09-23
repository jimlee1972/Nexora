#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    script = Path(__file__).with_name("PackageShowcase.py")
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for name, data in (("showcase", b"exe"), ("gameplay.so", b"module"),
                           ("api.json", b"{}"), ("LICENSE", b"license")):
            (root / name).write_bytes(data)
        output = root / "package"
        command = [sys.executable, str(script), "--profile", "Development", "--binary",
                   str(root / "showcase"), "--gameplay-module", str(root / "gameplay.so"),
                   "--api-manifest", str(root / "api.json"), "--license", str(root / "LICENSE"),
                   "--output", str(output)]
        subprocess.run(command, check=True)
        build = json.loads((output / "manifests/build.json").read_text())
        content = json.loads((output / "manifests/content.json").read_text())
        assert build["gameplay_linkage"] == "dynamic"
        assert len(content["artifacts"]) == 2
        assert (output / "manifests/SHA256SUMS").read_text().count("\n") == 6
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
