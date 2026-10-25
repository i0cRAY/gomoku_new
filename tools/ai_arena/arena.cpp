#include "arena.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>
#include <utility>

#include "app/game_controller.h"
#include "baseline_ai.h"
#include "core/ai_engine.h"
#include "core/config.h"

namespace {

constexpr TimeMs kTick = 50;                    // 和 GameClock 相同（design §4.7）
constexpr TimeMs kMatchLimit = 30 * 60 * 1000;  // spec §11：每場模擬時間上限 30 分鐘
constexpr double kZ95 = 1.96;

// 包住要測的 AI：量 decide 的計算時間、統計成功使用技能的次數，必要時指定技能
class MeasuredAI : public AIPlayer {
public:
    MeasuredAI(std::unique_ptr<AIPlayer> inner, std::optional<SkillId> forcedSkill, SideStats& stats)
        : inner(std::move(inner)), forcedSkill(forcedSkill), stats(stats) {}

    SkillId chooseSkill() override {
        const SkillId chosen = inner->chooseSkill();  // 照樣呼叫，亂數序列和實際遊戲相同
        return forcedSkill.value_or(chosen);
    }

    std::optional<Action> decide(const PlayerView& view) override {
        countSkillUse(view);
        const auto start = std::chrono::steady_clock::now();
        std::optional<Action> action = inner->decide(view);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++stats.decisions;
        stats.totalDecideMs += ms;
        stats.maxDecideMs = std::max(stats.maxDecideMs, ms);
        return action;
    }

    void newGame() override {
        inner->newGame();
        lastCharges = 0;
        lastDestroyUsed = false;
    }

    // 對局結束後呼叫，補算最後一次決策之後才生效的技能
    void finish(const PlayerView& view) { countSkillUse(view); }

private:
    // 霸道次數增加（SZ1）或摧毀變成已用過（SX3）就是成功用了一次技能
    void countSkillUse(const PlayerView& view) {
        if (view.self.dominateCharges > lastCharges || (view.self.destroyUsed && !lastDestroyUsed)) {
            ++stats.skillUses;
        }
        lastCharges = view.self.dominateCharges;
        lastDestroyUsed = view.self.destroyUsed;
    }

    std::unique_ptr<AIPlayer> inner;
    std::optional<SkillId> forcedSkill;
    SideStats& stats;
    int lastCharges = 0;
    bool lastDestroyUsed = false;
};

std::unique_ptr<AIPlayer> makeEngine(ArenaEngine engine, PlayerId player, const MatchConfig& match,
                                     const ArenaOptions& options, std::uint32_t seed) {
    const bool useSkills = options.skill != ArenaSkill::None;
    if (engine == ArenaEngine::Baseline) {
        return std::make_unique<baseline::BaselineAI>(player, baseline::frozenConfig(useSkills), match.skill, seed);
    }
    MatchConfig config = match;
    config.ai.useSkills = useSkills;
    if (options.reserve) {
        config.ai.reserveEnergy = *options.reserve;
    }
    return std::make_unique<AIEngine>(player, config, seed);
}

std::optional<SkillId> forcedSkillOf(ArenaSkill skill) {
    switch (skill) {
        case ArenaSkill::Dominate: return SkillId::Dominate;
        case ArenaSkill::Destroy: return SkillId::Destroy;
        case ArenaSkill::None:
        case ArenaSkill::Random: break;
    }
    return std::nullopt;  // none 照樣隨機選，只是不會用
}

bool finished(GameStatus s) {
    return s == GameStatus::BlackWon || s == GameStatus::WhiteWon || s == GameStatus::Draw ||
           s == GameStatus::Aborted;
}

const char* modeName(MatchMode mode) { return mode == MatchMode::TimeLimit ? "限時" : "達分"; }
const char* engineName(ArenaEngine engine) { return engine == ArenaEngine::New ? "new" : "baseline"; }

const char* skillName(ArenaSkill skill) {
    switch (skill) {
        case ArenaSkill::None: return "none";
        case ArenaSkill::Random: return "random";
        case ArenaSkill::Dominate: return "dominate";
        case ArenaSkill::Destroy: return "destroy";
    }
    return "";
}

const char* reasonName(RejectReason reason) {
    switch (reason) {
        case RejectReason::GameNotRunning: return "對局未進行";
        case RejectReason::OutOfBoard: return "超出棋盤";
        case RejectReason::DestroyedCell: return "已摧毀";
        case RejectReason::Occupied: return "已有棋子";
        case RejectReason::RestrictedZone: return "禁區";
        case RejectReason::PlaceCooldown: return "下子間隔";
        case RejectReason::NoEnergy: return "能量不足";
        case RejectReason::SkillNotOwned: return "沒有這項技能";
        case RejectReason::SkillUsedUp: return "技能已用完";
        case RejectReason::InvalidTarget: return "目標不合法";
    }
    return "?";
}

std::optional<int> parsePositive(const std::string& text) {
    if (text.empty() || text.size() > 9 || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    return std::stoi(text);
}

std::string percent(double rate) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f%%", rate * 100.0);
    return buffer;
}

std::string rejectionText(const SideStats& side) {
    if (side.rejections.empty()) {
        return "無";
    }
    std::string text;
    for (const auto& [reason, count] : side.rejections) {
        text += (text.empty() ? "" : "、") + std::string(reasonName(reason)) + " " + std::to_string(count);
    }
    return text;
}

}  // namespace

std::optional<ArenaOptions> parseArenaArgs(const std::vector<std::string>& args, std::string& error) {
    ArenaOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& flag = args[i];
        if (i + 1 >= args.size()) {
            error = flag + " 需要一個值";
            return std::nullopt;
        }
        const std::string& value = args[++i];
        if (flag == "--games") {
            const auto n = parsePositive(value);
            if (!n || *n == 0) {
                error = "--games 必須是正整數";
                return std::nullopt;
            }
            options.games = *n;
        } else if (flag == "--seed") {
            const auto n = parsePositive(value);
            if (!n) {
                error = "--seed 必須是非負整數";
                return std::nullopt;
            }
            options.seed = static_cast<std::uint32_t>(*n);
        } else if (flag == "--reserve") {
            const auto n = parsePositive(value);
            if (!n) {
                error = "--reserve 必須是非負整數";
                return std::nullopt;
            }
            options.reserve = *n;
        } else if (flag == "--mode") {
            if (value == "time") {
                options.modes = {MatchMode::TimeLimit};
            } else if (value == "score") {
                options.modes = {MatchMode::ScoreTarget};
            } else if (value == "both") {
                options.modes = {MatchMode::TimeLimit, MatchMode::ScoreTarget};
            } else {
                error = "--mode 只能是 time、score 或 both";
                return std::nullopt;
            }
        } else if (flag == "--skill") {
            const std::pair<const char*, ArenaSkill> table[] = {{"none", ArenaSkill::None},
                                                                {"random", ArenaSkill::Random},
                                                                {"dominate", ArenaSkill::Dominate},
                                                                {"destroy", ArenaSkill::Destroy}};
            const auto found = std::find_if(std::begin(table), std::end(table), [&](auto& e) { return value == e.first; });
            if (found == std::end(table)) {
                error = "--skill 只能是 none、random、dominate 或 destroy";
                return std::nullopt;
            }
            options.skill = found->second;
        } else if (flag == "--a" || flag == "--b") {
            if (value != "new" && value != "baseline") {
                error = flag + " 只能是 new 或 baseline";
                return std::nullopt;
            }
            (flag == "--a" ? options.a : options.b) = value == "new" ? ArenaEngine::New : ArenaEngine::Baseline;
        } else {
            error = "不認得的參數：" + flag;
            return std::nullopt;
        }
    }
    return options;
}

std::string arenaUsage() {
    return "用法：ai_arena [--games N] [--mode time|score|both] [--seed S]\n"
           "               [--skill none|random|dominate|destroy] [--a new|baseline] [--b new|baseline]\n"
           "               [--reserve R]（new 的保留能量，spec A15）\n"
           "預設：400 場、兩種模式、seed 1、不用技能、A 為 new、B 為 baseline\n";
}

ArenaResult runArena(const ArenaOptions& options, MatchMode mode) {
    ArenaResult result;
    std::mt19937 seeds(options.seed);  // 第 g 場的 seed 由 --seed 與 g 決定
    const std::optional<SkillId> forced = forcedSkillOf(options.skill);

    for (int g = 0; g < options.games; ++g) {
        MatchConfig match;
        match.mode = mode;
        const PlayerId a = g % 2 == 0 ? PlayerId::Black : PlayerId::White;  // A 方輪流執黑、執白
        const PlayerId b = opponent(a);
        const std::uint32_t seedA = seeds();
        const std::uint32_t seedB = seeds();

        GameController controller{match};
        QObject::connect(&controller, &GameController::actionRejected,
                         [&](PlayerId player, RejectReason reason, std::optional<Pos>) {
                             ++(player == a ? result.a : result.b).rejections[reason];
                         });
        auto aiA = std::make_unique<MeasuredAI>(makeEngine(options.a, a, match, options, seedA), forced, result.a);
        auto aiB = std::make_unique<MeasuredAI>(makeEngine(options.b, b, match, options, seedB), forced, result.b);
        MeasuredAI& watchA = *aiA;
        MeasuredAI& watchB = *aiB;
        controller.attachAI(a, std::move(aiA));  // 和人機模式同一條路：A2a 選技能、每次 tick 決策（design §4.8）
        controller.attachAI(b, std::move(aiB));

        for (TimeMs now = -match.countdown; now <= kMatchLimit && !finished(controller.viewFor(a).status);
             now += kTick) {
            controller.tick(now);
        }
        const PlayerView view = controller.viewFor(a);
        watchA.finish(view);
        watchB.finish(controller.viewFor(b));

        const GameStatus aWon = a == PlayerId::Black ? GameStatus::BlackWon : GameStatus::WhiteWon;
        const GameStatus bWon = a == PlayerId::Black ? GameStatus::WhiteWon : GameStatus::BlackWon;
        double points = 0.5;
        if (view.status == aWon) {
            ++result.wins;
            points = 1.0;
        } else if (view.status == bWon) {
            ++result.losses;
            points = 0.0;
        } else {
            ++result.draws;
            if (!finished(view.status)) {
                ++result.unfinished;
            }
        }
        ++result.games;
        if (a == PlayerId::Black) {
            ++result.blackGames;
            result.blackPoints += points;
        } else {
            ++result.whiteGames;
            result.whitePoints += points;
        }
        result.a.score += view.scores[static_cast<std::size_t>(a)];
        result.b.score += view.scores[static_cast<std::size_t>(b)];
    }
    return result;
}

RateInterval winRate(double points, int games) {
    if (games <= 0) {
        return {0.0, 0.0, 0.0};
    }
    const double rate = points / games;
    const double margin = kZ95 * std::sqrt(rate * (1.0 - rate) / games);
    return {rate, std::max(0.0, rate - margin), std::min(1.0, rate + margin)};
}

std::string formatResult(const ArenaOptions& options, MatchMode mode, const ArenaResult& r) {
    const RateInterval total = winRate(r.points(), r.games);
    const double games = std::max(r.games, 1);
    char buffer[512];
    std::string text;

    std::snprintf(buffer, sizeof buffer, "== %s | A=%s 對 B=%s | 技能 %s | %d 場 | seed %u%s ==\n", modeName(mode),
                  engineName(options.a), engineName(options.b), skillName(options.skill), r.games, options.seed,
                  options.reserve ? (" | R " + std::to_string(*options.reserve)).c_str() : "");
    text += buffer;
    std::snprintf(buffer, sizeof buffer, "A 方：勝 %d 負 %d 和 %d  勝率 %s（95%% 信賴區間 %s–%s，和局算半場）\n", r.wins,
                  r.losses, r.draws, percent(total.rate).c_str(), percent(total.low).c_str(),
                  percent(total.high).c_str());
    text += buffer;
    std::snprintf(buffer, sizeof buffer, "  執黑 %s（%d 場）  執白 %s（%d 場）\n",
                  percent(winRate(r.blackPoints, r.blackGames).rate).c_str(), r.blackGames,
                  percent(winRate(r.whitePoints, r.whiteGames).rate).c_str(), r.whiteGames);
    text += buffer;
    std::snprintf(buffer, sizeof buffer, "  平均分 A %.1f : B %.1f  平均技能次數 A %.2f : B %.2f\n", r.a.score / games,
                  r.b.score / games, r.a.skillUses / games, r.b.skillUses / games);
    text += buffer;
    text += "  被拒絕的請求：A " + rejectionText(r.a) + "；B " + rejectionText(r.b) + "\n";
    std::snprintf(buffer, sizeof buffer, "  沒有結束的場數：%d\n", r.unfinished);
    text += buffer;
    std::snprintf(buffer, sizeof buffer, "  decide 時間：A 平均 %.3f ms 最長 %.2f ms；B 平均 %.3f ms 最長 %.2f ms\n",
                  r.a.decisions ? r.a.totalDecideMs / r.a.decisions : 0.0, r.a.maxDecideMs,
                  r.b.decisions ? r.b.totalDecideMs / r.b.decisions : 0.0, r.b.maxDecideMs);
    text += buffer;
    return text;
}
