#include "ui/menu_model.h"

#include <algorithm>

namespace {

constexpr TimeMs kMinRegenInterval = 500;   // spec §4
constexpr TimeMs kMaxRegenInterval = 10000;
constexpr TimeMs kRegenIntervalStep = 500;
constexpr TimeMs kMsPerSecond = 1000;
constexpr TimeMs kMsPerTenth = 100;

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

std::vector<Difficulty> difficultyOptions() {
    return {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard};
}

std::size_t defaultDifficultyIndex() {
    const auto options = difficultyOptions();
    const auto it = std::find(options.begin(), options.end(), MatchConfig{}.ai.difficulty);
    return static_cast<std::size_t>(it - options.begin());
}

std::string difficultyLabel(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy: return "簡單";
        case Difficulty::Normal: return "普通";
        case Difficulty::Hard: return "困難";
    }
    return {};
}

std::string skillDescription(SkillId skill, const SkillConfig& config) {
    switch (skill) {
        case SkillId::Accelerate:
            return "回能速度變為 " + std::to_string(config.accelerateRegenMultiplier) + " 倍，持續 " +
                   secondsText(config.accelerateDuration) + "\n冷卻 " + secondsText(config.accelerateCooldown);
        case SkillId::Bomb:
            return "移除一顆對手的棋子\n冷卻 " + secondsText(config.bombCooldown);
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

bool isFinished(GameStatus status) {
    return status == GameStatus::BlackWon || status == GameStatus::WhiteWon || status == GameStatus::Draw ||
           status == GameStatus::Aborted;
}
