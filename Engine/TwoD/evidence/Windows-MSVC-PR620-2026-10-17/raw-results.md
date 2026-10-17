# Retained Windows host and guest observations

## project-contracts.log

```text
Project export: independent module, UI-only game, Unicode paths, process restart, immutable package and corruption rejection passed
```

## export-native.log

```text
**********************************************************************
** Visual Studio 2022 Developer PowerShell v17.14.35
** Copyright (c) 2025 Microsoft Corporation
**********************************************************************
Project export: independent module, UI-only game, Unicode paths, process restart, immutable package and corruption rejection passed
344 bytes written
416 bytes written
88 bytes written
175 bytes written
Real Zig project export, dependency closure and corruption rejection passed
G:\proj\Nexora\build\windows-pr620\UI-Shipping
project visits loaded=0
project stopped
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":3,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":6,"native_ui_texture_uploads":3,"resize_generations":1,"sprite_instanced_draws":0,"sprite_gpu_instances":0,"sprite_instance_upload_bytes":0,"visible":0,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":3,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}
{"device_output":"PASS","sample_rate":48000,"submitted_frames":48001,"reopened":true,"audible_acceptance":"NOT VERIFIED"}
Audio probe exit=0; audible acceptance NOT VERIFIED
```

## launcher.log

```text
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":3,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":6,"native_ui_texture_uploads":3,"resize_generations":1,"sprite_instanced_draws":0,"sprite_gpu_instances":0,"sprite_instance_upload_bytes":0,"visible":0,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":3,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}

project visits loaded=0
project stopped
```

## guest-environment.log

```text
{
    "os":  "10.0.19045.0",
    "toolchain":  [
                      {
                          "name":  "cl",
                          "found":  false
                      },
                      {
                          "name":  "cmake",
                          "found":  false
                      },
                      {
                          "name":  "zig",
                          "found":  false
                      },
                      {
                          "name":  "python",
                          "found":  false
                      },
                      {
                          "name":  "git",
                          "found":  false
                      },
                      {
                          "name":  "ninja",
                          "found":  false
                      }
                  ],
    "vulkan":  false
}
```

## vm-audio-unavailable.log

```text
{"device_output":"UNAVAILABLE","audible_acceptance":"NOT VERIFIED"}
Windows default stereo PCM output unavailable: 2
```

## vm-raw.log

```text
FILE ui-corrupt-save.stderr.log
Zig module load


FILE ui-corrupt-save.stdout.log


FILE ui-first.stderr.log
project visits loaded=0
project stopped


FILE ui-first.stdout.log
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":3,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":6,"native_ui_texture_uploads":3,"resize_generations":1,"sprite_instanced_draws":0,"sprite_gpu_instances":0,"sprite_instance_upload_bytes":0,"visible":0,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":3,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}


FILE ui-restart.stderr.log
project visits loaded=1
project stopped


FILE ui-restart.stdout.log
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":3,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":6,"native_ui_texture_uploads":3,"resize_generations":1,"sprite_instanced_draws":0,"sprite_gpu_instances":0,"sprite_instance_upload_bytes":0,"visible":0,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":3,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}


FILE ui-unicode-save.stderr.log
project visits loaded=0
project stopped


FILE ui-unicode-save.stdout.log
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":3,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":6,"native_ui_texture_uploads":3,"resize_generations":1,"sprite_instanced_draws":0,"sprite_gpu_instances":0,"sprite_instance_upload_bytes":0,"visible":0,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":3,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}


FILE zig-corrupt-cook.stderr.log
project cooked hash/type/identity


FILE zig-corrupt-cook.stdout.log


FILE zig-native.stderr.log


FILE zig-native.stdout.log
{"status":"PASS","module_abi":3,"project_mode":true,"api":1,"native_frames":120,"movement_frames":0,"entity_x":0.000000,"final_width":640,"final_height":480,"native_ui_draws":240,"native_ui_texture_uploads":2,"resize_generations":1,"sprite_instanced_draws":120,"sprite_gpu_instances":4080,"sprite_instance_upload_bytes":179520,"visible":34,"reloaded":false,"validation_package":false,"resumed":false,"initial_x":0.000000,"ui_frames":0,"ui_saves":0,"ui_opens":0,"cooked_reloads":0,"audio_submitted_frames":0,"save_reopened":false,"audible_acceptance":"NOT VERIFIED","allocations_balanced":true,"physical_windows":"NOT RUN"}


LOGS_END
```

## vm-finalize.log

```text
FINALIZER_STARTED
HASHES_PASS C:\NexoraAcceptance620\Unicode �ʸ�\UI-Shipping
HASHES_PASS C:\NexoraAcceptance620\Unicode �ʸ�\ZigGame-Shipping-Fixture
PLAYER_PROCESS_COUNT 0
```

## vm-acceptance-text.log

```text
RUN_PLAYER
RUN_PLAYER
RUN_PLAYER
RUN_PLAYER
RUN_PLAYER
RUN_PLAYER
RUNS_COMPLETED
FINAL_HASHES_COMPLETED
VBoxManage.exe: error: Code RPC_S_CALL_FAILED (0x800706be) (extended info not available)
VBoxManage.exe: error: Context: "Close()" at line 904 of file VBoxManageGuestCtrl.cpp
VBoxManage.exe: error: The session is not locked (session state: Unlocked)
VBoxManage.exe: error: Details: code E_UNEXPECTED (0x8000ffff), component SessionWrap, interface ISession, callee IUnknown
VBoxManage.exe: error: Context: "UnlockMachine()" at line 926 of file VBoxManageGuestCtrl.cpp
```

## vm-recovery-status.log

```text
COUNT 0
END
```

## vm-recovery-finalize.log

```text
FINALIZER_STARTED
ConvertFrom-Json : �L�Ī� JSON �򥻫��O: �C

��� �u��:5 �r��:58

+ ... ventory=Get-Content "$package\inventory.json" -Raw | ConvertFrom-Json

+                                                          ~~~~~~~~~~~~~~~~

    + CategoryInfo          : NotSpecified: (:) [ConvertFrom-Json], ArgumentException

    + FullyQualifiedErrorId : System.ArgumentException,Microsoft.PowerShell.Commands.ConvertFromJsonCommand
```
