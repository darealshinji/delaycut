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

#include "dragdroplineedit.h"

DragDropLineEdit::DragDropLineEdit(QWidget *parent) :
    QLineEdit(parent)
{
}

void DragDropLineEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
    {
        event->acceptProposedAction();
    }
}

void DragDropLineEdit::dropEvent(QDropEvent *event)
{
    QList<QUrl> urlList;
    QString fileName;
    QFileInfo fileInfo;

    if (event->mimeData()->hasUrls())
    {
        if (event->mimeData()->urls().length() > 1)
        {
            QMessageBox::warning(0, "Error", "Only one file at a time!");
        }
        else
        {
            urlList = event->mimeData()->urls();

            if ( urlList.size() > 0)
            {
                fileName = urlList[0].toLocalFile();
                if (fileInfo.isFile())
                {
                    setText(fileName);
                }
                emit onDropEvent(fileName);
            }
        }
    }

    event->acceptProposedAction();
}
