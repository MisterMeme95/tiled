/*
 * minimap.cpp
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

#include "minimap.h"

#include "changeevents.h"
#include "documentmanager.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "mapview.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "utils.h"
#include "zoomable.h"

#include <QCursor>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUndoStack>

using namespace Tiled;

MiniMap::MiniMap(QWidget *parent)
    : QFrame(parent)
    , mMapDocument(nullptr)
    , mDragging(false)
    , mMouseMoveCursorState(false)
    , mRedrawMapImage(false)
    , mRenderFlags(MiniMapRenderer::DrawTileLayers
                   | MiniMapRenderer::DrawMapObjects
                   | MiniMapRenderer::DrawImageLayers
                   | MiniMapRenderer::IgnoreInvisibleLayer
                   | MiniMapRenderer::SmoothPixmapTransform)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setMinimumSize(50, 50);

    // for cursor changes
    setMouseTracking(true);

    mMapImageUpdateTimer.setSingleShot(true);
    connect(&mMapImageUpdateTimer, &QTimer::timeout,
            this, &MiniMap::redrawTimeout);
}

void MiniMap::setMapDocument(MapDocument *map)
{
    const DocumentManager *dm = DocumentManager::instance();

    if (mMapDocument) {
        mMapDocument->disconnect(this);

        /*
         * The undo stack is a separate QObject, so the disconnect above does
         * not reach it. Left connected, the previous document's stack goes on
         * scheduling updates for a map this widget no longer shows - and since
         * those arrive with no dirty rectangle they each cost a full render.
         */
        mMapDocument->undoStack()->disconnect(this);

        if (MapView *mapView = dm->viewForDocument(mMapDocument))
            mapView->disconnect(this);
    }

    mMapDocument = map;

    /* A different map shares nothing with the one that was on screen. */
    mObjectBounds.clear();
    mDirtyMapRect = QRectF();
    mFullRedraw = true;

    if (mMapDocument) {
        connect(mMapDocument->undoStack(), &QUndoStack::indexChanged,
                this, &MiniMap::scheduleMapImageUpdate);

        /*
         * Tile edits. regionChanged is what MapItem repaints from too, and it
         * carries exactly the cells that changed.
         */
        connect(mMapDocument, &MapDocument::regionChanged, this,
                [this] (const QRegion &region, TileLayer *tileLayer) {
            if (region.isEmpty() || !tileLayer || !mMapDocument)
                return;

            QRectF rect = mMapDocument->renderer()->boundingRect(region.boundingRect());

            /*
             * A tile taller than its cell paints above it, so the pixels that
             * changed reach further than the cells that changed. drawMargins is
             * how far any tile in this layer can hang out of its cell; growing
             * the rectangle by it on every side is generous in two directions
             * and exactly right in the other two.
             */
            const QMargins margins = tileLayer->drawMargins();
            rect.adjust(-margins.left(), -margins.top(),
                        margins.right(), margins.bottom());

            mDirtyMapRect |= rect.translated(tileLayer->totalOffset());
        });

        /* Objects, and the fallback for everything else. */
        connect(mMapDocument, &Document::changed, this,
                [this] (const ChangeEvent &change) {
            if (!mMapDocument)
                return;

            const MapRenderer *renderer = mMapDocument->renderer();

            switch (change.type) {
            case ChangeEvent::MapObjectsAdded:
            case ChangeEvent::MapObjectsChanged:
            case ChangeEvent::MapObjectsAboutToBeRemoved: {
                /*
                 * AboutToBeRemoved rather than Removed, because the objects are
                 * still in their group here and can still say where they are.
                 */
                const bool leaving =
                        change.type == ChangeEvent::MapObjectsAboutToBeRemoved;
                auto &e = static_cast<const MapObjectsEvent&>(change);

                for (const MapObject *object : e.mapObjects) {
                    const ObjectGroup *group = object->objectGroup();
                    const QPointF offset = group ? group->totalOffset() : QPointF();
                    const QRectF now =
                            renderer->boundingRect(object).translated(offset);

                    /* Where it was, so a move does not leave the old copy. */
                    mDirtyMapRect |= mObjectBounds.value(object);
                    mDirtyMapRect |= now;

                    if (leaving)
                        mObjectBounds.remove(object);
                    else
                        mObjectBounds.insert(object, now);
                }
                break;
            }

            /*
             * The singular forms say the same thing as the plural ones that
             * bracket them, so acting on both would only dirty twice.
             */
            case ChangeEvent::MapObjectAboutToBeAdded:
            case ChangeEvent::MapObjectAdded:
            case ChangeEvent::MapObjectAboutToBeRemoved:
            case ChangeEvent::MapObjectRemoved:
            case ChangeEvent::MapObjectsRemoved:
                break;

            /*
             * Layer visibility, opacity, offsets, the map itself, a tileset
             * reloading, a class colour changing - none of these have a
             * rectangle worth computing, and all of them are rare enough that
             * a full render is the honest answer.
             */
            default:
                mFullRedraw = true;
                break;
            }
        });

        if (MapView *mapView = dm->viewForDocument(mMapDocument))
            connect(mapView, &MapView::viewRectChanged, this, [this] { update(); });
    }

    scheduleMapImageUpdate();
}

QSize MiniMap::sizeHint() const
{
    return Utils::dpiScaled(QSize(200, 200));
}

void MiniMap::scheduleMapImageUpdate()
{
    mMapImageUpdateTimer.start(100);
}

void MiniMap::paintEvent(QPaintEvent *pe)
{
    QFrame::paintEvent(pe);

    /*
     * Queued, not rendered here: drawing every layer and object into the
     * thumbnail measured 445ms on a 700x650 map, and a paint that long
     * freezes the editor. The paint below uses whatever image exists, so a
     * first open shows the minimap blank for a moment. 150ms not 0, to land
     * after the open settles.
     */
    if (mRedrawMapImage && !mRenderQueued) {
        mRenderQueued = true;

        QTimer::singleShot(150, this, [this]() {
            mRenderQueued = false;

            if (!mRedrawMapImage)
                return;

            mRedrawMapImage = false;
            renderMapToImage();
            update();
        });
    }

    if (mMapImage.isNull() || mImageRect.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHints(QPainter::SmoothPixmapTransform);

    /*
     * Rounded to match the dock that frames it. Clipped rather than merely
     * drawn rounded, because the map image below is a straight blit - without
     * a clip its square corners sit outside the curve and the rounding reads
     * as a border drawn over a rectangle.
     *
     * Only the bottom corners: the dock's title bar covers the top two, and
     * rounding those as well leaves two notches between the title and the map.
     */
    QPainterPath rounded;
    const QRectF face = contentsRect();
    constexpr qreal radius = 7.0;

    rounded.moveTo(face.topLeft());
    rounded.lineTo(face.topRight());
    rounded.lineTo(face.right(), face.bottom() - radius);
    rounded.quadTo(face.bottomRight(),
                   QPointF(face.right() - radius, face.bottom()));
    rounded.lineTo(face.left() + radius, face.bottom());
    rounded.quadTo(face.bottomLeft(),
                   QPointF(face.left(), face.bottom() - radius));
    rounded.closeSubpath();

    p.setClipPath(rounded);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor backgroundColor(palette().dark().color());
    if (mMapDocument && mMapDocument->map()->backgroundColor().isValid())
        backgroundColor = mMapDocument->map()->backgroundColor();
    p.setBrush(backgroundColor);
    p.setPen(Qt::NoPen);
    p.drawRect(contentsRect());

    p.drawImage(mImageRect, mMapImage);

    const QRect viewRect = viewportRect();
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(0, 0, 0, 128));
    p.translate(1, 1);
    p.drawRect(viewRect);

    QPen outLinePen(QColor(255, 0, 0), 2);
    outLinePen.setJoinStyle(Qt::MiterJoin);
    p.translate(-1, -1);
    p.setPen(outLinePen);
    p.drawRect(viewRect);
}

void MiniMap::resizeEvent(QResizeEvent *)
{
    updateImageRect();
    scheduleMapImageUpdate();
}

void MiniMap::updateImageRect()
{
    QRect imageRect = mMapImage.rect();
    if (imageRect.isEmpty()) {
        mImageRect = QRect();
        return;
    }

    // Scale and center the image
    const QRect r = contentsRect();
    qreal scale = qMin(static_cast<qreal>(r.width()) / imageRect.width(),
                       static_cast<qreal>(r.height()) / imageRect.height());
    imageRect.setSize(imageRect.size() * scale);
    imageRect.moveCenter(r.center());

    mImageRect = imageRect;
}

void MiniMap::renderMapToImage()
{
    if (!mMapDocument) {
        mMapImage = QImage();
        return;
    }

    MiniMapRenderer miniMapRenderer(mMapDocument->map());

    const QSize mapSize = miniMapRenderer.mapSize();
    if (mapSize.isEmpty()) {
        mMapImage = QImage();
        return;
    }

    // Determine the largest possible scale
    const QSize viewSize = contentsRect().size() * devicePixelRatioF();
    qreal scale = qMin(static_cast<qreal>(viewSize.width()) / mapSize.width(),
                       static_cast<qreal>(viewSize.height()) / mapSize.height());

    QSize imageSize = mapSize * scale;

    // Improve the quality if the image size is small
    if (imageSize.width() < 512 && imageSize.height() < 512)
        imageSize *= 2;

    // Allocate a new image when the size changed
    if (mMapImage.size() != imageSize) {
        mMapImage = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
        updateImageRect();

        /* A fresh buffer holds nothing, so there is nothing to patch. */
        mFullRedraw = true;
    }

    if (imageSize.isEmpty())
        return;

    /*
     * An empty dirty rectangle asks for a full render, which is what makes
     * this safe by default: anything that failed to report a rectangle gets
     * redrawn rather than silently skipped.
     */
    miniMapRenderer.renderToImage(mMapImage, mRenderFlags,
                                  mFullRedraw ? QRectF() : mDirtyMapRect);

    if (mFullRedraw) {
        /*
         * The image now matches the map exactly, so this is the one moment
         * where every object's position can be recorded cheaply enough to be
         * worth it - and having it recorded is what lets the FIRST move of any
         * object repaint the square it left behind.
         */
        mObjectBounds.clear();

        const MapRenderer *renderer = mMapDocument->renderer();
        for (const Layer *layer : mMapDocument->map()->objectGroups()) {
            auto objectGroup = static_cast<const ObjectGroup*>(layer);
            const QPointF offset = objectGroup->totalOffset();

            for (const MapObject *object : objectGroup->objects())
                mObjectBounds.insert(object,
                                     renderer->boundingRect(object).translated(offset));
        }
    }

    mFullRedraw = false;
    mDirtyMapRect = QRectF();
}

void MiniMap::centerViewOnLocalPixel(const QPointF &centerPos, int delta)
{
    MapView *mapView = DocumentManager::instance()->currentMapView();
    if (!mapView)
        return;

    if (delta != 0)
        mapView->zoomable()->handleWheelDelta(delta);

    mapView->forceCenterOn(mapToScene(centerPos));
}

void MiniMap::redrawTimeout()
{
    mRedrawMapImage = true;
    update();
}

void MiniMap::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y()) {
        centerViewOnLocalPixel(event->position(), event->angleDelta().y());
        return;
    }

    QFrame::wheelEvent(event);
}

void MiniMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint cursorPos = event->pos();
        QRect viewPort = viewportRect();

        if (viewPort.contains(cursorPos)) {
            mDragOffset = viewPort.center() - cursorPos + QPoint(1, 1);
            cursorPos += mDragOffset;
        } else {
            mDragOffset = QPoint();
            centerViewOnLocalPixel(cursorPos);
        }

        mDragging = true;
        setCursor(Qt::ClosedHandCursor);

        return;
    }

    QFrame::mouseReleaseEvent(event);
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mDragging) {
        mDragging = false;

        QRect viewPort = viewportRect();
        if (viewPort.contains(event->pos())) {
            setCursor(Qt::OpenHandCursor);
            mMouseMoveCursorState = true;
        } else if (rect().contains(event->pos())) {
            unsetCursor();
            mMouseMoveCursorState = false;
        }

        return;
    }

    QFrame::mouseReleaseEvent(event);
}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (mDragging) {
        centerViewOnLocalPixel(event->pos() + mDragOffset);
        return;
    }

    if (viewportRect().contains(event->pos())) {
        if (!mMouseMoveCursorState) {
            setCursor(Qt::OpenHandCursor);
            mMouseMoveCursorState = true;
        }
    } else {
        if (mMouseMoveCursorState) {
            unsetCursor();
            mMouseMoveCursorState = false;
        }
    }

    QFrame::mouseMoveEvent(event);
}

QRect MiniMap::viewportRect() const
{
    MapView *mapView = DocumentManager::instance()->currentMapView();
    if (!mapView)
        return QRect(0, 0, 1, 1);

    const QRectF mapRect = mapView->mapScene()->mapBoundingRect();
    const QRectF viewRect = mapView->viewRect();
    const QRectF ratioRect = QRectF((viewRect.x() - mapRect.x()) / mapRect.width(),
                                    (viewRect.y() - mapRect.y()) / mapRect.height(),
                                    viewRect.width() / mapRect.width(),
                                    viewRect.height() / mapRect.height());

    return QRect(ratioRect.x() * mImageRect.width() + mImageRect.x(),
                 ratioRect.y() * mImageRect.height() + mImageRect.y(),
                 ratioRect.width() * mImageRect.width(),
                 ratioRect.height() * mImageRect.height());
}

QPointF MiniMap::mapToScene(QPointF p) const
{
    if (mImageRect.isEmpty())
        return QPointF();

    MapView *mapView = DocumentManager::instance()->currentMapView();
    if (!mapView)
        return QPointF();

    const QRectF mapRect = mapView->mapScene()->mapBoundingRect();
    p -= mImageRect.topLeft();
    return QPointF(p.x() * (mapRect.width() / mImageRect.width()) + mapRect.x(),
                   p.y() * (mapRect.height() / mImageRect.height()) + mapRect.y());
}

#include "moc_minimap.cpp"
