#!/usr/bin/env python3
import json
import hashlib
import zipfile
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    script = Path(__file__).with_name("PackageShowcase.py")
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for name, data in (("showcase", b"exe"), ("gameplay.so", b"module"),
                           ("NexoraRuntime.dll", b"runtime"),
                           ("api.json", b"{}"), ("LICENSE", b"license")):
            (root / name).write_bytes(data)
        content_root = root / "content"
        content_root.mkdir()
        (content_root / "catalog.json").write_text('{"version":1}')
        output = root / "package"
        command = [sys.executable, str(script), "--profile", "Development", "--binary",
                   str(root / "showcase"), "--gameplay-module", str(root / "gameplay.so"),
                   "--runtime-libraries", str(root / "NexoraRuntime.dll"),
                   "--api-manifest", str(root / "api.json"), "--license", str(root / "LICENSE"),
                   "--output", str(output), "--content", str(content_root), "--archive"]
        subprocess.run(command, check=True)
        build = json.loads((output / "manifests/build.json").read_text())
        content = json.loads((output / "manifests/content.json").read_text())
        assert build["gameplay_linkage"] == "dynamic"
        assert "--gameplay-library=bin/gameplay.so" in build["launch"]
        assert {item["path"] for item in content["artifacts"]} == {
            "showcase", "gameplay.so", "NexoraRuntime.dll"}
        assert (output / "bin/NexoraRuntime.dll").read_bytes() == b"runtime"
        assert (output / "manifests/SHA256SUMS").read_text().count("\n") == 12
        assert content["showcase_content"][0]["path"] == "Content/Showcase/catalog.json"
        assert "--mode=interactive" in build["interactive_launch"]
        assert (output / "run-showcase.ps1").is_file()
        archive = output.with_suffix(".zip")
        before = hashlib.sha256(archive.read_bytes()).hexdigest()
        subprocess.run(command, check=True)
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
        with zipfile.ZipFile(archive) as zipped:
            assert "package/Content/Showcase/catalog.json" in zipped.namelist()
        mismatch = subprocess.run(command + ["--build-configuration", "Shipping"],
                                  text=True, capture_output=True, check=False)
        assert mismatch.returncode != 0
        shipping_command = [sys.executable, str(script), "--profile", "Shipping",
                            "--build-configuration", "Shipping", "--shipping-profile", "Full", "--binary",
                            str(root / "showcase"), "--api-manifest", str(root / "api.json"),
                            "--license", str(root / "LICENSE"), "--output", str(output)]
        subprocess.run(shipping_command, check=True)
        shipping = json.loads((output / "manifests/build.json").read_text())
        assert shipping["gameplay_linkage"] == "static"
        assert shipping["shipping_profile"] == "Full"
        assert "--gameplay-module=static" in shipping["launch"]
        assert not (output / "bin/gameplay.so").exists()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
