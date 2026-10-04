#!/usr/bin/env python3
"""Create a deterministic, self-describing Nexora Showcase directory."""

import argparse
import hashlib
import json
import platform
import re
import subprocess
import shutil
import zipfile
from pathlib import Path


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def copy(source: Path, destination: Path) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return {"path": destination.name, "bytes": destination.stat().st_size,
            "sha256": digest(destination)}


def is_macho(binary: Path) -> bool:
    with binary.open("rb") as source:
        return source.read(4) in (b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
                                  b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def macho_dependencies(binary: Path) -> tuple[list[str], list[str]]:
    """Read install names and LC_RPATH without executing the trusted Mach-O file."""
    linked = subprocess.run(["otool", "-L", str(binary)], text=True, capture_output=True, check=True)
    dependencies = [line.strip().split(" (", 1)[0] for line in linked.stdout.splitlines()[1:]]
    loads = subprocess.run(["otool", "-l", str(binary)], text=True, capture_output=True, check=True)
    rpaths = re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.+?) \(offset", loads.stdout)
    return dependencies, rpaths


def macho_engine_libraries(binary: Path, executable: Path | None = None,
                           visited: set[Path] | None = None) -> list[Path]:
    binary = binary.resolve()
    executable = executable or binary
    visited = visited if visited is not None else set()
    if binary in visited:
        return []
    visited.add(binary)
    dependencies, rpaths = macho_dependencies(binary)
    libraries = set()

    def expand(path: str) -> Path:
        return Path(path.replace("@loader_path", str(binary.parent))
                        .replace("@executable_path", str(executable.parent)))

    for dependency in dependencies:
        if not Path(dependency).name.startswith("libNexora"):
            continue
        # A dylib's first otool entry is its own install name, not a dependency.
        if Path(dependency).name == binary.name:
            continue
        if dependency.startswith("@rpath/"):
            candidates = [expand(root) / dependency.removeprefix("@rpath/") for root in rpaths]
        else:
            candidates = [expand(dependency)]
        library = next((path.resolve() for path in candidates if path.is_file()), None)
        if library is None:
            raise RuntimeError(f"missing Engine Mach-O dependency: {dependency} in {binary.name}")
        libraries.add(library)
        libraries.update(macho_engine_libraries(library, executable, visited))
    return sorted(libraries)


def relocate_macos_binaries(bin_dir: Path) -> None:
    """Replace build-tree Engine install names before hashing and ad-hoc sign modified files."""
    if platform.system() != "Darwin":
        return
    for binary in sorted(bin_dir.iterdir()):
        if not is_macho(binary):
            continue
        dependencies, _ = macho_dependencies(binary)
        edits = []
        for dependency in dependencies:
            name = Path(dependency).name
            if not name.startswith("libNexora"):
                continue
            if name == binary.name:
                edits.extend(["-id", f"@rpath/{name}"])
            elif (bin_dir / name).is_file():
                edits.extend(["-change", dependency, f"@loader_path/{name}"])
            else:
                raise RuntimeError(f"Engine dependency missing from package: {dependency}")
        if edits:
            subprocess.run(["install_name_tool", *edits, str(binary)], check=True)
        # install_name_tool invalidates existing signatures on arm64. No developer identity is used.
        subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(binary)], check=True)


def engine_runtime_libraries(binary: Path, environment: dict | None = None) -> list[Path]:
    """Discover the transitive Engine closure of a trusted built application."""
    if platform.system() == "Darwin":
        return macho_engine_libraries(binary) if is_macho(binary) else []
    with binary.open("rb") as source:
        elf = source.read(4) == b"\x7fELF"
    if platform.system() != "Linux" or not elf:
        return []
    result = subprocess.run(["ldd", str(binary.resolve())], text=True, capture_output=True, env=environment)
    if result.returncode:
        raise RuntimeError(f"could not inspect ELF dependencies: {result.stderr or result.stdout}")
    libraries = []
    for line in result.stdout.splitlines():
        match = re.match(r"\s*(libNexora\S+)\s+=>\s+(.+?)\s+\(0x[0-9a-fA-F]+\)", line)
        if "libNexora" in line and "not found" in line:
            raise RuntimeError(f"missing Engine dependency: {line.strip()}")
        absolute = re.match(r"\s*(/.*libNexora\S+)\s+\(0x[0-9a-fA-F]+\)", line)
        if match or absolute:
            library = Path(match.group(2) if match else absolute.group(1)).resolve()
            if not library.is_file():
                raise RuntimeError(f"missing Engine dependency: {library}")
            if match and library.name != match.group(1):
                raise RuntimeError(f"Engine dependency name mismatch: {line.strip()}")
            libraries.append(library)
    return sorted(set(libraries))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True, choices=("Development", "Shipping"))
    parser.add_argument("--build-configuration", choices=("Development", "Shipping"))
    parser.add_argument("--shipping-profile", choices=("Minimal", "Full", "Dedicated"), default="Minimal")
    parser.add_argument("--content", type=Path)
    parser.add_argument("--archive", action="store_true")
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--gameplay-module", type=Path)
    parser.add_argument("--runtime-libraries", nargs="*", type=Path, default=[])
    parser.add_argument("--api-manifest", required=True, type=Path)
    parser.add_argument("--license", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if args.profile == "Development" and args.gameplay_module is None:
        parser.error("Development packages require --gameplay-module")
    if args.profile == "Shipping" and args.gameplay_module is not None:
        parser.error("Shipping packages must use the static gameplay module")
    if args.build_configuration and args.build_configuration != args.profile:
        parser.error("package profile must match the build configuration")
    for path in (args.binary, args.api_manifest, args.license, args.gameplay_module,
                 *args.runtime_libraries):
        if path is not None and not path.is_file():
            parser.error(f"input does not exist: {path}")
    if args.content is not None and not args.content.is_dir():
        parser.error("--content must be a directory")
    package_binaries = [args.binary, *args.runtime_libraries]
    if args.gameplay_module:
        package_binaries.append(args.gameplay_module)
    names = [path.name for path in package_binaries]
    if len(names) != len(set(names)):
        parser.error("package binaries must have distinct file names")

    if args.output.exists():
        shutil.rmtree(args.output)
    bin_dir = args.output / "bin"
    manifest_dir = args.output / "manifests"
    artifacts = [copy(args.binary, bin_dir / args.binary.name)]
    if args.gameplay_module:
        artifacts.append(copy(args.gameplay_module, bin_dir / args.gameplay_module.name))
    for library in args.runtime_libraries:
        artifacts.append(copy(library, bin_dir / library.name))
    relocate_macos_binaries(bin_dir)
    # Mach-O relocation/signing changes bytes; manifests describe the final runnable copies.
    for artifact in artifacts:
        path = bin_dir / artifact["path"]
        artifact.update(bytes=path.stat().st_size, sha256=digest(path))
    copy(args.license, args.output / "LICENSE")
    copy(args.api_manifest, manifest_dir / "api.json")

    content_artifacts = []
    if args.content:
        for source in sorted(args.content.rglob("*")):
            if source.is_file():
                relative = source.relative_to(args.content)
                destination = args.output / "Content" / "Showcase" / relative
                item = copy(source, destination)
                item["path"] = destination.relative_to(args.output).as_posix()
                content_artifacts.append(item)
    (args.output / "run-showcase.sh").write_text(
        '#!/bin/sh\nset -eu\nshowcase_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'cd "$showcase_root"\nexport LD_LIBRARY_PATH="$showcase_root/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        + f'exec "./bin/{args.binary.name}" --mode=interactive --scene=hub --backend=auto'
        + (f' --gameplay-module=dynamic --gameplay-library="./bin/{args.gameplay_module.name}"' if args.gameplay_module else ' --gameplay-module=static')
        + ' "$@"\n', encoding="utf-8")
    (args.output / "run-showcase.sh").chmod(0o755)
    artifacts.sort(key=lambda item: item["path"])
    module_argument = ""
    if args.gameplay_module:
        module_argument = (f" --gameplay-module=dynamic"
                           f" --gameplay-library=bin/{args.gameplay_module.name}")
    else:
        module_argument = " --gameplay-module=static"
    build = {
        "schema_version": 1,
        "application": "NexoraShowcase",
        "profile": args.profile,
        "shipping_profile": args.shipping_profile if args.profile == "Shipping" else "Full",
        "gameplay_linkage": "dynamic" if args.gameplay_module else "static",
        "launch": (f"bin/{args.binary.name} --headless --validate-v1 --scene=tour --frames=4 --no-reload"
                   f"{module_argument} --report=launch-report.json"),
    }
    build["interactive_launch"] = f"bin/{args.binary.name} --mode=interactive --scene=hub --backend=auto{module_argument}"
    content = {"schema_version": 1, "artifacts": artifacts, "showcase_content": content_artifacts}
    (args.output / "README.txt").write_text(
        "Nexora Visual Showcase\nRun run-showcase.ps1 on Windows or use interactive_launch in manifests/build.json.\n"
        "Controls: 1-8 rooms; P Rendering primitive / Scene Play; F1 overview; F2 profiler; F3 matrix; F5 reload; T tour; Space pause; R replay/probe.\n"
        "Drag mouse to orbit; wheel zoom; WASD movement. Native media/WebView adapters are explicitly unavailable.\n"
        "Verify manifests/SHA256SUMS before launching. Headless launch validates portable integration only.\n"
        "Windows: run accept-v1.ps1 for isolated-copy native screenshots/report; -PhysicalDisplay/-CleanHost are operator attestations.\n", encoding="utf-8")
    (args.output / "run-showcase.ps1").write_text(
        "$ErrorActionPreference = 'Stop'\nPush-Location $PSScriptRoot\ntry {\n"
        f"  & './bin/{args.binary.name}' --mode=interactive --scene=hub --backend=auto"
        + (f" --gameplay-module=dynamic --gameplay-library='./bin/{args.gameplay_module.name}'" if args.gameplay_module else " --gameplay-module=static")
        + "\n  exit $LASTEXITCODE\n} finally { Pop-Location }\n", encoding="utf-8")
    copy(Path(__file__).with_name("AcceptShowcaseWindows.ps1"), args.output / "accept-v1.ps1")
    manifest_dir.mkdir(parents=True, exist_ok=True)
    (manifest_dir / "build.json").write_text(json.dumps(build, indent=2) + "\n", encoding="utf-8")
    (manifest_dir / "content.json").write_text(json.dumps(content, indent=2) + "\n", encoding="utf-8")

    checksums = []
    for path in sorted(p for p in args.output.rglob("*") if p.is_file()):
        checksums.append(f"{digest(path)}  {path.relative_to(args.output).as_posix()}")
    (manifest_dir / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")
    if args.archive:
        archive = args.output.with_suffix(".zip")
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as target:
            for path in sorted(p for p in args.output.rglob("*") if p.is_file()):
                info = zipfile.ZipInfo(path.relative_to(args.output.parent).as_posix(), (1980, 1, 1, 0, 0, 0))
                info.external_attr = (0o100755 if path.parent == bin_dir or path.name == "run-showcase.sh" else 0o100644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                target.writestr(info, path.read_bytes())
        archive.with_suffix(".zip.sha256").write_text(f"{digest(archive)}  {archive.name}\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
