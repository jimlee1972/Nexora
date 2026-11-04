# 持久化 crash 與遊戲診斷紀錄

✅ Linux 本機捕捉／持久化驗收：21 種子程序情境與不可寫輸出拒絕通過，Development 完整 gate 及 Monolithic Shipping build 通過。其他原生平台尚未在此驗證。


本文件定義 V1 Crash Reporting 與 Logging 的交付範圍。只有記憶體 crash ring 或
`CrashReporter::Capture` 並不代表原生 crash 捕捉；原本無條件的「Crash Infrastructure」
完成標記須依此縮小解讀。

## 本機執行路徑

- 應用程式入口持有 `core::DiagnosticSession`，涵蓋錯誤回報與模組卸載。Editor、
  ProjectPlayer、TwoDPlayer、獨立 2D 遊戲、Showcase、Host、DedicatedServer 使用
  `RunDiagnosed`；其他 embedding 在 Engine bootstrap 取得共用的備援 session。
- 每筆獲准進入 Core 的 log 在 `Write` 返回前，由 producer 寫入 OS；非同步觀察與 ring
  語意保留。這項刻意採用的同步 OS-write 成本，避免 consumer 尚未消化時丟失已接受的
  breadcrumb；不代表斷電耐久保證。
- Player V3 log callback 也保存遊戲訊息。Gameplay load/create/start/update/fixed-update/
  狀態遷移/stop/destroy 與 library finalizer 都帶有故障執行緒的 context。
  錯誤回傳、C++ 例外、PlaySession 失敗與 job 例外會留下持久紀錄。
- POSIX fatal signal 與 `std::terminate` 使用預先開啟的 descriptor，不在 handler 取得 logger
  鎖、配置記憶體、搜尋檔案、執行 C++ stack unwind 或解析 symbols。捕捉後仍異常終止，
  保留 OS 的 core/native report 政策。擁有者執行緒使用 alternate signal stack；
  任意外部執行緒的 stack overflow 不在保證範圍。
- Windows SEH 與 CRT fatal signal 使用預先開啟的 crash file，以及預先載入的系統 DbgHelp
  函式盡力產生 MiniDump。缺少 dump 支援時，`crash.log` 會明確記錄；OS 或程序損壞時，
  程序內 dump 仍可能失敗。
- 每次執行的 `game.log` 在 4 MiB 輪替，另保留三份舊檔。Error/Fatal 的 `crash.log` 副本
  上限 4 MiB，原生故障另保留追加路徑。原生紀錄包含 signal/exception code、address、
  instruction pointer 與 callback context。安裝 handler 前先保存 build ID、application、
  platform、process、啟動時間；在 gameplay 初始化前更新 `modules.txt` 供位址對應模組。
  原子替換的 `context.txt` 保存最多 32 筆有界 metadata，包含最近建立的 GPU/backend、
  已提交的主要遊戲場景及 Engine bootstrap 時的 process RSS；render graph callback 也會
  在故障執行緒上識別目前 pass。

## 如何找檔案

`NEXORA_LOG_DIR` 可指定根目錄（POSIX 使用 UTF-8，Windows 使用原生 Unicode）。預設位置：

| 平台 | 根目錄 |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Nexora\Logs` |
| Linux | `$XDG_STATE_HOME/Nexora/Logs`，或 `~/.local/state/Nexora/Logs` |
| Apple | `~/Library/Logs/Nexora` |
| 沒有使用者資料根目錄 | OS 暫存目錄下的 `Nexora/Logs` |

每次啟動建立獨立的 `run-<time>-<pid>-<counter>` 目錄，並輸出位置到 stderr，不重新導向
應用程式 streams。Windows 獨立遊戲啟動錯誤對話框也會顯示位置。請保留整個 run 目錄。
`crash.log` 一定有啟動資訊；以 `fatal kind=` 區別原生 crash、正常 session 與可恢復遊戲錯誤。
Windows 啟動時預留 `crash.dmp`；只有實際寫入的 dump 才是 crash 證據。

重啟不覆蓋或自動刪除 run 目錄。產品可在收集證據後自行採取保存政策，不應刪除仍使用中的
目錄。沒有自動上傳，也無法補回安裝此功能前已經發生的 crash。指定不可寫目錄時會明確
啟動失敗。Error/Fatal 會先保存到獨立 critical sink，再嘗試 game log 寫入／輪替；
任一 sink 失敗不會阻止另一個 sink 保存原始紀錄，輪替例外也不會丟失真正錯誤原因。
執行中 I/O 失敗則盡可能回報 stderr 與獨立 crash file，不保證磁碟滿或斷電時耐久。

## 驗收與仍待平台驗證的範圍

`runtime.crash_diagnostics` 使用獨立子程序與真正的 V3 動態 library，涵蓋正常/並行寫入、
escaping、輪替、C++ 與非標準例外、terminate、abort、worker crash、模組 create/start/update/
fixed-update/stop/destroy crash、callback 錯誤回傳、被捕捉的 job 例外與 job crash。
可攜式輪替障礙注入驗證 primary 輪替失敗後仍保留原始 Fatal；Linux 另以 `/dev/full`
分別注入兩個 sink 的寫入失敗。POSIX 另驗證擁有者執行緒 stack exhaustion。測試在沒有 Flush 的情況下確認最後一筆已接受
log、故障 callback、module map、異常 exit 與不可寫目錄的明確失敗；接入既有跨平台 CI。
參見[驗收證據](../../Tools/Build/evidence/Crash-Logging-Linux-2026-11-03.md)。

Windows/macOS/Android/iOS 原生執行與實體裝置驗收仍是平台 gate，Linux 雲端證據不代表完成。
POSIX 捕捉原始 instruction address，不在 handler 產生 symbolic backtrace；完整 stack 需要
OS core/native report 與相符的 unstripped ELF/PDB/dSYM。受管理的 out-of-process crash backend、
集中式 symbol archive/lookup、上傳同意/佇列/backend 整合，以及任意外部執行緒 stack exhaustion
捕捉，仍屬獨立 production 擴充，不得因本機 crash-log 交付就標為完成。
