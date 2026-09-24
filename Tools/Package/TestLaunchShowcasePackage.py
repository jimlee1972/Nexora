#!/usr/bin/env python3
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from LaunchShowcasePackage import safe_join  # noqa: E402


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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
