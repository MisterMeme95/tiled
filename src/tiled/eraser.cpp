/*
 * eraser.cpp
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "eraser.h"

#include "brushitem.h"
#include "geometry.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QtMath>

using namespace Tiled;

Eraser::Eraser(QObject *parent)
    : AbstractTileTool("EraserTool",
                       tr("Eraser"),
                       QIcon(QLatin1String(
                               ":images/22/stock-tool-eraser.png")),
                       QKeySequence(Qt::Key_E),
                       nullptr,
                       parent)
{
}

void Eraser::tilePositionChanged(QPoint tilePos)
{
    Q_UNUSED(tilePos)

    brushItem()->setTileRegion(eraseArea());

    if (mMode == Erase)
        doErase(true);
}

void Eraser::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (brushItem()->isVisible() && mMode == Nothing) {
        if (event->button() == Qt::LeftButton) {
            mMode = Erase;
            doErase(false);
            updateStatusInfo();
            return;
        }

        if (event->button() == Qt::RightButton && !(event->modifiers() & Qt::ControlModifier)) {
            mStart = tilePosition();
            mMode = RectangleErase;
            updateStatusInfo();
            return;
        }
    }

    AbstractTileTool::mousePressed(event);
}

void Eraser::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    switch (mMode) {
    case Nothing:
        break;
    case Erase:
        if (event->button() == Qt::LeftButton)
            mMode = Nothing;
        break;
    case RectangleErase:
        if (event->button() == Qt::RightButton) {
            doErase(false);
            mMode = Nothing;
            brushItem()->setTileRegion(eraseArea());
            updateStatusInfo();
        }
        break;
    }
}

void Eraser::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    mAllLayers = modifiers & Qt::ShiftModifier;
}

void Eraser::keyPressed(QKeyEvent *event)
{
    /*
     * [ and ] resize, the way every painting application spells it. Handled
     * here rather than as an application shortcut because the tool owns the
     * size, and MapScene::keyPressEvent() already forwards to whichever tool
     * is selected - so the keys only mean this while the eraser is active.
     */
    int size = mSize;

    switch (event->key()) {
    case Qt::Key_BracketLeft:   size = qMax(1, mSize - 1); break;
    case Qt::Key_BracketRight:  size = qMin(64, mSize + 1); break;
    default:
        AbstractTileTool::keyPressed(event);
        return;
    }

    if (size != mSize) {
        mSize = size;

        /* The preview reads eraseArea(), so it only needs telling to re-read. */
        brushItem()->setTileRegion(eraseArea());
        updateStatusInfo();
    }

    event->accept();
}

void Eraser::languageChanged()
{
    setName(tr("Eraser"));
}

void Eraser::doErase(bool continuation)
{
    QRegion globalEraseRegion(eraseArea());
    QPoint tilePos = tilePosition();

    if (continuation) {
        /*
         * The full square at every interpolated point, not a single tile.
         * These fill the gaps the mouse skipped over, so they have to be the
         * same shape as the one being dragged - otherwise a fast drag leaves a
         * one-tile trail with a full-size square only where it stopped.
         */
        const QVector<QPoint> points = pointsOnLine(mLastTilePos, tilePos);
        for (const QPoint &p : points)
            globalEraseRegion |= QRegion(eraseSquare(p));
    }
    mLastTilePos = tilePos;

    mapDocument()->eraseTileLayers(globalEraseRegion, mAllLayers, continuation);
}

QRect Eraser::eraseArea() const
{
    if (mMode == RectangleErase) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QRect rect = QRect(mStart, tilePosition()).normalized();
        if (rect.width() == 0)
            rect.adjust(-1, 0, 1, 0);
        if (rect.height() == 0)
            rect.adjust(0, -1, 0, 1);
        return rect;
#else
        return QRect::span(mStart, tilePosition());
#endif
    }

    return eraseSquare(tilePosition());
}

QRect Eraser::eraseSquare(QPoint tilePos) const
{
    /*
     * Centred on the tile, so growing the eraser does not make it drift off
     * the one being pointed at. An even size cannot be centred exactly; the
     * extra row and column go right and down, which is the direction the
     * bottom-left anchoring of everything else already leans.
     */
    const int size = qMax(1, mSize);
    const int half = (size - 1) / 2;

    return QRect(tilePos - QPoint(half, half), QSize(size, size));
}

void Eraser::updateStatusInfo()
{
    if (!isBrushVisible()) {
        AbstractTileTool::updateStatusInfo();
        return;
    }

    if (mMode == RectangleErase) {
        const QPoint pos = tilePosition();
        const QRect rect = eraseArea();
        setStatusInfo(tr("%1, %2 - Erase area (%4 x %5)")
                          .arg(pos.x())
                          .arg(pos.y())
                          .arg(rect.width())
                          .arg(rect.height()));
        return;
    }

    /*
     * The size is otherwise invisible until something is erased, and the base
     * shows only the cursor position. Reported only when it is not 1, so the
     * ordinary case reads exactly as it always did.
     */
    if (mSize > 1) {
        const QPoint pos = tilePosition();

        setStatusInfo(tr("%1, %2 - Eraser size %3 ([ and ] to change)")
                          .arg(pos.x())
                          .arg(pos.y())
                          .arg(mSize));
        return;
    }

    AbstractTileTool::updateStatusInfo();
}

#include "moc_eraser.cpp"
