#include "ui/board_view.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>

#include "core/board.h"
#include "ui/board_geometry.h"

namespace {

const QColor kBoardColor(220, 179, 92);
const QColor kLineColor(40, 30, 20);
const QColor kBlackStone(20, 20, 20);
const QColor kWhiteStone(245, 245, 240);
const QColor kWhiteStoneOutline(120, 120, 120);
const QColor kWinHighlight(220, 30, 30);
const QColor kRejectFlash(230, 40, 40);

constexpr double kStoneRadius = 0.45;     // 以格距為單位
constexpr double kStarRadius = 0.1;
constexpr double kWinRingRadius = 0.5;
constexpr double kWinRingWidth = 0.08;
constexpr int kFlashDurationMs = 400;
constexpr int kAnimationIntervalMs = 16;
constexpr int kRejectFlashMaxAlpha = 170;
constexpr int kPreferredSize = 640;
constexpr int kMinimumSize = 320;

constexpr std::array<Pos, 5> kStarPoints{{{3, 3}, {11, 3}, {7, 7}, {3, 11}, {11, 11}}};

bool isWin(GameStatus status) {
    return status == GameStatus::BlackWon || status == GameStatus::WhiteWon;
}

QPointF toQt(BoardGeometry::Point p) {
    return QPointF(p.x, p.y);
}

}  // namespace

BoardView::BoardView(QWidget* parent) : QWidget(parent) {
    animation.setInterval(kAnimationIntervalMs);
    connect(&animation, &QTimer::timeout, this, [this] {
        flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
                                     [](const Flash& f) { return f.started.elapsed() >= kFlashDurationMs; }),
                      flashes.end());
        if (flashes.empty()) {
            animation.stop();
        }
        update();
    });
}

void BoardView::setView(const PlayerView& newView) {
    view = newView;
    update();
}

void BoardView::flashRejected(Pos pos) {
    Flash flash{pos, {}};
    flash.started.start();
    flashes.push_back(flash);
    animation.start();
    update();
}

QSize BoardView::sizeHint() const {
    return QSize(kPreferredSize, kPreferredSize);
}

QSize BoardView::minimumSizeHint() const {
    return QSize(kMinimumSize, kMinimumSize);
}

void BoardView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), kBoardColor);

    const BoardGeometry g(width(), height());
    const double cell = g.cellSize();

    // 格線
    painter.setPen(QPen(kLineColor, 1.0));
    for (int i = 0; i < Board::kSize; ++i) {
        painter.drawLine(toQt(g.cellCenter({i, 0})), toQt(g.cellCenter({i, Board::kSize - 1})));
        painter.drawLine(toQt(g.cellCenter({0, i})), toQt(g.cellCenter({Board::kSize - 1, i})));
    }
    painter.setBrush(kLineColor);
    for (Pos star : kStarPoints) {
        painter.drawEllipse(toQt(g.cellCenter(star)), cell * kStarRadius, cell * kStarRadius);
    }

    // 棋子
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Cell c = view.board.at({x, y});
            if (c == Cell::Empty) {
                continue;
            }
            painter.setPen(c == Cell::Black ? QPen(kBlackStone) : QPen(kWhiteStoneOutline, 1.0));
            painter.setBrush(c == Cell::Black ? kBlackStone : kWhiteStone);
            painter.drawEllipse(toQt(g.cellCenter({x, y})), cell * kStoneRadius, cell * kStoneRadius);
        }
    }

    // U6：勝利連線
    if (isWin(view.status)) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(kWinHighlight, cell * kWinRingWidth));
        for (Pos p : view.winningLine) {
            painter.drawEllipse(toQt(g.cellCenter(p)), cell * kWinRingRadius, cell * kWinRingRadius);
        }
    }

    // P6：被拒絕的格子閃紅色，逐漸淡出
    painter.setPen(Qt::NoPen);
    for (const Flash& f : flashes) {
        const double remaining = 1.0 - static_cast<double>(f.started.elapsed()) / kFlashDurationMs;
        QColor color = kRejectFlash;
        color.setAlpha(static_cast<int>(kRejectFlashMaxAlpha * std::clamp(remaining, 0.0, 1.0)));
        painter.setBrush(color);
        const QPointF center = toQt(g.cellCenter(f.pos));
        painter.drawRect(QRectF(center.x() - cell / 2, center.y() - cell / 2, cell, cell));
    }
}

void BoardView::mousePressEvent(QMouseEvent* event) {
    const BoardGeometry g(width(), height());
    if (const auto cell = g.pixelToCell(event->position().x(), event->position().y())) {
        emit cellClicked(*cell, event->button());
    }
}
