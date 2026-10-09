#include "ui/board_view.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>

#include "core/board.h"
#include "ui/board_geometry.h"
#include "ui/board_overlay.h"

namespace {

const QColor kBoardColor(220, 179, 92);
const QColor kLineColor(40, 30, 20);
const QColor kBlackStone(20, 20, 20);
const QColor kWhiteStone(245, 245, 240);
const QColor kWhiteStoneOutline(120, 120, 120);
const QColor kClearHighlight(220, 30, 30);
const QColor kRejectFlash(230, 40, 40);
const QColor kDestroyedFill(70, 60, 55);
const QColor kDestroyedMark(120, 105, 95);
const QColor kBlackZone(40, 90, 200);      // 黑方的禁區（限制白方）
const QColor kWhiteZone(200, 60, 160);     // 白方的禁區（限制黑方）
const QColor kDestroyPreview(200, 30, 30);

constexpr double kStoneRadius = 0.45;     // 以格距為單位
constexpr double kStarRadius = 0.1;
constexpr double kClearRingRadius = 0.5;
constexpr double kClearRingWidth = 0.08;
constexpr int kZoneMaxAlpha = 110;
constexpr int kPreviewAlpha = 70;
constexpr double kScoreFontScale = 0.6;   // 以格距為單位
constexpr double kZoneFontScale = 0.4;
constexpr int kFlashDurationMs = 400;
constexpr int kAnimationIntervalMs = 16;
constexpr int kRejectFlashMaxAlpha = 170;
constexpr int kPreferredSize = 640;
constexpr int kMinimumSize = 320;

constexpr std::array<Pos, 5> kStarPoints{{{3, 3}, {11, 3}, {7, 7}, {3, 11}, {11, 11}}};

QPointF toQt(BoardGeometry::Point p) {
    return QPointF(p.x, p.y);
}

}  // namespace

BoardView::BoardView(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);  // U3：摧毀預覽跟著滑鼠
    animation.setInterval(kAnimationIntervalMs);
    connect(&animation, &QTimer::timeout, this, [this] {
        flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
                                     [](const Flash& f) { return f.started.elapsed() >= kFlashDurationMs; }),
                      flashes.end());
        if (!clearing.empty() && clearingStarted.elapsed() >= kClearFlashDurationMs) {
            clearing.clear();
        }
        if (flashes.empty() && clearing.empty()) {
            animation.stop();
        }
        update();
    });
}

void BoardView::startAnimation() {
    if (!animation.isActive()) {
        animation.start();
    }
}

void BoardView::setView(const PlayerView& newView) {
    view = newView;
    update();
}

void BoardView::flashRejected(Pos pos) {
    Flash flash{pos, {}};
    flash.started.start();
    flashes.push_back(flash);
    startAnimation();
    update();
}

void BoardView::setTargeting(bool targeting, std::optional<int> radius) {
    setCursor(targeting ? Qt::CrossCursor : Qt::ArrowCursor);
    previewRadius = targeting ? radius : std::nullopt;
    update();
}

void BoardView::setZoneDuration(TimeMs duration) {
    zoneDuration = duration;
}

void BoardView::flashCleared(const std::vector<ClearedLine>& lines) {
    clearing = lines;
    clearingStarted.start();
    startAnimation();
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

    // B4、U9：已摧毀的格子畫成深色方塊加叉
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            if (view.board.at({x, y}) != Cell::Destroyed) {
                continue;
            }
            const QPointF c = toQt(g.cellCenter({x, y}));
            const QRectF box(c.x() - cell / 2, c.y() - cell / 2, cell, cell);
            painter.fillRect(box, kDestroyedFill);
            painter.setPen(QPen(kDestroyedMark, 1.5));
            painter.drawLine(box.topLeft(), box.bottomRight());
            painter.drawLine(box.topRight(), box.bottomLeft());
        }
    }

    paintZones(painter, cell);

    // 棋子
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Cell c = view.board.at({x, y});
            if (c != Cell::Black && c != Cell::White) {
                continue;
            }
            painter.setPen(c == Cell::Black ? QPen(kBlackStone) : QPen(kWhiteStoneOutline, 1.0));
            painter.setBrush(c == Cell::Black ? kBlackStone : kWhiteStone);
            painter.drawEllipse(toQt(g.cellCenter({x, y})), cell * kStoneRadius, cell * kStoneRadius);
        }
    }

    paintClearFlash(painter, cell);
    paintDestroyPreview(painter, cell);

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

void BoardView::mouseMoveEvent(QMouseEvent* event) {
    const BoardGeometry g(width(), height());
    const auto cell = g.pixelToCell(event->position().x(), event->position().y());
    if (cell != hover) {
        hover = cell;
        if (previewRadius) {
            update();
        }
    }
}

void BoardView::leaveEvent(QEvent*) {
    hover.reset();
    update();
}

// U9：禁區依所屬方上色，隨剩餘時間漸淡，並標出剩餘秒數
void BoardView::paintZones(QPainter& painter, double cell) {
    const BoardGeometry g(width(), height());
    QFont font = painter.font();
    font.setPixelSize(std::max(1, static_cast<int>(cell * kZoneFontScale)));
    painter.setFont(font);
    for (const ZoneCell& zone : view.zones) {
        QColor color = zone.owner == PlayerId::Black ? kBlackZone : kWhiteZone;
        color.setAlpha(static_cast<int>(kZoneMaxAlpha * zoneOpacity(zone, view.now, zoneDuration)));
        const QPointF c = toQt(g.cellCenter(zone.pos));
        const QRectF box(c.x() - cell / 2, c.y() - cell / 2, cell, cell);
        painter.fillRect(box, color);
        if (view.board.isEmpty(zone.pos)) {
            painter.setPen(zone.owner == PlayerId::Black ? kBlackZone : kWhiteZone);
            painter.drawText(box, Qt::AlignCenter, QString::number(zoneRemainingSeconds(zone, view.now)));
        }
    }
}

// U11：被消除的連線以原本的棋子畫出並加紅圈，漸淡消失，同時顯示「+N」
void BoardView::paintClearFlash(QPainter& painter, double cell) {
    if (clearing.empty()) {
        return;
    }
    const BoardGeometry g(width(), height());
    const double remaining =
        std::clamp(1.0 - static_cast<double>(clearingStarted.elapsed()) / kClearFlashDurationMs, 0.0, 1.0);
    const int alpha = static_cast<int>(255 * remaining);
    for (const ClearedLine& line : clearing) {
        QColor stone = line.owner == PlayerId::Black ? kBlackStone : kWhiteStone;
        stone.setAlpha(alpha);
        QColor ring = kClearHighlight;
        ring.setAlpha(alpha);
        for (Pos p : line.stones) {
            const QPointF c = toQt(g.cellCenter(p));
            painter.setPen(Qt::NoPen);
            painter.setBrush(stone);
            painter.drawEllipse(c, cell * kStoneRadius, cell * kStoneRadius);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(ring, cell * kClearRingWidth));
            painter.drawEllipse(c, cell * kClearRingRadius, cell * kClearRingRadius);
        }
    }
    if (const auto label = clearedLabelCell(clearing)) {
        QFont font = painter.font();
        font.setPixelSize(std::max(1, static_cast<int>(cell * kScoreFontScale)));
        font.setBold(true);
        painter.setFont(font);
        QColor text = kClearHighlight;
        text.setAlpha(alpha);
        painter.setPen(text);
        const QPointF c = toQt(g.cellCenter(*label));
        painter.drawText(QRectF(c.x() - cell * 2, c.y() - cell * 1.5, cell * 4, cell), Qt::AlignCenter,
                         QString::fromStdString(clearedScoreText(clearing)));
    }
}

// U3：摧毀選目標時，預覽滑鼠所在位置的範圍
void BoardView::paintDestroyPreview(QPainter& painter, double cell) {
    if (!previewRadius || !hover) {
        return;
    }
    const BoardGeometry g(width(), height());
    QColor color = kDestroyPreview;
    color.setAlpha(kPreviewAlpha);
    for (Pos p : destroyPreviewCells(*hover, *previewRadius)) {
        const QPointF c = toQt(g.cellCenter(p));
        painter.fillRect(QRectF(c.x() - cell / 2, c.y() - cell / 2, cell, cell), color);
    }
}
