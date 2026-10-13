# Vulkan fence-owned geometry upload reuse — Linux cloud, 2026-10-13

Beads: `nexora-owg.1.2`. Base: `9406b626951a2db67d7862b4ede80300f62ce2a2`.

Each Vulkan frame slot owns an exact byte key for its vertex/index prefix, bounded by the existing
65,535-vertex and 1,048,576-index limits (less than5 MiB per slot; at most three slots). A matching key
skips a geometry write only while the same upload allocation remains valid and large enough. Instance,
material and tone data still update every draw. Keys contain no borrowed data, IDs or approximate
hashes. Replacement/resize/teardown invalidate the key, including buffer growth driven only by more
instances. One staged replacement adds at most5 MiB CPU payload; GPU buffers retain the8 MiB slot cap.

The candidate CPU key is captured before GPU replacement and published after the mapped write.
Failed allocation preserves the old key and upload; a corrected smaller submission can retry.
Geometry counters measure actual prefix writes/reuse, including later draw failures. Retained bytes
measure owning CPU payload, not GPU memory. Other backends explicitly report unavailable. Public C++
diagnostics consumers rebuild; the stable C ABI is unchanged.

The actual private adapter fixture uses Vulkan call tracing and real X11 pixels. It verifies fixed
geometry with changing instance tint/translation, instance-only buffer growth, original geometry/index
changes, maximum descriptor budgets, failed growth conservation, slot fences, Scene-free retention,
resize and idempotent teardown. Existing assertions and deadlines remain intact. This supports slot
upload reuse; a persistent per-asset GPU registry and full ED-M2 acceptance remain open.

## Validation

Required commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Linux Development configure and the429-action build passed. The complete suite passed **244/244**,
zero failures/skips, in **826.49s**; native upload reuse passed1.91s inside the full gate. The focused
actual Vulkan tracing/pixel check passed1/1 in2.04s after adding instance-only growth. An initial
fixture treated an optional model matrix as an array; explicit identity initialization corrected it
without changing product guards, existing assertions or deadlines.

Shipping Monolithic configure and the9-action build passed with Editor/testing stripped. Three
changed C++ files passed clang-format dry-run; documentation routing passed16/16, and changed Markdown
links/bilingual pairs passed. Fresh hosted status is recorded in the PR and Beads after publication.
Physical display/GPU and target-host acceptance remain separate. Overall graphical acceptance stays0/8;
root README is unchanged.
