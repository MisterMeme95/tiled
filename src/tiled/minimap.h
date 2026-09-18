/*
 * minimap.h
 * Copyright 2012, Christoph Schnackenberg <bluechs@gmx.de>
 * Copyright 2012, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "minimaprenderer.h"

#include <QFrame>
#include <QHash>
#include <QImage>
#include <QTimer>

namespace Tiled {

class MapDocument;
class MapObject;

class MiniMap : public QFrame
{
    Q_OBJECT

public:
    MiniMap(QWidget *parent);

    void setMapDocument(MapDocument *);

    MiniMapRenderer::RenderFlags renderFlags() const { return mRenderFlags; }
    void setRenderFlags(MiniMapRenderer::RenderFlags flags) { mRenderFlags = flags; }

    QSize sizeHint() const override;

public slots:
    /** Schedules a redraw of the minimap image. */
    void scheduleMapImageUpdate();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void redrawTimeout();

    MapDocument *mMapDocument;
    QImage mMapImage;
    QRect mImageRect;
    QTimer mMapImageUpdateTimer;
    bool mDragging;
    QPoint mDragOffset;
    bool mMouseMoveCursorState;
    bool mRedrawMapImage;
    MiniMapRenderer::RenderFlags mRenderFlags;

    /*
     * What changed since the image was last drawn, in map pixel coordinates,
     * and whether the whole thing has to be redrawn instead.
     *
     * A full render walks every cell of every tile layer and draws every
     * object, which on a large map is a quarter of a second - and it used to
     * happen on every single undo command, which is what made editing feel
     * laggy. Accumulating what actually changed lets renderMapToImage() patch
     * the image it already has.
     *
     * mFullRedraw is the safe answer and everything unrecognised falls back to
     * it. An empty dirty rectangle means a full render as well, not a skipped
     * one, which is what makes an edit emitting no change event we model
     * correct rather than invisible.
     */
    QRectF mDirtyMapRect;
    bool mFullRedraw = true;

    /*
     * Where each object was when the image was last drawn.
     *
     * MapObjectsChanged arrives AFTER the move, so the event alone says where
     * an object is now and not where it was - and repainting only the new
     * position leaves the old one smeared across the image. This is the other
     * half of that rectangle. Refilled on every full render, which is what
     * makes the first move of any object correct.
     */
    QHash<const MapObject *, QRectF> mObjectBounds;

    QRect viewportRect() const;
    QPointF mapToScene(QPointF p) const;
    void updateImageRect();
    void renderMapToImage();
    void centerViewOnLocalPixel(const QPointF &centerPos, int delta = 0);
};

} // namespace Tiled
