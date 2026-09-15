/*
 * eraser.h
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

#pragma once

#include "abstracttiletool.h"

namespace Tiled {

/**
 * Implements a simple eraser tool.
 */
class Eraser : public AbstractTileTool
{
    Q_OBJECT

public:
    Eraser(QObject *parent = nullptr);

    void mousePressed(QGraphicsSceneMouseEvent *event) override;
    void mouseReleased(QGraphicsSceneMouseEvent *event) override;

    void modifiersChanged(Qt::KeyboardModifiers) override;

    void languageChanged() override;

protected:
    void keyPressed(QKeyEvent *event) override;
    void tilePositionChanged(QPoint tilePos) override;
    void updateStatusInfo() override;

private:
    void doErase(bool continuation);
    QRect eraseArea() const;

    /*
     * The square the eraser covers at a given tile. Shared by eraseArea(),
     * which drives the preview, and by the line interpolation in doErase() -
     * they have to agree or a fast drag erases a different shape from the one
     * the cursor showed.
     */
    QRect eraseSquare(QPoint tilePos) const;

    enum Mode {
        Nothing,
        Erase,
        RectangleErase
    };

    Mode mMode = Nothing;
    bool mAllLayers = false;

    /*
     * Erase area in TILES, square and odd-centred. [ and ] change it.
     * Everything follows from eraseArea(), the preview included, so there is
     * nothing else to keep in step.
     */
    int mSize = 1;
    QPoint mLastTilePos;
    QPoint mStart;
};

} // namespace Tiled
