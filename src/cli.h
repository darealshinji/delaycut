/*  Copyright (C) 2004 by jsoto
    2007-09-11 E-AC3 support added by www.madshi.net
    Copyright (C) 2010 by Adam Thomas-Murphy
    Copyright (C) 2016-2017 by djcj <djcj@gmx.de>

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

#ifndef CLI_H
#define CLI_H

#include <QFileInfo>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QValidator>

/* Reminder:
 * On a new release, don't forget to bump the version
 * in delaycut.ui and delaycut.1 too and update the ChangeLog.
 */
#define VERSION "1.4.3.9"

class Delayac3;
class QValidator;
struct FILEINFO;

Delayac3 *delayac3;
QString crcMode;
QValidator *fpsValidator;
FILEINFO *fileInfo;
FILE* inputFile;
FILE* outputFile;
FILE* logFile;
bool _abort, isCLI, writeConsole, isCut;
QString currentInputMode;
qreal startCut, endCut, startDelay, endDelay, fps;
QString inFileName, outFileName, logFileName;

void printHelp();
void GetInputFileInfo();
void CalculateTarget();
void onProcessingFinished(bool success);
qint32 logInputInfo(QString fileName, QString extension);
qint32 logTargetInfo(QString fileName, QString extension);

#endif // CLI_H
