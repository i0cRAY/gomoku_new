

> 本文件說明「怎麼做」：架構、類別介面、資料流。規則細節以 `docs/spec.md` 為準。

---

## 1. 技術選型

|項目|選擇|
|---|---|
|語言|C++17|
|GUI / 網路|Qt 6（Core、Widgets、Network）|
|建置|CMake ≥ 3.21|
|測試|GoogleTest（用 CMake `FetchContent` 取得）|
|版本控制 / CI|Git、GitHub Actions|

---

## 2. 架構

```mermaid
%%{init: {"flowchart": {"useMaxWidth": true}}}%%
flowchart TD
    subgraph UI[介面層 src/ui]
        direction LR
        View[棋盤顯示<br/>BoardView]
        HUD[能量條／技能列<br/>HudView]
    end
    subgraph CTRL[控制層 src/app]
        direction LR
        Clock[計時器<br/>GameClock]
        GC[遊戲流程<br/>GameController]
    end
    subgraph LOGIC[邏輯層 src/core]
        Rule[規則判斷<br/>RuleChecker]
        Energy[能量管理<br/>EnergyManager]
        Skill[技能系統<br/>SkillSystem]
        AI[AI 引擎<br/>AIEngine]
    end
    subgraph NET[網路 src/net]
        Net[網路連線<br/>NetworkManager]
    end
    subgraph DATA[資料層 src/core]
        Board[棋盤狀態<br/>Board 15×15]
        PS[玩家狀態<br/>PlayerState]
    end

    View -->|"下子請求 (x, y)"| GC
    HUD -->|使用技能| GC
    GC -.->|"signal：狀態更新"| UI
    Clock -.->|tick| GC
    GC --> Rule
    GC --> Energy
    GC --> Skill
    GC --> AI
    GC -->|送出行動| Net
    Net -.->|對手行動| GC
    Rule --> Board
    Skill -->|炸掉棋子| Board
    AI --> Board
    AI --> PS
    Energy --> PS
```

實線：呼叫（上層依賴下層）　虛線：signal 通知

### 2.1 分層規則（必須遵守）

|層|目錄|可以依賴|不可以依賴|
|---|---|---|---|
|邏輯層 + 資料層|`src/core`|C++ 標準函式庫|**任何 Qt**、系統時間、亂數裝置|
|網路|`src/net`|`src/core`、QtCore、QtNetwork|QtWidgets|
|控制層|`src/app`|`src/core`、`src/net`、QtCore|QtWidgets|
|介面層|`src/ui`|以上全部、QtWidgets|—|

原本的架構圖把 NetworkManager 畫在邏輯層。因為它需要 QtNetwork，實作上獨立放在 `src/net`，好讓 `src/core` 維持完全不依賴 Qt。

這樣分層的好處：`src/core` 是純 C++，時間和亂數都從外部傳入，所以測試完全可以重現，也不需要開視窗。

---

## 3. 核心型別（`src/core/types.h`）

```cpp
using TimeMs = std::int64_t;              // 對局時間（毫秒），由呼叫端傳入

enum class PlayerId : std::uint8_t { Black = 0, White = 1 };
inline PlayerId opponent(PlayerId p);

enum class Cell : std::uint8_t { Empty, Black, White };

struct Pos { int x; int y; };             // 0–14

enum class SkillId : std::uint8_t { Accelerate, Bomb };

enum class RejectReason : std::uint8_t {
    GameNotRunning, OutOfBoard, Occupied,
    PlaceCooldown, NoEnergy, SkillNotOwned, SkillCooldown, InvalidTarget
};

// 玩家送出的請求
struct PlaceAction { PlayerId player; Pos pos; };
struct SkillAction { PlayerId player; SkillId skill; std::optional<Pos> target; };
using Action = std::variant<PlaceAction, SkillAction>;

// 請求的結果
struct ActionResult {
    bool accepted;
    std::optional<RejectReason> reason;   // accepted == false 時才有值
};

enum class GameStatus : std::uint8_t { SkillSelect, Countdown, Running, BlackWon, WhiteWon, Draw, Aborted };
```

### 3.1 設定（`src/core/config.h`）

所有數值集中在這裡，程式其他地方不寫魔術數字。

```cpp
enum class Difficulty : std::uint8_t { Easy, Normal, Hard };

struct SkillConfig {
    TimeMs accelerateCooldown = 25000;   // spec SA
    TimeMs accelerateDuration = 5000;
    TimeMs bombCooldown = 20000;         // spec SB
};

struct AIConfig {
    Difficulty difficulty = Difficulty::Normal;
    TimeMs reactionEasy = 1200, reactionNormal = 700, reactionHard = 350;  // spec A2
    double defenseWeight = 0.8;          // spec A10，各難度相同
};

struct MatchConfig {
    TimeMs regenInterval = 2000;         // spec §4：500–10000 且為 500 的倍數，不合法時 assert（UI 只提供合法值）
    int maxEnergy = 10;
    int startEnergy = 1;
    TimeMs placeCooldown = 1000;         // spec §5
    TimeMs countdown = 3000;             // spec G2
    SkillConfig skill;
    AIConfig ai;                         // 僅 M1 使用
};
```

---

## 4. 各類別職責與介面

### 4.1 Board（`src/core/board.h`）— spec B1–B3

```cpp
class Board {
public:
    static constexpr int kSize = 15;
    Cell at(Pos p) const;
    bool inBounds(Pos p) const;
    bool isEmpty(Pos p) const;
    void set(Pos p, Cell c);      // 不做規則檢查，由呼叫端負責
    bool isFull() const;
    void clear();
};
```

### 4.2 RuleChecker（`src/core/rule_checker.h`）— spec W1–W4

```cpp
class RuleChecker {
public:
    // 檢查剛下在 last 的那顆子是否造成連五；有的話回傳那 5 顆以上棋子的座標（給 U6 用）
    static std::optional<std::vector<Pos>> findFive(const Board&, Pos last);
};
```

只檢查 `last` 所在的四條線，不掃描整個棋盤。

### 4.3 PlayerState（`src/core/player_state.h`）

```cpp
struct PlayerState {
    int energy;
    TimeMs regenProgress;                 // E2–E4
    std::optional<TimeMs> lastPlaceTime;  // P1-4、P4
    TimeMs accelerateUntil;               // SA1，0 表示沒有加速
    SkillId skill;                        // 開局前選的技能，S1
    TimeMs skillReadyAt;                  // 技能冷卻結束的時間，S3、S4
};
```

### 4.4 EnergyManager（`src/core/energy_manager.h`）— spec E1–E4、SA2

```cpp
class EnergyManager {
public:
    explicit EnergyManager(const MatchConfig&);   // 使用 regenInterval、maxEnergy
    // 把 state 從 from 推進到 to，套用回能規則（要處理加速在區間中途開始或結束的情況）
    void advance(PlayerState&, TimeMs from, TimeMs to) const;
    bool canConsume(const PlayerState&) const;
    void consume(PlayerState&) const;
    // 下一格能量的累積比例 0.0–1.0（= regenProgress / T）；能量已滿回傳 0（spec E6）
    double nextEnergyRatio(const PlayerState&) const;
};
```

**重點：回能採「懶惰計算」**。每次處理請求前，以及每次 tick，都呼叫 `advance(上次時間, 現在)`。因此回能的正確性不受 tick 頻率影響（spec E3）。

### 4.5 SkillSystem（`src/core/skill_system.h`）— spec S1–S6、SA1–SA5、SB1–SB4

```cpp
class SkillSystem {
public:
    explicit SkillSystem(SkillConfig);   // 定義於 config.h
    void initPlayer(PlayerState&, TimeMs matchStart) const;   // S4
    std::optional<RejectReason> check(const SkillAction&, const PlayerState&, const Board&, TimeMs now) const;
    void apply(const SkillAction&, PlayerState&, Board&, TimeMs now) const;  // 呼叫前必須先通過 check
    TimeMs remainingCooldown(const PlayerState&, TimeMs now) const;
};
`check` 第一步先比對 `action.skill == state.skill`，不同就回傳 `SkillNotOwned`（spec S1a）。
```

### 4.6 GameController（`src/app/game_controller.h`）— spec G、P、W

唯一可以修改遊戲狀態的地方。QObject，只依賴 QtCore。

```cpp
class GameController : public QObject {
    Q_OBJECT
public:
    GameController(MatchConfig, QObject* parent = nullptr);
    void selectSkill(PlayerId, SkillId);              // 技能選擇階段才有效（spec S1、G1a）
    void confirmSkill(PlayerId);                      // 未選技能時忽略；雙方都確定後進入倒數，對局時間從 −3000 起算（spec G2）
    ActionResult submit(const Action&, TimeMs now);   // 依 spec P1 / SB4 的順序檢查
    void tick(TimeMs now);                            // 推進能量、處理倒數與 AI
    PlayerView viewFor(PlayerId) const;               // 某位玩家看得到的資訊（spec E5）
    void restart();                                   // G4
signals:
    void stateChanged();                              // 收到後呼叫 viewFor(自己) 取資料
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void gameOver(GameStatus, std::vector<Pos> winningLine);
};
```

`submit` 的步驟：

1. 把雙方 PlayerState 推進到 `now`（EnergyManager::advance）
2. 依 spec 的順序檢查，失敗就發 `actionRejected` 並回傳
3. 套用變更 → 下子的話呼叫 `RuleChecker::findFive` → 判斷勝負或和局
4. 發 `stateChanged`（結束時再發 `gameOver`）

**資訊隱藏（spec E5）**：雙方完整的 PlayerState 只存在 GameController 內部，不對外公開。外部（UI、AI、網路）一律透過 `viewFor` 取得 `PlayerView`（定義在 `src/core/player_view.h`，因為 `src/net` 也要用，而 net 不能依賴 app）：

```cpp
struct PlayerView {
    PlayerId me;
    Board board;                         // 公開
    GameStatus status;
    TimeMs now;
    PlayerState self;                    // 只有自己的狀態
    double nextEnergyRatio;              // spec E6，下一格的累積比例 0.0–1.0
    bool accelerating;                   // 加速中（能量條可換顏色）
    std::optional<SkillId> opponentSkillRevealed;  // 對手第一次用技能後才有值，只有名稱（spec S1）
    TimeMs countdownRemaining;           // Countdown 狀態時的剩餘毫秒（spec G2）
    std::vector<Pos> winningLine;        // 結束時的連線，可能含多條線（spec U6）
};
```

`PlayerView` 裡完全沒有對手的 PlayerState 欄位，所以 UI、AI、網路就算寫錯也拿不到對手的能量。開發用本機模式（M3）可以分別呼叫 `viewFor(Black)` 與 `viewFor(White)` 來同時顯示。

**GameSession（`src/app/game_session.h`）**：UI 不直接依賴 GameController，而是依賴這個抽象介面，讓本機、主機、加入方共用同一套畫面。

```cpp
class GameSession : public QObject {
    Q_OBJECT
public:
    virtual void selectSkill(PlayerId, SkillId) = 0;
    virtual void confirmSkill(PlayerId) = 0;
    virtual void request(const Action&) = 0;          // 時間由實作自己取；結果經由 signals 回報
    virtual PlayerView viewFor(PlayerId) const = 0;
    virtual void requestRematch() = 0;                // spec G4
signals:
    void stateChanged();
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void gameOver(GameStatus, std::vector<Pos> winningLine);
    void opponentReady();                             // 只告知對手已確定技能（spec S1、U7）
};
```

- `LocalSession`：持有 GameController 與 GameClock（人機模式時 GameController 內含 AI）。M1、M3 與區網主機端都用它；主機端另外接上 NetworkManager。
- `RemoteSession`：區網加入方用。只保存最近一次快照的 PlayerView，`request` 轉成網路訊息，不做任何判定。

### 4.7 GameClock（`src/app/game_clock.h`）

```cpp
class GameClock : public QObject {
    Q_OBJECT
public:
    void start(TimeMs startAt);  // 從 startAt 起算（倒數開始時傳 −3000，spec G2）；用 QElapsedTimer 記錄起點，用 QTimer 每 50 ms 發一次 tick
    TimeMs now() const;
signals:
    void tick(TimeMs now);
};
```

測試時不使用 GameClock，直接呼叫 `controller.tick(t)` 和 `submit(a, t)`，自己指定時間。

### 4.8 AIEngine（`src/core/ai_engine.h`）— spec A1–A12

```cpp
enum class Pattern { Five, OpenFour, Four, OpenThree, Three, OpenTwo, Two, None };

class AIEngine {
public:
    AIEngine(PlayerId self, TimeMs reactionTime, double defenseWeight, std::uint32_t seed);
    SkillId chooseSkill();   // 技能選擇階段呼叫，用同一個 seed 的亂數隨機選（spec A2a）
    // 反應時間未到，或判斷本次不行動時，回傳 nullopt
    // 只拿得到自己的 PlayerView，讀不到對手的能量與冷卻（spec A3、E5）
    std::optional<Action> decide(const PlayerView&);
    // 以下公開，方便單元測試
    // pos 必須是空格：回傳假設 player 下在 pos 後，dirIndex 方向最強的棋型；邊界與對手棋子視為擋住
    static Pattern patternAt(const Board&, Pos, PlayerId, int dirIndex);
    double score(const Board&, Pos) const;
};
```

AIEngine 只回傳「想做的動作」，由 GameController 的 `tick` 呼叫它，再把結果丟進 `submit`。所以 AI 一定會通過和玩家相同的規則檢查（spec A1）。亂數使用 `std::mt19937`，以建構時傳入的 seed 初始化（spec A11）。

AI 決策流程（對應 spec A4–A9）：

```mermaid
%%{init: {"flowchart": {"useMaxWidth": true}, "themeVariables": {"fontSize": "12px"}}}%%
flowchart TD
Place{"能量 ≥ 1 且<br/>下子間隔已過？"}
Start([GameController tick]) --> Ready{"反應時間到了？"}
Ready -->|否| End([本次不行動])
Ready -->|是| Win{"A4 自己<br/>一步成五？"}
Win -->|是| Place
Win -->|否| Threat{"A5 對手有<br/>成五點？"}
Threat -->|"1 個且能下子"| Place
Threat -->|"擋不完或不能下子"| Bomb{"選了炸彈且<br/>冷卻已好？"}
Bomb -->|是| UseBomb[炸對手威脅棋型的一子]
Bomb -->|"否，擋不完"| BlockOne["擋威脅分降最多的<br/>那個成五點"]
Bomb -->|"否，不能下子"| End
BlockOne --> Place
Threat -->|否| Four{"A6 自己能<br/>做活四？"}
Four -->|是| Place
Four -->|否| Three{"A7 對手<br/>有活三？"}
Three -->|是| Place
Three -->|否| Buff{"A8 選了加速、冷卻已好<br/>且能量 ≤ 3？"}
Buff -->|是| UseBuff[使用加速]
Buff -->|否| Eval["A9 / A10 全盤評分<br/>選最高分"]
Eval --> Place
Place -->|是| Send["回傳 Action<br/>交給 GameController::submit"]
Place -->|否| End
UseBomb --> Send
UseBuff --> Send
Send --> End
```

### 4.9 NetworkManager（`src/net/`）— spec N1–N7

```cpp
class NetworkManager : public QObject {
    Q_OBJECT
public:
    bool host(quint16 port);                       // QTcpServer
    void join(const QHostAddress&, quint16 port);  // QTcpSocket
    void sendRequest(const Action&, std::uint32_t seq);   // 加入方 → 主機
    void sendSnapshot(const PlayerView&);                  // 主機 → 加入方（傳 viewFor(加入方)）
    void sendReject(std::uint32_t seq, RejectReason);      // 主機 → 加入方
signals:
    void connected(MatchConfig);
    void requestReceived(Action, std::uint32_t seq);   // 主機收到
    void snapshotReceived(PlayerView);                 // 加入方收到
    void rejected(std::uint32_t seq, RejectReason);
    void disconnected();
};
```

**協定**：每則訊息是一行 JSON（以 `\n` 結尾），編碼與解碼放在 `src/net/protocol.h/.cpp`，函式寫成純函式，方便單元測試。

|type|方向|內容|
|---|---|---|
|`hello`|加入方 → 主機|`version`（`kProtocolVersion`，整數常數，定義於 `protocol.h`；不同就拒絕，spec N5）|
|`welcome`|主機 → 加入方|`version`、`config`（回能間隔等）、`yourColor`|
|`select_skill`|加入方 → 主機|`skill`（選擇階段可多次送出，以最後一次為準）|
|`confirm_skill`|加入方 → 主機|—|
|`opponent_ready`|主機 → 加入方|—（只告知對手已確定，不透露選了什麼，spec S1）|
|`request`|加入方 → 主機|`seq`、`action`|
|`reject`|主機 → 加入方|`seq`、`reason`|
|`snapshot`|主機 → 加入方|加入方的 PlayerView（不含主機方的能量與冷卻）|
|`ping`|雙向|—|
|`rematch`|雙向|—（雙方都送出後才回到技能選擇；一方回主選單即斷線，spec G4）|

**主機端**：網路收到的 `request` 和本機玩家的操作，都進同一個 `GameController::submit`，時間一律用主機的 GameClock。 **加入方**：不執行任何規則，只把操作轉成 `request` 送出，再依收到的 `snapshot` 更新畫面。加入方使用 `RemoteSession`（見 §4.6）：只保存收到的 PlayerView，不自己判定。加入方的能量條使用快照裡的 `nextEnergyRatio`；兩次快照之間（200 ms）由本機時鐘依回能速度往上補，讓動畫連續，收到新快照時再校正。

### 4.10 介面層（`src/ui/`）

- `BoardView`（QWidget，覆寫 `paintEvent`）：畫格線、棋子、被拒絕時的紅色閃爍、炸彈選目標模式、勝利連線標示。
- `HudView`（QWidget）：自己的能量條（10 格，最後一格依比例部分填滿）、自己所選技能的按鈕與冷卻。能量條用 `paintEvent` 自己畫，不用 `QProgressBar`，才能畫出 10 格分段加部分填滿的樣子。
- `MainWindow`：主選單（人機 / 開房 / 加入）、設定、技能選擇畫面（spec U7）、結束畫面。
- UI 收到 `stateChanged` 後呼叫 `viewFor(自己)` 重畫，不自己保存遊戲狀態。

---

## 5. 主要資料流

**本機玩家下子（人機模式或主機端）**

```
BoardView 點擊 (x,y)
 → GameController::submit(PlaceAction, clock.now())
 → EnergyManager::advance → 依 P1 檢查 → Board::set → RuleChecker::findFive
 → emit stateChanged → BoardView / HudView 用 viewFor(自己) 重畫
   （主機端另外呼叫 NetworkManager::sendSnapshot(viewFor(加入方))）
```

**加入方下子**

```
BoardView 點擊 → NetworkManager::sendRequest(seq)
 → 主機 requestReceived → GameController::submit(主機的時間)
 → 成功：sendSnapshot；失敗：sendReject(seq)
 → 加入方 snapshotReceived / rejected → 更新畫面或顯示提示
```

---

## 6. 目錄結構

```
gomoku-rt/
├─ CMakeLists.txt
├─ CLAUDE.md
├─ docs/
│  ├─ spec.md
│  ├─ design.md
│  └─ tasks.md
├─ src/
│  ├─ core/        # 純 C++，編成 gomoku_core 靜態函式庫
│  ├─ net/         # QtNetwork，編成 gomoku_net
│  ├─ app/         # QtCore，編成 gomoku_app
│  ├─ ui/          # QtWidgets
│  └─ main.cpp
├─ tests/
│  ├─ core/        # 只連結 gomoku_core
│  ├─ net/
│  └─ app/
└─ .github/workflows/ci.yml
```

---

## 7. 測試策略

|層|怎麼測|
|---|---|
|core|GoogleTest。時間直接給數字，例如 `advance(s, 0, 4500)`|
|app|GoogleTest + QtCore（`QCoreApplication`，不開視窗）。不用 GameClock，手動指定時間呼叫 `submit` / `tick`|
|net|protocol 編解碼用單元測試；連線行為用 localhost 的整合測試|
|ui|手動測試，清單在 `tasks.md`|
|AI|棋型與評分用單元測試（用小盤面字串建立棋盤）；外加 100 場 AI 對 AI 的煙霧測試|

測試名稱要包含規則編號，例如 `TEST(EnergyTest, E3_CatchUpAfterLongGap)`。

---

## 8. 設計權衡

|問題|決定|理由|
|---|---|---|
|能量上限低，還是上限高再加下子間隔？|上限 10 格，每子間隔 1 秒|保留「存能量再連續進攻」的策略，又不會讓人一瞬間連下五子直接獲勝|
|連線架構用 client-server 還是 P2P？|client-server，主機權威|只有主機的 GameController 能判定，兩人同時下同一格時以主機先收到的為準，不會出現雙方狀態不一致|
|AI 用單純評分還是 minimax + α-β 剪枝？|規則優先 + 評分|即時制沒有太多時間慢慢搜尋；而且難度可以直接用反應時間調整|
|回能用固定 tick 加總還是懶惰計算？|懶惰計算（每次 `advance(from, to)`）|結果與 tick 頻率無關，測試可以精確重現|
|加入方要不要先預測畫面？|不預測，等主機快照|區網延遲很低，預測帶來的同步問題比延遲更麻煩|
|網路同步用事件還是完整快照？|完整快照|狀態很小（225 格 + 自己的狀態），快照最簡單，也不會累積誤差|
|對手的能量與冷卻要不要公開？|不公開，只看得到自己的|增加心理戰，看不到對手存了多少能量、技能好了沒；用 `PlayerView` 從型別上就拿不到對手資料，避免不小心洩漏|
