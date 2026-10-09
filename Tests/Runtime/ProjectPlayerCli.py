"""Exercise the production static package reader, independently of Editor/source content."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(args):
    return subprocess.run(args, capture_output=True, text=True, timeout=30)


def main():
    fixture, player = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="nexora-static-project-") as temporary:
        root = Path(temporary).resolve()
        package = root / "專案 package with spaces.nxproject"
        generated = run([fixture, "--write-fixture", str(package)])
        assert generated.returncode == 0, generated.stderr
        verified = run([player, "--verify-package", str(package)])
        assert verified.returncode == 0, verified.stderr
        report = json.loads(verified.stdout)
        assert report["status"] == "VERIFIED_STATIC_VIEW"
        assert report["entity_count"] == 2 and report["resolved_mesh_renderers"] == 2
        assert report["asset_count"] == 4 and report["inactive_components"] == 1
        assert report["inactive_component_details"][0]["type"] == "111"
        assert bytes.fromhex(report["inactive_component_details"][0]["type_name_hex"]) == b"old.plugin.component"
        assert report["native_rendering"] is False and report["gameplay_loaded"] is False
        assert report["render_items"][0]["legacy_shader"] == str(2**64 - 8)
        assert all(item["vertices"] == 3 and item["scalar_pbr"] for item in report["render_items"])
        large_package = root / "large static project.nxproject"
        assert run([fixture, "--write-large-fixture", str(large_package)]).returncode == 0
        large = run([player, "--verify-package", str(large_package)])
        assert large.returncode == 0, large.stderr
        large_report = json.loads(large.stdout)
        assert large_report["resolved_mesh_renderers"] == 1000
        assert len(large_report["render_items"]) == 64
        assert large_report["unreported_render_items"] == 936
        assert all(item["scalar_pbr"] is False for item in large_report["render_items"])
        damaged = root / "corrupt.nxproject"
        data = bytearray(package.read_bytes())
        data[len(data) // 2] ^= 1
        damaged.write_bytes(data)
        assert run([player, "--verify-package", str(damaged)]).returncode != 0
        damaged.write_bytes(data[:59])
        assert run([player, "--verify-package", str(damaged)]).returncode != 0
        assert run([player, "--verify-package", str(root)]).returncode != 0
        assert run([player, "--verify-package", str(root / "missing")]).returncode != 0
        oversized = root / "oversized.nxproject"
        with oversized.open("wb") as output:
            output.truncate(256 * 1024 * 1024 + 1)
        assert run([player, "--verify-package", str(oversized)]).returncode != 0
        if hasattr(os, "mkfifo"):
            fifo = root / "not-a-regular-file"
            os.mkfifo(fifo)
            assert run([player, "--verify-package", str(fifo)]).returncode != 0
        alias = root / "alias.nxproject"
        try:
            alias.symlink_to(package)
        except (OSError, NotImplementedError):
            pass
        else:
            assert run([player, "--verify-package", str(alias)]).returncode != 0
        directory_alias = root / "directory-alias"
        try:
            directory_alias.symlink_to(root, target_is_directory=True)
        except (OSError, NotImplementedError):
            pass
        else:
            assert run([player, "--verify-package", str(directory_alias / package.name)]).returncode != 0
        assert run([player]).returncode == 2
    print("Production static project CLI verified owning cooked artifacts and failure paths")


if __name__ == "__main__":
    main()
