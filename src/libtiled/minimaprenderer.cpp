/*
 * minimaprenderer.cpp
 * Copyright 2017, Yuriy Natarov <natarur@gmail.com>
 * Copyright 2012, Christoph Schnackenberg <bluechs@gmx.de>
 * Copyright 2012-2020, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "minimaprenderer.h"

#include "imagelayer.h"
#include "map.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include <QPainter>

using namespace Tiled;

MiniMapRenderer::MiniMapRenderer(const Map *map)
    : mMap(map)
    , mRenderer(MapRenderer::create(map))
{
    mRenderer->setFlag(ShowTileAnimations, false);
}

MiniMapRenderer::~MiniMapRenderer()
{
}

QSize MiniMapRenderer::mapSize() const
{
    QRect mapBoundingRect = mRenderer->mapBoundingRect();
    mMap->adjustBoundingRectForOffsetsAndImageLayers(mapBoundingRect);
    return mapBoundingRect.size();
}

QImage MiniMapRenderer::render(QSize size, RenderFlags renderFlags) const
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    renderToImage(image, renderFlags);
    return image;
}

static bool objectLessThan(const MapObject *a, const MapObject *b)
{
    return a->y() < b->y();
}

static QRectF cellRect(const MapRenderer &renderer,
                       const Cell &cell,
                       const QPointF &tileCoords)
{
    const Tile *tile = cell.tile();
    if (!tile)
        return QRectF();

    QPointF pixelCoords = renderer.tileToScreenCoords(tileCoords);
    QPointF offset = tile->offset();
    QSize size = tile->size();

    if (cell.flippedAntiDiagonally())
        std::swap(size.rwidth(), size.rheight());

    // This is a correction needed because tileToScreenCoords does not return
    // the bottom-left origin of the tile image, but rather the top-left
    // corner of the cell.
    pixelCoords.ry() += renderer.map()->tileHeight() - size.height();

    return QRectF(pixelCoords, size).translated(offset);
}

static void extendMapRect(QRect &mapBoundingRect, const MapRenderer &renderer)
{
    // Start with the basic map size
    QRectF rect(mapBoundingRect);

    // Take into account large tiles extending beyond their cell
    for (const Layer *layer : renderer.map()->tileLayers()) {
        const TileLayer *tileLayer = static_cast<const TileLayer*>(layer);
        const QPointF offset = tileLayer->totalOffset();

        for (int y = 0; y < tileLayer->height(); ++y) {
            for (int x = 0; x < tileLayer->width(); ++x) {
                const Cell &cell = tileLayer->cellAt(x, y);

                if (!cell.isEmpty()) {
                    QRectF r = cellRect(renderer, cell, QPointF(x, y));
                    r.translate(offset);
                    rect |= r;
                }
            }
        }
    }

    mapBoundingRect = rect.toAlignedRect();
}

void MiniMapRenderer::renderToImage(QImage &image, RenderFlags renderFlags,
                                    const QRectF &dirtyMapRect) const
{
    if (!mMap)
        return;
    if (image.isNull())
        return;

    const bool drawObjects = renderFlags.testFlag(RenderFlag::DrawMapObjects);
    const bool drawTileLayers = renderFlags.testFlag(RenderFlag::DrawTileLayers);
    const bool drawImageLayers = renderFlags.testFlag(RenderFlag::DrawImageLayers);
    const bool drawTileGrid = renderFlags.testFlag(RenderFlag::DrawGrid);
    const bool visibleLayersOnly = renderFlags.testFlag(RenderFlag::IgnoreInvisibleLayer);

    QRect mapBoundingRect = mRenderer->mapBoundingRect();

    if (renderFlags.testFlag(IncludeOverhangingTiles))
        extendMapRect(mapBoundingRect, *mRenderer);

    if (!renderFlags.testFlag(IgnoreOffsetsAndImages))
        mMap->adjustBoundingRectForOffsetsAndImageLayers(mapBoundingRect);

    QSize mapSize = mapBoundingRect.size();

    // Determine the largest possible scale
    const qreal scale = qMin(static_cast<qreal>(image.width()) / mapSize.width(),
                             static_cast<qreal>(image.height()) / mapSize.height());

    const QColor clearColor =
            (renderFlags.testFlag(DrawBackground) && mMap->backgroundColor().isValid())
            ? mMap->backgroundColor()
            : QColor(Qt::transparent);

    /*
     * Inflate the patch so it covers whole device pixels of the image.
     *
     * At mini-map scale a 32px tile lands on a fraction of a pixel, so a patch
     * the size of the edit clips to nothing and draws nothing. Two device
     * pixels of slack on each side also means the neighbouring tiles that bleed
     * into those pixels get redrawn, which is what stops a seam forming along
     * the edge of the patch under SmoothPixmapTransform.
     */
    QRectF patch;
    if (!dirtyMapRect.isEmpty() && scale > 0) {
        const qreal slack = 2 / scale;
        patch = dirtyMapRect.adjusted(-slack, -slack, slack, slack)
                .intersected(QRectF(mapBoundingRect));
    }

    const bool incremental = !patch.isEmpty();

    if (!incremental)
        image.fill(clearColor);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::SmoothPixmapTransform, renderFlags.testFlag(SmoothPixmapTransform));

    // Center the map in the requested size
    const QSize scaledMapSize = mapSize * scale;
    const QPointF centerOffset((image.width() - scaledMapSize.width()) / 2,
                               (image.height() - scaledMapSize.height()) / 2);

    painter.translate(centerOffset);
    painter.scale(scale, scale);
    painter.translate(-mapBoundingRect.topLeft());

    mRenderer->setPainterScale(scale);

    /*
     * The painter is now in map pixel coordinates, so the patch can be used as
     * a clip directly. Source composition rather than SourceOver, because the
     * old contents of the patch have to go - drawing over them would stack the
     * new state on top of the state it replaces.
     */
    if (incremental) {
        painter.setClipRect(patch);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(patch, clearColor);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    LayerIterator iterator(mMap);
    while (const Layer *layer = iterator.next()) {
        if (visibleLayersOnly && layer->isHidden())
            continue;

        const auto offset = layer->totalOffset();
        const auto compositionMode = layer->compositionMode();

        const auto layerOpacity = layer->effectiveOpacity();
        painter.setOpacity(layerOpacity);
        painter.translate(offset);

        /*
         * Everything below draws in layer-local coordinates, since the layer
         * offset has just been folded into the painter. The patch is in map
         * coordinates, so it has to come back the other way to be compared
         * against anything a layer reports about itself.
         */
        const QRectF layerPatch = incremental ? patch.translated(-offset) : QRectF();

        switch (layer->layerType()) {
        case Layer::TileLayerType: {
            if (drawTileLayers) {
                painter.setCompositionMode(compositionMode);

                const TileLayer *tileLayer = static_cast<const TileLayer*>(layer);
                mRenderer->drawTileLayer(&painter, tileLayer, layerPatch);
            }
            break;
        }

        case Layer::ObjectGroupType: {
            if (drawObjects) {
                const ObjectGroup *objectGroup = static_cast<const ObjectGroup*>(layer);
                QList<MapObject*> objects = objectGroup->objects();

                /*
                 * Discard what the patch does not touch BEFORE sorting.
                 *
                 * The clip would discard these anyway, but only after the
                 * painter state changes and the pixmap draw have been paid for
                 * - drawMapObject constructs a CellRenderer per object, so
                 * every one of them is its own drawPixmapFragments call.
                 *
                 * Filtering first also means the sort below runs on the few
                 * survivors instead of on all 10,000, and the list detaches at
                 * a fraction of the size. objects() hands back a const
                 * reference and QList is implicitly shared, so the copy above
                 * costs nothing until something here writes to it.
                 */
                if (incremental) {
                    objects.removeIf([&] (const MapObject *object) {
                        return !mRenderer->boundingRect(object).intersects(layerPatch);
                    });
                }

                if (objectGroup->drawOrder() == ObjectGroup::TopDownOrder)
                    std::stable_sort(objects.begin(), objects.end(), objectLessThan);

                for (const MapObject *object : std::as_const(objects)) {
                    if (object->isVisible()) {
                        if (object->isTileObject())
                            painter.setCompositionMode(compositionMode);
                        else
                            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

                        if (object->rotation() != qreal(0)) {
                            QPointF origin = mRenderer->pixelToScreenCoords(object->position());
                            painter.save();
                            painter.translate(origin);
                            painter.rotate(object->rotation());
                            painter.translate(-origin);
                        }

                        painter.setOpacity(layerOpacity * object->opacity());

                        mRenderer->drawMapObject(&painter, object, object->effectiveColors());

                        if (object->rotation() != qreal(0))
                            painter.restore();
                    }
                }
            }
            break;
        }
        case Layer::ImageLayerType: {
            if (drawImageLayers) {
                painter.setCompositionMode(compositionMode);

                const ImageLayer *imageLayer = static_cast<const ImageLayer*>(layer);
                mRenderer->drawImageLayer(&painter, imageLayer);
            }
            break;
        }

        case Layer::GroupLayerType:
            // Recursion handled by LayerIterator
            break;
        }

        painter.translate(-offset);
    }

    if (drawTileGrid)
        mRenderer->drawGrid(&painter, mapBoundingRect, mGridColor);

    if (drawObjects && mRenderObjectLabelCallback) {
        for (const Layer *layer : mMap->objectGroups()) {
            if (visibleLayersOnly && layer->isHidden())
                continue;

            const ObjectGroup *objectGroup = static_cast<const ObjectGroup*>(layer);

            for (const MapObject *object : objectGroup->objects())
                if (object->isVisible())
                    mRenderObjectLabelCallback(painter, object, *mRenderer);
        }
    }
}
