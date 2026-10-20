"""Check complete driver discovery and fail-closed canonical ordering."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "order", Path(__file__).resolve().parents[1] / "PrepareVulkanICDOrder.py")
order = importlib.util.module_from_spec(spec)
spec.loader.exec_module(order)


def manifest(path, library):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"file_format_version": "1.0.1",
                                "ICD": {"library_path": library, "api_version": "1.4.318"}}))


class ICDOrderTests(unittest.TestCase):
    def test_preserves_every_driver_across_directories_and_creation_orders(self):
        names = ["virtio", "gfxstream", "radeon", "asahi", "lvp", "nouveau", "intel", "intel_hasvk"]
        for rotation in range(len(names)):
            with self.subTest(rotation=rotation), tempfile.TemporaryDirectory(prefix="ICD set ") as d:
                roots = [Path(d) / "system", Path(d) / "configured"]
                files = []
                for index, name in enumerate(names[rotation:] + names[:rotation]):
                    path = roots[index % 2] / (name + ".json")
                    manifest(path, "libvulkan_" + name + ".so")
                    files.append(path.absolute())
                prepared = order.ordered_manifests(roots + roots, {})
                self.assertEqual(set(prepared), set(files))
                self.assertEqual(len(prepared), 8)
                self.assertEqual(prepared[0].name, "lvp.json")
                self.assertEqual(prepared[1:], sorted(p for p in files if p.name != "lvp.json"))

    def test_refuses_missing_ambiguous_and_malformed_descriptors(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            with self.assertRaises(ValueError):
                order.ordered_manifests([root], {})
            manifest(root / "lvp.json", "libvulkan_lvp.so")
            manifest(root / "other-lvp.json", "/lib/libvulkan_lvp.so")
            with self.assertRaises(ValueError):
                order.ordered_manifests([root], {})
            (root / "other-lvp.json").write_text("{broken")
            with self.assertRaises(ValueError):
                order.ordered_manifests([root], {})
            (root / "other-lvp.json").write_text('{"file_format_version":"1.0.1","ICD":{}}')
            with self.assertRaises(ValueError):
                order.ordered_manifests([root], {})

    def test_preserves_existing_selector_intent_by_refusing_override(self):
        for selector in ("VK_DRIVER_FILES", "VK_ICD_FILENAMES", "VK_ADD_DRIVER_FILES",
                         "VK_LOADER_DRIVERS_SELECT", "VK_LOADER_DRIVERS_DISABLE"):
            with self.subTest(selector=selector), self.assertRaises(ValueError):
                order.ordered_manifests([], {selector: "existing choice"})

    def test_includes_configured_default_discovery_roots(self):
        roots = order.discovery_directories({"XDG_CONFIG_HOME": "/custom-config",
                                             "XDG_CONFIG_DIRS": "/site-config:/etc/xdg",
                                             "XDG_DATA_HOME": "/custom-data",
                                             "XDG_DATA_DIRS": "/site-share:/usr/share"}, Path("/home/test"))
        self.assertEqual(roots, [Path(p) / "vulkan/icd.d" for p in [
            "/custom-config", "/site-config", "/etc/xdg", "/etc", "/custom-data",
            "/site-share", "/usr/share"]])
        with self.assertRaises(ValueError):
            order.discovery_directories({"XDG_DATA_HOME": "relative"}, Path("/home/test"))


if __name__ == "__main__":
    unittest.main()
