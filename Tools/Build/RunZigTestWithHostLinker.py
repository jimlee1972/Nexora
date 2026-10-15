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
    # Preserve builtin.is_test and Zig's standard runner. Like the existing Zig object
    # consumers, omit stack-probe thunks that require Zig's separately linked compiler-rt.
    subprocess.run([args.zig, "test", "-fno-stack-check", *arguments, "-lc", "--test-no-exec",
                    "-fno-emit-bin", f"-femit-asm={assembly}"], check=True)
    link = [args.cxx, str(assembly), "-o", str(binary)]
    if args.sysroot:
        link.extend(["-isysroot", args.sysroot])
    subprocess.run(link, check=True)
    return subprocess.run([str(binary)]).returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode if error.returncode > 0 else 1)
