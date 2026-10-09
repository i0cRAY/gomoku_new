# CLAUDE.md

即時制五子棋（C++17 / Qt 6 / CMake / GoogleTest）。雙方用能量即時下子，有技能、AI 與區網對戰。

## 文件
- `docs/spec.md`：規則（做什麼）。**規格與程式衝突時以 spec 為準。**
- `docs/design.md`：架構與介面（怎麼做）。
- `docs/tasks.md`：任務清單，由上往下做。

## 工作流程
1. 開始前先讀 `docs/tasks.md`，找出第一個沒打勾的任務，只做那一項。
2. 先依該任務列出的規格編號寫測試，確認測試失敗，再寫實作讓它通過。
3. 執行全部測試，全數通過後才在 `tasks.md` 打勾。
4. 做完一項就停下來，簡短說明改了什麼，等我確認後再繼續。
5. 遇到規格沒寫清楚、需要猜的地方，**先問我，不要自己決定**。標示 ⚠️待確認 的數值照預設值實作即可。
6. 需要改 spec 或 design 時，先提出修改內容讓我同意，再改文件，最後才改程式。

## 指令
- 設定：`cmake -B build -DCMAKE_BUILD_TYPE=Debug`
- 建置：`cmake --build build -j`
- 測試：`ctest --test-dir build --output-on-failure`

## 硬性規則
- `src/core` 不得 include 任何 Qt 標頭，也不得讀取系統時間或使用 `std::random_device`。時間一律由參數傳入，亂數用外部指定 seed 的 `std::mt19937`。
- `src/app`、`src/net` 不得依賴 QtWidgets。
- 只有 `GameController` 可以修改遊戲狀態；UI 收到 `stateChanged` 後透過 `viewFor(自己)` 更新畫面。
- 對手的能量、回能進度與技能冷卻不能出現在 UI、AI 或網路封包裡（spec E5）；一律使用 `PlayerView`，不要另開後門讀取對手的 `PlayerState`。
- AI 只能回傳 `Action`，必須經過 `GameController::submit`，不能直接改棋盤。
- 測試名稱要包含規格編號，例如 `TEST(EnergyTest, E3_CatchUpAfterLongGap)`。

## 程式風格
- 類別 `PascalCase`，函式與變數 `camelCase`，成員變數不加前綴，常數 `kPascalCase`。
- 檔名 `snake_case`（`energy_manager.h`）。
- 盡量用 `std::optional`、`enum class`，不要用魔術數字，數值集中放在設定結構中。
