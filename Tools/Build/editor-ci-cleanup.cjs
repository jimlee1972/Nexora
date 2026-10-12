'use strict';

const legacyBranches = Object.freeze([
  'codex/ed-m0-native-validation', 'codex/ed-m0-desktop-parity',
  'codex/ed-m0-surface-font-domain', 'codex/ed-m0-native-images',
]);
// Exact session-owned names, not a general feature-branch prefix policy.
const sessionBranches = Object.freeze([
  'feat/editor-plugin-lifecycle-20261009', 'feat/project-player-native-20261009',
  'feat/editor-static-export-job-20261009', 'feat/editor-tool-capabilities-20261009',
  'fix/editor-deep-hierarchy-20261009', 'fix/editor-ci-cleanup-20261009',
]);
// Published branches owned by the current roadmap session. Keep this exact list:
// similarly named work from another contributor is outside cleanup authority.
const currentSessionBranches = Object.freeze([
  'feat/editor-signed-extension-native-admission-20261010',
  'feat/editor-isolated-prefab-session-20261010',
  'feat/editor-prefab-isolation-controls-20261010',
  'feat/editor-signed-plugin-manager-20261010',
  'feat/editor-prefab-revision-history-20261010',
  'feat/editor-prefab-stable-diff-20261010',
  'feat/editor-atomic-property-snapshots-20261010',
  'feat/editor-prefab-property-revert-plan-20261010',
  'feat/editor-prefab-review-revert-20261010',
  'feat/editor-prefab-targeted-property-plan-20261010',
  'feat/editor-prefab-selected-revert-controls-20261010',
  'feat/editor-chrome-cpu-trace-import-20261010',
  'feat/editor-prefab-apply-source-controls-20261010',
  'feat/editor-prefab-reference-document-history-20261010',
  'fix/editor-native-reflected-frame-readiness-20261010',
  'feat/editor-guarded-prefab-three-way-rebase-20261010',
  'feat/editor-nested-prefab-materialization-20261010',
  'feat/editor-prefab-three-way-rebase-controls-20261010',
  'feat/editor-persistent-prefab-placement-bindings-20261010',
  'fix/editor-ci-cleanup-current-session-20261010',
]);

async function cleanup({github, context, core}) {
  const {owner, repo} = context.repo;
  const repository = `${owner}/${repo}`;
  const pull = context.payload?.pull_request;
  if (pull && pull.head?.repo?.full_name !== repository) return;
  const branch = pull?.head?.ref || context.ref.replace(/^refs\/heads\//, '');
  const currentSession = currentSessionBranches.includes(branch);
  if (currentSession && (!context.actor || context.actor !== owner)) return;
  const branches = currentSession ? currentSessionBranches :
    sessionBranches.includes(branch) ? sessionBranches :
    branch.startsWith('codex/ed-m0-') ? legacyBranches : [];
  for (const candidate of branches) {
    try {
      const ref = await github.rest.git.getRef({owner, repo, ref: `heads/${candidate}`});
      const head = ref.data.object.sha;
      const runs = await github.paginate(github.rest.actions.listWorkflowRuns,
        {owner, repo, workflow_id: 'build.yml', branch: candidate, per_page: 100});
      for (const run of runs) {
        if (!['queued', 'in_progress'].includes(run.status) ||
            run.head_branch !== candidate || run.head_repository?.full_name !== repository ||
            (currentSession && run.actor?.login !== context.actor) ||
            run.head_sha === head || run.id === context.runId) continue;
        const current = await github.rest.git.getRef({owner, repo, ref: `heads/${candidate}`});
        if (current.data.object.sha !== head) {
          core.info(`${candidate} advanced during cleanup; keeping the new head's runs`);
          break;
        }
        await github.rest.actions.cancelWorkflowRun({owner, repo, run_id: run.id});
        core.info(`Cancelled superseded ${candidate} run ${run.id} (${run.head_sha})`);
      }
    } catch (error) {
      // Cleanup is best-effort. Build and current-head CI still determine acceptance.
      core.warning(`Editor CI cleanup for ${candidate}: ${error.message}`);
    }
  }
}

module.exports = {cleanup};
