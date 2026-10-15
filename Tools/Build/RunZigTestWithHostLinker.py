#!/usr/bin/env python3
"""Compile the real Zig test runner to assembly, link with the host C++ driver, and run it."""

import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--sysroot")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    arguments = args.arguments
    if arguments[:1] == ["--"]:
        arguments = arguments[1:]
    if not arguments:
        parser.error("Zig test arguments are required after --")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    assembly = output / "tests.s"
    binary = output / "tests"
    runtime_root = output / "compiler-runtime.zig"
    runtime_object = output / "compiler-runtime.o"
    runtime_root.write_text("// Root for Zig's bundled compiler-rt.\n", encoding="utf-8")
    # Preserve builtin.is_test, Debug instrumentation and Zig's standard runner.
    subprocess.run([args.zig, "test", *arguments, "-lc", "--test-no-exec",
                    "-fno-emit-bin", f"-femit-asm={assembly}"], check=True)
    # Assembly emission does not bundle compiler-rt. UBSan's float diagnostics need
    # Zig's quad-precision conversion routines, which Apple Clang does not provide.
    subprocess.run([args.zig, "build-obj", str(runtime_root), "-OReleaseFast", "-fPIC",
                    "-fcompiler-rt", f"-femit-bin={runtime_object}",
                    "--cache-dir", str(output / "runtime-cache"),
                    "--global-cache-dir", str(output / "runtime-global-cache")], check=True)
    link = [args.cxx, str(assembly), str(runtime_object), "-o", str(binary)]
    if args.sysroot:
        link.extend(["-isysroot", args.sysroot])
    subprocess.run(link, check=True)
    return subprocess.run([str(binary)]).returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode if error.returncode > 0 else 1)
