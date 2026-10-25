#include <QCoreApplication>

#include <cstdio>
#include <string>
#include <vector>

#include "arena.h"

// AI 對戰跑分工具（spec §11）。建議用 Release 建置執行
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
        std::fputs(arenaUsage().c_str(), stdout);
        return 0;
    }
    std::string error;
    const auto options = parseArenaArgs(args, error);
    if (!options) {
        std::fprintf(stderr, "%s\n%s", error.c_str(), arenaUsage().c_str());
        return 2;
    }
    for (MatchMode mode : options->modes) {
        std::fputs(formatResult(*options, mode, runArena(*options, mode)).c_str(), stdout);
        std::fflush(stdout);
    }
    return 0;
}
