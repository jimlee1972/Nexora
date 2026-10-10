# Current Editor CI queue cleanup — Linux, 2026-10-10

Beads `nexora-62u.1.10`. The previous exact allowlist covers six 20261009
branches, leaving superseded current roadmap runs queued after integration.

The new exact group includes own published PR484–489,491–492,494–503 branches,
persistent binding PR505 and this cleanup branch. It requires the current
workflow actor to equal the repository owner, and each candidate run's actor to
match. Missing/foreign actors, forks, unrelated branches, all current-head push/PR
runs and the active workflow remain retained. Every cancellation rereads the
branch; an advancing ref stops that branch. Errors remain warning-only.

- All **14 Node guard cases passed**, including current actor, forks, unchanged
  newest push/PR heads, exact branch triggers and advancement before/after writes.
- All **16 documentation-routing cases passed** with pinned parser dependencies.
- Full graphical Linux Development configure/build passed (288 steps), with
  cryptography/Slang/Zig/showcase/native Player enabled. CTest **239/239 passed,
  605.50s**, zero failures or skips.
- Minimal Shipping configure/build passed (five actual steps).
- No hosted cancellation result is claimed before the new workflow executes.

No direct cancellation API was invoked in the cloud session. The existing
workflow permission and legacy group remain unchanged. Main/feature acceptance
still requires complete fresh CI. Full graphical Editor milestones remain **0/8**.
