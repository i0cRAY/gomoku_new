#include "ui/skill_select_view.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>


#include "ui/hud_model.h"
#include "ui/menu_model.h"

namespace {

constexpr int kDescriptionIndent = 24;

QString toQString(const std::string& s) {
    return QString::fromStdString(s);
}

QString playerName(PlayerId p) {
    return p == PlayerId::Black ? QStringLiteral("黑方") : QStringLiteral("白方");
}

}  // namespace

SkillSelectView::SkillSelectView(SkillConfig config, QWidget* parent)
    : QWidget(parent), config(config), columnsArea(new QWidget), statusLabel(new QLabel) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(QStringLiteral("選擇技能"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() * 2);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    statusLabel->setAlignment(Qt::AlignCenter);

    new QHBoxLayout(columnsArea);
    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(columnsArea);
    layout->addWidget(statusLabel);
    auto* backButton = new QPushButton(QStringLiteral("回主選單"));
    layout->addWidget(backButton, 0, Qt::AlignCenter);
    layout->addStretch();
    connect(backButton, &QPushButton::clicked, this, &SkillSelectView::backToMenuRequested);  // G4b
}

void SkillSelectView::reset(const std::vector<PlayerId>& players, const std::map<PlayerId, SkillId>& defaults) {
    for (Column& c : columns) {
        delete c.box;
    }
    columns.clear();
    opponentReady = false;

    auto* row = static_cast<QHBoxLayout*>(columnsArea->layout());
    for (PlayerId player : players) {
        auto* box = new QGroupBox(players.size() > 1 ? playerName(player) : QString());
        auto* boxLayout = new QVBoxLayout(box);
        auto* group = new QButtonGroup(box);
        for (SkillId skill : skillOptions()) {
            auto* radio = new QRadioButton(toQString(skillName(skill)));
            auto* description = new QLabel(toQString(skillDescription(skill, config)));
            description->setContentsMargins(kDescriptionIndent, 0, 0, 0);
            group->addButton(radio, static_cast<int>(skill));
            boxLayout->addWidget(radio);
            boxLayout->addWidget(description);
            if (const auto it = defaults.find(player); it != defaults.end() && it->second == skill) {
                radio->setChecked(true);  // G4：預設選中上一局的技能
            }
        }
        auto* confirmButton = new QPushButton(QStringLiteral("確定"));
        boxLayout->addWidget(confirmButton);
        row->addWidget(box);

        columns.push_back(Column{player, box, group, confirmButton});
        const std::size_t index = columns.size() - 1;

        connect(group, &QButtonGroup::idClicked, this, [this, index](int id) {
            columns[index].confirmButton->setEnabled(true);
            emit skillSelected(columns[index].player, static_cast<SkillId>(id));
        });
        connect(confirmButton, &QPushButton::clicked, this, [this, index] {
            Column& c = columns[index];
            const auto skill = checkedSkill(c);
            if (!skill) {
                return;
            }
            c.confirmed = true;
            c.box->setEnabled(false);
            c.confirmButton->setText(QStringLiteral("已確定"));
            refreshStatus();
            emit skillConfirmed(c.player, *skill);
        });
    }
    refreshStatus();
}

void SkillSelectView::setOpponentReady(bool ready) {
    opponentReady = ready;
    refreshStatus();
}

void SkillSelectView::refreshStatus() {
    bool allConfirmed = !columns.empty();
    for (Column& c : columns) {
        if (!c.confirmed) {
            c.confirmButton->setEnabled(checkedSkill(c).has_value());  // G1a：沒選不能確定
            allConfirmed = false;
        }
    }
    if (columns.size() > 1) {
        statusLabel->clear();  // M3：雙方都在本機
    } else if (allConfirmed) {
        statusLabel->setText(opponentReady ? QStringLiteral("對手已準備") : QStringLiteral("等待對手選擇…"));
    } else {
        statusLabel->setText(opponentReady ? QStringLiteral("對手已準備") : QString());
    }
}

std::optional<SkillId> SkillSelectView::checkedSkill(const Column& c) const {
    const int id = c.group->checkedId();
    if (id < 0) {
        return std::nullopt;
    }
    return static_cast<SkillId>(id);
}
