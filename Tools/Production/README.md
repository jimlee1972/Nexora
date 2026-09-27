# Production Toolchain contract

`NexoraTool.py` is the portable, headless V2-M1 entry point for reflection metadata, validation,
isolated imports, cooking, external entity storage, and structural scene diffs. All persisted JSON
uses schema version 1, sorted keys, compact separators, UTF-8, and a trailing newline. Reflection
types and fields, commandlet inputs, entity manifests, and diff object keys have defined lexical
ordering so identical inputs produce identical bytes.

## Commands

```text
NexoraTool.py reflect <header> --output <metadata.json> [--clang <clang++>]
NexoraTool.py validate <json>... --ddc <directory> [--output <report.json>]
NexoraTool.py import <asset>... --ddc <directory> [--output <report.json>]
NexoraTool.py cook <asset>... --ddc <directory> [--output <report.json>]
NexoraTool.py externalize <scene.json> --output <world-directory>
NexoraTool.py diff <before.json> <after.json> --output <diff.json>
NexoraTool.py world-partition <world.json> --output <partition.json>
  [--previous <partition.json> --changed-region <id>] [--leaf-size <size>]
```

Reflection is sourced from Clang's JSON AST and includes complete record declarations carrying a
Clang `annotate` attribute. The cache key covers importer identity/version, canonical settings, and
source content; artifacts are written atomically beneath a two-character hash fanout. Source file
names are report metadata rather than artifact inputs, so byte-identical sources reuse one cache
entry after a rename or move.

## Ownership, failure, and process isolation

The invoking commandlet owns reports and cache roots. Each import runs in a child process; a worker
crash becomes a per-input failure and cannot terminate the caller. Cache entries are immutable: an
existing key with different bytes is rejected as a collision. External entity IDs are restricted to
portable filename characters, duplicate IDs are rejected, and the manifest records each entity's
content hash. Operations are synchronous; callers may run independent commandlets concurrently,
while atomic replacement prevents partial cache or metadata files from becoming visible.

`world-partition` consumes region-addressed scene metadata in deterministic region/item order and
emits stable cell coordinates, per-region hashes, and a whole-build hash. With `--previous` and one
or more `--changed-region` arguments it rebuilds only those regions and carries all other immutable
region records forward byte-for-byte. It is the headless CI/cook entry point and never requires the
complete runtime world to be resident.

## V2-M10 shared DDC, distributed work, and LiveOps foundation

The V2-M10 portable layer extends the same production tool rather than creating a parallel build
system. Artifacts use an explicit `sha256:<digest>` content address, while `derivation_key()`
hashes a canonical descriptor containing protocol version, work kind, tool identity/version,
sorted input content addresses, and canonical settings. `distributed_work_unit()` applies that
identity to shader, HLOD, and cook jobs, so independently scheduled workers receive the same
deterministic work ID for identical inputs.

`SharedDerivedDataCache` is local-first. A local hit never depends on the shared service; a remote
outage or failed publish cannot stop local development. Remote fills are accepted only when the
returned content address matches the bytes and are then copied into the immutable local cache.
This contract intentionally does not define a transport, authentication mechanism, or hosted DDC
service.

LiveOps patch activation is transactional. `PatchVerificationTransaction` validates every manifest
entry, relative path, allowed remote-content kind, exact byte size, SHA-256 digest, and native-code
policy before it can advance `GenerationRegistry`. The registry retains retired generations while
they have pins and permits drain only after the reference count reaches zero. Remote payloads with
native executable extensions or PE/ELF/Mach-O magic are rejected; data overlays, localization
packs, assets, and media remain data-only.

`build.v2_production_toolchain` covers deterministic hash vectors, input-order independence,
shared-cache fill and outage fallback, corrupt remote rejection, verify-before-activate,
generation pin/drain, traversal rejection, and native executable rejection. Distributed worker
deployment, remote service availability/SLOs, signing/key management, and production rollout
orchestration remain infrastructure gates.
