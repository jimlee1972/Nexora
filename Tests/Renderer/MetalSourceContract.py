#!/usr/bin/env python3
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
required = (
    "newComputePipelineStateWithFunction",
    "BindStorageBuffer",
    "dispatchThreadgroups",
    "GPUDrivenIndirectCommandStride",
    "drawPrimitives:MTLPrimitiveTypeTriangle",
    "indirectBufferOffset:",
)
missing = [token for token in required if token not in source]
if missing:
    raise SystemExit("missing Metal source contracts: " + ", ".join(missing))
print("Metal source contracts present: " + ", ".join(required))
