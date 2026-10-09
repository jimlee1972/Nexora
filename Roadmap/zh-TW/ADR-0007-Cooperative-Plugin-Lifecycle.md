# ADR-0007：合作式原生外掛生命週期

狀態：接受真實 host 的前置功能；完整 ED-M6 仍未完成。

## 背景

真實外掛 host 已檢查 ABI 並註冊 borrowed service，但無條件卸載 native library 可能讓 service
pointer 或外掛工作仍指向已卸載的程式碼。圖形化 manager 需要 copied diagnostic 與安全停用，
不能假設所有既有外掛都能停止。

## 決策

保留必要 engine ABI 與可選 registration entry point。新增獨立版本的可選 C lifecycle getter，
檢查 capacity／size／schema，提供 shutdown／quiescence callback。先驗證 ABI 再做其他呼叫；
inspection 不得啟動工作。以 weak provider identity 暫存有界 registration，再原子發布。
Shutdown 前撤銷所有 registry copy 的查詢可見性；同步 registration 結束後不保留 registry pointer。

Owner-thread caller 先排空 service borrow／job，只有確認 quiescence 才卸載 native library。
Legacy、拒絕停止與尚未靜止的外掛保留至 process restart，host 析構亦不強制卸載。
Diagnostic row 有界且 owning，不公開 native handle。Windows 將 UTF-8 path 轉成原生 wide path，
POSIX 保留 native byte。Public C++ SDK layout 變更需重編 consumer；新增 C ABI 不影響 legacy admission。

## 影響與驗收

合約採合作式、序列化且不可重入；不自動排空既有 borrowed service，不提供 sandbox、signature
verification 或 native crash isolation。外掛必須真實回報 quiescence，且 registration 前不得啟動工作。
Restart pin 上限依每個 host 的 admission budget 計算，非 process-global 上限；Runtime code 必須
存活至 restart。

ExamplePlugin 實作此 lifecycle。真實 compiled module 驗證 background work、native unload event、
copy、Unicode path、拒絕、rollback 及精確 service／admission 上限；真正 C translation unit 使用
public header。交付需完整 Linux graphical Development 與 Monolithic Shipping，另包含明確啟用
Full Shipping SDK 的測試 build。完整 command／result 記於 evidence；完整 PluginManager 與所有
ED milestone 均保持未完成。
