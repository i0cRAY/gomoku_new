#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <optional>
#include <vector>

#include "core/player_view.h"
#include "core/types.h"

// 棋盤顯示（spec U1、U3、U5、U9、U11、P6）。不保存遊戲狀態，只畫最近一次收到的 PlayerView。
class BoardView : public QWidget {
    Q_OBJECT
public:
    explicit BoardView(QWidget* parent = nullptr);

    void setView(const PlayerView&);
    void flashRejected(Pos);  // P6：被拒絕的格子閃一下紅色
    // U3：選目標時改用十字游標；previewRadius 有值時（摧毀）預覽滑鼠所在位置的範圍
    void setTargeting(bool, std::optional<int> previewRadius = std::nullopt);
    void setZoneDuration(TimeMs);                      // U9：禁區漸淡用的總時間
    void flashCleared(const std::vector<ClearedLine>&);  // U11：消除的連線閃一下並顯示「+N」

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void cellClicked(Pos, Qt::MouseButton);  // U1；M3 用左右鍵區分黑白

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Flash {
        Pos pos;
        QElapsedTimer started;
    };

    void paintZones(QPainter&, double cell);
    void paintClearFlash(QPainter&, double cell);
    void paintDestroyPreview(QPainter&, double cell);
    void startAnimation();

    PlayerView view;
    std::vector<Flash> flashes;
    std::vector<ClearedLine> clearing;  // U11：正在閃的連線（棋子已從棋盤移除）
    QElapsedTimer clearingStarted;
    std::optional<int> previewRadius;
    std::optional<Pos> hover;
    TimeMs zoneDuration = 3000;
    QTimer animation;
};
