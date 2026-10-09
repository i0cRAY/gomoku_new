#pragma once

#include <string>
#include <vector>

#include "core/player_view.h"
#include "core/types.h"

// HUD 要顯示的數值與文字（純計算，不依賴 Qt，方便測試）。只使用自己的 PlayerView（spec E5）。

// E6：每格的填滿比例 0.0–1.0。前 energy 格填滿，下一格依回能比例部分填滿；滿格時全部填滿
std::vector<double> energySegments(const PlayerView&, int maxEnergy);
// U2：下子間隔是否已過
bool isPlaceReady(const PlayerView&, TimeMs placeCooldown);
// U2：技能剩餘冷卻秒數（無條件進位），0 表示可使用
int skillCooldownSeconds(const PlayerView&);
// U2、SA1：加速剩餘秒數（無條件進位），沒有加速時為 0
int accelerateRemainingSeconds(const PlayerView&);

std::string rejectReasonText(RejectReason);  // U5
std::string skillName(SkillId);
