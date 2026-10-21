# Scoped file-only semantic merge publication — Linux, 2026-10-21

Beads `nexora-gu7.21.5`, after accepted owning candidate `.21.2`. This independent slice adds
one-shot scoped source publication and immutable provenance inspection. Graphical confirmation,
live authoring adoption, scene creation/deletion and full project/reference reconciliation remain
open under M4-05; overall graphical milestones remain 0/8. No physical-host acceptance is claimed.

The exported C++ API owns exact raw base/local/disk/candidate snapshots and file-session/root/
project/document observations. It retains no live borrow or native/plugin callback. Serialized
Prepare/Publish re-resolve the complete semantic candidate and repeat ownership/writer/recovery,
exact baseline/local/disk and strict managed-path checks. A capability's copies share one consumed
state. Raw data never substitutes for current caller authority.

Version1 SHA256 archives reserve exclusive content-addressed directories and preserve all source
bytes, stable-path field choices and the actual candidate. Strict read-only inspection verifies
manifest/version/project/integrity and re-resolves the candidate in isolated Worlds. Unknown future,
corrupt, occupied or incomplete archives remain untouched. Missing cryptography is an explicit
unavailable/rejection branch. The existing Editor -> Cryptography dependency is reused.

Native exclusive-create staging, chunked writes, flush/close and exact reread precede one native
atomic source replacement. All potentially throwing allocations and checks precede replacement.
POSIX directory fsync and Windows MoveFileExW WRITE_THROUGH have different durability contracts;
this is not an OS compare-and-swap, sandbox or universal power-loss guarantee. Native uncertainty
and post-replacement directory failure have distinct statuses and require inspection before retry.
Cancellation applies before commit and cannot interrupt blocking OS IO/fsync. Failed precommit
work may retain owned staging residue for explicit inspection; it never silently deletes a foreign
path. File-only publication leaves the live document/history/selection/baseline unchanged, so
ordinary Save still detects the published external version.

## Focused verification

The real isolated-project test uses an owned writer session, production World/documents and raw
sources with unknown bytes `{0,255,42}`. Local Name plus remote Z=7 survive actual publication and
fresh reopen; exact base/local/disk/candidate snapshots survive archive inspection. The live
World/generation/selection and preexisting Undo/Redo remain unchanged, and ordinary Save returns
NeedsOverwrite. A second read-only project inspects the owning archive but cannot prepare writes.

Negative cases cover unresolved/stale choices, caller revocation, same-timestamp external disk
changes, same-generation local changes, one-shot copied authority, replacement file sessions,
recovery gating, missing/oversized disk, occupied archive/temporary paths, unavailable SHA256,
actual isolated-World allocation failure, unsupported checksummed future manifests, truncation,
corrupt source payloads and supported-platform hard-link/symlink rejection. A private test-build-only
precommit gate lets another real thread change disk bytes or request cancellation after archive
and staged-candidate verification; neither operation reaches replacement. Source bytes and unknown
payloads remain intact after rejection. No deadlines/assertions are relaxed.

Initial smoke before the scheduling gate passes 1/1 in0.04s; after the real late-cancel/late-change
cases, focus passes1/1 in0.06s. A first strict compile rejected deprecated `filesystem::u8path`;
UTF-8 is now validated before constructing a C++20 u8string path. Failed compilation is excluded
from positive build evidence. Final required full Linux, Shipping and documentation results are
pending; this file does not yet mark the implementation slice accepted.
