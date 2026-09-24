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
```

Reflection is sourced from Clang's JSON AST and includes complete record declarations carrying a
Clang `annotate` attribute. The cache key covers importer identity/version, canonical settings, and
source content; artifacts are written atomically beneath a two-character hash fanout.

## Ownership, failure, and process isolation

The invoking commandlet owns reports and cache roots. Each import runs in a child process; a worker
crash becomes a per-input failure and cannot terminate the caller. Cache entries are immutable: an
existing key with different bytes is rejected as a collision. External entity IDs are restricted to
portable filename characters, duplicate IDs are rejected, and the manifest records each entity's
content hash. Operations are synchronous; callers may run independent commandlets concurrently,
while atomic replacement prevents partial cache or metadata files from becoming visible.
