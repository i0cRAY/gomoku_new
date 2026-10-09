#include "ui/score_board.h"

#include <QVBoxLayout>

#include "ui/hud_model.h"

namespace {

constexpr int kScoreFontScale = 2;

}  // namespace

ScoreBoard::ScoreBoard(QWidget* parent) : QWidget(parent), scoreLabel(new QLabel), infoLabel(new QLabel) {
    auto* layout = new QVBoxLayout(this);
    QFont font = scoreLabel->font();
    font.setPointSize(font.pointSize() * kScoreFontScale);
    font.setBold(true);
    scoreLabel->setFont(font);
    scoreLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(scoreLabel);
    layout->addWidget(infoLabel);
}

void ScoreBoard::setView(const PlayerView& view) {
    scoreLabel->setText(QString::fromStdString(scoreText(view)));
    infoLabel->setText(QString::fromStdString(matchInfoText(view)));
}
