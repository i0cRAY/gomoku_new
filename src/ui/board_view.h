#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <vector>

#include "core/player_view.h"
#include "core/types.h"

// 棋盤顯示（spec U1、U5、U6、P6）。不保存遊戲狀態，只畫最近一次收到的 PlayerView。
class BoardView : public QWidget {
    Q_OBJECT
public:
    explicit BoardView(QWidget* parent = nullptr);

    void setView(const PlayerView&);
    void flashRejected(Pos);  // P6：被拒絕的格子閃一下紅色
    void setTargeting(bool);  // U3：選炸彈目標時改用十字游標

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void cellClicked(Pos, Qt::MouseButton);  // U1；M3 用左右鍵區分黑白

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    struct Flash {
        Pos pos;
        QElapsedTimer started;
    };

    PlayerView view;
    std::vector<Flash> flashes;
    QTimer animation;
};
