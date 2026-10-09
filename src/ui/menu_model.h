#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/types.h"

// 主選單、技能選擇與結果畫面的文字與選項（純計算，不依賴 Qt，方便測試）

std::vector<TimeMs> regenIntervalOptions();     // G1：500–10000 ms，500 的倍數
std::size_t defaultRegenIntervalIndex();        // 預設值（MatchConfig 的 regenInterval）在選項中的位置
std::string regenIntervalLabel(TimeMs interval);
std::string skillDescription(SkillId, const SkillConfig&);  // U7：效果說明與冷卻時間
std::string countdownText(TimeMs remaining);    // G2：3、2、1
std::string resultText(GameStatus);             // U6：勝方或和局；未結束時為空字串
bool isFinished(GameStatus);
