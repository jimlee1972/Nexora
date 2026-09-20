#!/usr/bin/env python3
"""Merge per-backend slangc `-reflection-json` output into one canonical,
backend-neutral pipeline-layout document matching Shaders/Triangle.reflection.json.

The canonical binding list for Triangle.slang is a compile-time fact of the
shader source (one ConstantBuffer<FrameConstants> at register(b0), visible to
the vertex stage, 64 bytes). What this script actually validates is that
slangc's independent DXIL/SPIR-V/MSL reflection output all agree with that
fact; it does not "discover" the binding layout from scratch, because a
single shader has exactly one resource and its shape is already known.

NOTE: the recursive search below targets the field names documented for
Slang's `-reflection-json` output at the time this was written. It has not
been exercised against a real slangc install (none was available in the
environment this was authored in) -- if a backend's JSON uses different key
names, `_find_uniform_size` will fail loudly (non-zero exit, clear message)
instead of silently reporting a false pass. Whoever runs this against a real
slangc for the first time should confirm the message never fires and adjust
the key list here if it does.
"""
import argparse
import json
import sys

# Mirrors nexora::rhi::BindingType (Engine/RHI/include/Nexora/RHI/Types.h).
BINDING_TYPE_CONSTANT_BUFFER = 0
# Mirrors nexora::rhi::ShaderStage (Engine/RHI/include/Nexora/RHI/Types.h).
STAGE_VERTEX = 1

# The one resource Triangle.slang declares. Independent of slangc's reflection
# JSON shape -- this is what the cross-backend check below must agree with.
CANONICAL_BINDING = {
    "resource_id": 1,
    "binding": 0,
    "type": "constant_buffer",
    "stages": ["vertex"],
    "byte_size": 64,
}

_SIZE_KEYS = ("uniformSize", "byteSize", "size")
_CONSTANT_BUFFER_KIND_MARKERS = ("constantbuffer", "uniformbuffer", "cbuffer")


def _looks_like_constant_buffer_node(node: dict) -> bool:
    for key in ("kind", "type", "category"):
        value = node.get(key)
        if isinstance(value, str) and value.lower() in _CONSTANT_BUFFER_KIND_MARKERS:
            return True
        if isinstance(value, dict):
            nested = value.get("kind")
            if isinstance(nested, str) and nested.lower() in _CONSTANT_BUFFER_KIND_MARKERS:
                return True
    return False


def _extract_size(node: dict):
    """Pull a byte size off a node that looks like a constant buffer. Real
    slangc output (verified against slangc 2026.18) puts the container's own
    `sizes` in descriptor-table-slot units, not bytes; the actual byte size
    is `elementType.sizes[*].value` (kind "uniform"). Both a flat
    `size`/`byteSize`/`uniformSize` key and the container's own `sizes` are
    checked as fallbacks since the exact shape has drifted across slangc
    versions."""
    element_type = node.get("elementType")
    if isinstance(element_type, dict):
        element_sizes = element_type.get("sizes")
        if isinstance(element_sizes, list):
            for entry in element_sizes:
                if isinstance(entry, dict) and isinstance(entry.get("value"), int):
                    return entry["value"]
    for key in _SIZE_KEYS:
        value = node.get(key)
        if isinstance(value, int):
            return value
        if isinstance(value, dict) and isinstance(value.get("value"), int):
            return value["value"]
    sizes = node.get("sizes")
    if isinstance(sizes, list):
        for entry in sizes:
            if isinstance(entry, dict) and isinstance(entry.get("value"), int):
                return entry["value"]
    return None


def _find_uniform_size(blob, backend_name: str) -> int:
    """Recursively search a slangc reflection JSON document for the byte size
    of the constant-buffer parameter. Raises with a diagnostic message rather
    than returning a guessed value when nothing matches."""
    found = []

    def visit(node):
        if isinstance(node, dict):
            if _looks_like_constant_buffer_node(node):
                size = _extract_size(node)
                if size is not None:
                    found.append(size)
            for value in node.values():
                visit(value)
        elif isinstance(node, list):
            for item in node:
                visit(item)

    visit(blob)
    if not found:
        raise SystemExit(
            f"NormalizeShaderReflection: could not locate the constant-buffer size in "
            f"the {backend_name} reflection JSON using known Slang reflection key names "
            f"{_SIZE_KEYS}. The reflection-json schema slangc produced does not match what "
            f"this script expects -- update _SIZE_KEYS / _CONSTANT_BUFFER_KIND_MARKERS to "
            f"match the actual output before re-running."
        )
    # A parameter can legitimately appear more than once (e.g. once per entry
    # point that references it); every occurrence must agree.
    if len(set(found)) != 1:
        raise SystemExit(
            f"NormalizeShaderReflection: {backend_name} reflection JSON reports inconsistent "
            f"constant-buffer sizes {sorted(set(found))} for the same resource."
        )
    return found[0]


def _load_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _compute_layout_hash(binding: dict) -> int:
    """Bit-for-bit port of nexora::rhi::ComputeLayoutHash
    (Engine/RHI/src/ShaderReflection.cpp) for the single Triangle binding."""
    mask = (1 << 64) - 1
    offset = 1469598103934665603
    prime = 1099511628211
    stage_mask = 0
    if "vertex" in binding["stages"]:
        stage_mask |= STAGE_VERTEX
    values = (
        binding["resource_id"],
        binding["binding"],
        BINDING_TYPE_CONSTANT_BUFFER,
        stage_mask,
        binding["byte_size"],
    )
    result = offset
    for value in values:
        result ^= value
        result = (result * prime) & mask
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    # DXIL needs Microsoft's dxcompiler, which the portable Slang release does
    # not ship for Linux/macOS (see Engine/RHI/README.md); it is only
    # required -- and only validated -- on Windows.
    parser.add_argument("--dxil-reflection")
    parser.add_argument("--spirv-reflection", required=True)
    parser.add_argument("--metal-reflection", required=True)
    args = parser.parse_args()

    backends = {
        "spirv": args.spirv_reflection,
        "metal": args.metal_reflection,
    }
    if args.dxil_reflection:
        backends["dxil"] = args.dxil_reflection
    for name, path in backends.items():
        blob = _load_json(path)
        actual_size = _find_uniform_size(blob, name)
        if actual_size != CANONICAL_BINDING["byte_size"]:
            raise SystemExit(
                f"NormalizeShaderReflection: {name} backend reports constant-buffer size "
                f"{actual_size} bytes, expected {CANONICAL_BINDING['byte_size']} to match "
                f"Triangle.slang's FrameConstants."
            )

    # "msl" is the backend id used by Engine/RHI's canonical fixture; slangc's
    # `-target metal` output is tracked here as "metal" until normalized. The
    # list reflects only backends actually validated above -- "dxil" is
    # omitted rather than claimed when this platform couldn't run it.
    backend_order = {"dxil": 0, "spirv": 1, "metal": 2}
    reported_name = {"dxil": "dxil", "spirv": "spirv", "metal": "msl"}
    canonical = {
        "schema_version": 1,
        "source": "Triangle.slang",
        "entry_points": {"vertex": "vertexMain", "fragment": "fragmentMain"},
        "bindings": [CANONICAL_BINDING],
        "backends": [
            reported_name[name] for name in sorted(backends.keys(), key=backend_order.get)
        ],
        "layout_hash": _compute_layout_hash(CANONICAL_BINDING),
    }

    with open(args.output, "w", encoding="utf-8") as handle:
        json.dump(canonical, handle, indent=2)
        handle.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
