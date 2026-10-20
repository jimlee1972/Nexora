#!/usr/bin/env python3
"""Order the complete discovered Linux ICD set for reproducible sanitizer execution."""
import argparse
import json
import os
from pathlib import Path


def discovery_directories(environment: dict, home: Path) -> list[Path]:
    config = environment.get("XDG_CONFIG_HOME") or str(home / ".config")
    data = environment.get("XDG_DATA_HOME") or str(home / ".local/share")
    configs = (environment.get("XDG_CONFIG_DIRS") or "/etc/xdg").split(":")
    data_dirs = (environment.get("XDG_DATA_DIRS") or "/usr/local/share:/usr/share").split(":")
    roots = [Path(config), *(Path(p) for p in configs if p), Path("/etc"), Path(data),
             *(Path(p) for p in data_dirs if p)]
    if any(not path.is_absolute() for path in roots):
        raise ValueError("Vulkan XDG discovery directories must be absolute")
    return [path / "vulkan/icd.d" for path in roots]


def ordered_manifests(directories: list[Path], environment: dict) -> list[Path]:
    selectors = ("VK_DRIVER_FILES", "VK_ICD_FILENAMES", "VK_ADD_DRIVER_FILES",
                 "VK_LOADER_DRIVERS_SELECT", "VK_LOADER_DRIVERS_DISABLE")
    if any(environment.get(name) for name in selectors):
        raise ValueError("Existing Vulkan driver selectors must not be silently replaced")
    files = sorted({path.absolute() for directory in directories
                    for path in directory.glob("*.json")})
    if not files:
        raise ValueError("No Vulkan ICD manifests found")
    lavapipe = []
    for path in files:
        if any(character in str(path) for character in (":", "\n", "\r")):
            raise ValueError("ICD path cannot be represented as a driver-file list")
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, dict) or data.get("file_format_version") not in ("1.0.0", "1.0.1"):
            raise ValueError(f"Unsupported ICD manifest format: {path.name}")
        icd = data.get("ICD") if isinstance(data, dict) else None
        library = icd.get("library_path") if isinstance(icd, dict) else None
        if not isinstance(library, str) or not library:
            raise ValueError(f"Invalid ICD library identity: {path.name}")
        if Path(library).name == "libvulkan_lvp.so":
            lavapipe.append(path)
    if len(lavapipe) != 1:
        raise ValueError("Exactly one lavapipe ICD is required")
    return lavapipe + [path for path in files if path != lavapipe[0]]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--icd-dir", type=Path, action="append")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    try:
        directories = args.icd_dir or discovery_directories(os.environ, Path.home())
        files = ordered_manifests(directories, os.environ)
        print(json.dumps({"scope": "complete discovered ICD set; lavapipe first",
                          "count": len(files), "ordered_manifests": [str(p) for p in files]}))
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as output:
                output.write("driver_files=" + ":".join(str(p) for p in files) + "\n")
    except (OSError, ValueError) as error:
        print(f"FAIL: Vulkan ICD ordering: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
