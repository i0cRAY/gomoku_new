#pragma once

#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <vector>

#include "core/config.h"
#include "core/player_view.h"
#include "core/types.h"

// 10 格能量條（spec E6），自己畫才能做出分段加部分填滿
class EnergyBar : public QWidget {
    Q_OBJECT
public:
    explicit EnergyBar(QWidget* parent = nullptr);
    void setSegments(std::vector<double> segments, bool accelerating);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::vector<double> segments;
    bool accelerating = false;
};

// 只顯示自己的資訊（spec U2、E5）：能量條、下子間隔、技能按鈕與冷卻、加速剩餘時間、拒絕原因
class HudView : public QWidget {
    Q_OBJECT
public:
    // skillKey：使用技能的快捷鍵（U4 預設 Q；M3 白方用另一個鍵避免衝突）
    explicit HudView(MatchConfig, QKeySequence skillKey = QKeySequence(Qt::Key_Q), QWidget* parent = nullptr);

    void setConfig(const MatchConfig&);
    void setTitle(const QString&);  // M3 同時顯示雙方時標示是哪一方
    void setView(const PlayerView&);
    void setTargeting(bool);           // U3：顯示「選擇目標」提示
    void showRejection(RejectReason);  // U5

signals:
    void skillTriggered();  // 技能按鈕或快捷鍵（U4）

private:
    void refreshSkillText();

    MatchConfig config;
    QKeySequence skillKey;
    PlayerView view;
    bool targeting = false;
    QLabel* titleLabel;
    EnergyBar* energyBar;
    QLabel* placeLabel;
    QPushButton* skillButton;
    QLabel* skillLabel;
    QLabel* accelerateLabel;
    QLabel* messageLabel;
    QTimer messageTimer;
};
