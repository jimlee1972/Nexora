'use strict';
const assert = require('node:assert/strict');
const test = require('node:test');
const {cleanup} = require('../editor-ci-cleanup.cjs');

const branch = 'feat/editor-plugin-lifecycle-20261009';
function fixture({current = branch, target = branch, pull, advance = false,
                  advanceAfter = Infinity, fail = false} = {}) {
  const calls = [], cancelled = [], warnings = [];
  let reads = 0;
  const run = (id, values = {}) => ({id, status: 'queued', head_branch: target,
    head_repository: {full_name: 'owner/Nexora'}, head_sha: 'obsolete', ...values});
  const runs = [
    run(1), run(2, {status: 'in_progress'}), run(3, {head_sha: 'current'}),
    run(4, {head_branch: 'main'}), run(5, {head_repository: {full_name: 'foreign/Nexora'}}),
    run(6, {status: 'completed'}), run(7, {id: 900}), run(8, {head_repository: null}),
  ];
  const github = {rest: {git: {getRef: async args => {
    calls.push(['ref', args]);
    if (fail) throw new Error('Ref unavailable');
    if (args.ref !== `heads/${target}`) throw new Error('Unconfigured sibling fixture');
    reads += 1;
    return {data: {object: {sha: reads > (advance ? 1 : advanceAfter) ? 'advanced' : 'current'}}};
  }}, actions: {listWorkflowRuns: () => {}, cancelWorkflowRun: async args => {
    calls.push(['cancel', args]); cancelled.push(args.run_id);
  }}}, paginate: async (_method, args) => {
    calls.push(['runs', args]); return runs;
  }};
  const context = {repo: {owner: 'owner', repo: 'Nexora'}, ref: `refs/heads/${current}`,
    payload: pull ? {pull_request: pull} : {}, runId: 900};
  const core = {info: () => {}, warning: message => warnings.push(message)};
  return {github, context, core, calls, cancelled, warnings};
}

test('only obsolete live same-repository/session-branch runs cancel', async () => {
  const f = fixture(); await cleanup(f);
  assert.deepEqual(f.cancelled, [1, 2]);
  for (const [, args] of f.calls) {
    assert.equal(args.owner, 'owner'); assert.equal(args.repo, 'Nexora');
  }
  assert(f.calls.some(([kind, args]) => kind === 'runs' && args.workflow_id === 'build.yml'));
});
test('branch advancement stops cancellations before the first write', async () => {
  const f = fixture({advance: true}); await cleanup(f);
  assert.deepEqual(f.cancelled, []);
});
test('branch advancement after a write retains all remaining runs', async () => {
  const f = fixture({advanceAfter: 2}); await cleanup(f);
  assert.deepEqual(f.cancelled, [1]);
});
test('existing legacy ED-M0 cleanup behavior remains available', async () => {
  const legacy = 'codex/ed-m0-native-validation';
  const f = fixture({current: legacy, target: legacy}); await cleanup(f);
  assert.deepEqual(f.cancelled, [1, 2]);
});
test('main, arbitrary branches and future similarly named branches make no API calls', async () => {
  for (const current of ['main', 'feature/other', 'feat/editor-plugin-lifecycle-20261010']) {
    const f = fixture({current}); await cleanup(f);
    assert.deepEqual(f.calls, []);
  }
});
test('foreign or missing PR repository is excluded before any API call', async () => {
  for (const repository of [{full_name: 'foreign/Nexora'}, null]) {
    const f = fixture({pull: {head: {ref: branch, repo: repository}}}); await cleanup(f);
    assert.deepEqual(f.calls, []);
  }
});
test('same-repository PR uses the actual head branch and retains current runs', async () => {
  const f = fixture({current: 'pull/1/merge',
    pull: {head: {ref: branch, repo: {full_name: 'owner/Nexora'}}}});
  await cleanup(f); assert.deepEqual(f.cancelled, [1, 2]);
});
test('missing refs remain best-effort without cancellation or a build exception', async () => {
  const f = fixture({fail: true}); await cleanup(f);
  assert.deepEqual(f.cancelled, []); assert(f.warnings.length > 0);
});
