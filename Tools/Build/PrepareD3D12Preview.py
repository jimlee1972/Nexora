"""Stage opt-in, hash-pinned public Windows x64 preview packages outside the source tree."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

PACKAGES = {
    "agility": (
        "https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.d3d12/1.721.3-preview/microsoft.direct3d.d3d12.1.721.3-preview.nupkg",
        "0131bce1e4bace3fc08c03018c29a09ede2570b263721c6449b0ec75762ab22d"),
    "warp": (
        "https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.warp/1.65535.20-preview/microsoft.direct3d.warp.1.65535.20-preview.nupkg",
        "dc935a30e13e35ff4d4a270889b2c1282bdd5e3dee79a4bc7934fc87351111bc"),
    "dxc": (
        "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.10.2605.37/dxc_2026_08_11.zip",
        "18da85ccac4d1736209bad960f0a124928a5c5a6046882e331c605d66744a889"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    root = args.destination.resolve()
    root.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, (url, expected) in PACKAGES.items():
        archive = root / (name + ".zip")
        data = archive.read_bytes() if archive.exists() else b""
        if hashlib.sha256(data).hexdigest() != expected:
            request = urllib.request.Request(url, headers={"User-Agent": "Nexora-preview-bootstrap"})
            with urllib.request.urlopen(request, timeout=180) as response:
                data = response.read(128 * 1024 * 1024 + 1)
            if len(data) > 128 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f"{name}: package size or SHA-256 mismatch")
            archive.write_bytes(data)
        destination = root / name
        with zipfile.ZipFile(io.BytesIO(data)) as package:
            for member in package.infolist():
                path = (destination / member.filename).resolve()
                if not path.is_relative_to(destination.resolve()):
                    raise ValueError(f"{name}: archive path escapes destination")
            package.extractall(destination)
        manifest[name] = {"url": url, "sha256": expected}
        print(f"{name}: verified {expected}")
    (root / "packages.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
