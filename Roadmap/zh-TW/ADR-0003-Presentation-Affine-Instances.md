# ADR-0003：精確 affine Presentation instance

狀態：已接受實作。日期：2026-10-04。

## 背景

旋轉子物件位於非等比縮放底下時，其 affine world matrix 無法由 TRS instance 表示。
Presentation 原本直接上傳 public TRS descriptor，因此 native preview 無法跨這類 ancestry
保留精確 geometry 或 inverse-transpose normal。Runtime 已提供精確 column-major world matrix，
SceneDocument 也擁有相符的預期 matrix。Presentation 必須維持不相依於 Runtime／Editor，
且不暴露 native handle。

## 決策

附加 optional `SceneInstance::model_transform`，作為 row-major float 4x4 affine override。
有設定時由它決定 transform，忽略未使用的 TRS field；tint 仍必須有限。Matrix 必須所有值有限，
最後一列為 `[0, 0, 0, 1]`、linear part 可逆，推導的 normal coefficient 在有限 float 範圍內。
未設定時維持既有 TRS validation、empty-instance identity、有界 mesh range、單次 scene submission
規則與 completion ownership。

共用 CPU packer 驗證並產生 private 112-byte record：三列 float4 model、三列 padded float4
inverse-transpose normal，以及 float4 tint。Vulkan、DX12 與 Metal 上傳此 record，不直接上傳
含 optional 的 public descriptor。Vertex shader 使用 model／normal row dot product；MVP、
lighting 與 UV／material binding 保留原本慣例。Normal row 可共用正數 rescale，以保留方向並約束 GPU 算術。Shader 透過 magnitude scaling 正規化
有限的 input／output normal 及 light vector，包含 zero normal（僅 ambient），避免長度運算溢位或
下溢。Source span 只在 DrawScene 期間借用；backend
返回前將 packed byte 複製到 acquired frame 的 fence-owned storage。GPU allocation／recording 前
拒絕無效輸入，不消耗 scene submission。`ValidateSceneInstance` 提供相同的純 CPU gate。

## 影響與證據

既有 aggregate caller 保留 identity／TRS 行為。C++ consumer 需重新建置；stable C／Zig ABI、
scene format、module graph 與 native handle ownership 不變。Editor caller 提交前必須明確將
owning Runtime column-major matrix 轉置；此 boundary change 本身並未接上 Scene／Game authored mesh，
也不代表 ED-M2 完成。

Portable test 驗證 closed-form point、normal／tangent 正交、鏡像 transform、無效 affine data、
override 語意、legacy TRS 與 upload budget。Vulkan X11 pixel 比較 affine instance 與獨立烘焙的
geometry／normal，並測試 invalid-then-valid recovery。執行完整 Linux Development 與 Shipping gate、
以同一 compiler 確定性重建 shader，以及跨平台 CI。Physical-GPU／display 與 native Editor 驗收
仍為獨立項目。
