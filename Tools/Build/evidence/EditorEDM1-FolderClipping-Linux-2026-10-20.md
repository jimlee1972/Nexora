# ED-M1 graphical Content folder clipping

Date: 2026-10-20 (Asia/Taipei). Task: `nexora-xke.6`.
Baseline: main `110b265de594f30b93f52fd6173faf195b3f36ed` (PR654).
This is a supporting WP2 rendering slice; Editor milestone acceptance remains 0/8.

## Behavior and compatibility

Content asset rows already used a clipper, but every immediate folder still submitted a selectable,
literal text and drop-target handling on every frame. The folder snapshot now uses its own ImGui
clipper with the exact text-line-plus-spacing height of the existing literal row. Logical folder
counts and the full scroll extent are preserved. Visible and navigation-required ranges retain the
same project-root/generation/role/path IDs and deferred folder entry. Frame-local position
observations retain only visible controls. A private test counter distinguishes logical folders
from submitted folder rows; it is reset before testing whether the Content window can draw.

Asset clipping follows the completed folder clipper and still supports explicit endpoint reveal.
Parent breadcrumbs, read-only navigation, pointer double-click, folder Enter ownership and existing
typed drop admission remain unchanged. There is no public API, persisted format, stable C/Zig ABI,
module graph, dependency, linkage boundary or CI/deadline change. The feature itself introduces no Shipping linkage change; the latest-main linkage gate is recorded below.

This bounds control submission, not all Content work: ChildFolders still scans the item index,
allocates an owning frame snapshot and sorts it, and activation lookup still scans that snapshot.
The fixture's submission threshold is not a hard-coded production row cap or a whole-frame timing
budget. Broader production asset scale and physical workflow acceptance remain open. The rendering
contract and EN/zh-TW Editor roadmaps agree; the concise root README is unchanged. Other roadmap
references to virtualized Content do not add a new milestone claim.

## Regression evidence

The existing real ImGui dependency test now includes 2048 same-marker-suffix folders plus two
current-folder assets at 1x and 2x, in writable and read-only sessions. Synthetic indexed items avoid
creating 2048 unnecessary files; exact existing source/sidecar bytes are still snapshotted and checked.
Assertions cover logical count and total scroll extent, middle scrolling, absent far-row controls,
Tab across the clipped boundary and Enter, owning parent breadcrumbs, asset End reveal below the
folder block, far-folder pointer double-click, filters, selection, revision and pending Rename Undo.

Before clipping, the new regression failed in 0.51 s because all folder rows were submitted.
An initial one-Tab expectation was corrected to Dear ImGui's existing input policy: the first Tab
after mouse input reveals the clicked navigation cursor; the next Tab advances across the boundary.
Both behaviors are asserted, followed by entry into the exact next folder. No navigation assertion,
input guard or production deadline was removed.

The final focused command was:

```bash
ctest --preset linux-development -R '^editor.content_(dependency_reveal|keyboard_shortcuts|diagnostic_display)$' -V
```

All three passed in 32.91 s: diagnostic display 0.04 s, keyboard shortcuts 26.60 s, and dependency/
folder controls 6.26 s. The observed maximum was 55 submitted folder rows out of 2048 logical folders
at each DPI, including the tested navigation range. Existing literal glyph, copied project/generation,
forward/reverse Reveal, input/modal/close and source conservation regressions remain in this target.

## Full local gate

Host: Linux 6.18.55 x86_64, GCC 14.2, CMake 4.4.4 and Ninja 1.13.2. The existing local
Development cache uses Modular linkage with graphical Editor and 2D enabled and Slang disabled.
Pinned vendor source caches and machine-local settings are not committed. Exact required commands:

```bash
cmake --preset linux-development
CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure
```

Configure and full build passed. CTest registered 285 tests: 273 passed, 12 native GPU/display/backend
cases skipped, zero failures, 387.30 s. The actual Content folder/dependency test passed in 4.58 s,
keyboard shortcuts in 27.60 s and graphical diagnostics in 0.04 s. Deep hierarchy passed in 86.69 s;
100k lifecycle passed in 282.93 s under its unchanged 300 s deadline. No compilation ran alongside
CTest. No timing threshold, native assertion or sanitizer setting was relaxed. Skips do not certify
physical or native GPU acceptance; existing scale/timing follow-ups remain open.

Documentation tests passed 16/16; changed-document links, bilingual routing, touched-file formatting
and `git diff --check` passed. All roadmap references to Content virtualization were reviewed; only
the paired Editor roadmap requires this supporting entry. Source/test digests match the validated
files. No production performance budget or milestone was closed.

| Retained log | SHA-256 |
| --- | --- |
| `folder-clip-red.log` | `ee3369135f22715d66879e840b917395b30665a7fd91fe80b64fdf91d70e7cea` |
| `folder-clip-focused-controls.log` | `f635fc6ff24caa823cca7417e94e2cb95379be35ce479e428140fd6bcf8cfa59` |
| `folder-clip-full-configure.log` | `00507a10e3f89a2c730848101943cc5f9ebf06ef96717e51173afec0282d0fa9` |
| `folder-clip-full-build.log` | `584cc8652a2ff9f829fba445f86a91587689db8751cf6a0ecd7ac616bdc68712` |
| `folder-clip-full-ctest.log` | `1e09bbe08e6c83ea6733c997aed36915fae6f2c58a1e079d9dc7b69966991f42` |

## Latest-main integration

Main advanced to `cd673ffd32b99b27111d31fac36423bcd6aeb060` (PR655 bounded native ray instance
transforms) while the original source was in CI. Integrated that actual main without conflicts;
the three feature source/test file digests are unchanged and the feature diff remains seven files.
The original head `77987bbc` is not the final combined runtime validation. No retry of that obsolete
head was requested; its failed Vulkan sanitizer job and passing identical-head counterpart remain
historical investigation evidence in PR656 and `nexora-0ua.1.17`, without any root-fix claim.

The complete Development configure/build/CTest gate above passed again on the combined tree:
285 registered, 273 passed, 12 native environment skips, zero failures, 371.09 s. Lifecycle passed
244.46 s, deep hierarchy 119.40 s, folder/dependency controls 8.38 s, keyboard shortcuts 35.61 s and
graphical diagnostics 0.05 s. The retained full test output again observes at most 55 submitted
folders out of 2048 at each DPI. Default deadlines and all assertions remain unchanged.

Because the integrated main extends the public C++ RHI virtual surface and diagnostics, also ran:

```bash
cmake --preset linux-shipping
CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-shipping
node --test Tools/Build/tests/editor-ci-cleanup.test.cjs
```

Shipping configured and built successfully with Monolithic linkage, Editor/Editor SDK disabled,
Vulkan enabled and Slang disabled. The updated CI-cleanup unit test passed 1/1. Shipping compilation
followed successful Development CTest, with no compilation competing with those timing gates.
Final documentation, bilingual links/routing, source-digest, formatting and clean-diff checks pass.
The feature adds no new public RHI changes beyond main and claims no physical/milestone acceptance.

| Integrated log | SHA-256 |
| --- | --- |
| `folder-clip-integrated-configure.log` | `92ed32629fa27aeff9db24893a9fc0d273391bfa4e8ff494329aa862f1b885ee` |
| `folder-clip-integrated-build.log` | `d90c4cefc6498759d86b37977c21b3775fd0e72e9eb6d9d1d3d63dbe287ee48d` |
| `folder-clip-integrated-ctest.log` | `959f73ff59458cb15ccf809460fb7d3f15281a7a247c1f5886f863c9077b9dcb` |
| `folder-clip-integrated-shipping-configure.log` | `2fb27900083f6cdef6d2a387238e81358083f173583bd7a654b7f92988363c55` |
| `folder-clip-integrated-shipping-build.log` | `157892197f0011f6490d6804cf8b7f0b73663bf33df81bff4baa4344f79ed097` |
| `folder-clip-integrated-last-test.log` | `35b992b0ed34f0e885c1d32c711a286b48883633870feb225d1dec8ffca1a116` |
## Complete ICD ordering integration — 2026-10-20

The feature's three C++/test source digests remain unchanged from the initial implementation.
The combined tree integrates main5cb842ac and own diagnostic sourced1c75a42, then stabilizes the
complete Linux ASan ICD order and adds the separate real native17-byte leak control. The native
helper, original four acceptance assertions and deadlines remain intact. This mitigates CI
driver-retention variability; Mesa unload cleanup remains an open follow-up. See the
[same-process attribution and controls](EditorEDM7-VulkanSanitizerDiagnostics-Linux-2026-10-20.md).

Final combined local gates:

- `cmake --preset linux-development` and
  `CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-development`: passed.
- `cmake --preset linux-shipping` and
  `CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-shipping`: passed.
- `CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure`:
  285 registered,273 passed,12 existing native environment skips,0 failures,335.34s.
  Lifecycle245.24s, deep hierarchy80.35s, Content dependency/clipping5.11s under unchanged
  deadlines. Actual output again reports55/2048 at both1x/2x.
- Full native ASan focused acceptance with complete generated eight-ICD order:5/5 passed,
  zero skips,38.32s. Four original executables pass; the separate positive control passes only
  after its actual native17-byte leak is detected at the correct injection source symbol.
- Vulkan ordering/control/diagnostic Python contracts:10/10 passed; GitHub-output preservation,
  205-test ASan registration including the four originals and positive control, workflow parsing,
  source digests, touched C++ formatting and clean diff passed. Documentation routing16/16 passed.

The ASan-only control option stays off in ordinary Development and Shipping. No production
Presentation API, persisted format, dependency or source preset changed. Root README and all
Editor milestone acceptance counts remain unchanged. Exact published-head CI remains mandatory
before merge; earlier failed heads and their original logs are retained rather than certified
by this local gate.
