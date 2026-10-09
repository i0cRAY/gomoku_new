#include "ui/hud_model.h"

#include <algorithm>

namespace {

constexpr TimeMs kMsPerSecond = 1000;

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

int skillCooldownSeconds(const PlayerView& view) {
    return secondsRoundedUp(view.self.skillReadyAt - view.now);
}

int accelerateRemainingSeconds(const PlayerView& view) {
    return view.accelerating ? secondsRoundedUp(view.self.accelerateUntil - view.now) : 0;
}

std::string rejectReasonText(RejectReason reason) {
    switch (reason) {
        case RejectReason::GameNotRunning: return "對局尚未開始或已結束";
        case RejectReason::OutOfBoard: return "超出棋盤";
        case RejectReason::Occupied: return "該格已有棋子";
        case RejectReason::PlaceCooldown: return "下子間隔未到";
        case RejectReason::NoEnergy: return "能量不足";
        case RejectReason::SkillNotOwned: return "沒有這項技能";
        case RejectReason::SkillCooldown: return "技能冷卻中";
        case RejectReason::InvalidTarget: return "目標必須是對手的棋子";
    }
    return {};
}

std::string skillName(SkillId skill) {
    switch (skill) {
        case SkillId::Accelerate: return "加速";
        case SkillId::Bomb: return "炸彈";
    }
    return {};
}
