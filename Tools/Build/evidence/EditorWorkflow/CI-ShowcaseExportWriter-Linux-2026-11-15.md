# Showcase export fixture writer lifetime

Task: `nexora-gu7.38.12`. Base: `0d961ed92d96a3c79bbe29dcd90d5dd5bc891e6a`.
The fixture ownership correction and portable Linux acceptance pass. Exact-head hosted Windows
acceptance is required before delivery; no parent milestone is accepted here.

## Observed failure

PR795 source `4139551c010fa469bb78c5acbbc3382ba3cf9dc6`, push run43736903959,
Windows job132881577353 failed `showcase.windows_state_export` in 6.59 s with an unhandled background
`File.WriteAllText` sharing exception. The unchanged PR Windows job132881820314 passed in 9.59 s;
one unchanged push retry132905574774 passed in 10.13 s. Those successes establish nondeterminism,
not repair. Original decoded logs and their comparison remain retained separately.

## Correction and regression scope

The application retains two `ofstream` handles through its export. The fixture reopened its files
between empty, partial and complete phases; a polling `ReadAllText` reader could acquire a
deny-write handle between producer opens. The fixture now owns both write streams throughout
those phases, including flushes, with the application's read/write sharing policy. The production
reader/exporter, five-second rejection bound, stale removal and exact owning evidence checks stay
unchanged. A bounded join surfaces worker I/O exceptions on the PowerShell owner.

A Windows-only deterministic regression keeps fully flushed valid data open, requires bounded
reader rejection, then releases the writer and requires exact closed-snapshot admission. A real
missing-directory producer error must reach the owner without terminating PowerShell. Original
fresh export, Markdown bytes and incomplete/evidence rejection assertions remain. The existing
20-second CTest limit is unchanged. Portable PowerShell runs explicitly exclude the Windows
sharing assertion; Linux does not establish Windows handle semantics.

## Final local acceptance

Baseline and corrected portable PowerShell runs pass using the existing 7.4.6 runtime. Full
`linux-development` configure/build/unfiltered CTest passes **554/554**, zero failures/skips,
**4599.24 s**. The [receipt](CI-ShowcaseExportWriter-Cloud.json) retains source and original failure
log hashes; changed-document and metadata checks are required before publication.
Actual Windows-specific checks require the unchanged final-head hosted jobs. This fixture-only
change introduces no engine or linkage boundary change; root README, production contracts and
Workflow5/9, graphical Editor0/8 and TwoD9/11 remain unchanged.
