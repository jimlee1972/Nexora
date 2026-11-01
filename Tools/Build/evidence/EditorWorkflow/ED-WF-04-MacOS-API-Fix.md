# ED-WF-04 macOS SDK API follow-up

Status: pending complete validation; ED-WF-04 is not accepted and workflow acceptance remains 3/9.

Candidate `0a388d88bb6fb3a206eb6c493122e62cdb6b2237` failed native macOS compilation before tests. Both push and pull-request workflows reported `getsectbynamefromheader_64` deprecated since macOS 13.0, promoted to an error by the existing warning policy. Example: workflow 41269513504, job 124739359488. Other macOS consumers failed at the same Runtime translation unit. No CI retry or warning suppression can accept this source.

Use Apple's replacement `getsectiondata` with an explicit returned size for the same Objective-C metadata admission check. TLS/Objective-C restart-required behavior, strict transaction admission, legacy behavior, and all unload assertions remain unchanged. Primary SDK header: <https://github.com/apple-oss-distributions/cctools/blob/main/include/mach-o/getsect.h>.

Native Windows regression after rebuilding the changed Runtime: `cmake --build build/editor-workflow-native --config Development --target NexoraGameplayModuleHostTests NexoraGameplayReloadTransactionTests NexoraZigReloadTransactionTests --parallel 2` exited 0; `ctest --test-dir build/editor-workflow-native -C Development -R '^runtime\.(gameplay_module_host|gameplay_reload_transaction|zig_reload_transaction)$' --output-on-failure` exited 0, 3/3 passed in 11.19s, including real Zig reload in 10.78s. This does not execute the Apple-only path. Full Codex Cloud and native macOS CI are required again for the new commit. Logs: task build directory `ed-wf04-api-fix-build.log` and `ed-wf04-api-fix-tests.log`; original failed CI remains available in GitHub Actions.

Unity input changes for issue #759 remain separate unpublished working-tree changes and are not part of this follow-up.
