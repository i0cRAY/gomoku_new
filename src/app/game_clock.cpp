#include "app/game_clock.h"

GameClock::GameClock(QObject* parent) : QObject(parent) {
    timer.setInterval(static_cast<int>(kTickInterval));
    connect(&timer, &QTimer::timeout, this, [this] { emit tick(now()); });
}

void GameClock::start(TimeMs startAt) {
    origin = startAt;
    elapsed.start();
    timer.start();
}

void GameClock::stop() {
    timer.stop();
    elapsed.invalidate();
}

bool GameClock::isRunning() const {
    return elapsed.isValid();
}

TimeMs GameClock::now() const {
    return isRunning() ? origin + elapsed.elapsed() : origin;
}
