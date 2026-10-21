# Owning asynchronous merge review — Linux, 2026-10-21

Beads `nexora-gu7.21.3`, depending on candidate SDK `.21.2`. Broader M4-05 graphical field choices,
live project/reference validation and atomic file/document publication remain open.

The production SceneComparisonJob now calls SceneMerge::Prepare on its real worker. A ready
snapshot retains the immutable owning review and aliases its comparison without copying another
projection. Existing authoring-owner baseline/local/path/project/file-session checks precede ready
publication. Cancellation, stale and failed work exposes neither output. Shutdown drains outstanding
work and clears the current observation, including already-consumed ready operations; previously
copied observations remain valid captured data. No publication authority or extension callback is
introduced, no source or baseline is written, and persisted formats are unchanged. The additive C++
snapshot layout requires matching consumers to rebuild. EditorCore stays loaded until all retained
reviews and their resolution calls drain. Logical bounds and cancellation granularity remain those
of the synchronous candidate parser, rather than a claim about total RSS or immediate interruption.

The actual asynchronous test owns an unknown provider payload containing NUL/high-bit bytes,
independent conflicting Local/Disk names and captured raw base/local/disk versions. It checks exact
raw bytes, shared comparison identity and actual selected candidate reload with literal expected
names and complete opaque payloads. Queued cancellation, changed local/generation/saved baseline/
file-session, corrupt/oversized disk, real read-only/reference inspection and deleted disk branches
retain their original conservation checks. A separate worker owner is actually destroyed; both its
review and an earlier review still resolve after other jobs stop and the original disk is deleted.
Data-only resolution must not recreate the file. Shutdown's current snapshot becomes cancelled and
empty. A real sole-worker blocker also verifies Shutdown waits for queued cancellation to drain;
only after releasing that worker may shutdown return, with no output. Focused production
target/registered test passes1/1 in.09 s after this drain case is added.

Initial focused testing caught the previous Shutdown early return for consumed operations, which
kept a ready current snapshot; this is repaired without changing the drain requirement. A separate
test-only byte-equality expectation conflated exact raw retained input with canonical migrated output:
the existing parser adds a default Euler record. The test now separately asserts exact raw sources
and independently reloads/checks the selected literal name and all opaque bytes. Failed attempts are
excluded from acceptance.

Required linux-development final configure/build/serial full CTest pass on private
b162e641ffc560ed9fbd2c62d04ebc92c63b2bca/tree23f9042ed5c1d9e91e1bb20868a87f8c3d0cbb2d:
**312 registered,300 actual passes,12 environment skips,zero failures,654.21 s**. All70 frozen
combined source hashes match. New job test passes.06 s in this suite; Deep/100k/overview pass
67.74/203.47/44.55 s under unchanged limits and scale1. A separate bounded Shipping build and
then a two-job integration compilation overlap portions of this shared-cloud functional gate;
these are not isolated production performance measurements. Bare native/display tests remain
skipped and provide no physical-host claim.

A fresh full-product preparation initially used one job while the preceding suite ran. It was
deliberately interrupted to restore the normal build budget once that suite finished, and its
log is retained without claiming completion. Required final configure and the complete remaining
462 product/consumer build steps then succeed before this suite; all consumers use current headers.
Shipping fresh configure/build completes102 steps with Editor/SDK stripped and BUILD_TESTING OFF;
CTest registers no tests and target enumeration confirms no Editor test/consumer. This does not
certify an Editor-enabled Shipping SDK. Both touched translation units pass strict -Wshadow
-Werror=shadow syntax checks; C++ format/diff and initial documentation16/16 in1.695 s pass.
Final documentation, prerequisite publication and fresh prospective-main/hosted validation follow
separately. No public PR or parent completion is claimed by these private gates.


With accepted own SDK sample PR686 and prospective candidate PR687, private integration
0fefca4df50ebdca8b0e4646cfb8e898fb76d8f7/tree57a628a3ae3c0d3fe4a3f92c32af6dd68cfa9a15
contains their exact combined base plus only this seven-file feature. All399 consumers rebuild.
Required final configure/build/serial full Linux passes **315 registered,303 actual passes,12
environment skips,zero failures,668.49 s**; all79 frozen combined source hashes match.
Deep/100k/overview59.69/226.51/51.33 s retain original limits and scale1. Separate bounded UI
compilation and native/Shipping checks overlap portions; these remain shared-cloud functional
observations, not isolated production percentiles. Publication still waits for prerequisite687
acceptance and fresh accepted-main/feature-only tree verification, then all exact-head hosted CI.
Stop/join JobSystem before EditorCore unload covers post-completion descriptor cleanup too.
