#!/usr/bin/env python3
import sys
import platform
import shutil
import subprocess
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from LaunchShowcasePackage import safe_join, verify_runtime_closure  # noqa: E402
from PackageShowcase import engine_runtime_libraries, macho_engine_libraries, relocate_macos_binaries  # noqa: E402


def expect_rejected(base: Path, relative: str, label: str) -> None:
    try:
        safe_join(base, relative)
    except RuntimeError:
        return
    raise AssertionError(f"{label} was not rejected: {relative}")


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        (base / "bin").mkdir()
        (base / "bin/showcase").write_bytes(b"exe")

        # A well-formed relative path pointing inside the package must resolve normally.
        resolved = safe_join(base, "bin/showcase")
        assert resolved == (base / "bin/showcase").resolve()
        assert safe_join(base / ".", "bin/showcase") == resolved

        # A SHA256SUMS entry or build.json launch command crafted to escape the
        # package root -- via a parent-directory traversal or an absolute path --
        # must be rejected rather than silently resolving (and, for the launch
        # command, later being chmod'd and executed) outside the isolated copy
        # this tool is meant to confine itself to. (Path.is_absolute() only
        # recognizes a Windows drive letter as absolute when run under Windows,
        # so that case is not covered by this POSIX-only Linux gate.)
        expect_rejected(base, "../escape", "a parent-directory traversal")
        expect_rejected(base, "bin/../../escape", "a nested parent-directory traversal")
        expect_rejected(base, "/etc/passwd", "a POSIX absolute path")
        expect_rejected(base, "", "an empty path")
        # Exercise Mach-O closure resolution on every host; these are parser fixtures, not Mac execution evidence.
        from unittest.mock import patch
        mac_lib = base / "libNexoraFixture.dylib"
        mac_lib.write_bytes(b"fixture")
        staged = base / "mac-staged"
        (staged / "bin").mkdir(parents=True)
        mac_exe = staged / "bin/showcase"
        mac_exe.write_bytes(b"fixture")
        with patch("PackageShowcase.macho_dependencies", return_value=(["@rpath/libNexoraFixture.dylib"], [str(base)])):
            assert macho_engine_libraries(mac_exe) == [mac_lib.resolve()]
        shutil.copy2(mac_lib, staged / "bin/libNexoraFixture.dylib")
        with patch("PackageShowcase.macho_dependencies", return_value=(["@loader_path/libNexoraFixture.dylib"], [])):
            assert macho_engine_libraries(mac_exe) == [(staged / "bin/libNexoraFixture.dylib").resolve()]
        with patch("PackageShowcase.macho_dependencies", return_value=(["@loader_path/libNexoraMissing.dylib"], [])):
            try:
                macho_engine_libraries(mac_exe)
            except RuntimeError:
                pass
            else:
                raise AssertionError("missing Mach-O Engine dependency was accepted")
        if platform.system() == "Darwin":
            library = base / "libNexoraNativeFixture.dylib"
            source = base / "fixture.cpp"
            source.write_text('extern "C" int fixture() { return 42; }\n')
            subprocess.run(["c++", "-dynamiclib", str(source),
                            "-Wl,-install_name,@rpath/libNexoraNativeFixture.dylib", "-o", str(library)], check=True)
            source.write_text('extern "C" int fixture(); int main() { return fixture() == 42 ? 0 : 1; }\n')
            binary = base / "fixture"
            subprocess.run(["c++", str(source), str(library), f"-Wl,-rpath,{base}", "-o", str(binary)], check=True)
            staged = base / "native-staged"
            (staged / "bin").mkdir(parents=True)
            executable = staged / "bin/fixture"
            shutil.copy2(binary, executable)
            try:
                verify_runtime_closure(executable, staged)
            except RuntimeError as error:
                assert "outside the staged package" in str(error)
            else:
                raise AssertionError("Mach-O build-tree fallback was accepted")
            shutil.copy2(library, staged / "bin" / library.name)
            relocate_macos_binaries(staged / "bin")
            assert verify_runtime_closure(executable, staged) == [library.name]
            subprocess.run([str(executable)], check=True)
        if platform.system() == "Linux":
            # A real ELF fixture demonstrates the hidden build-tree fallback that a copied
            # executable and checksum-only launch could previously certify as isolated.
            library = base / "libNexoraFixture.so"
            source = base / "fixture.cpp"
            source.write_text('extern "C" int fixture() { return 42; }\n')
            subprocess.run(["c++", "-shared", "-fPIC", str(source),
                            "-Wl,-soname,libNexoraFixture.so", "-o", str(library)], check=True)
            source.write_text('extern "C" int fixture(); int main() { return fixture() == 42 ? 0 : 1; }\n')
            binary = base / "fixture"
            subprocess.run(["c++", str(source), f"-L{base}", "-lNexoraFixture",
                            f"-Wl,-rpath,$ORIGIN:{base}", "-o", str(binary)], check=True)
            if engine_runtime_libraries(binary) != [library.resolve()]:
                raise AssertionError("Engine ELF dependency closure was not discovered")
            staged = base / "staged"
            (staged / "bin").mkdir(parents=True)
            executable = staged / "bin/fixture"
            shutil.copy2(binary, executable)
            subprocess.run([str(executable)], check=True)  # False-positive old launch succeeds.
            try:
                verify_runtime_closure(executable, staged)
            except RuntimeError as error:
                if "outside the staged package" not in str(error):
                    raise
            else:
                raise AssertionError("build-tree Engine dependency fallback was accepted")
            shutil.copy2(library, staged / "bin/libNexoraFixture.so")
            if verify_runtime_closure(executable, staged) != ["libNexoraFixture.so"]:
                raise AssertionError("relocated Engine runtime closure was not verified")
            subprocess.run([str(executable)], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
