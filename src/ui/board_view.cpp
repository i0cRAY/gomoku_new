#include "ui/board_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QUrl>

#include <algorithm>
#include <array>

#include "core/board.h"
#include "ui/board_geometry.h"
#include "ui/board_overlay.h"

// U13、U14：爆炸、大笑與靜音的資源檔編在靜態函式庫裡，要手動初始化，否則連結時會被丟掉。
// Q_INIT_RESOURCE 不能放在具名或匿名 namespace 裡
static void initEffectResources() {
    Q_INIT_RESOURCE(explosion);
    Q_INIT_RESOURCE(laugh);
    Q_INIT_RESOURCE(silence);
}

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
const QColor kSkillPreview(200, 30, 30);

constexpr double kStoneRadius = 0.45;     // 以格距為單位
constexpr double kStarRadius = 0.1;
constexpr double kClearRingRadius = 0.5;
constexpr double kClearRingWidth = 0.08;
constexpr int kZoneMaxAlpha = 110;
constexpr int kPreviewAlpha = 70;
constexpr double kScoreFontScale = 0.6;   // 以格距為單位
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
    initEffectResources();
    explosionFrames.reserve(kExplosionFrameCount);
    for (int i = 0; i < kExplosionFrameCount; ++i) {
        explosionFrames.emplace_back(QStringLiteral(":/explosion/explosion_%1.png").arg(i, 2, 10, QLatin1Char('0')));
    }
    explosionSound.setSource(QUrl(QStringLiteral("qrc:/explosion.wav")));
    laughFrames.reserve(kLaughFrameCount);
    for (int i = 0; i < kLaughFrameCount; ++i) {
        laughFrames.emplace_back(QStringLiteral(":/laugh/laugh_%1.png").arg(i, 2, 10, QLatin1Char('0')));
    }
    laughSound.setSource(QUrl(QStringLiteral("qrc:/laugh.wav")));
    laughSound.setLoopCount(QSoundEffect::Infinite);
    keepAliveSound.setSource(QUrl(QStringLiteral("qrc:/silence.wav")));
    keepAliveSound.setLoopCount(QSoundEffect::Infinite);
    animation.setInterval(kAnimationIntervalMs);
    connect(&animation, &QTimer::timeout, this, [this] {
        flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
                                     [](const Flash& f) { return f.started.elapsed() >= kFlashDurationMs; }),
                      flashes.end());
        if (!clearing.empty() && clearingStarted.elapsed() >= kClearFlashDurationMs) {
            clearing.clear();
        }
        if (explosionStarted && !explosionFrame(explosionStarted->elapsed())) {
            explosionStarted.reset();
        }
        if (flashes.empty() && clearing.empty() && !explosionStarted && !laughStarted) {
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
    if (audioKeepAlive(newView) && !keepAliveSound.isPlaying()) {
        keepAliveSound.play();
    } else if (!audioKeepAlive(newView)) {
        keepAliveSound.stop();
    }
    // U13：新出現的已摧毀格子代表剛有人用了摧毀
    if (!newlyDestroyedCells(view.board, newView.board).empty()) {
        explosionStarted.emplace();
        explosionStarted->start();
        explosionSound.play();
        startAnimation();
    }
    // U14：禁區出現時開始大笑，全部到期（或對局結束）時停止
    if (laughActive(newView) && !laughStarted) {
        laughStarted.emplace();
        laughStarted->start();
        laughSound.play();
        startAnimation();
    } else if (!laughActive(newView) && laughStarted) {
        laughStarted.reset();
        laughSound.stop();
    }
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

void BoardView::setTargeting(bool targeting, std::optional<SkillArea> area) {
    setCursor(targeting ? Qt::CrossCursor : Qt::ArrowCursor);
    previewArea = targeting ? area : std::nullopt;
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
    paintExplosion(painter);
    paintSkillPreview(painter, cell);

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
        if (previewArea) {
            update();
        }
    }
}

void BoardView::leaveEvent(QEvent*) {
    hover.reset();
    update();
}

// U9：禁區依所屬方上色，隨剩餘時間漸淡；U14：再疊上大笑動畫（之後畫的棋子會蓋住有子的格子）
void BoardView::paintZones(QPainter& painter, double cell) {
    const BoardGeometry g(width(), height());
    const QPixmap* laugh = laughStarted ? &laughFrames[laughFrame(laughStarted->elapsed())] : nullptr;
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const ZoneCell& zone : view.zones) {
        QColor color = zone.owner == PlayerId::Black ? kBlackZone : kWhiteZone;
        color.setAlpha(static_cast<int>(kZoneMaxAlpha * zoneOpacity(zone, view.now, zoneDuration)));
        const QPointF c = toQt(g.cellCenter(zone.pos));
        const QRectF box(c.x() - cell / 2, c.y() - cell / 2, cell, cell);
        painter.fillRect(box, color);
        if (laugh) {
            painter.drawPixmap(box, *laugh, QRectF(laugh->rect()));
        }
    }
    painter.restore();
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

// U3：炸彈、摧毀選目標時，預覽滑鼠所在位置的範圍
void BoardView::paintSkillPreview(QPainter& painter, double cell) {
    if (!previewArea || !hover) {
        return;
    }
    const BoardGeometry g(width(), height());
    QColor color = kSkillPreview;
    color.setAlpha(kPreviewAlpha);
    for (Pos p : areaCells(*hover, *previewArea)) {
        const QPointF c = toQt(g.cellCenter(p));
        painter.fillRect(QRectF(c.x() - cell / 2, c.y() - cell / 2, cell, cell), color);
    }
}

// U13：摧毀的爆炸影格蓋滿整個棋盤
void BoardView::paintExplosion(QPainter& painter) {
    if (!explosionStarted) {
        return;
    }
    const auto frame = explosionFrame(explosionStarted->elapsed());
    if (!frame) {
        return;
    }
    const BoardGeometry::Square area = BoardGeometry(width(), height()).boardArea();
    const QPixmap& pixmap = explosionFrames[*frame];
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRectF(area.x, area.y, area.side, area.side), pixmap, QRectF(pixmap.rect()));
    painter.restore();
}
