# Nexora Showcase Validation

Schema: `nexora.showcase.validation.v1`

| Probe | Milestone | Status | Summary |
| --- | --- | --- | --- |
| `v1.M0` | M0 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M1` | M1 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M2` | M2 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M3` | M3 | UNSUPPORTED | Native counters are reported separately in windowed_evidence |
| `v1.M4` | M4 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M5` | M5 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M6` | M6 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M7` | M7 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M8` | M8 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M9` | M9 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M10` | M10 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M11` | M11 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |
| `v1.M12` | M12 | PASS | Live public Runtime integration; contract/visual acceptance tracked separately |

## M0 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 0 |
| input.error_case | 0 |
| output.accepted | true |
| contract_test | build.module_graph |

## M1 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 100 |
| input.milestone | 1 |
| input.error_case | 0 |
| fixed_ticks | 100 |
| output.accepted | true |
| contract_test | core.runtime |

## M2 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 2 |
| input.error_case | 0 |
| output.accepted | true |
| contract_test | renderer.contracts |

## M3 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 3 |
| input.error_case | 0 |
| output.accepted | false |
| contract_test | renderer.native_backend |

## M4 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 4 |
| input.error_case | 0 |
| output.accepted | true |
| contract_test | runtime.v1_m4_vertical_slice |

## M5 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 553 |
| input.milestone | 5 |
| input.error_case | 2 |
| hash | ba85a5f7c46a16fe |
| resident_bytes | 253 |
| generation | 1 |
| output.error_observation | Cyclic bundle dependencies rejected |
| output.accepted | true |
| contract_test | runtime.v1_m5_asset_pipeline |

## M6 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 554 |
| input.milestone | 6 |
| input.error_case | 3 |
| editor_sdk | available |
| graphical_editor | separate application |
| input.host_abi | 2 |
| input.plugin_library | /tmp/nexora-native-release-lk30eukm/NexoraShowcase/bin/libNexoraExamplePlugin.so |
| output.reported_abi | 1 |
| output.loaded | false |
| output.registered | false |
| output.error_observation | Plugin ABI mismatch rejected before registration |
| output.accepted | true |
| contract_test | runtime.v1_m6_editor_sdk |

## M7 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 7 |
| input.error_case | 0 |
| output.accepted | true |
| contract_test | runtime.v1_m7_input_ui_localization |

## M8 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 8 |
| input.error_case | 0 |
| adapter | portable CPU / no third-party physics SDK |
| character_x | -2.00 |
| output.accepted | true |
| contract_test | runtime.v1_m8_gameplay_simulation |

## M9 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 9 |
| input.error_case | 0 |
| native_audio_video | adapter unavailable / contract only |
| particles | 1 |
| output.accepted | true |
| contract_test | runtime.v1_m9_presentation |

## M10 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 10 |
| input.error_case | 0 |
| full_cells | 3 |
| output.accepted | true |
| contract_test | runtime.v1_m10_large_world |

## M11 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 1 |
| input.milestone | 11 |
| input.error_case | 0 |
| webview | adapter unavailable |
| output.accepted | true |
| contract_test | runtime.v1_m11_platform |

## M12 run details

| Input / output metric | Value |
| --- | --- |
| scope | runtime_integration |
| contract_gate | NOT_RUN |
| sample_tick | 559 |
| input.milestone | 12 |
| input.error_case | 4 |
| rollback_generation | 1 |
| output.error_observation | Shipping update rolled back to generation 1 |
| output.accepted | true |
| contract_test | runtime.v1_m12_shipping |

## Capability-aware rooms

| Room | Status | Headless evidence | Visual complete |
| --- | --- | --- | --- |
