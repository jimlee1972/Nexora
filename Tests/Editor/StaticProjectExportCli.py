"""Feed a real Editor producer artifact to the standalone Runtime player."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(arguments):
    return subprocess.run(arguments, capture_output=True, text=True, timeout=120)


def main():
    producer, player = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="nexora-editor-static-export-") as temporary:
        package = Path(temporary).resolve() / "專案 owning export.nxproject"
        generated = run([producer, "--write-package", str(package)])
        require(generated.returncode == 0, generated.stderr)
        original = package.read_bytes()
        verified = run([player, "--verify-package", str(package)])
        require(verified.returncode == 0, verified.stderr)
        report = json.loads(verified.stdout)
        require(report["status"] == "VERIFIED_STATIC_VIEW", "wrong consumer status")
        require(report["project"] == "12345678-9abc-def0-0000-00000000004d", "project UUID lost")
        require(report["scene_asset"] == "fedcba98-7654-3210-0000-000000000058", "scene UUID lost")
        require(report["asset_count"] == 3 and report["entity_count"] == 3, "closure/world lost")
        require(report["resolved_mesh_renderers"] == 2 and report["inactive_components"] == 1,
                "tracked/untracked renderers or preserved opaque data lost")
        require(report["native_rendering"] is False and report["gameplay_loaded"] is False,
                "StaticView consumer unexpectedly executes native/gameplay work")
        require(report["render_items"][0]["legacy_shader"] == str(2**64 - 1), "legacy ID truncated")
        require(report["render_items"][0]["scalar_pbr"] is True and
                report["render_items"][1]["scalar_pbr"] is False, "material/neutral binding changed")
        require(all(item["vertices"] == 3 and item["indices"] == 3 for item in report["render_items"]),
                "real imported geometry did not reach Runtime consumer")
        detail = report["inactive_component_details"][0]
        require(detail["type"] == "5" and bytes.fromhex(detail["type_name_hex"]) == b"legacy\xff",
                "opaque identity/name bytes changed")
        require(package.read_bytes() == original, "verification changed the generated artifact")
        repeated = run([producer, "--write-package", str(package)])
        require(repeated.returncode == 0 and package.read_bytes() == original,
                "independent owning captures/imports do not cook deterministically")
    print("Editor producer artifact verified by standalone StaticView ProjectPlayer")


if __name__ == "__main__":
    main()
