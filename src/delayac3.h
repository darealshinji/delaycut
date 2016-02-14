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

struct FILEINFO;

class Delayac3 : public QObject
{
    Q_OBJECT

public slots:
    void delayFile();

public:
    Delayac3();
    ~Delayac3();
    void SetDelayValues(FILE *inputFile, FILE *outputFile, FILE *logFile, FILEINFO* fileInfo, bool isCLI, bool *abort, QString fixCRC, bool writeConsole, QString extension);
    void ac3_crc_init();
    void delayac3();
    void delayeac3();
    void delaydts();
    void delaympa();
    void delaywav();
    qint32 getwavinfo(FILE *in, FILEINFO *fileinfo);
    qint32 getac3info(FILE *in, FILEINFO *fileinfo);
    qint32 geteac3info (FILE *in, FILEINFO *fileinfo);
    qint32 getdtsinfo(FILE *in, FILEINFO *fileinfo);
    qint32 getmpainfo(FILE *in, FILEINFO *fileinfo);
    qint32 gettargetinfo(FILEINFO *fileinfo, qreal startDelay, qreal endDelay, qreal startCut, qreal endCut);
    qint64 round(double value);

signals:
    void UpdateProgress(qint32 progressValue);
    void ProcessingFinished(bool success);

private:
    quint32 ac3_crc(uchar *data, qint32 n, quint32 crc);
    quint32 ac3_crc_bit(uchar *data, qint32 n, quint32 crc);
    quint32 getbits(qint32 number, uchar *p_frame);
    quint32 mul_poly(quint32 a, quint32 b, quint32 poly);
    quint32 pow_poly(quint32 a, quint32 n, quint32 poly);
    qint32 readac3frame(FILE *filein, uchar *p_frame);
    qint32 readdtsframe(FILE *filein, uchar *p_frame);
    qint32 readeac3frame(FILE *filein, uchar *p_frame);
    qint32 readmpaframe(FILE *filein, uchar *p_frame);
    qint32 readwavsample(FILE *filein, uchar *p_frame, qint32 nubytes);
    void writeac3frame(FILE *fileout, uchar *p_frame);
    void writeeac3frame(FILE *fileout, uchar *p_frame);
    void writedtsframe(FILE *fileout, uchar *p_frame);
    void writempaframe(FILE *fileout, uchar *p_frame);
    void writewavsample(FILE *fileout, uchar *p_frame, qint32 nubytes);

    quint32 p_bit;
    quint32 crc_table[256];
    bool tooManyErrors;
    FILE *inputFile;
    FILE *outputFile;
    FILE *logFile;
    FILEINFO* fileInfo;
    bool isCLI;
    bool *abort;
    QString fixCRC;
    bool writeConsole;
    QString extension;

    void printline(QString csLinea, bool writeConsole);
    void printlog(FILE *logFile, QString csLinea, bool isCLI, bool writeConsole);
};

#endif // DELAYAC3_H
