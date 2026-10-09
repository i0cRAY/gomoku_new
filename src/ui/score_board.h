#pragma once

#include <QLabel>
#include <QWidget>

#include "core/player_view.h"

// 計分板（spec U8）：雙方分數，以及限時模式的剩餘時間或達分模式的目標分數（公開資訊，E5）
class ScoreBoard : public QWidget {
    Q_OBJECT
public:
    explicit ScoreBoard(QWidget* parent = nullptr);
    void setView(const PlayerView&);

private:
    QLabel* scoreLabel;
    QLabel* infoLabel;
};
