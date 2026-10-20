# ED-M0 to ED-M7 accessibility handoff

Updated: 2026-10-20. Status: ✅ handoff inventory delivered; semantic bridge and target acceptance open.
Scope: the shell, Hierarchy, Console and recovery boundary in
[the ImGui integration plan](Editor_ImGui_Integration_Plan.md). This inventory does not implement
screen-reader support, certify keyboard access, or authorize a new dependency.

## Existing identity and missing semantic data

The authoritative [ProductShell inventory](../../Engine/Editor/src/EditorWorkspace.cpp) exposes only
`PanelDescriptor{id,title}`. The current panel IDs and English titles are:

| Stable panel ID | Current title | Future semantic role/data still missing |
| --- | --- | --- |
| `nexora.project` | Project | Region; project identity/access/state and available project actions |
| `nexora.hierarchy` | Hierarchy | Tree; parent/child structure, selected/expanded state, level, item position/count |
| `nexora.scene` | Scene | Region/canvas; selection, tool/mode, camera and keyboard alternatives to gestures |
| `nexora.game` | Game | Region/canvas; Play state and explicit input-capture ownership |
| `nexora.inspector` | Inspector | Region/property groups; names, roles, typed values/units/ranges, read-only/invalid state |
| `nexora.content` | Content | Tree/list; asset identity, selection, loading/error and available actions |
| `nexora.console` | Console | Log; severity, source, message/operation identity and announcement policy |
| `nexora.profiler` | Profiler | Region/charts; measurement source, unit, availability and textual alternatives |

The later panels appear in the current inventory; listing them does not add them to ED-M0's scope
or complete their own milestones. Host window labels use `###stable.id`; neither the visible label,
a dock-node number nor an ImGui hash is a portable accessibility identity.

[The host](../../Engine/EditorImGui/src/EditorImGui.cpp) routes these existing scene command strings:
`editor.scene.save`, `undo`, `redo`, `create`, `rename`, `delete`, `copy`, `cut`, `paste`, `duplicate`
and `select-all` (each suffix follows `editor.scene.`). `ProductShell::RouteCommand` only checks the
`editor.` prefix and stores `LastCommand`; it is not an action registry, permission system, native
accessibility provider, or callable command dispatcher. The bridge still needs explicit labels,
roles, shortcuts, enabled/busy state, current scope and admitted action handlers from each owner.
It must not execute arbitrary strings through this prefix check.

Recovery uses host-local popup identity `editor.recovery`, visible text "Recover workspace",
"A recovery journal is available.", "Recover" and "Discard recovery data". The buttons currently
have display labels, not published semantic action IDs. Proposed future IDs such as
`editor.recovery.recover` and `editor.recovery.discard` are design suggestions only, not existing
registered commands. Required roles are modal dialog, summary, button and error/status region;
destructive semantics, modal ownership and busy/disabled state must be explicit.

## Snapshot, ownership and action boundary to design

The application/window owner thread already serializes ImGui and document operations. A future
bridge should consume an owning semantic snapshot published by that owner, with copied labels,
values, bounds and state. Platform callbacks must enqueue bounded, owning action requests and
return to the owner for admission; they must not call ImGui or mutate documents on an OS callback
thread. No semantic snapshot API or cross-thread queue is introduced by this handoff.

Scope should combine the stable panel/control identity with current project identity and document
lifetime. [SceneDocument::NodeKey](../../Engine/Editor/include/Nexora/Editor/EditorWorkspace.h)
already contains entity ID, entity generation and document generation; `NodeView::name` is a
borrowed string view. Do not publish or retain that view, widget pointers, row indices, native GPU
handles or ECS pointers. Revalidate node/project/document generations and current permission at
action execution, including read-only, recovery, focus, modal and Play gates. Closing/replacing
scope must retire old nodes and invalidate queued requests. Virtualized rows need identity from
the model and a bounded reveal/query policy, not only the currently drawn ImGui item list.

## Keyboard, focus and failure handoff

Keyboard navigation is enabled; OS-level ImGui multi-viewports remain disabled. Existing Hierarchy
Up/Down/Home/End, range selection and Left/Right expansion behavior is documented in the
[host contract](../../Engine/EditorImGui/README.md#hierarchy-keyboard-navigation). Text editing,
blur, held pointers/gestures, Game capture and modal gates can intentionally suppress commands.
The bridge must report current availability and focus rather than bypassing those guards.

Current automated recovery drills use Tab/Enter to Recover and two Tabs/Enter to Discard. This
proves those bounded keyboard paths, not a native focus tree or a universal tab order. Target-host
acceptance must inspect visible focus, Shift+Tab, modal focus containment, ordinary shell/panel
traversal after docking, and focus restoration after close/recovery/error. Proposed semantic order:
announce dialog title/summary, expose any error, then non-destructive Recover before destructive
Discard. Default focus must not itself perform Discard. The docked shell's spatial/order changes
must be reflected by the future semantic snapshot; this document does not freeze a global order.

`ApplyRecoveryChoice` delegates only to `ProjectWorkspace::RecoverWorkspace`/`DiscardRecovery`.
Failure preserves the modal/error and journal; success is consumed once by `TakeRecoveryChoice`.
A future action must use this existing boundary and preserve its duplicate-submit guard. Announce
failure with actionable context; permit retry and a safe native close policy without inventing a
success, deleting a journal or silently selecting Discard. Verify that policy on actual hosts;
a dedicated published semantic safe-exit action is currently missing.

Native input/control stability remains open: the [current Linux validation record](../../Tools/Build/evidence/EditorEDM0-EvidenceAdmission-Linux-2026-10-20.md)
retains four unchanged workflow failures and subsequent successful serial reruns. Beads
`nexora-nqn.15` tracks diagnosis; reruns do not establish a timing fix or physical keyboard acceptance.

Live-region requirements: announce a new recovery error once per operation/change, then recovery
success/modal dismissal and focus restoration. Console entries need stable message identity,
severity, bounded backlog and opt-in/coalesced announcements; never announce every rendered frame.
Repeated/unchanged errors must not overwhelm speech. Progress, unavailable values, read-only
state and focus changes need explicit semantic state, not color alone. The current host has no
platform announcement/event channel.

## Platform options and acceptance still required

| Host | Candidate bridge | Required target evidence |
| --- | --- | --- |
| Windows | UI Automation provider/patterns | Inspect/Narrator or another installed reader; invoke/selection/value/focus/live events, DPI/IME and teardown |
| Linux | AT-SPI bridge | A real desktop with an installed reader such as Orca; focus, tree/reveal, actions and error announcements |
| macOS | NSAccessibility/AX bridge | VoiceOver and AX inspection on a declared supported Editor host; roles, actions, focus and lifecycle |

These are candidate platform interfaces, not selected implementations. A secondary semantic library
may be evaluated separately; adding a dependency requires the project's own ADR/review. Plugins
must provide approved semantic data/actions through a future explicit contract and must not inspect
the ImGui widget tree or acquire native handles to synthesize one.

ED-M7 still needs snapshot/action contracts, role/value coverage, bounded virtualized queries,
platform adapters/events, automated stale-scope/failure tests and real assistive-technology sessions.
Record exact source, host/reader versions, keyboard paths and artifacts. Existing Xvfb, synthetic
IME and keyboard tests do not certify speech output, reader focus, real IME or physical display
behavior. The current claim remains: basic keyboard-operable ED-M0 shell; screen-reader support
is not implemented and target-host acceptance remains open.
