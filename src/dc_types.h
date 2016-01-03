/*  Copyright (C) 2004 by jsoto
    Copyright (C) 2010 by Adam Thomas-Murphy

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

#ifndef DC_TYPES_H
#define DC_TYPES_H

#include <QString>

// actions in crc errors
#define CRC_IGNORE 0
#define CRC_FIX 1
#define CRC_SILENCE 2
#define CRC_SKIP 3

//actions to write frame
#define WF_SKIP 0
#define WF_WRITE 1
#define WF_SILENCE 2

#define MAXFRAMESIZE 16384

struct FILEINFO {
    QString type;
    qint32 bitrate;
    qint32 byterate;
    qint32 iInidata; // ini of data chunk in wave files
    qreal dactrate;
    qint32 mode;
    qint32 iBits;
    QString csMode, csLFE;
    qint64 i64Filesize, i64TotalFrames, i64rest;
    qreal dBytesperframe;
    qint32 layer;
    qint32 fsample;
    qreal dFrameduration, dFramesPerSecond;
    QString csOriginalDuration;
    quint32 bsmod, acmod, frmsizecod, fscod,cpf;
    //Estimated results
    QString csTimeLengthEst;
    qint64 i64StartFrame;
    qint64 i64EndFrame;
    qint64 i64frameinfo;
    qreal dNotFixedDelay;
};

#endif // DC_TYPES_H
