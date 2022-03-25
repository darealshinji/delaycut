/*  Copyright (C) 2004 by jsoto
    2007-09-11 E-AC3 support added by www.madshi.net
    Copyright (C) 2010 by Adam Thomas-Murphy
    Copyright (C) 2014 by djcj <djcj@gmx.de>

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

#ifndef DELAYAC3_H
#define DELAYAC3_H

#include <QObject>
#include <cstdio>
#include "frame.h"

struct FILEINFO;

class Delayac3 : public QObject
{
    Q_OBJECT

public slots:
    void delayFile();

public:
    Delayac3(QString inputFileName, QString outputFileName, QString logFileName, FILEINFO* fileInfo, bool isCLI, QString fixCRC, bool writeConsole);
    ~Delayac3();
    void delayac3(FILE* inputFile, FILE* outputFile, FILE* logFile);
    void delayeac3(FILE* inputFile, FILE* outputFile, FILE* logFile);
    void delaydts(FILE* inputFile, FILE* outputFile, FILE* logFile);
    void delaympa(FILE* inputFile, FILE* outputFile, FILE* logFile);
    void delaywav(FILE* inputFile, FILE* outputFile, FILE* logFile);
    static bool getFileInfo(QString inputFileName, FILEINFO *fileinfo);
    static qint32 gettargetinfo(FILEINFO *fileinfo, qreal startDelay, qreal endDelay, qreal startCut, qreal endCut, qreal startSilence, qreal lengthSilence);
    static qint64 round(double value);

signals:
    void UpdateProgress(qint32 progressValue);
    void ProcessingFinished(bool success, bool abort);

private:
    static quint32 ac3_crc(const uchar *data, qint32 n, quint32 crc);
    static quint32 ac3_crc_bit(const uchar *data, qint32 n, quint32 crc);
    static QString compute_time_string(qint64 i64, qreal dFrameduration);
    static quint32 mul_poly(quint32 a, quint32 b, quint32 poly);
    static quint32 pow_poly(quint32 a, quint32 n, quint32 poly);
    static qint32 getwavinfo(FILE* inputFile, FILEINFO *fileInfo);
    static qint32 getac3info (FILE* inputFile, FILEINFO *fileInfo, bool eac3);
    static qint32 getdtsinfo(FILE* inputFile, FILEINFO *fileInfo);
    static qint32 getmpainfo(FILE* inputFile, FILEINFO *fileInfo);
    static uint readac3frame(FILE *filein, Frame& frame);
    static uint readdtsframe(FILE *filein, Frame& frame);
    static uint readeac3frame(FILE *filein, Frame& frame);
    static uint readmpaframe(FILE *filein, Frame& frame);
    static uint readwavsample(FILE *filein, Frame& frame, uint nubytes);
    static void writeac3frame(FILE *fileout, const Frame& p_frame);
    static void writeeac3frame(FILE *fileout, const Frame& p_frame);
    static void writedtsframe(FILE *fileout, const Frame& p_frame);
    static void writempaframe(FILE *fileout, const Frame& p_frame);
    static void writewavsample(FILE *fileout, const Frame& p_frame, qint32 nubytes);
    void ac3_crc_init();

    static quint32 crc_table[256];
    QString inputFileName;
    QString outputFileName;
    QString logFileName;
    FILEINFO* fileInfo;
    bool isCLI;
    QString fixCRC;
    bool writeConsole;

    void printline(QString csLinea, bool writeConsole);
    void printlog(FILE *logFile, QString csLinea, bool isCLI, bool writeConsole);
};

#endif // DELAYAC3_H
