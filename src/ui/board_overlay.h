#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/types.h"

// 棋盤上疊加顯示的計算（純計算，不依賴 Qt，方便測試）：技能範圍預覽、禁區、消除動畫、摧毀爆炸動畫

constexpr int kClearFlashDurationMs = 500;  // U11 ⚠️待確認
// U13：爆炸影格數與播放速度，要和 resources/explosion/ 一致（scripts/make_explosion_frames.sh）
constexpr int kExplosionFrameCount = 44;
constexpr int kExplosionFps = 30;
constexpr int kExplosionDurationMs = kExplosionFrameCount * 1000 / kExplosionFps;

// U3：技能影響範圍相對於目標的位置：x、y 各從 target − before 到 target + after
struct SkillArea {
    int before;
    int after;
};
// 炸彈：以目標為左上角的 bombSize × bombSize（SB3）；摧毀：中心 ±destroyRadius（SX2）；霸道沒有範圍
std::optional<SkillArea> skillArea(SkillId, const SkillConfig&);
// 範圍內、截掉棋盤外的格子
std::vector<Pos> areaCells(Pos target, SkillArea);
// U9：禁區的不透明度 1.0 → 0.0，隨剩餘時間漸淡
double zoneOpacity(const ZoneCell&, TimeMs now, TimeMs duration);
// U9：禁區剩餘秒數（無條件進位），到期為 0
int zoneRemainingSeconds(const ZoneCell&, TimeMs now);
// U11：消除得分文字，例如 "+5"；沒有連線時為空字串
std::string clearedScoreText(const std::vector<ClearedLine>&);
// U11：「+N」文字顯示的位置（第一條線中間那顆）
std::optional<Pos> clearedLabelCell(const std::vector<ClearedLine>&);
// U13：before → after 之間新變成已摧毀的格子（依 y、x 排序）
std::vector<Pos> newlyDestroyedCells(const Board& before, const Board& after);
// U13：爆炸開始後經過 elapsed 毫秒要畫的影格；播完（或時間為負）時為 nullopt
std::optional<int> explosionFrame(TimeMs elapsed);
