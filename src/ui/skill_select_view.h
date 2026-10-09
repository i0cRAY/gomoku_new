#pragma once

#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <map>
#include <optional>
#include <vector>

#include "core/config.h"
#include "core/types.h"

// 技能選擇畫面（spec U7、S1、G1a、G4）：顯示兩項技能的名稱、效果與冷卻，點選後按「確定」。
// 只記得畫面上的勾選狀態；真正的選擇由 GameSession 保存與判定。
class SkillSelectView : public QWidget {
    Q_OBJECT
public:
    explicit SkillSelectView(SkillConfig, QWidget* parent = nullptr);

    // players：這台電腦要選技能的玩家（M3 為雙方）；defaults：上一局的選擇（G4）
    void reset(const std::vector<PlayerId>& players, const std::map<PlayerId, SkillId>& defaults);
    void setOpponentReady(bool);  // U7：區網或人機時顯示對手是否已確定

signals:
    void skillSelected(PlayerId, SkillId);
    void skillConfirmed(PlayerId, SkillId);

private:
    struct Column {
        PlayerId player;
        QWidget* box;
        QButtonGroup* group;
        QPushButton* confirmButton;
        bool confirmed = false;
    };

    void refreshStatus();
    std::optional<SkillId> checkedSkill(const Column&) const;

    SkillConfig config;
    QWidget* columnsArea;
    QLabel* statusLabel;
    std::vector<Column> columns;
    bool opponentReady = false;
};
