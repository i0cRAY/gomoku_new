#include "ui/hud_view.h"

#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>

#include <utility>

#include "ui/hud_model.h"

namespace {

const QColor kSegmentEmpty(60, 60, 60);
const QColor kSegmentFill(60, 170, 230);
const QColor kSegmentBorder(30, 30, 30);
const QColor kRejectText(200, 30, 30);

constexpr int kSegmentGap = 3;
constexpr int kBarHeight = 22;
constexpr int kBarWidth = 220;
constexpr int kMessageDurationMs = 2000;

QString toQString(const std::string& s) {
    return QString::fromStdString(s);
}

}  // namespace

EnergyBar::EnergyBar(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kBarHeight);
}

void EnergyBar::setSegments(std::vector<double> newSegments) {
    segments = std::move(newSegments);
    update();
}

QSize EnergyBar::sizeHint() const {
    return QSize(kBarWidth, kBarHeight);
}

void EnergyBar::paintEvent(QPaintEvent*) {
    if (segments.empty()) {
        return;
    }
    QPainter painter(this);
    const int count = static_cast<int>(segments.size());
    const double segmentWidth = (width() - kSegmentGap * (count - 1)) / static_cast<double>(count);
    for (int i = 0; i < count; ++i) {
        const QRectF box(i * (segmentWidth + kSegmentGap), 0, segmentWidth, height() - 1);
        painter.fillRect(box, kSegmentEmpty);
        const double ratio = segments[static_cast<std::size_t>(i)];
        if (ratio > 0.0) {
            painter.fillRect(QRectF(box.left(), box.top(), box.width() * ratio, box.height()), kSegmentFill);
        }
        painter.setPen(kSegmentBorder);
        painter.drawRect(box);
    }
}

HudView::HudView(MatchConfig config, QKeySequence skillKey, QWidget* parent)
    : QWidget(parent),
      config(config),
      skillKey(skillKey),
      titleLabel(new QLabel),
      energyBar(new EnergyBar),
      placeLabel(new QLabel),
      skillButton(new QPushButton),
      skillLabel(new QLabel),
      detailLabel(new QLabel),
      messageLabel(new QLabel) {
    auto* layout = new QVBoxLayout(this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->hide();
    layout->addWidget(titleLabel);
    layout->addWidget(new QLabel(QStringLiteral("能量")));
    layout->addWidget(energyBar);
    layout->addWidget(placeLabel);
    layout->addSpacing(12);
    layout->addWidget(skillButton);
    layout->addWidget(skillLabel);
    layout->addWidget(detailLabel);
    layout->addSpacing(12);
    layout->addWidget(messageLabel);
    layout->addStretch();
    setFixedWidth(kBarWidth + 24);

    skillButton->setFocusPolicy(Qt::NoFocus);
    messageLabel->setWordWrap(true);
    QPalette palette = messageLabel->palette();
    palette.setColor(QPalette::WindowText, kRejectText);
    messageLabel->setPalette(palette);

    connect(skillButton, &QPushButton::clicked, this, &HudView::skillTriggered);
    skillShortcut = new QShortcut(skillKey, this);
    connect(skillShortcut, &QShortcut::activated, this, &HudView::skillTriggered);

    messageTimer.setSingleShot(true);
    messageTimer.setInterval(kMessageDurationMs);
    connect(&messageTimer, &QTimer::timeout, messageLabel, &QLabel::clear);

    setView(view);
}

void HudView::setConfig(const MatchConfig& newConfig) {
    config = newConfig;
    setView(view);
}

void HudView::setTitle(const QString& title) {
    titleLabel->setText(title);
    titleLabel->setVisible(!title.isEmpty());
}

void HudView::setView(const PlayerView& newView) {
    view = newView;
    energyBar->setSegments(energySegments(view, config.maxEnergy));
    placeLabel->setText(isPlaceReady(view, config.placeCooldown) ? QStringLiteral("可以下子")
                                                                 : QStringLiteral("下子間隔中…"));
    const std::string detail = skillDetailText(view);
    detailLabel->setText(toQString(detail));
    detailLabel->setVisible(!detail.empty());
    refreshSkillText();
}

void HudView::setReadOnly(bool isReadOnly) {
    readOnly = isReadOnly;
    messageLabel->setVisible(!readOnly);
    refreshSkillText();
}

void HudView::setTargeting(bool isTargeting) {
    targeting = isTargeting;
    refreshSkillText();
}

void HudView::showRejection(RejectReason reason) {
    if (readOnly) {
        return;
    }
    messageLabel->setText(toQString(rejectReasonText(reason)));
    messageTimer.start();
}

void HudView::refreshSkillText() {
    const QString name = toQString(skillName(view.self.skill));
    skillButton->setText(readOnly ? name : QStringLiteral("%1（%2）").arg(name, skillKey.toString()));
    const bool usable = !readOnly && isSkillButtonEnabled(view);  // U2：摧毀用過後變灰，快捷鍵也停用
    skillButton->setEnabled(usable);
    skillShortcut->setEnabled(usable);
    if (targeting) {
        skillLabel->setText(toQString(targetingPrompt(view.self.skill, config.skill)));
    } else {
        skillLabel->setText(toQString(skillStatusText(view, config.skill)));  // U2、S3
    }
}
