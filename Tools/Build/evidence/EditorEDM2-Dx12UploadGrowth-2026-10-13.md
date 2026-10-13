# DX12 staged upload growth conservation — 2026-10-13

Beads: `nexora-owg.1.3`. Base: `18a0d4f051e13ef90a000b91f16c57d45be328d8`.

Scene/UI growth previously advanced capacity and passed the retained ComPtr directly to
`CreateCommittedResource`. Its output adapter releases the old resource before allocation;
failure can leave stale enlarged capacity and an empty resource. A smaller retry then skips
allocation and attempts to map the empty resource.

Both paths now use the same private staged-allocation helper. It retains a valid allocation when
capacity suffices, captures geometric capacity locally with overflow rejection, allocates into a
fresh owning candidate, and publishes resource/capacity only after successful nonempty creation.
The existing caller waits the owning frame fence; no submission, descriptor or public layout/C ABI
changes. UI reallocation counters count committed replacements. The generic private helper uses
ComPtr-like ownership and the actual DX12 callback invokes `CreateCommittedResource` on its candidate.

Portable owning-resource fault tests verify reuse without allocation, partial candidate failure and
release, old payload/capacity preservation, smaller retry, exact successful retirement, empty successful
candidate rejection, overflow before allocator invocation, empty-slot recovery despite stale capacity,
and leak-free teardown. These tests prove allocation-state conservation; they do not emulate the D3D
driver or claim hardware OOM injection. Hosted Windows native scene/UI evidence is separate.

## Validation

Required local commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Focused portable allocation test passed1/1 before the full gate. A temporary replay of the old
capacity/ownership policy fails the same fault-conservation fixture; the new helper passes.
The first full245-test run passed244 tests but the existing native build-process Console fixture
missed operation3 after operation2 failed (831.86s). Its unchanged isolated rerun passed1/1 in7.45s;
no timer/assertion/source change masks the failure. A second complete run checks suite-order behavior.
The unchanged second complete Linux Development run passed **245/245**, zero failures/skips,
in **825.01s**. The native Console fixture passed7.39s within that complete run. Configure and the
431-action Development build passed; Shipping configure and the9-action build passed. Three changed
C++ files passed clang-format dry-run; documentation routing passed16/16, along with changed Markdown
links/bilingual pairs and diff checks. Fresh hosted compilation/runtime results are recorded in the
PR and Beads after publication. Physical GPU and full ED-M2 acceptance remain open; overall graphical
acceptance stays0/8 and root README is unchanged.
