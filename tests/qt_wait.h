#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <functional>

// 處理 Qt 事件直到 condition 成立或逾時；回傳 condition 最後是否成立（區網整合測試用）
inline bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return true;
}

// 處理 Qt 事件一段時間（確認某件事「沒有」發生時使用）
inline void processEventsFor(int ms) {
    waitUntil([] { return false; }, ms);
}
