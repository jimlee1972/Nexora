# Owning supported project upgrades and retained originals — Linux evidence

ProjectWorkspace::PreviewUpgrade reads supported schema-1/2 descriptors and workspace metadata into
an owning plan without writer lease, directories or writes. Exact original CRLF/UTF-8 bytes survive
after caller/files change. Legacy UUID derivation and read-only Required/current behavior remain
compatible. Proposed bytes are a plan, not later publication authority. Actual descriptor reads cap
4 KiB before line parsing and reject overflow after stat; workspace path/count limits are unchanged.

Read-write Open reinspects under its cooperating-writer lease. Before legacy descriptor publication,
.nexora/project-upgrade.schema1.backup retains exact originals and the schema-one report records
from/to/UUID/source-byte count with publication=plan-only. This report does not certify successful
publication. Matching dedicated regular evidence permits retry; foreign/corrupt evidence, leaf
symlinks/hard-link aliases and occupied staging are retained and reject. Source bytes revalidate
before/between evidence writes and before native atomic replacement. Prepared evidence remains
when publication fails. Current-schema restart preserves backup/report and stable UUID.

## Actual acceptance

Real Unicode/CRLF legacy sources prove dry-run and read-only open create no metadata directory or
source write. Foreign backup/report, occupied backup directory staging and descriptor staging,
byte-identical hard-link backup and changed source revision preserve every source/evidence version.
After the occupied stage is repaired, retry publishes exact planned schema-2 bytes and retains
originals, plan report, workspace documents and stable UUID. Restart requires no further upgrade.

Unsupported, extra-record, corrupt UUID, empty-name and oversized descriptors reject without losing
source/workspace/evidence. Corrupt workspace and missing-root previews retain data/create no files.
Failed candidate Open leaves the previous live workspace, document list and OS writer lease intact;
a real competing writer remains denied. Existing legacy no-metadata, current/read-only, staging,
recovery/layout and workspace budget fixtures remain unchanged. No assertions/deadlines weakened.

Focused configure/build completed 78 steps: the new upgrade and existing preview tests passed
2/2 in 0.03s; the exact three-test registration check and expanded upgrade/preview/workspace-budget
gate passed **3/3 in 1.15s**. Full Linux completed 403 incremental steps and **219/219 in 507.65s**,
zero skips; minimal Shipping completed 74 steps. Rebase onto accepted semantic SDK main33ccd7b2
changed no ProjectWorkspace implementation or upgrade test bytes; both module sections were retained.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and software Mesa Vulkan were used.
Linux CI requires editor.project_upgrade_evidence; fresh hosted gates remain required before merge.
No local Windows/macOS, power-loss durability, hostile filesystem isolation, all-file transaction
or synchronous-call cancellation is inferred. This slice upgrades the project descriptor only;
source scenes, layout/workspace migration and broader graphical recovery remain separate work.
Public C++ consumers rebuild; stable C/Gameplay ABI and module graph are unchanged. Full milestones
remain **0/8**.
