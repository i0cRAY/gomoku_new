#include "ui/menu_model.h"

#include <algorithm>

namespace {

constexpr TimeMs kMinRegenInterval = 500;   // spec §4
constexpr TimeMs kMaxRegenInterval = 10000;
constexpr TimeMs kRegenIntervalStep = 500;
constexpr TimeMs kMsPerSecond = 1000;
constexpr TimeMs kMsPerTenth = 100;
constexpr TimeMs kMsPerMinute = 60000;

// 1500 → "1.5 秒"、2000 → "2 秒"
std::string secondsText(TimeMs ms) {
    std::string text = std::to_string(ms / kMsPerSecond);
    const TimeMs tenths = (ms % kMsPerSecond) / kMsPerTenth;
    if (tenths != 0) {
        text += "." + std::to_string(tenths);
    }
    return text + " 秒";
}

}  // namespace

std::vector<TimeMs> regenIntervalOptions() {
    std::vector<TimeMs> options;
    for (TimeMs t = kMinRegenInterval; t <= kMaxRegenInterval; t += kRegenIntervalStep) {
        options.push_back(t);
    }
    return options;
}

std::size_t defaultRegenIntervalIndex() {
    const auto options = regenIntervalOptions();
    const auto it = std::find(options.begin(), options.end(), MatchConfig{}.regenInterval);
    return static_cast<std::size_t>(it - options.begin());
}

std::string regenIntervalLabel(TimeMs interval) {
    return secondsText(interval);
}

std::vector<TimeMs> timeLimitOptions() {
    return {5 * kMsPerMinute, 3 * kMsPerMinute, 1 * kMsPerMinute};
}

std::size_t defaultTimeLimitIndex() {
    const auto options = timeLimitOptions();
    const auto it = std::find(options.begin(), options.end(), MatchConfig{}.timeLimit);
    return static_cast<std::size_t>(it - options.begin());
}

std::string timeLimitLabel(TimeMs limit) {
    return std::to_string(limit / kMsPerMinute) + " 分鐘";
}

std::string matchModeLabel(MatchMode mode) {
    return mode == MatchMode::TimeLimit ? "限時" : "達分";
}

std::vector<SkillId> skillOptions() {
    return {SkillId::Bomb, SkillId::Dominate, SkillId::Destroy};
}

std::string skillDescription(SkillId skill, const SkillConfig& config) {
    const std::string cost = "消耗 " + std::to_string(config.costOf(skill)) + " 格能量";  // U7、S2
    switch (skill) {
        case SkillId::Bomb: {
            const std::string side = std::to_string(config.bombSize);
            return "清掉以目標為左上角 " + side + "×" + side + " 範圍內\n雙方的棋子\n" + cost;
        }
        case SkillId::Dominate:
            return "接下來 " + std::to_string(config.dominateStones) + " 顆棋子的上下左右\n對手 " +
                   secondsText(config.zoneDuration) + "內不能下\n" + cost;
        case SkillId::Destroy: {
            const int side = config.destroyRadius * 2 + 1;
            return "清空 " + std::to_string(side) + "×" + std::to_string(side) +
                   " 區域，被清空的格子整局不能再下\n每局一次，" + cost;
        }
    }
    return {};
}

std::string countdownText(TimeMs remaining) {
    return std::to_string((std::max<TimeMs>(remaining, 0) + kMsPerSecond - 1) / kMsPerSecond);
}

std::string resultText(GameStatus status) {
    switch (status) {
        case GameStatus::BlackWon: return "黑方獲勝";
        case GameStatus::WhiteWon: return "白方獲勝";
        case GameStatus::Draw: return "和局";
        case GameStatus::Aborted: return "連線中斷";
        default: return {};
    }
}

std::string finalResultText(GameStatus status, const std::array<int, 2>& scores) {
    const std::string result = resultText(status);
    if (result.empty()) {
        return {};
    }
    return result + "\n黑 " + std::to_string(scores[0]) + " : " + std::to_string(scores[1]) + " 白";
}

bool isFinished(GameStatus status) {
    return status == GameStatus::BlackWon || status == GameStatus::WhiteWon || status == GameStatus::Draw ||
           status == GameStatus::Aborted;
}
