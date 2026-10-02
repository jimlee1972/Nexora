#!/usr/bin/env python3
"""Every RHI backend must check the full indirect-draw range at DrawIndirect.

The D3D12 and Metal backends cannot be built on every host, so this guards their source the same
way MetalSourceContract.py does: each backend's DrawIndirect must call the shared
IndirectDrawRangeFits rule and record the bound buffer's size.
"""

from pathlib import Path
import re
import sys


def draw_indirect_body(source: str) -> str:
    match = re.search(r"::DrawIndirect\(std::uint32_t \w+\) \{(.*?)\n\}", source, re.DOTALL)
    return match.group(1) if match else ""


failures = []
for name in sys.argv[1:]:
    source = Path(name).read_text(encoding="utf-8")
    body = draw_indirect_body(source)
    if not body:
        failures.append(f"{name}: no DrawIndirect definition found")
    elif "IndirectDrawRangeFits(" not in body:
        failures.append(f"{name}: DrawIndirect does not check IndirectDrawRangeFits")
    if not re.search(r"indirect_size_ = record\.descriptor\.size|BufferSize\(buffer\)", source):
        failures.append(f"{name}: BindIndirectBuffer does not record the bound buffer's size")
if failures:
    raise SystemExit("\n".join(failures))
print("indirect-range checks present in " + ", ".join(Path(n).name for n in sys.argv[1:]))
