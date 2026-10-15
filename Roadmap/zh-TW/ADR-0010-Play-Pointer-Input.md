# ADR-0010：可選的 copied Play 指標輸入

狀態：接受實作；下列驗收 gate 仍適用。日期：2026-10-14。

## 背景

原有 V3 input snapshot 只有 movement 與 action button，不能在不改變舊模組 wire 意義／
layout 的情況下傳遞 pointer motion／wheel。Editor 已擁有 native Window batch、capture／
release admission 與單一 selected logical user。

## 決策

在既有 2D callback 後附加可選的 `capture_pointer_input` 與獨立 capability。保留原始 V3
required prefix、legacy input snapshot 與 2D extension。Consumer 先檢查 size 才讀 suffix，
再檢查 version、capability 與 callback。每個 extension 檢查自己欄位的結尾，不以未來完整
host table 的大小拒絕舊 host。C／Zig convenience guard 在介面不存在時保留 output。

`NexoraPointerInputSnapshotV1` 含 caller-sized／versioned prefix、既有 batch sequence、
signed double client-pixel X／Y delta、horizontal／vertical Window wheel unit 與五個 raw
held button bit（左／右／中／返回／前進）。Raw bit 不同於 legacy profile action bit。
不傳遞 native pointer、ownership 或非同步工作。Caller 提供有效 aligned storage；錯誤保留
其 bytes，較大的未來 output buffer 保留未知 suffix。Provider context 必須活過 module
Stop／Destroy。

Editor 在每個 batch 有限飽和加總：motion 每軸限 +/-65,536 native client pixels，wheel
每軸限 +/-122,880 units（1,024 個 120-unit step）。讀取不消耗結果；同一批 Fixed／Update
在 owner 的 input／admission 變更前，讀取保留相同 sequence／data。Consumer 若以 frame displacement 套用，
需依 sequence 去重，並以 Update 補上沒有 fixed tick 的 batch。
下一個空 batch 清除 delta／wheel，但保留 held button；sequence 與 legacy snapshot 同步，
中立 batch 也遞增。

Acquisition 丟棄 action／motion／wheel，但可用最後 pointer position 初始化 anchor；若沒有
seed，第一筆 sample 僅建立 anchor。失焦／Escape 優先於整批事件。Focus／user／profile 更換
與 Stop 重設 motion、wheel、anchor。Resize／DPI／display change 使該批 pointer delta／wheel
中立並重新定錨；一般 key／button release 仍照常處理。未選中的 logical user 收到相同 sequence
的 owning neutral snapshot。Pause／manual Step 不接受 pointer input，Resume 必須重新擷取。

## 相容性與驗收

Host suffix 與 wire 為 additive；不改 ABI major、legacy prefix、scene format、shader 或
module dependency。C++ consumer 需重建；未實作 provider 的 host 保留 callback null 與
capability unset。舊 2D-only host size 仍須成功協商 2D。

必要 gate 包含 C11／C++／真正 Zig layout／size guard、短 original／2D host buffer、
descriptor／error output 保留、32 輪 arithmetic／reset／user／focus 邊界，以及真正 C-ABI
module 使用 captured pointer／wheel 改變 native Game 像素。完整 Linux Development／Shipping
與最終來源的全部 hosted CI 通過後才可合併。

本切片僅處理有界的 native-client motion。Cursor lock、raw／無界 relative input、應用程式
camera sensitivity／inversion、實機裝置、gamepad 與完整 pointer-look／ED-M3 驗收仍屬後續工作。
