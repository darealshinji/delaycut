/*  Copyright (C) 2010 by Adam Thomas-Murphy

    This file is part of DelayCut.

    DelayCut is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DelayCut is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DelayCut.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DRAGDROPLINEEDIT_H
#define DRAGDROPLINEEDIT_H

#include <QtGui>
#include <QLineEdit>
#include <QMessageBox>

class DragDropLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    DragDropLineEdit(QWidget *parent = 0);

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

signals:
    void onDropEvent(QString fileName);
};

#endif // DRAGDROPLINEEDIT_H
