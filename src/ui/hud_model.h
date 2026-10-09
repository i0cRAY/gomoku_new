#pragma once

#include <string>
#include <vector>

#include "core/config.h"
#include "core/player_view.h"
#include "core/types.h"

// HUD 要顯示的數值與文字（純計算，不依賴 Qt，方便測試）。只使用自己的 PlayerView（spec E5）。

// E6：每格的填滿比例 0.0–1.0。前 energy 格填滿，下一格依回能比例部分填滿；滿格時全部填滿
std::vector<double> energySegments(const PlayerView&, int maxEnergy);
// U2：下子間隔是否已過
bool isPlaceReady(const PlayerView&, TimeMs placeCooldown);

// U2、S3：技能是否可以使用（能量 ≥ energyCost；摧毀還沒用過）
bool isSkillAvailable(const PlayerView&, int energyCost);
// U2：「可使用（消耗 3 格）」或「能量不足（需要 3 格）」；摧毀已用過時為空字串
std::string skillStatusText(const PlayerView&, int energyCost);
// U2：霸道剩餘次數、摧毀是否已用；其他技能為空字串
std::string skillDetailText(const PlayerView&);
// U2：技能按鈕能不能按。摧毀用過後變灰；能量不足時仍可按，由 GameController 拒絕並提示（U5）
bool isSkillButtonEnabled(const PlayerView&);
// U3：選目標模式的提示文字（摧毀的範圍取自設定）
std::string targetingPrompt(SkillId, const SkillConfig&);
// U8：雙方分數，例如「黑 5 : 11 白」
std::string scoreText(const PlayerView&);
// U8：限時模式顯示剩餘時間（m:ss，無條件進位到秒），達分模式顯示目標分數
std::string matchInfoText(const PlayerView&);

std::string rejectReasonText(RejectReason);  // U5
std::string skillName(SkillId);
