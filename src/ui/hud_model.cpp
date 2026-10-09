#include "ui/hud_model.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

constexpr TimeMs kMsPerSecond = 1000;
constexpr int kSecondsPerMinute = 60;

int secondsRoundedUp(TimeMs ms) {
    return ms <= 0 ? 0 : static_cast<int>((ms + kMsPerSecond - 1) / kMsPerSecond);
}

}  // namespace

std::vector<double> energySegments(const PlayerView& view, int maxEnergy) {
    std::vector<double> segments(static_cast<std::size_t>(std::max(0, maxEnergy)), 0.0);
    const int energy = std::clamp(view.self.energy, 0, maxEnergy);
    for (int i = 0; i < energy; ++i) {
        segments[static_cast<std::size_t>(i)] = 1.0;
    }
    if (energy < maxEnergy) {
        segments[static_cast<std::size_t>(energy)] = std::clamp(view.nextEnergyRatio, 0.0, 1.0);
    }
    return segments;
}

bool isPlaceReady(const PlayerView& view, TimeMs placeCooldown) {
    return !view.self.lastPlaceTime || view.now - *view.self.lastPlaceTime >= placeCooldown;
}

bool isSkillAvailable(const PlayerView& view, int energyCost) {
    if (view.self.skill == SkillId::Destroy && view.self.destroyUsed) {
        return false;
    }
    return view.self.energy >= energyCost;
}

std::string skillStatusText(const PlayerView& view, int energyCost) {
    if (view.self.skill == SkillId::Destroy && view.self.destroyUsed) {
        return {};  // skillDetailText 已顯示「已使用」
    }
    const std::string cost = std::to_string(energyCost) + " 格）";
    return isSkillAvailable(view, energyCost) ? "可使用（消耗 " + cost : "能量不足（需要 " + cost;
}

std::string skillDetailText(const PlayerView& view) {
    switch (view.self.skill) {
        case SkillId::Dominate:
            return view.self.dominateCharges > 0 ? "霸道：還有 " + std::to_string(view.self.dominateCharges) + " 子"
                                                 : std::string{};
        case SkillId::Destroy:
            return view.self.destroyUsed ? "已使用" : "每局一次";
        default:
            return {};
    }
}

bool isSkillButtonEnabled(const PlayerView& view) {
    return !(view.self.skill == SkillId::Destroy && view.self.destroyUsed);  // SX3
}

std::string targetingPrompt(SkillId skill, const SkillConfig& config) {
    if (skill == SkillId::Destroy) {
        const std::string side = std::to_string(config.destroyRadius * 2 + 1);
        return "選擇目標：點 " + side + "×" + side + " 範圍的中心\n右鍵或 Esc 取消";
    }
    return "選擇目標：點對手的棋子\n右鍵或 Esc 取消";
}

std::string scoreText(const PlayerView& view) {
    return "黑 " + std::to_string(view.scores[0]) + " : " + std::to_string(view.scores[1]) + " 白";
}

std::string matchInfoText(const PlayerView& view) {
    if (view.mode == MatchMode::ScoreTarget) {
        return "先得 " + std::to_string(view.targetScore) + " 分獲勝";
    }
    const int total = secondsRoundedUp(view.timeRemaining);
    std::ostringstream text;
    text << "剩餘 " << total / kSecondsPerMinute << ':' << std::setw(2) << std::setfill('0')
         << total % kSecondsPerMinute;
    return text.str();
}

std::string rejectReasonText(RejectReason reason) {
    switch (reason) {
        case RejectReason::GameNotRunning: return "對局尚未開始或已結束";
        case RejectReason::OutOfBoard: return "超出棋盤";
        case RejectReason::DestroyedCell: return "該格已被摧毀";
        case RejectReason::Occupied: return "該格已有棋子";
        case RejectReason::RestrictedZone: return "該格是對手的禁區";
        case RejectReason::PlaceCooldown: return "下子間隔未到";
        case RejectReason::NoEnergy: return "能量不足";
        case RejectReason::SkillNotOwned: return "沒有這項技能";
        case RejectReason::SkillUsedUp: return "技能已用完";
        case RejectReason::InvalidTarget: return "目標必須是對手的棋子";
    }
    return {};
}

std::string skillName(SkillId skill) {
    switch (skill) {
        case SkillId::Bomb: return "炸彈";
        case SkillId::Dominate: return "霸道";
        case SkillId::Destroy: return "摧毀";
    }
    return {};
}
