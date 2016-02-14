/*  Copyright (C) 2010 by Adam Thomas-Murphy
    Copyright (C) 2016 by djcj <djcj@gmx.de>

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

#include <QApplication>
#include "delaycut.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    DelayCut w;

    bool isGUI = argc < 2;

    if (isGUI)
    {
#ifdef Q_OS_WIN
        FreeConsole();
#endif
        w.show();
    }
    else
    {
        w.execCLI(argc);
    }
    return a.exec();
}
