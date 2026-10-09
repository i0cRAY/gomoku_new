#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "core/types.h"

// 對局時鐘：把真實經過的時間換成對局時間，並定時發出 tick。
// 測試不使用它，直接以指定的時間呼叫 GameController。
class GameClock : public QObject {
    Q_OBJECT
public:
    static constexpr TimeMs kTickInterval = 50;

    explicit GameClock(QObject* parent = nullptr);

    void start(TimeMs startAt);  // 從 startAt 起算（倒數開始時傳 −3000，spec G2）
    void stop();
    bool isRunning() const;
    TimeMs now() const;

signals:
    void tick(TimeMs now);

private:
    QElapsedTimer elapsed;
    QTimer timer;
    TimeMs origin = 0;
};
