# Named additive scene composition — Linux cloud evidence

`AdditiveSceneComposition` borrows a serialized AdditiveSceneSession and current project. It owns
an exact-byte metadata baseline and copies at most sixteen named paths, ownership/reference roles,
prior-row dependency indexes and active selection into a bounded 32 KiB UTF-8 project-UUID record.
The saved topological first path is the startup bootstrap; active selection is restored afterward.
Runtime scene IDs and document generations are newly allocated, while original entity identities
and opaque source bytes are preserved. No unknown references are rewritten.

Restore requires one clean borrowed bootstrap. All source sizes are checked before candidate
admission, with a 64 MiB per-file and 128 MiB aggregate logical payload limit (not total RSS).
Every additional source and dependency loads into a candidate before publishing membership.
Actual retained disk baselines are counted after every admission and again before publication;
size preflight alone cannot authorize a source that grows while being read. Save and final Restore
require existing sources and recheck distinct destinations, including byte-identical hard links.
Immediately before publication the metadata, current project scope and every exact source baseline
revalidate. Missing/corrupt sources, foreign metadata, aliases/case collisions, invalid/forward/cyclic
or repeated dependency indexes, source identity collisions, late modifications and interruptions
preserve the primary and release candidate World records. Play/readers must be stopped/drained.

Save changes only metadata. Every document must be live, named and have its original source revision;
dirty authoring may remain dirty. Exact-byte metadata changes, occupied staging, read-only/recovery,
old project scope and unresolved sources reject while preserving previous versions. A failed Restore
has no writable metadata baseline. Metadata replacement uses the existing native atomic file helper;
this does not certify fsync/power-loss durability or hostile concurrent filesystem isolation.

## Actual acceptance

Real source files include Unicode paths, authored Euler turns and unknown binary component payloads.
Tests verify owned/reference and active selection round-trip, deterministic load order, primary tokens
and content preservation, read-only reopen, unchanged canonical sources, invalid/mixed-project/bounded
metadata, missing sources, sixty-four failed restores without leaked records, source identity collision,
occupied temporary files, exact-byte external metadata/source changes, unnamed documents, recovery and
old project scope. Private serialized interruption hooks alter fixture data only, not production plugin
callbacks. Late metadata/source changes and a failing last source after earlier candidate admission
verify full rollback of membership. A genuine sixteen-document dependency chain reopens and reproduces
its exact metadata; real sparse sources under the per-file cap but above the aggregate cap reject
before candidate membership publication.

Initial 74-step focused build passed **1/1 in 0.03s**. Expanded admission/interruption/capacity cases
passed **1/1 in 0.05s**. The final source-baseline focused build passed eight incremental steps and
**1/1 in 0.04s**. The full graphical/native build completed 404 steps and **218/218 in 500.16s**,
zero skips; minimal Shipping completed 74 steps. Integration onto the published owner head
**928055a7c23198f53f2f46f4f4b3b8166bdd9936** changed only the owner's accepted evidence/documentation,
with no additional functional source change.

Three automated review findings were reproduced and corrected: a named missing baseline cannot
produce an unrestorable composition; byte-identical external hard-link replacement cannot bypass
destination distinction at Save or final Restore; and growth after size preflight cannot bypass the
128 MiB aggregate limit. Real valid sources grow to 45 MiB each after preflight (135 MiB aggregate)
and reject while preserving primary membership/token/content, metadata and every grown source.
Missing-source and actual hard-link fixtures also verify rejection, repair and successful retries.
The corrected focused gate passed **1/1 in 6.62s** after 15 build steps. Final review build completed
237 steps, full Linux passed **218/218 in 503.42s**, zero skips, and minimal Shipping completed five
incremental steps. Functional review correction is **b4a5ac38**; subsequent evidence changes do not
change its source.

An integrated graphical candidate independently passed actual 1x/2x ImGui controls **1/1 in 0.11s**
and actual Editor X11 **1/1 in 21.81s**. The native driver authors two independent dirty documents,
saves both, preserves independent Undo, creates/closes/reopens a reference, tests aggregate dirty
window-close protection and then relaunches in both read-only and writer modes with three documents,
one active reference and identical source/metadata bytes. A corrupt last source retains one bootstrap,
freezes source authoring and preserves foreign source/metadata bytes; repair permits a complete reopen.
That application/test counterpart belongs to the graphical tabs work and still requires its own full
and hosted acceptance; these local integration results do not claim graphical-main completion.

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
Touched C++ formatting/diff checks pass; Linux CI requires the composition acceptance registration.
Accepted foundation integration and fresh final-head hosted checks are required before merge. No
Windows/macOS or physical-host result is inferred from Linux. This rebuild-required C++ API introduces
only a versioned private Editor metadata file, with no scene/gameplay schema change. Complete graphical
additive acceptance and full ED-M4 remain open; all complete Editor milestones remain 0/8.
