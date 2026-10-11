# Scalar PBR material native tool

This optional module uses only public Editor/Renderer/Foundation headers. It dynamically
registers `nexora.editor.scalar-material.v1`, an immutable version-1 byte service using
the production `ImportMaterial`, `ValidateMaterialAsset` and canonical `ExportMaterial`
APIs. It does not load automatically, borrow engine documents or grant operation authority.

Inspect and Serialize take exact schema-1 material source and return canonical scalar
PBR source. Edit takes `NXM1`, a little-endian uint32 lane (0–8), a little-endian IEEE-754
binary32 value and source bytes. Lanes cover base RGB, metallic, roughness, occlusion and
emission RGB. All values must be finite and within the production range; unselected
values remain intact. Input is at most 64 KiB including the 12-byte edit prefix; output
is at most 1024 bytes. The public wire contract is in `ScalarMaterialTool.h`.

Callbacks synchronously parse and validate before copying any successful output. Invalid
requests, unsupported sources and insufficient output capacities reject without changing
caller-owned source/output; `output_bytes` becomes zero. Exceptions are contained as
Failed. Native code remains trusted and must obey call-scoped capacity/borrowing contracts.
Host/registry access is externally serialized, and cooperative shutdown requires draining
calls before unload. PluginHost shutdown revokes qualified visibility per admission;
the stateless module has no global shutdown flag that could disable another host.
Owning SDK results survive unload.

GPU Preview is undeclared and unavailable. Rendering, document authoring, Undo, writer
permission, source/generation guards, Save/reopen and missing-plugin payload restoration
belong to future host integration. Serialization does not write a file or authorize Save.
Unsupported/unknown material payloads must remain with the host, never be silently replaced.
This backend is not complete graphical reference-tool acceptance.

`NEXORA_FEATURE_SCALAR_MATERIAL_TOOL` defaults to the Editor build option, supports explicit
OFF and is forced OFF when Editor is absent (including standard Shipping/headless profiles).
Its module/manifest declares the public Editor dependency. It adds no required plugin
exports, lifecycle schema or engine ABI change; public C++ consumers rebuild.
