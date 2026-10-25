#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/types.h"

// AI 對戰跑分（spec §11、design §4.8）：A、B 兩個 AI 用 GameController 對戰，統計 A 方的勝率

enum class ArenaEngine { New, Baseline };                  // new：目前的 AIEngine；baseline：凍結的 BaselineAI
enum class ArenaSkill { None, Random, Dominate, Destroy };  // none：雙方都不用技能（AIConfig::useSkills = false）

struct ArenaOptions {
    int games = 400;
    std::vector<MatchMode> modes{MatchMode::TimeLimit, MatchMode::ScoreTarget};
    std::uint32_t seed = 1;
    ArenaSkill skill = ArenaSkill::None;
    ArenaEngine a = ArenaEngine::New;
    ArenaEngine b = ArenaEngine::Baseline;
    std::optional<int> reserve;  // 指定 new 的保留能量 R（spec A15），沒指定用 AIConfig 預設
};

// 解析命令列參數（不含程式名稱）；不合法時回傳 nullopt，原因寫進 error
std::optional<ArenaOptions> parseArenaArgs(const std::vector<std::string>& args, std::string& error);
std::string arenaUsage();

struct SideStats {
    long long score = 0;   // 所有場次的分數總和
    int skillUses = 0;     // 成功使用技能的次數
    std::map<RejectReason, int> rejections;
    long long decisions = 0;  // decide 被呼叫的次數
    double totalDecideMs = 0.0;
    double maxDecideMs = 0.0;
};

struct ArenaResult {
    int games = 0;
    int wins = 0;  // 以 A 方計
    int losses = 0;
    int draws = 0;       // 含沒有結束的場次
    int unfinished = 0;  // 模擬 30 分鐘還沒結束
    int blackGames = 0;  // A 方執黑的場數
    double blackPoints = 0.0;
    int whiteGames = 0;
    double whitePoints = 0.0;
    SideStats a;
    SideStats b;

    double points() const { return wins + 0.5 * draws; }  // 勝 1、和 0.5
};

ArenaResult runArena(const ArenaOptions&, MatchMode);

struct RateInterval {
    double rate;
    double low;
    double high;
};

// 勝率（和局算半場）與 95% 信賴區間：rate ± 1.96 × √(rate × (1 − rate) ÷ games)，截在 0–1
RateInterval winRate(double points, int games);

std::string formatResult(const ArenaOptions&, MatchMode, const ArenaResult&);
