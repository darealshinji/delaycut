/*  Copyright (C) 2004 by jsoto
    2007-09-11 E-AC3 support added by www.madshi.net
    Copyright (C) 2010 by Adam Thomas-Murphy
    Copyright (C) 2014, 2016-2017 by djcj <djcj@gmx.de>

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

#include "delayac3.h"
#include "dc_types.h"
#include "sil48.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <math.h>
#include <QTextStream>
#include <QFileInfo>
#include <QThread>

/* Probably better fix for building on mac */
#if defined(__APPLE__)
#include <sys/cdefs.h>
#endif
#if defined(__APPLE__) && defined(_DARWIN_FEATURE_ONLY_64_BIT_INODE)
#define stat64 stat
#define fstat64 fstat
#endif

#define NUMREAD 8
#define MAXBYTES_PER_FRAME 2*1280
#define MAXLOGERRORS 100
#define debug  0
#define CRC16_POLY ((1 << 0) | (1 << 2) | (1 << 15) | (1 << 16))

bool crcTableInitialized=false;
quint32 crcTable[256];

void ac3_crc_init()
{
    if(crcTableInitialized)
        return;

    unsigned int c, k;

    //quint32* p = new quint32[256];
    for(uint n=0; n<256; n++)
    {
        c = n << 8;
        for (k = 0; k < 8; k++) {
            if (c & (1 << 15))
                c = ((c << 1) & 0xffff) ^ (CRC16_POLY & 0xffff);
            else
                c = c << 1;
        }
        crcTable[n] = c;
    }

    crcTableInitialized=true;
}

Delayac3::Delayac3(QString inputFileName, QString outputFileName, QString logFileName, FILEINFO* fileInfo, bool isCLI, QString fixCRC, bool writeConsole) :
    inputFileName(inputFileName), outputFileName(outputFileName), logFileName(logFileName), fileInfo(fileInfo), isCLI(isCLI), fixCRC(fixCRC), writeConsole(writeConsole)
{
    ::ac3_crc_init();
}

Delayac3::~Delayac3()
{
}

void Delayac3::delayFile()
{

#ifndef Q_OS_WIN
    FILE* outputFile = fopen(outputFileName.toUtf8().constData(),"wb");
#else
    FILE* outputFile = _wfopen(outputFileName.toStdWString().c_str(), L"wb");
#endif
    if (!outputFile)
    {
        emit ProcessingFinished(false, false);
        return;
    }

#ifndef Q_OS_WIN
    FILE* inputFile = fopen(inputFileName.toUtf8().constData(),"rb");
#else
    FILE* inputFile = _wfopen(inputFileName.toStdWString().c_str(), L"rb");
#endif
    if (!inputFile)
    {
        fclose(outputFile);
        emit ProcessingFinished(false, false);
        return;
    }

#ifndef Q_OS_WIN
    FILE* logFile = fopen(logFileName.toUtf8().constData(),"a");
#else
    FILE* logFile = _wfopen(logFileName.toStdWString().c_str(), L"a");
#endif
    if (!logFile)
    {
        fclose(outputFile);
        fclose(inputFile);
        emit ProcessingFinished(false, false);
        return;
    }

    QString extension = QFileInfo(inputFileName).suffix().toLower();

    if (extension == "wav")
        delaywav(inputFile, outputFile, logFile);
    else if (extension == "ac3")
        delayac3(inputFile, outputFile, logFile);
    else if (extension == "eac3" || extension == "ddp" || extension == "ec3" || extension == "dd+")
        delayeac3(inputFile, outputFile, logFile);
    else if (extension == "dts")
        delaydts(inputFile, outputFile, logFile);
    else if (extension == "mpa" || extension == "mp2" || extension == "mp3" )
        delaympa(inputFile, outputFile, logFile);

    fclose(outputFile);
    fclose(inputFile);
    fclose(logFile);
}

bool Delayac3::getFileInfo(QString inputFileName, FILEINFO *fileInfo)
{

#ifndef Q_OS_WIN
    FILE* inputFile = fopen(inputFileName.toUtf8().constData(),"rb");
#else
    FILE* inputFile = _wfopen(inputFileName.toStdWString().c_str(), L"rb");
#endif
    if (!inputFile)
    {
        return false;
    }

    QString extension = QFileInfo(inputFileName).suffix().toLower();

    if (extension == "wav")
        Delayac3::getwavinfo(inputFile, fileInfo);
    else if (extension == "ac3")
        Delayac3::getac3info(inputFile, fileInfo, false);
    else if (extension == "eac3" || extension == "ddp" || extension == "ec3" || extension == "dd+")
        Delayac3::getac3info(inputFile, fileInfo, true);
    else if (extension == "dts")
        Delayac3::getdtsinfo(inputFile, fileInfo);
    else if (extension == "mpa" || extension == "mp2" || extension == "mp3")
        Delayac3::getmpainfo(inputFile, fileInfo);

    fclose(inputFile);
    return true;
}

void Delayac3::delayeac3(FILE* inputFile, FILE* outputFile, FILE* logFile)
{
    Frame frame;
    quint32 syncwordn;
    quint32 fscodn, frmsizen, acmodn, cal_crc1;
    qint32 iBytesPerFramen, iBytesPerFramePrev, crc;
    qint64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten,i64TotalFrames, i64nubytes, n64skip;
    qreal dFrameduration, f_writeframe, nuerrors;
    QString csAux, csAux1;

    if (fixCRC == "IGNORED")
    {
        crc = CRC_IGNORE;
    }
    if (fixCRC == "FIXED")
    {
        crc = CRC_FIX;
    }
    if (fixCRC == "SILENCED")
    {
        crc = CRC_SILENCE;
    }
    if (fixCRC == "SKIPPED")
    {
        crc = CRC_SKIP;
    }

    i64StartFrame = fileInfo->i64StartFrame;
    i64EndFrame = fileInfo->i64EndFrame;
    i64nuframes = i64EndFrame-i64StartFrame+1;
    dFrameduration = fileInfo->dFrameduration;
    i64TotalFrames = fileInfo->i64TotalFrames;

    i64frameswritten = 0;
    nuerrors = 0;

    printlog (logFile, "====== PROCESSING LOG ======================", isCLI, writeConsole);
    if (i64StartFrame > 0)
    {
        printlog (logFile, "Seeking....", isCLI, writeConsole);
        fseek ( inputFile, (long)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        printlog (logFile, "Done.", isCLI, writeConsole);
        printlog (logFile, "Processing....", isCLI, writeConsole);
    }
    if (i64EndFrame == (fileInfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (qint64)1<<60);

    bool endOfFile = false, tooManyErrors = false, crcError = false;
    iBytesPerFramePrev = iBytesPerFramen=0;
    i64nubytes = 0;
    for (i64 = i64StartFrame; i64 < i64EndFrame+1 && !QThread::currentThread()->isInterruptionRequested() && endOfFile==false; i64++)
    {
        if (isCLI == false)
            emit UpdateProgress((int)(((i64 - i64StartFrame) * 100) / i64nuframes));
        else
        {
            csAux = QString("Processing %1 %").arg((int) ((i64 * 100) / i64nuframes), 2, 10, QChar('0'));
            printline(csAux, writeConsole);
        }

        if(i64 == fileInfo->i64StartSilenceFrame) {
            for(qint64 i64Counter=0; i64Counter < fileInfo->i64LengthSilenceFrame; i64Counter++) {
                for(quint32 iSubstreamNum=0; iSubstreamNum < fileInfo->iSubstreamsCount; iSubstreamNum++) {
                    i64frameswritten++;
                    writeeac3frame (outputFile, fileInfo->silence[iSubstreamNum]);
                }
            }
        }

        for(quint32 iSubstreamNum=0; iSubstreamNum < fileInfo->iSubstreamsCount; iSubstreamNum++) {
            if (i64 < 0 || i64 >= i64TotalFrames)
            {
                f_writeframe = WF_SILENCE;
            }
            else
            {
                /////////////////////////////////////////////////////////
                // Read & write frame by frame
                //	 If EndFrames<0 stop before the end
                /////////////////////////////////////////////////////////
                iBytesPerFramePrev=(int)i64nubytes;

                f_writeframe=WF_WRITE; //indicates write frame
                i64nubytes=readeac3frame (inputFile, frame);
                if( i64nubytes <5)
                {
                    endOfFile=true;
                    break;
                }
                iBytesPerFramen=(((frame[2] & 7) << 8) + frame[3] + 1) * 2;

                if (frame[0]!= 0x0B || frame[1]!=0x77)
                {
                    nuerrors++;
                    QString csTime = compute_time_string(i64, dFrameduration);
                    csAux = QString("Time %1; Frame#= %2. Unsynchronized frame...")
                                    .arg(csTime).arg(i64 + 1);
                    // Try to find the next sync Word and continue...
                    // rewind nubytes (last frame), and if previous had error,
                    if (!crcError)
                    {
                        n64skip=0;
                        fseek(inputFile, (long)(-1*i64nubytes), SEEK_CUR);
                    }
                    else
                    {
                        n64skip= -1*iBytesPerFramePrev+2;
                        fseek(inputFile, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);
                    }

                    frame[0]=fgetc(inputFile);
                    frame[1]=fgetc(inputFile);
                    while ((frame[0]!= 0x0B || frame[1]!=0x77 ) && !feof(inputFile) )
                    {
                        frame[0]= frame[1];
                        frame[1]= fgetc(inputFile);
                        n64skip++;
                    }
                    if (frame[0]== 0x0B && frame[1]==0x77)
                    {
                        if (n64skip>0)
                            csAux1 = QString("SKIPPED  %1 bytes. Found new synch word").arg(n64skip);
                        else
                            csAux1 = QString("REWINDED %1 bytes. Found new synch word").arg(-1 * n64skip);

                        csAux+=csAux1;
                        printlog(logFile, csAux, isCLI, writeConsole);
                // rewind 2 bytes
                        fseek(inputFile, -2L, SEEK_CUR);
                        f_writeframe=WF_SKIP; // do not write this frame
                    }
                    else // nothing to do, reached end of file
                    {
                        csAux+="NOT FIXED. Reached end of file ";
                        printlog(logFile, csAux, isCLI, writeConsole);
                        f_writeframe=WF_SKIP; // do not write this frame
                        endOfFile=true;
                    }
                }
                else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
                {
                    nuerrors++;
                    f_writeframe=WF_SKIP; // do not write this frame
                    QString csTime = compute_time_string(i64, dFrameduration);
                    csAux = QString("Time %1; Frame#= %2. Uncomplete frame...SKIPPED").arg(csTime).arg(i64 + 1);
                    printlog(logFile, csAux, isCLI, writeConsole);
                }
                else
                {
                    syncwordn=frame.getBits(16);
                              frame.getBits(2);
                              frame.getBits(3);
                    frmsizen= frame.getBits(11)*2+2;
                    fscodn=   frame.getBits(2);
                              frame.getBits(2);
                    acmodn=   frame.getBits(3);

                    if(iSubstreamNum == 0) {
                        // Some consistence checks
                        if (fscodn != fileInfo->fscod ||
                            acmodn != fileInfo->acmod)
                        {
                            nuerrors++;
                            QString csTime = compute_time_string(i64, dFrameduration);
                            csAux = QString("Time %1; Frame# %2. Some basic parameters changed between Frame# %3 and this frame. Frame# %2: (bytesPerFrame: %4, fsCod: %5, acMod: %6), Frame# %3: (bytesPerFrame: %7, fsCod: %8, acMod: %9)")
                                            .arg(csTime).arg(i64 + 1).arg(fileInfo->i64frameinfo).arg(frmsizen).arg(fscodn).arg(acmodn).arg(fileInfo->dBytesperframe).arg(fileInfo->fscod).arg(fileInfo->acmod);
                            printlog(logFile, csAux, isCLI, writeConsole);

                            fileInfo->fscod=fscodn;
                            fileInfo->acmod= acmodn;
                            fileInfo->i64frameinfo=i64+1;
                        }
                    }
    
                    // CRC calculation and fixing.
                    cal_crc1 = ac3_crc(frame.getData() + 2, frmsizen - 4, 0);

                    crcError=false;
                    if (frame[frmsizen-2]!=(cal_crc1 >> 8) ||
                        frame[frmsizen-1]!=(cal_crc1 & 0xff) )
                    {
                        crcError=true;
                        nuerrors++;
                        {
                            QString csTime = compute_time_string(i64, dFrameduration);
                            csAux = QString("Time %1; Frame#= %2.  Crc error %3: read = %4%5; calculated=%6%7")
                                            .arg(csTime).arg(i64 + 1).arg(fixCRC)
                                            .arg(frame[frmsizen - 2], 16, 2, QChar('0')).arg(frame[frmsizen - 1], 16, 2, QChar('0'))
                                            .arg(cal_crc1 >> 8, 16, 2, QChar('0')).arg(cal_crc1 & 0xff, 16, 2, QChar('0'));
                            printlog(logFile, csAux, isCLI, writeConsole);
                        }
                        if (crc==CRC_FIX)
                        {
                            frame[frmsizen-2]=(cal_crc1 >> 8);
                            frame[frmsizen-1]=cal_crc1 & 0xff;
                        }
                        else if (crc==CRC_SKIP) f_writeframe=WF_SKIP;
                        else if (crc==CRC_SILENCE) f_writeframe=WF_SILENCE;
                    }
                }
            }


            //	Write frame
            if (f_writeframe==WF_WRITE)
            {
                i64frameswritten++;
                writeeac3frame (outputFile, frame);
            }
            else if (f_writeframe==WF_SILENCE)
            {
                i64frameswritten++;
                writeeac3frame (outputFile, fileInfo->silence[iSubstreamNum]);
            }

            if (!tooManyErrors && nuerrors > MAXLOGERRORS)
            {
                printlog(logFile, "Too Many Errors. Stop Logging.", isCLI, writeConsole);
                tooManyErrors=true;
            }
        }
    }

    csAux = QString("Number of written frames = %1").arg(i64frameswritten);
    printlog(logFile, csAux, isCLI, writeConsole);
    csAux = QString("Number of Errors= %1").arg(nuerrors);
    printlog(logFile, csAux, isCLI, writeConsole);
    if (isCLI == false)
    {
        emit UpdateProgress(100);
    }

    emit ProcessingFinished(true, QThread::currentThread()->isInterruptionRequested());
}

void Delayac3::writeeac3frame(FILE *fileout, const Frame& frame)
{
    quint32 nubytes;

    nubytes = (((frame[2] & 7) << 8) + frame[3] + 1) * 2;
    if(nubytes > MAXFRAMESIZE)
        nubytes = MAXFRAMESIZE;

    fwrite(frame.getData(), sizeof(uchar), nubytes, fileout);
}

qint32 Delayac3::getac3info (FILE *inputFile, FILEINFO *fileInfo, bool eac3)
{
    Frame frame;

    //fseek(inputFile, (long)0, SEEK_SET);

    quint32 independentSubstreamsCount=0, substreamsCount=0;
    quint32 nubytesTotal=0, rateTotal=0, BytesPerFrameTotal=0;

    // Read first frames to detect substreams
    while(eac3 || substreamsCount < 1) {        
        quint32 nubytes, BytesPerFrame, frmsizecod=0;
        if(eac3) {
            nubytes = readeac3frame (inputFile, frame);
            BytesPerFrame = (((frame[2] & 7) << 8) + frame[3] + 1) * 2;
        } else {
            nubytes = readac3frame (inputFile, frame);
            frmsizecod=frame[4];
            BytesPerFrame = FrameSize_ac3[frmsizecod]*2;
        }
        
        while (( frame[0]!= 0x0B || frame[1]!=0x77 ||
                nubytes < 12 || nubytes != BytesPerFrame) && !feof(inputFile))
        {
    // Try to find the next sync Word and continue...
                // rewind nubytes (last frame)
            fseek(inputFile, (long)(-1*nubytes), SEEK_CUR);
            frame[0]=fgetc(inputFile);
            frame[1]=fgetc(inputFile);
            while ((frame[0]!= 0x0B || frame[1]!=0x77) && !feof(inputFile) )
            {
                frame[0]= frame[1];
                frame[1]= fgetc(inputFile);
            }
            if (frame[0]== 0x0B && frame[1]==0x77)
            {
                // rewind 2 bytes
                fseek(inputFile, -2L, SEEK_CUR);
                if(eac3) {
                    nubytes = readeac3frame (inputFile, frame);
                    BytesPerFrame = (((frame[2] & 7) << 8) + frame[3] + 1) * 2;
                } else {
                    nubytes = readac3frame (inputFile, frame);
                    frmsizecod=frame[4];
                    BytesPerFrame = FrameSize_ac3[frmsizecod]*2;
                }
            }
        }

        if (nubytes <12 || frame[0]!= 0x0B || frame[1]!=0x77)
            return -1;

        frame.getBits(16);
        quint32 acmod, lfeon, fscod;
        qreal fsample;
        if(eac3) {
            quint32 strmtyp=    frame.getBits(2);
            quint32 substreamid=frame.getBits(3);
            
            if(strmtyp == 0) {
                if(substreamid < independentSubstreamsCount)
                    break; // we already cycled over all available substreams
            
                independentSubstreamsCount++;
            }
            
            // Retain metadata only for the first substream
                                frame.getBits(11);
            fscod=              frame.getBits(2);
            quint32 fscod2=     frame.getBits(2);
            quint32 numblckscod=fscod2;
            acmod=              frame.getBits(3);
            lfeon=              frame.getBits(1);
                                frame.getBits(5);
                                frame.getBits(5);
            quint32 compre=     frame.getBits(1);
            quint32 compr=0;
            if (compre) 
                compr=frame.getBits(8);

            switch(fscod) {
                case 0:
                    fsample=48.0;
                    break;
                case 1:
                    fsample=44.1;
                    break;
                case 2:
                    fsample=32.0;
                    break;
                default:
                    numblckscod = 3;
                    switch(fscod2) {
                        case 0:
                            fsample=24.0;
                            break;
                        case 1:
                            fsample=22.05;
                            break;
                        case 2:
                            fsample=16.0;
                            break;
                        default:
                            fsample=0.0;
                    }
            }

            quint32 numblcks=1;
            switch(numblckscod) {
                case 0:
                    numblcks=1;
                    break;
                case 1:
                    numblcks=2;
                    break;
                case 2:
                    numblcks=3;
                    break;
                default:
                    numblcks=6;
            }

            BytesPerFrameTotal += BytesPerFrame;
            rateTotal += (BytesPerFrame * 8 * (750 / numblcks)) / 4000;
            
            fileInfo->type="eac3";
        }
        else { // NOT enhanced AC3
                                frame.getBits(16);
            fscod=              frame.getBits(2);
            quint32 frmsizecod= frame.getBits(6);
                                frame.getBits(5);
            quint32 bsmod=      frame.getBits(3);
            acmod=              frame.getBits(3);
            quint32 cmixlev=0;
            if ((acmod & 0x01) && (acmod != 0x01)) 
                cmixlev=        frame.getBits(2);
            quint32 surmixlev=0;
            if (acmod & 0x4)
                surmixlev=      frame.getBits(2);
            quint32 dsurmod=0;
            if (acmod == 0x2)
                dsurmod=        frame.getBits(2);
            lfeon=              frame.getBits(1);
                                frame.getBits(5);
            quint32 compre=     frame.getBits(1);
            quint32 compr=0;
            if (compre) 
                compr=          frame.getBits(8);

            switch(fscod) {
                case 0:
                    fsample=48.0;
                    break;
                case 1:
                    fsample=44.1;
                    break;
                case 2:
                    fsample=32.0;
                    break;
                default:
                    fsample=0.0;
            }
            
            fileInfo->bsmod=bsmod;
            fileInfo->frmsizecod=frmsizecod;

            BytesPerFrameTotal += FrameSize_ac3[frmsizecod + fscod*64]*2;
            rateTotal += bitrate[frmsizecod];

            fileInfo->bsmod=bsmod;
            fileInfo->frmsizecod=frmsizecod;
            fileInfo->type="ac3";
        }

        substreamsCount++;
        
        /////////////////////////////////////////////////////////////////////////////
        // Fill silence frame, If acmod or bitrate is not included, use first frame
        /////////////////////////////////////////////////////////////////////////////
        for (quint32 i=0; i<nubytes; i++)
            fileInfo->silence[substreamsCount-1][i]=frame[i];

        nubytesTotal += nubytes;

        if(substreamsCount == 1) {
            fileInfo->fsample=(int)(fsample*1000);
            
            fileInfo->acmod=acmod;
            switch(acmod) {
                case 0:
                    fileInfo->csMode = "1+1: A+B";
                    break;
                case 1:
                    fileInfo->csMode = "1/0: C";
                    break;
                case 2:
                    fileInfo->csMode = "2/0: L+R";
                    break;
                case 3:
                    fileInfo->csMode = "3/0: L+C+R";
                    break;
                case 4:
                    fileInfo->csMode = "2/1: L+R+S";
                    break;
                case 5:
                    fileInfo->csMode = "3/1: L+C+R+S";
                    break;
                case 6:
                    fileInfo->csMode = "2/2: L+R+SL+SR";
                    break;
                case 7:
                    fileInfo->csMode = "3/2: L+C+R+SL+SR";
                    break;
                default:
                    fileInfo->csMode = QString("Unknown acmode: %1").arg(acmod);
            }
            
            if (lfeon)
                fileInfo->csLFE="LFE: Present";
            else
                fileInfo->csLFE="LFE: Not present";
                
            fileInfo->fscod=fscod;
        }
    }

#ifndef Q_OS_WIN
    struct stat64 statbuf;
    fstat64(fileno(inputFile), &statbuf);
#else
    struct _stati64 statbuf;
    _fstati64(fileno(inputFile), &statbuf);
#endif

    qint64 nuframes=       statbuf.st_size/BytesPerFrameTotal;
    qint64 rest=           statbuf.st_size%nuframes;
    qreal FramesPerSecond=((double)(rateTotal))*1000.0/(BytesPerFrameTotal * 8);
    qreal FrameDuration=  1000.0/FramesPerSecond; // in msecs
    qint64 TimeLengthIni=  statbuf.st_size/(rateTotal/8); // in msecs

    fileInfo->csOriginalDuration = QString("%1:%2:%3.%4")
                                    .arg(TimeLengthIni / 3600000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 3600000) / 60000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 60000) / 1000, 2, 10, QChar('0'))
                                    .arg(TimeLengthIni % 1000, 3, 10, QChar('0'));
    fileInfo->layer=0;
    fileInfo->iSubstreamsCount=substreamsCount;
    if(substreamsCount > 1)
        fileInfo->csMode += " / multiple substreams";
    fileInfo->dFrameduration=FrameDuration;
    fileInfo->i64Filesize=statbuf.st_size;
    fileInfo->i64TotalFrames=nuframes;
    fileInfo->bitrate=rateTotal;
    fileInfo->dactrate=(double) rateTotal;
    fileInfo->dBytesperframe=(double) BytesPerFrameTotal;
    fileInfo->dFramesPerSecond=FramesPerSecond;
    fileInfo->i64rest=rest;
    fileInfo->cpf=1;
    fileInfo->i64frameinfo=1;

    return 0;
}

uint Delayac3::readeac3frame(FILE *filein, Frame& frame)
{
    //p_bit=0;
    uint i;
    uchar buff[MAXFRAMESIZE];

    for (i=0; !feof(filein) && i < NUMREAD; i++)
    {
        buff[i]=fgetc(filein);
    }

    uint nNumRead=i;

    if (feof(filein))
        return i;

    uint BytesPerFrame = (((buff[2] & 7) << 8) + buff[3] + 1) * 2;

    if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

    uint nRead=fread(&buff[nNumRead], sizeof (unsigned char), BytesPerFrame-nNumRead, filein);
    frame.setData(buff, nRead+nNumRead);

    return nRead+nNumRead;
}

void Delayac3::delayac3(FILE* inputFile, FILE* outputFile, FILE* logFile)
{
    qint64 i64;
    qint64 i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten,i64TotalFrames;
    qint64 i64nubytes;
    qreal dFrameduration;
    qint32 iBytesPerFramen, iBytesPerFramePrev;
    quint32 syncwordn, crc1n, bsidn;
    quint32 iFrmsizecodn;
    quint32 fscodn, bsmodn, acmodn;
    QString csTime, csAux, csAux1;
    qint32 f_writeframe, nuerrors;
    quint32 frame_size, frame_size_58, cal_crc2, cal_crc1, crc_inv;
    qint64 n64skip;
    qint32 m_iCrc;

    if (fixCRC == "IGNORED")
    {
        m_iCrc = CRC_IGNORE;
    }
    if (fixCRC == "FIXED")
    {
        m_iCrc = CRC_FIX;
    }
    if (fixCRC == "SILENCED")
    {
        m_iCrc = CRC_SILENCE;
    }
    if (fixCRC == "SKIPPED")
    {
        m_iCrc = CRC_SKIP;
    }

    Frame frame;


    // init ac3_crc
//	ac3_crc_init(); Done in Initinstance

    i64StartFrame=fileInfo->i64StartFrame;
    i64EndFrame=fileInfo->i64EndFrame;
    i64nuframes=i64EndFrame-i64StartFrame+1;
    dFrameduration=fileInfo->dFrameduration;
    i64TotalFrames = fileInfo->i64TotalFrames;

    i64frameswritten=0;
    nuerrors=0;

    printlog(logFile, "====== PROCESSING LOG ======================", isCLI, writeConsole);
    if (i64StartFrame > 0)
    {
        printlog(logFile, "Seeking....", isCLI, writeConsole);
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        fseek ( inputFile, (long)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        printlog (logFile, "Done.", isCLI, writeConsole);
        printlog (logFile, "Processing....", isCLI, writeConsole);
    }
    if (i64EndFrame == (fileInfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (qint64)1<<60);

    bool bEndOfFile = false, tooManyErrors = false, bCRCError = false;
    iBytesPerFramePrev=iBytesPerFramen=0;
    i64nubytes=0;
    for (i64=i64StartFrame; i64< i64EndFrame+1 && !QThread::currentThread()->isInterruptionRequested() && bEndOfFile==false ;i64++)
    {
        if (isCLI == false)
            emit UpdateProgress((int)(((i64-i64StartFrame)*100)/i64nuframes));
        else
        {
            csAux = QString("Processing %1 %").arg((int)((i64*100)/i64nuframes), 2, 10, QChar('0'));
            printline(csAux, writeConsole);
        }

        if(i64 == fileInfo->i64StartSilenceFrame) {
            for(qint64 i64Counter=0; i64Counter < fileInfo->i64LengthSilenceFrame; i64Counter++) {
                i64frameswritten++;
                writeac3frame (outputFile, fileInfo->silence[0]);
            }
        }

        if (i64 < 0 || i64 >= i64TotalFrames)
            f_writeframe=WF_SILENCE;
        else
        {
/////////////////////////////////////////////////////////
// Read & write frame by frame
//	 If EndFrames<0 stop before the end
/////////////////////////////////////////////////////////
            iBytesPerFramePrev=(int)i64nubytes;

            f_writeframe=WF_WRITE; //indicates write frame
            i64nubytes=readac3frame (inputFile, frame);
            if( i64nubytes <5)
            {
                bEndOfFile=true;
                break;
            }
//			iFrmsizecodn=frame[4]& 0x3F;
            iFrmsizecodn=frame[4];
            iBytesPerFramen=FrameSize_ac3[iFrmsizecodn]*2;
            csTime = QString("%1:%2:%3.%4")
                            .arg(((int) (dFrameduration * i64)) / 3600000, 2, 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 3600000) / 60000, 2, 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 60000) / 1000, 2, 10, QChar('0'))
                            .arg(((int) (dFrameduration * i64)) % 1000, 3, 10, QChar('0'));

            if ( frame[0]!= 0x0B || frame[1]!=0x77)
            {
                nuerrors++;
                csAux = QString("Time %1; Frame#= %2. Unsynchronized frame...").arg(csTime).arg(i64 + 1);
// Try to find the next sync Word and continue...
            // rewind nubytes (last frame), and if previous had error,
                if (!bCRCError)
                {
                    n64skip=0;
                    fseek(inputFile, (long)(-1*i64nubytes), SEEK_CUR);
                }
                else
                {
                    n64skip= -1*iBytesPerFramePrev+2;
                    fseek(inputFile, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);
                }

                frame[0]=fgetc(inputFile);
                frame[1]=fgetc(inputFile);
                while ((frame[0]!= 0x0B || frame[1]!=0x77 ) && !feof(inputFile) )
                {
                    frame[0]= frame[1];
                    frame[1]= fgetc(inputFile);
                    n64skip++;
                }
                if (frame[0]== 0x0B && frame[1]==0x77)
                {
                    if (n64skip>0)
                        csAux1 = QString("SKIPPED  %1 bytes. Found new synch word").arg(n64skip);
                    else
                        csAux1 = QString("REWINDED %1 bytes. Found new synch word").arg(-1 * n64skip);

                    csAux+=csAux1;
                    printlog(logFile, csAux, isCLI, writeConsole);
            // rewind 2 bytes
                    fseek(inputFile, -2L, SEEK_CUR);
                    f_writeframe=WF_SKIP; // do not write this frame
                }
                else // nothimg to do, reached end of file
                {
                    csAux+="NOT FIXED. Reached end of file ";
                    printlog(logFile, csAux, isCLI, writeConsole);
                    f_writeframe=WF_SKIP; // do not write this frame
                    bEndOfFile=true;
                }
            }
            else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
            {
                nuerrors++;
                f_writeframe=WF_SKIP; // do not write this frame
                csAux = QString("Time %1; Frame#= %2. Uncomplete frame...SKIPPED").arg(csTime).arg(i64 + 1);
                printlog(logFile, csAux, isCLI, writeConsole);
            }
            else
            {

// Some consistence checks
                syncwordn=   frame.getBits(16);
                crc1n=       frame.getBits(16);
                fscodn=      frame.getBits(2);
                iFrmsizecodn=frame.getBits(6);
                bsidn=       frame.getBits(5);
                bsmodn=      frame.getBits(3);
                acmodn=      frame.getBits(3);
                if ((iFrmsizecodn >> 1) != (fileInfo->frmsizecod >>1) ||
                    fscodn != fileInfo->fscod ||
                    bsmodn != fileInfo->bsmod || acmodn != fileInfo->acmod)
                {
                    fileInfo->fscod=fscodn;
                    fileInfo->frmsizecod= iFrmsizecodn;
                    fileInfo->bsmod=bsmodn;
                    fileInfo->acmod= acmodn;

                    nuerrors++;
                    csAux = QString("Time %1; Frame#= %2. Some basic parameters changed between Frame #%3 and this frame")
                                .arg(csTime)
                                .arg(i64 + 1)
                                .arg(fileInfo->i64frameinfo);
                    printlog(logFile, csAux, isCLI, writeConsole);
                    fileInfo->i64frameinfo=i64+1;
                }

// CRC calculation and fixing.

//              frame_size = FrameSize_48[iFrmsizecodn];
                frame_size = FrameSize_ac3[(iFrmsizecodn + (64 * fscodn))];
                frame_size_58 = ((frame_size >> 1) + (frame_size >> 3));

                cal_crc1 = ac3_crc(frame.getData() + 4, (2 * frame_size_58) - 4, 0);
                crc_inv  = pow_poly((CRC16_POLY >> 1), (16 * frame_size_58) - 16, CRC16_POLY);
                cal_crc1 = mul_poly(crc_inv, cal_crc1, CRC16_POLY);
                cal_crc2 = ac3_crc(frame.getData() + 2 * frame_size_58, (frame_size - frame_size_58) * 2 - 2, 0);

                bCRCError=false;
                if (frame[2]!=(cal_crc1 >> 8) ||
                    frame[3]!=(cal_crc1 & 0xff) )
                {
                    bCRCError=true;
                    nuerrors++;
                    {
                        csAux = QString("Time %1; Frame#= %2.  Crc1 error %3: read = %4%5; calculated=%6%7")
                                        .arg(csTime).arg(i64 + 1).arg(fixCRC)
                                        .arg(frame[2], 2, 16, QChar('0')).arg(frame[3], 2, 16, QChar('0'))
                                        .arg(cal_crc1 >> 8, 2, 16, QChar('0')).arg(cal_crc1 & 0xff, 2, 16, QChar('0'));
                        printlog(logFile, csAux, isCLI, writeConsole);
                    }
                    if (m_iCrc==CRC_FIX)
                    {
                        frame[2]=(cal_crc1 >> 8);
                        frame[3]=cal_crc1 & 0xff;
                    }
                    else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
                    else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
                }

                if (frame[2*frame_size - 2]!=(cal_crc2 >> 8) ||
                    frame[2*frame_size - 1]!=(cal_crc2 & 0xff) )
                {
                    bCRCError=true;
                    nuerrors++;
                    {
                        csAux = QString("Time %1; Frame#= %2.  Crc2 error %3: read = %4%5; calculated=%6%7")
                            .arg(csTime).arg(i64 + 1).arg(fixCRC)
                            .arg(frame[(2 * frame_size) - 2], 2, 16, QChar('0')).arg(frame[(2 * frame_size) - 1], 2, 16, QChar('0'))
                            .arg(cal_crc2 >> 8, 2, 16, QChar('0')).arg(cal_crc2 & 0xff, 2, 16, QChar('0'));
                        printlog(logFile, csAux, isCLI, writeConsole);
                    }
                    if (m_iCrc==CRC_FIX)
                    {
                        frame[2*frame_size - 2]=(cal_crc2 >> 8);
                        frame[2*frame_size - 1]=cal_crc2 & 0xff;
                    }
                    else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
                    else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
                }

            }
        }


//	Write frame
        if (f_writeframe==WF_WRITE)
        {
            i64frameswritten++;
            writeac3frame (outputFile, frame);
        }
        else if (f_writeframe==WF_SILENCE)
        {
            i64frameswritten++;
            writeac3frame (outputFile, silence);
        }

        if (!tooManyErrors && nuerrors > MAXLOGERRORS)
        {
            printlog(logFile, "Too Many Errors. Stop Logging.", isCLI, writeConsole);
            tooManyErrors=true;
        }

    }
    csAux = QString("Number of written frames = %1").arg(i64frameswritten);
    printlog(logFile, csAux, isCLI, writeConsole);
    csAux = QString("Number of Errors= %1").arg(nuerrors);
    printlog(logFile, csAux, isCLI, writeConsole);
    if (isCLI == false)
        emit UpdateProgress(100);

    emit ProcessingFinished(true, QThread::currentThread()->isInterruptionRequested());
}

void Delayac3::writeac3frame (FILE *fileout, const Frame& frame)
{
    quint32 frmsizecod, nubytes;

    frmsizecod = frame[4];
    nubytes = FrameSize_ac3[frmsizecod] * 2;
    if(nubytes > MAXFRAMESIZE)
        nubytes = MAXFRAMESIZE;

    fwrite(frame.getData(), sizeof(unsigned char), nubytes, fileout);
}

void Delayac3::delaydts(FILE* inputFile, FILE* outputFile, FILE* logFile)
{
    qint64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
    qreal dFrameduration;
    qint64 i64nubytes;
    qint32 iBytesPerFramen, iBytesPerFramePrev;
    QString csTime, csAux, csAux1;
    qint32 f_writeframe, nuerrors;
    quint32 unused, amode, sfreq;
    qint32 rate;
    qint64 n64skip;

    Frame frame;

//	ac3_crc_init(); Done in Initinstance

    i64StartFrame=fileInfo->i64StartFrame;
    i64EndFrame=fileInfo->i64EndFrame;
    i64nuframes=i64EndFrame-i64StartFrame+1;
    dFrameduration=fileInfo->dFrameduration;
    i64TotalFrames=fileInfo->i64TotalFrames;

    i64frameswritten=0;
    nuerrors=0;

    printlog(logFile, "====== PROCESSING LOG ======================", isCLI, writeConsole);

    if (i64StartFrame > 0)
    {
        printlog (logFile, "Seeking....", isCLI, writeConsole);
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        fseek ( inputFile, (long)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        printlog (logFile, "Done.", isCLI, writeConsole);
        printlog (logFile, "Processing....", isCLI, writeConsole);
    }
    if (i64EndFrame == (fileInfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (qint64)1<<60);
    bool bEndOfFile = false, tooManyErrors = false;

    iBytesPerFramePrev=iBytesPerFramen=0;
    i64nubytes=0;

    for (i64=i64StartFrame; i64< i64EndFrame+1 && !QThread::currentThread()->isInterruptionRequested() && bEndOfFile==false ;i64++)
    {
        if (isCLI == false)
            emit UpdateProgress((int)(((i64-i64StartFrame)*100)/i64nuframes));
        else
        {
            csAux = QString("Processing %1 %").arg((int) ((i64 * 100) / i64nuframes), 2, 10, QChar('0'));
            printline(csAux, writeConsole);
        }

        if(i64 == fileInfo->i64StartSilenceFrame) {
            for(qint64 i64Counter=0; i64Counter < fileInfo->i64LengthSilenceFrame; i64Counter++) {
                i64frameswritten++;
                writedtsframe(outputFile, silence);
            }
        }

        if (i64 < 0 || i64 >= i64TotalFrames)
        {
            f_writeframe=WF_SILENCE;
        }
        else
        {
            iBytesPerFramePrev=(int)i64nubytes;
            f_writeframe=WF_WRITE; //indicates write frame
            i64nubytes=readdtsframe (inputFile, frame);
            if( i64nubytes <5)
            {
                bEndOfFile=true;
                break;
            }
            iBytesPerFramen=((frame[5]&0x3)*256 + frame[6])*16 + (frame[7]&0xF0)/16 +1;

            csTime = QString("%1:%2:%3.%4")
                            .arg((dFrameduration * i64) / 3600000, 2, 'd', 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 3600000) / 60000, 2, 'd', 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 60000) / 1000, 2, 'd', 10, QChar('0'))
                            .arg(((int) (dFrameduration * i64)) % 1000, 3, 'd', 10, QChar('0'));

            if (frame[0] != 0x7F || frame[1] != 0xFE ||
                    frame[2] != 0x80 || frame[3] != 0x01)
            {
                nuerrors++;
                csAux = QString("Time %1; Frame#= %2. Unsynchronized frame...").arg(csTime).arg(i64 + 1);
// Try to find the next sync Word and continue...
             // rewind nubytes (last frame) + previous frame...
                n64skip= -1*iBytesPerFramePrev+4;
                fseek(inputFile, (long)(-1*(i64nubytes+iBytesPerFramePrev-4)), SEEK_CUR);

                frame[0]=fgetc(inputFile);
                frame[1]=fgetc(inputFile);
                frame[2]=fgetc(inputFile);
                frame[3]=fgetc(inputFile);
                while ((frame[0] != 0x7F || frame[1] != 0xFE ||
                    frame[2] != 0x80 || frame[3] != 0x01)  && !feof(inputFile) )
                {
                    frame[0]= frame[1];
                    frame[1]= frame[2];
                    frame[2]= frame[3];
                    frame[3]= fgetc(inputFile);
                    n64skip++;
                }
                if (frame[0] == 0x7F && frame[1] == 0xFE &&
                    frame[2] == 0x80 && frame[3] == 0x01)
                {
                    if (n64skip>0)
                        csAux1 = QString("SKIPPED  %1 bytes. Found new synch word").arg(n64skip);
                    else
                        csAux1 = QString("REWINDED %1 bytes. Found new synch word").arg(-1 * n64skip);
                    csAux+=csAux1;
                    printlog(logFile, csAux, isCLI, writeConsole);

             // rewind 4 bytes
                    fseek(inputFile, -4L, SEEK_CUR);
                    f_writeframe=WF_SKIP; // do not write this frame
                }
                else // nothimg to do, reached end of file
                {
                    csAux+="NOT FIXED. Reached end of file ";
                    printlog(logFile, csAux, isCLI, writeConsole);
                    f_writeframe=WF_SKIP; // do not write this frame
                    bEndOfFile=true;
                }
            }
            else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
            {
                nuerrors++;
                f_writeframe=WF_SKIP; // do not write this frame
                csAux = QString("Time %1; Frame#= %2. Uncomplete frame...SKIPPED").arg(csTime).arg(i64 + 1);
                printlog(logFile, csAux, isCLI, writeConsole);
            }
            else
            {

// Some consistence checks


/*				syncword=   frame.getBits(32);
                ftype=      frame.getBits(1);
                fshort=     frame.getBits(5);
                cpf=        frame.getBits(1);
                nblks=      frame.getBits(7);
                fsize=      frame.getBits(14);
*/
                unused=     frame.getBits(32);
                unused=     frame.getBits(1);
                unused=     frame.getBits(5);
                unused=     frame.getBits(1);
                unused=     frame.getBits(7);
                unused=     frame.getBits(14);
                amode=      frame.getBits(6);
                sfreq=      frame.getBits(4);
                rate=       frame.getBits(5);
                rate=       dtsbitrate[rate];

                if (sfreq != fileInfo->fscod ||	amode != fileInfo->acmod  || rate != fileInfo->bitrate )
                {

                        fileInfo->fscod=sfreq;
                        fileInfo->acmod=amode;
                        fileInfo->bitrate=rate;

                        nuerrors++;
                        csAux = QString("Time %1; Frame#= %2. Some basic parameters changed between Frame #%3 and this frame")
                                   .arg(csTime).arg(i64 + 1).arg(fileInfo->i64frameinfo);
                        printlog(logFile, csAux, isCLI, writeConsole);

                        fileInfo->i64frameinfo=i64+1;

                }
            }
        }

//	Write frame
        if (f_writeframe==WF_WRITE)
        {
            i64frameswritten++;
            writedtsframe(outputFile, frame);
        }
        else if (f_writeframe==WF_SILENCE)
        {
            i64frameswritten++;
            writedtsframe(outputFile, silence);
        }
        if (!tooManyErrors && nuerrors > MAXLOGERRORS)
        {
            printlog(logFile, "Too Many Errors. Stop Logging.", isCLI, writeConsole);
            tooManyErrors=true;
        }

    }
    csAux = QString("Number of written frames = %1").arg(i64frameswritten);
    printlog(logFile, csAux, isCLI, writeConsole);
    csAux = QString("Number of Errors= %1").arg(nuerrors);
    printlog(logFile, csAux, isCLI, writeConsole);
    if (isCLI == false)
        emit UpdateProgress(100);

    emit ProcessingFinished(true, QThread::currentThread()->isInterruptionRequested());
}

void Delayac3::writedtsframe(FILE *fileout, const Frame& frame)
{
    quint32 BytesPerFrame;

    BytesPerFrame=((frame[5]&0x3)*256 + frame[6])*16 + (frame[7]&0xF0)/16 +1;

    if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

    fwrite(frame.getData(), sizeof (uchar), BytesPerFrame, fileout);
}

void Delayac3::delaympa(FILE* inputFile, FILE* outputFile, FILE* logFile)
{
    qint64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
    qreal dFrameduration;
    qint64  i64nubytes;
    qint32 iBytesPerFramen, iBytesPerFramePrev;
    QString csTime, csAux, csAux1;
    qint32 f_writeframe, nuerrors;
    qint64 n64skip;
    quint32 syncword, iID, irate, padding_bit, private_bit;
    qint32 layer, protection_bit, rate, fsamp, unused;
    quint32 crc_cal1;
    bool bEndOfFile;
    Frame frame;
    qint32 j, i;
    qint32 mode;
    qint32 nbits;
    qint32 *layerIIsub;
    qint32 ratech;
    qint32 m_iCrc;
    quint32 crc_check = 0;

    if (fixCRC == "IGNORED")
    {
        m_iCrc = CRC_IGNORE;
    }
    if (fixCRC == "FIXED")
    {
        m_iCrc = CRC_FIX;
    }
    if (fixCRC == "SILENCED")
    {
        m_iCrc = CRC_SILENCE;
    }
    if (fixCRC == "SKIPPED")
    {
        m_iCrc = CRC_SKIP;
    }

//	ac3_crc_init(); Done in Initinstance

    i64StartFrame=fileInfo->i64StartFrame;
    i64EndFrame=fileInfo->i64EndFrame;
    i64nuframes=i64EndFrame-i64StartFrame+1;
    dFrameduration=fileInfo->dFrameduration;
    i64TotalFrames=fileInfo->i64TotalFrames;

    i64frameswritten=0;
    nuerrors=0;

    printlog (logFile, "====== PROCESSING LOG ======================", isCLI, writeConsole);

    if (i64StartFrame > 0)
    {
        printlog (logFile, "Seeking....", isCLI, writeConsole);
//		_lseeki64 ( _fileno (m_Fin), (int)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        fseek ( inputFile, (long)(((double)i64StartFrame)*fileInfo->dBytesperframe), SEEK_SET);
        printlog (logFile, "Done.", isCLI, writeConsole);
        printlog (logFile, "Processing....", isCLI, writeConsole);
    }

    if (i64EndFrame == (fileInfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (qint64)1<<60);
    bool tooManyErrors = false;
    iBytesPerFramePrev=iBytesPerFramen=0;
    i64nubytes=0;

    for (i64=i64StartFrame; i64< i64EndFrame+1 && !QThread::currentThread()->isInterruptionRequested() && bEndOfFile==false;i64++)
    {
        if (isCLI == false)
            emit UpdateProgress((int)(((i64-i64StartFrame)*100)/i64nuframes));
        else
        {
            csAux = QString("Processing %1 %").arg((int) ((i64 * 100) / i64nuframes), 2, 10, QChar('0'));
            printline(csAux, writeConsole);
        }

        if(i64 == fileInfo->i64StartSilenceFrame) {
            for(qint64 i64Counter=0; i64Counter < fileInfo->i64LengthSilenceFrame; i64Counter++) {
                i64frameswritten++;
                writempaframe (outputFile, silence);
            }
        }

        if (i64 < 0 || i64 >= i64TotalFrames)
                f_writeframe=WF_SILENCE;
        else
        {
/////////////////////////////////////////////////////////
// Read & write frame by frame
//	 If EndFrames<0 stop before the end
/////////////////////////////////////////////////////////
            f_writeframe=WF_WRITE; //indicates write frame

            iBytesPerFramePrev=(int)i64nubytes;

            i64nubytes=readmpaframe (inputFile, frame);
            if( i64nubytes <4)
            {
                bEndOfFile=true;
                break;
            }
            iBytesPerFramen=(int)i64nubytes;

            csTime = QString("%1:%2:%3.%4")
                            .arg((int) ((dFrameduration * i64) / 3600000), 2, 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 3600000) / 60000, 2, 10, QChar('0'))
                            .arg((((int) (dFrameduration * i64)) % 60000) / 1000, 2, 10, QChar('0'))
                            .arg(((int) (dFrameduration * i64)) % 1000, 3, 10, QChar('0'));

            if (frame[0] != 0xFF || (frame[1] & 0xF0 )!= 0xF0 )

            {
                nuerrors++;

                n64skip= -1*iBytesPerFramePrev+2;
                fseek(inputFile, (long)(-1*(i64nubytes+iBytesPerFramePrev-2)), SEEK_CUR);

                csAux = QString("Time %1; Frame#= %2. Unsynchronized frame...").arg(csTime).arg(i64 + 1);
// Try to find the next sync Word and continue...
             // rewind nubytes (last frame)

                fseek(inputFile, (long)(-1*i64nubytes), SEEK_CUR);

                frame[0]=fgetc(inputFile);
                frame[1]=fgetc(inputFile);
                while ((frame[0] != 0xFF || (frame[1] & 0xF0 )!= 0xF0 ) && !feof(inputFile) )
                {
                    frame[0]= frame[1];
                    frame[1]= fgetc(inputFile);
                    n64skip++;
                }
                if (frame[0] == 0xFF && (frame[1] & 0xF0 )== 0xF0 )
                {
                    if (n64skip>0)
                        csAux1 = QString("SKIPPED  %1 bytes. Found new synch word").arg(n64skip);
                    else
                        csAux1 = QString("REWINDED %1 bytes. Found new synch word").arg(-1 * n64skip);
                    csAux+=csAux1;
                    printlog(logFile, csAux, isCLI, writeConsole);

             // rewind 4 bytes
                    fseek(inputFile, -2L, SEEK_CUR);
                    f_writeframe=WF_SKIP; // do not write this frame
                }
                else // nothimg to do, reached end of file
                {
                    csAux+="NOT FIXED. Reached end of file ";
                    printlog(logFile, csAux, isCLI, writeConsole);
                    f_writeframe=WF_SKIP; // do not write this frame
                    bEndOfFile=true;
                }
            }
            else  if (i64nubytes < 12 || i64nubytes != iBytesPerFramen)
            {
                nuerrors++;
                f_writeframe=WF_SKIP; // do not write this frame
                csAux = QString("Time %1; Frame#= %2. Uncomplete frame...SKIPPED").arg(csTime).arg(i64 + 1);
                printlog(logFile, csAux, isCLI, writeConsole);
            }
            else
            {

// Some consistence checks
//	nubytes=readmpaframe (in, frame);

                syncword=      frame.getBits(12);
                iID=           frame.getBits(1);
                layer=         frame.getBits(2);
                protection_bit=frame.getBits(1);
                rate=          frame.getBits(4);
                irate=rate;
                fsamp=         frame.getBits(2);

                padding_bit=   frame.getBits(1);
                private_bit=   frame.getBits(1);
                mode=          frame.getBits(2);
/*				mode_extension=frame.getBits(2);
                copyright=     frame.getBits(1);
                original=      frame.getBits(1);
                emphasis=      frame.getBits(2);
*/
                unused=        frame.getBits(6);

                if (protection_bit==0)
                    crc_check=frame.getBits(16);

                if		(layer==1) layer=3;
                else if (layer==3) layer=1;

                if (layer==3)
                    rate=layerIIIrate[rate];
                else if (layer==2)
                    rate=layerIIrate[rate];
                else if (layer==1)
                    rate=layerIrate[rate];
                else rate=0;

                if (fsamp==0)	fsamp=44100;
                else if (fsamp==1)	fsamp=48000;
                else if (fsamp==2)	fsamp=32000;
                else fsamp=44100;

                if (fsamp != fileInfo->fsample || layer != fileInfo->layer  || rate != fileInfo->bitrate )
                {

                    fileInfo->fsample=fsamp;
                    fileInfo->layer=layer;
                    fileInfo->bitrate=rate;

                    nuerrors++;
                    csAux = QString("Time %1; Frame#= %2. Some basic parameters changed between Frame #%3 and this frame")
                                    .arg(csTime).arg(i64 + 1).arg(fileInfo->i64frameinfo);
                    printlog(logFile, csAux, isCLI, writeConsole);
                        fileInfo->i64frameinfo=i64+1;
                }

// CRC calculation and fixing.
                if (layer==2 && protection_bit==0)
                {
                    if (mode==3) ratech=rate;
                    else ratech=rate/2;

                    if (fsamp==44100)
                    {
                        if (ratech <=48) {layerIIsub=layerIIsub2c; nbits=42;}
                        else if (ratech<=80) {layerIIsub=layerIIsub2a; nbits=142;}
                            else {layerIIsub=layerIIsub2b; nbits=154;}
                    }
                    else if (fsamp==32000)
                    {
                        if (ratech <=48) {layerIIsub=layerIIsub2d; nbits=62;}
                        else if (ratech<=80) {layerIIsub=layerIIsub2a;nbits=142;}
                        else {layerIIsub=layerIIsub2b;nbits=154;}
                    }
                    else
                    {
                        if (ratech <=48) {layerIIsub=layerIIsub2c; nbits=42;}
                        else { layerIIsub=layerIIsub2a; nbits=142;}
                    }
                    if (mode != 3) nbits*=2;

//	nbits has now the maximum  # of bits to be protected,
//  but the actual number could be lower, different per frame
                    for (i=0;i<layerIIsub[0];i++)
                    {
                        unused=	frame.getBits(4);
                        if (unused==0) nbits-=2;
                        if (mode != 3)
                        {
                            unused=	frame.getBits(4);
                            if (unused==0) nbits-=2;
                        }
                    }
                    for (i=0;i<layerIIsub[1];i++)
                    {
                        unused=	frame.getBits(3);
                        if (unused==0) nbits-=2;
                        if (mode != 3)
                        {
                            unused=	frame.getBits(3);
                            if (unused==0) nbits-=2;
                        }
                    }
                    for (i=0;i<layerIIsub[2];i++)
                    {
                        unused=	frame.getBits(2);
                        if (unused==0) nbits-=2;
                        if (mode != 3)
                        {
                            unused=	frame.getBits(2);
                            if (unused==0) nbits-=2;
                        }
                    }

                    crc_cal1 = ac3_crc(frame + 2, 2 , 0xffff);
//					crc_cal1 = ac3_crc(frame + 6, (int)(i64nubytes-6), crc_cal1);
                    if (6+nbits/8 <= (int)(i64nubytes))
                    {
                        crc_cal1 = ac3_crc(frame + 6, nbits/8, crc_cal1);
                        for (j=0;j<(nbits%8);j++)
                        {
                            crc_cal1 = ac3_crc_bit(frame+6+nbits/8, 7-j, crc_cal1);
                        }
//						csAux.Format(_T("read=%04X ;cal=%04X"), crc_check, crc_cal1);
//						if (AfxMessageBox(csAux, MB_RETRYCANCEL, 0) == IDCANCEL)
//							return 0;
                    }

/*
                    for (i=0; i<(int)(i64nubytes);i++)
                    {
                        for (j=0;j<8;j++)
                        {
                            crc_cal1 = ac3_crc_bit(frame+6+i, 7-j, crc_cal1);
                            if (crc_cal1 == crc_check)
                            {
                                csAux.Format(_T("Found i=%d; j=%d; nbits=%d; nbitsc=%d"),i,j,i*8+j+1,nbits);
                                if (AfxMessageBox(csAux, MB_RETRYCANCEL, 0) == IDCANCEL)
                                    return 0;
                                i=(int)i64nubytes;
                                j=10;
                            }
                        }
                    }
*/
                    if (crc_cal1 != crc_check)
                    {
                        nuerrors++;
                        {
                            csAux = QString("Time %1; Frame#= %2.  Crc error %3: read = %4; calculated=%5")
                                            .arg(csTime).arg(i64 + 1).arg(fixCRC)
                                            .arg(crc_check, 4, 16, QChar('0')).arg(crc_cal1, 4, 16, QChar('0'));
                            printlog(logFile, csAux, isCLI, writeConsole);
                        }
                        if (m_iCrc==CRC_FIX)
                        {
                            frame[4]=(crc_cal1 >> 8);
                            frame[5]=crc_cal1 & 0xff;
                        }
                        else if (m_iCrc==CRC_SKIP) f_writeframe=WF_SKIP;
                        else if (m_iCrc==CRC_SILENCE) f_writeframe=WF_SILENCE;
                    }
                }
            }
        }

//	Write frame
        if (f_writeframe==WF_WRITE)
        {
            i64frameswritten++;
            writempaframe (outputFile, frame);
        }
        else if (f_writeframe==WF_SILENCE)
        {
            i64frameswritten++;
            writempaframe (outputFile, silence);
        }
        if (!tooManyErrors && nuerrors > MAXLOGERRORS)
        {
            printlog(logFile, "Too Many Errors. Stop Logging.", isCLI, writeConsole);
            tooManyErrors=true;
        }

    }
    csAux = QString("Number of written frames = %1").arg(i64frameswritten);
    printlog(logFile, csAux, isCLI, writeConsole);
    csAux = QString("Number of Errors= %1").arg(nuerrors);
    printlog(logFile, csAux, isCLI, writeConsole);
    if (isCLI == false)
        emit UpdateProgress(100);

    emit ProcessingFinished(true, QThread::currentThread()->isInterruptionRequested());
}

void Delayac3::writempaframe (FILE *fileout, const Frame& frame)
{
    quint32 BytesPerFrame;
    quint32 layer, bitrate, rate, fsample, padding_bit;

    layer=(frame[1] & 0x6)>>1;
    if (layer==3) layer=1;
    else if (layer==1) layer=3;
/*
    For Layer I the following equation is valid:
    N = 12 * bit_rate / sampling_frequency.
    For Layers II and III the equation becomes:
    N = 144 * bit_rate / sampling_frequency.
     384000 / 44100
*/
    rate=frame[2]>>4;

    bitrate=0;
    if (layer==3)
        bitrate=layerIIIrate[rate];
    else if (layer==2)
        bitrate=layerIIrate[rate];
    else if (layer==1)
        bitrate=layerIrate[rate];
    bitrate*=1000;

    fsample=(frame[2]>>2) & 0x3;

    if (fsample==0)      fsample=44100;
    else if (fsample==1) fsample=48000;
    else if (fsample==2) fsample=32000;
    else                 fsample=44100;

    padding_bit=(frame[2]>>1) & 0x1;
    if (layer==1)
    {
        BytesPerFrame=(12 * bitrate)/fsample;
        if ( padding_bit) BytesPerFrame++;
    }
    else
    {
        BytesPerFrame=(144*bitrate)/fsample;
        if ( padding_bit) BytesPerFrame++;
    }

    if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

    fwrite(frame.getData(), sizeof (unsigned char), BytesPerFrame, fileout);

    return;
}

quint32 Delayac3::ac3_crc_bit(const uchar *data, qint32 n, quint32 crc)
{
    qint32 bit;

    bit=(data[0] >> n) & 1;

    if (((crc >> 15) & 1 ) ^ bit)
        crc = ((crc << 1) & 0xffff) ^ (CRC16_POLY & 0xffff);
    else
        crc = (crc << 1 ) & 0xffff;

    return crc;
}

void Delayac3::delaywav(FILE* inputFile, FILE* outputFile, FILE* logFile)
{
    qint64 i64, i64StartFrame, i64EndFrame, i64nuframes, i64frameswritten, i64TotalFrames;
//  qreal dFrameduration;
    qint64 i64nubytes;
    qint32 iBytesPerFrame;
    qint32 iInidata;
    QString csAux;
    qint32 f_writeframe, nuerrors;
    qint64 i64Aux;
    Frame frame;

    iInidata=fileInfo->iInidata;
    i64StartFrame=fileInfo->i64StartFrame;
    i64EndFrame=fileInfo->i64EndFrame;
    i64nuframes=i64EndFrame-i64StartFrame+1;
//  dFrameduration=fileInfo->dFrameduration;
    i64TotalFrames=fileInfo->i64TotalFrames;
    iBytesPerFrame=(int)fileInfo->dBytesperframe;

    i64frameswritten=0;
    nuerrors=0;

    printlog (logFile, "====== PROCESSING LOG ======================", isCLI, writeConsole);

// Write wav header
    readwavsample (inputFile, frame, iInidata);

/*	i64Aux=i64nuframes*iBytesPerFrame+iInidata-8;
    frame[4]=(unsigned char)(i64Aux%256);
    frame[5]=(unsigned char)((i64Aux >> 8)%256);
    frame[6]=(unsigned char)((i64Aux >>16)%256);
    frame[7]=(unsigned char)((i64Aux >>24)%256);

    i64Aux=i64nuframes*iBytesPerFrame;
    frame[iInidata-4]=(unsigned char)(i64Aux%256);
    frame[iInidata-3]=(unsigned char)((i64Aux >> 8)%256);
    frame[iInidata-2]=(unsigned char)((i64Aux >>16)%256);
    frame[iInidata-1]=(unsigned char)((i64Aux >>24)%256);
*/
    writewavsample (outputFile, frame, iInidata);

    if (i64StartFrame > 0)
    {
        printlog (logFile, "Seeking....", isCLI, writeConsole);
//		i64Aux=_lseeki64 ( _fileno (m_Fin), i64StartFrame*iBytesPerFrame + iInidata, SEEK_SET);
        fseek ( inputFile,(long)(i64StartFrame*iBytesPerFrame + iInidata), SEEK_SET);
        printlog (logFile, "Done.", isCLI, writeConsole);
        printlog (logFile, "Processing....", isCLI, writeConsole);
    }
    if (i64EndFrame == (fileInfo->i64TotalFrames-1)) i64TotalFrames=i64EndFrame=( (qint64)1<<60);
    bool bEndOfFile = false, tooManyErrors = false;
    for (i64=i64StartFrame; i64< i64EndFrame+1 && !QThread::currentThread()->isInterruptionRequested() && bEndOfFile==false ;i64++)
    {
        if (isCLI == false)
        {
            if ((i64%10)==0)
                emit UpdateProgress((int)(((i64-i64StartFrame)*100)/i64nuframes));
        }
        else
        {
            csAux = QString("Processing %1 %").arg((int)((i64 * 100) / i64nuframes), 2, 10, QChar('0'));
            printline(csAux, writeConsole);
        }

        if(i64 == fileInfo->i64StartSilenceFrame) {
            for(qint64 i64Counter=0; i64Counter < fileInfo->i64LengthSilenceFrame; i64Counter++) {
                i64frameswritten++;
                writewavsample (outputFile, silence, iBytesPerFrame);
            }
        }

        if (i64 < 0 || i64 >= i64TotalFrames)
            f_writeframe=WF_SILENCE;
        else
        {
            f_writeframe=WF_WRITE; //indicates write frame
            i64nubytes=readwavsample (inputFile, frame,iBytesPerFrame);
            if( i64nubytes <1)
            {
                bEndOfFile=true;
                break;
            }
        }
//	Write frame
        if (f_writeframe==WF_WRITE)
        {
            i64frameswritten++;
            writewavsample (outputFile, frame, iBytesPerFrame);
        }
        else if (f_writeframe==WF_SILENCE)
        {
            i64frameswritten++;
            writewavsample (outputFile, silence, iBytesPerFrame);
        }

        if (!tooManyErrors && nuerrors > MAXLOGERRORS)
        {
            printlog(logFile, "Too Many Errors. Stop Logging.", isCLI, writeConsole);
            tooManyErrors=true;
        }

    }

// Rewrite in header length chunk & data subchunk

    fseek ( outputFile,(long)4, SEEK_SET);
    i64Aux=i64frameswritten*iBytesPerFrame+iInidata-8;
    fputc((unsigned char)(i64Aux%256),outputFile);
    fputc((unsigned char)((i64Aux >> 8)%256),outputFile);
    fputc((unsigned char)((i64Aux >>16)%256),outputFile);
    fputc((unsigned char)((i64Aux >>24)%256),outputFile);

    fseek ( outputFile,(long)(iInidata-4), SEEK_SET);
    i64Aux=i64frameswritten*iBytesPerFrame;
    fputc((unsigned char)(i64Aux%256),outputFile);
    fputc((unsigned char)((i64Aux >> 8)%256),outputFile);
    fputc((unsigned char)((i64Aux >>16)%256),outputFile);
    fputc((unsigned char)((i64Aux >>24)%256),outputFile);

    csAux = QString("Number of written samples = %1").arg(i64frameswritten);
    printlog(logFile, csAux, isCLI, writeConsole);
    csAux = QString("Number of Errors= %1").arg(nuerrors);
    printlog(logFile, csAux, isCLI, writeConsole);
    if (isCLI == false)
        emit UpdateProgress(100);

    emit ProcessingFinished(true, QThread::currentThread()->isInterruptionRequested());
}

uint Delayac3::readwavsample(FILE *filein, Frame& frame, uint nubytes)
{
    uchar buff[MAXFRAMESIZE];

    uint nRead=fread(buff, sizeof(unsigned char), nubytes, filein);
    frame.setData(buff, nRead);

    return nRead;
}

void Delayac3::writewavsample(FILE *fileout, const Frame& frame, int nubytes)
{
    if(nubytes > MAXFRAMESIZE)
        nubytes = MAXFRAMESIZE;

    fwrite(frame.getData(),sizeof (unsigned char), nubytes, fileout);

    return;
}

qint32 Delayac3::getwavinfo(FILE* inputFile, FILEINFO *fileInfo)
{
    quint32 BytesPerFrame, i, nextbyte;
    quint32 iByterate, nChannels, nBits, iRate, fsample;
    uchar frame[50];
    uchar mybuffer[20];
#ifndef Q_OS_WIN
    struct stat64 statbuf;
#else
    struct _stati64 statbuf;
#endif
    qreal FrameDuration, FramesPerSecond;
    qint64 nuframes, TimeLengthIni, rest, i64size;
    qreal dactrate;

// search for RIFF */
    for (i=0;!feof(inputFile) && i < 12 ;i++)
        frame[i]=fgetc(inputFile);

    if (frame[0] != 'R' || frame[1] != 'I' ||
        frame[2] != 'F' || frame[3] != 'F' ||
        frame[8] != 'W' || frame[9] != 'A' ||
        frame[10] != 'V' || frame[11] != 'E' ) return -1;

// search for fmt //

    fseek(inputFile, 12, SEEK_SET);
    for (i=0;!feof(inputFile) && i < 4 ;i++)
        mybuffer[i]=fgetc(inputFile);
    nextbyte=16;

    while ((mybuffer[0] != 'f' || mybuffer[1] != 'm' ||
            mybuffer[2] != 't' || mybuffer[3] != ' ') && !feof(inputFile))
    {
            fseek(inputFile, -3L, SEEK_CUR);
                // search for 7FFE8001 */
            for (i=0;!feof(inputFile) && i < 4 ;i++)
                mybuffer[i]=fgetc(inputFile);
            nextbyte++;
    }
    if (mybuffer[0] != 'f' || mybuffer[1] != 'm' ||
            mybuffer[2] != 't' || mybuffer[3] != ' ')  return -1;

    fseek(inputFile, -4L, SEEK_CUR);

    for (i=0;i<24;i++)
        frame[12+i]=fgetc(inputFile);
    nextbyte+=20;

// search for data //

    for (i=0;!feof(inputFile) && i < 4 ;i++)
        mybuffer[i]=fgetc(inputFile);
    nextbyte+=4;

    while ((mybuffer[0] != 'd' || mybuffer[1] != 'a' ||
            mybuffer[2] != 't' || mybuffer[3] != 'a') && !feof(inputFile))
    {
            fseek(inputFile, -3L, SEEK_CUR);
                // search for data */
            for (i=0;!feof(inputFile) && i < 4 ;i++)
                mybuffer[i]=fgetc(inputFile);
            nextbyte++;
    }
    if (mybuffer[0] != 'd' || mybuffer[1] != 'a' ||
            mybuffer[2] != 't' || mybuffer[3] != 'a')  return -1;

    fseek(inputFile, -4L, SEEK_CUR);

    for (i=0;i<8;i++)
        frame[24+12+i]=fgetc(inputFile);

    nextbyte+=4;

#ifndef Q_OS_WIN
    fstat64(fileno(inputFile), &statbuf);
#else
    _fstati64(fileno(inputFile), &statbuf);
#endif

    BytesPerFrame=frame[32]+frame[33]*256;
    i64size=      (quint32)(frame[40]+(frame[41]<<8)+(frame[42]<<16)+(frame[43]<<24));
    if (i64size > statbuf.st_size-nextbyte) i64size= statbuf.st_size-nextbyte;
    nuframes=     i64size/BytesPerFrame;
    rest=         i64size%nuframes;
    fsample=      frame[24]+(frame[25]<<8)+(frame[26]<<16)+(frame[27]<<24);
    iByterate=    frame[28]+(frame[29]<<8)+(frame[30]<<16)+(frame[31]<<24);
    nChannels=    frame[22]+(frame[23]<<8);
    nBits=        frame[34]+(frame[35]<<8);
    iRate=        (quint32)(BytesPerFrame*fsample*8);
    dactrate=     (qreal)iByterate*8;

    FramesPerSecond=fsample;
    FrameDuration=1000.0/FramesPerSecond; // in msecs
    TimeLengthIni=(qint64)((qreal)(statbuf.st_size-nextbyte)*1000.0/(dactrate/8)); // in msecs
    fileInfo->csOriginalDuration = QString("%1:%2:%3.%4")
                                    .arg(TimeLengthIni / 3600000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 3600000) / 60000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 60000) / 1000, 2, 10, QChar('0'))
                                    .arg(TimeLengthIni % 1000, 3, 10, QChar('0'));
    fileInfo->fsample=(int)fsample;
    fileInfo->layer=0;
    fileInfo->type="wav";
    fileInfo->dFrameduration=FrameDuration;
    fileInfo->i64Filesize=statbuf.st_size;
    fileInfo->iInidata=nextbyte;
    fileInfo->i64TotalFrames=nuframes;
    fileInfo->bitrate=iRate;
    fileInfo->iBits=nBits;
    fileInfo->byterate=iByterate;
    fileInfo->dactrate=dactrate;
    fileInfo->dBytesperframe=(double)BytesPerFrame;
    fileInfo->csMode = QString("%1 Channels").arg(nChannels);
    fileInfo->dFramesPerSecond=FramesPerSecond;
    fileInfo->i64rest=rest;
    fileInfo->fscod=0;
    fileInfo->acmod=0;
    fileInfo->cpf=0;
    fileInfo->i64frameinfo=0;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
    for (i=0; i<BytesPerFrame; i++)
        silence[i]=0;

    return 0;
}

////////////////////////////////////////////////////////////////////////
//      readac3frame
////////////////////////////////////////////////////////////////////////

uint Delayac3::readac3frame(FILE *filein, Frame& frame)
{
    uchar buff[MAXFRAMESIZE];
    uint i;

    //p_bit=0;
    for (i=0; !feof(filein) && i < NUMREAD ; i++)
    {
        buff[i]=fgetc(filein);
    }

    uint nNumRead=i;

    if (feof(filein))
        return i;

//	uint frmsizecod=buff[4] & 0x3F;
    uint frmsizecod=buff[4];
    uint BytesPerFrame=FrameSize_ac3[frmsizecod]*2;

    if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

    uint nRead=fread(&buff[nNumRead], sizeof(unsigned char), BytesPerFrame-nNumRead, filein);
    frame.setData(buff, nRead+nNumRead);

    return nRead+nNumRead;
}

////////////////////////////////////////////////////////////////////////
//      ac3_crc: Calculates the crc (byte per byte)
////////////////////////////////////////////////////////////////////////
quint32 Delayac3::ac3_crc(const uchar *data, qint32 n, quint32 crc)
{
    int i;
    // loop in bytes
    for(i=0;i<n;i++)
    {
        crc = (::crcTable[(data[i] & 0xff)^ (crc >> 8)] ^ (crc << 8)) & 0xffff;
    }
    return crc;
}

qint32 Delayac3::getdtsinfo(FILE* inputFile, FILEINFO *fileInfo)
{
    quint32 nubytes, BytesPerFrame, i;
    Frame frame;
    quint32 cpf;
    quint32 fsize, amode, sfreq, rate, lfeon;
    quint32 ftype, fshort, nblks, unused, fsample;
    quint32 syncword;
//    qreal fsample;

#ifndef Q_OS_WIN
    struct stat64 statbuf;
#else
    struct _stati64 statbuf;
#endif
    qreal FrameDuration, FramesPerSecond;
    qint64 nuframes, TimeLengthIni, rest;
    qreal dactrate;

//    p_silence=dts_768k_48;  asigned after

    // search for 7FFE8001 */
    for (i=0;!feof(inputFile) && i < 4 ;i++)
        frame[i]=fgetc(inputFile);

    while ((frame[0] != 0x7F || frame[1] != 0xFE ||
            frame[2] != 0x80 || frame[3] != 0x01) && !feof(inputFile))
    {
            fseek(inputFile, -3L, SEEK_CUR);
                // search for 7FFE8001 */
            for (i=0;!feof(inputFile) && i < 4 ;i++)
                    frame[i]=fgetc(inputFile);

    }

    if (frame[0] != 0x7F || frame[1] != 0xFE ||
        frame[2] != 0x80 || frame[3] != 0x01) return -1;

    fseek(inputFile, -4L, SEEK_CUR);

    nubytes=readdtsframe (inputFile, frame);

    syncword=frame.getBits(32);
    ftype=   frame.getBits(1);
    fshort=  frame.getBits(5);
    cpf=     frame.getBits(1);
    nblks=   frame.getBits(7);
    fsize=   frame.getBits(14);
    amode=   frame.getBits(6);
    sfreq=   frame.getBits(4);
    rate=    frame.getBits(5);
    unused=  frame.getBits(10);
    lfeon=   frame.getBits(1);

    BytesPerFrame=fsize+1;
    rate=dtsbitrate[rate];
//  dactrate=(qreal)rate;
//  if (rate==768)   dactrate=754.5;
//  if (rate== 1536) dactrate=1509.75;
//  fsample=((qreal)(dtsfsample[sfreq]))/1000.0;
    fsample=dtsfsample[sfreq];
//  Real bitrate in Kb/s is: (SampleRate * BytesPerFrame * 8 / SamplesPerFrame) / 1000
//  Where SamplesPerFrame = 32 * (nblks+1). CHECK if op are correct
    dactrate=((qreal)(fsample*BytesPerFrame))/(4000.0*(nblks+1));

#ifndef Q_OS_WIN
    fstat64(fileno(inputFile), &statbuf);
#else
    _fstati64(fileno(inputFile), &statbuf);
#endif

    nuframes=       statbuf.st_size/BytesPerFrame;
    rest=           statbuf.st_size%nuframes;
    FramesPerSecond=(dactrate)*1000.0/(BytesPerFrame * 8);
    FrameDuration=  1000.0/FramesPerSecond; // in msecs
    TimeLengthIni=  (qint64)((qreal)(statbuf.st_size)/(dactrate/8)); // in msecs

    fileInfo->csOriginalDuration = QString("%1:%2:%3.%4")
                                    .arg(TimeLengthIni / 3600000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 3600000) / 60000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 60000) / 1000, 2, 10, QChar('0'))
                                    .arg(TimeLengthIni % 1000, 3, 10, QChar('0'));
//    fileInfo->fsample=48000;
    fileInfo->fsample=fsample;
    fileInfo->layer=0;
    fileInfo->type="dts";
    fileInfo->dFrameduration=FrameDuration;
    fileInfo->i64Filesize=statbuf.st_size;
    fileInfo->i64TotalFrames=nuframes;
    fileInfo->bitrate=rate;
    fileInfo->dactrate=dactrate;
    fileInfo->dBytesperframe=(double)BytesPerFrame;
    if		(amode==0)  fileInfo->csMode="Mono";
    else if (amode==1)  fileInfo->csMode="A+B (Dual Mono)";
    else if (amode==2)  fileInfo->csMode="L+R (Stereo)";
    else if (amode==3)  fileInfo->csMode="(L+R) + (L-R): (Sum + Diff)";
    else if (amode==4)  fileInfo->csMode="LT + RT ";
    else if (amode==5)  fileInfo->csMode="C+L+R";
    else if (amode==6)  fileInfo->csMode="L+R+S";
    else if (amode==7)  fileInfo->csMode="C+L+R+S";
    else if (amode==8)  fileInfo->csMode="L+R+SL+SR";
    else if (amode==9)  fileInfo->csMode="C+L+R+SL+SR";
    else if (amode==10) fileInfo->csMode="CL+CR+L+R+SL+SR";
    else if (amode==11) fileInfo->csMode="C+L+R+LR+RR+OV";
    else if (amode==12) fileInfo->csMode="CF+CR+LF+RF+LR+RR";
    else if (amode==13) fileInfo->csMode="CL+C+CR+L+R+SL+SR";
    else if (amode==14) fileInfo->csMode="CL+CR+L+R+SL1+SL2+SR1+SR2";
    else if (amode==15) fileInfo->csMode="CL+C+CR+L+R+SL+S+SR";
    else                fileInfo->csMode="User defined";
    if (lfeon)          fileInfo->csLFE="LFE: Present";
    else                fileInfo->csLFE="LFE: Not present";
    fileInfo->dFramesPerSecond=FramesPerSecond;
    fileInfo->i64rest=rest;
    fileInfo->fscod=sfreq;
    fileInfo->acmod=amode;
    fileInfo->cpf=cpf;
    fileInfo->i64frameinfo=1;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
    for (i=0; i<nubytes; i++)
        silence[i]=frame[i];

    if (rate == 768 && nubytes ==1006)
        silence.setData(dts_768k_48, sizeof(dts_768k_48));
    if (rate == 1536 && nubytes ==2013)
        silence.setData(dts_1536k_48, sizeof(dts_1536k_48));

    return 0;
}

uint Delayac3::readdtsframe (FILE *filein, Frame& frame)
{
    uint i;
    uchar buff[MAXFRAMESIZE];

    // search for 7FFE8001 */
    for (i=0; !feof(filein) && i < 4; i++)
    {
        buff[i]=fgetc(filein);
    }

    uint nNumRead=i;

    if (feof(filein))
        return i;

/*	while (buff[0] != 0x7F || buff[1] != 0xFE ||
            buff[2] != 0x80 || buff[3] != 0x01)
    {
            fseek(filein, -3L, SEEK_CUR);
                // search for 7FFE8001
            for (i=0;!feof(filein) && i < 4 ;i++)
                    buff[i]=fgetc(filein);
    }
*/
    for (i=4; !feof(filein) && i < 8; i++)
    {
        buff[i]=fgetc(filein);
    }

    nNumRead=i;

    if (feof(filein))
        return i;

    uint BytesPerFrame=((buff[5]&0x3)*256 + buff[6])*16 + (buff[7]&0xF0)/16 +1;

    if (BytesPerFrame > MAXFRAMESIZE)
        BytesPerFrame = MAXFRAMESIZE;

    uint nRead=fread(buff+nNumRead, sizeof(unsigned char), BytesPerFrame-nNumRead, filein);
    frame.setData(buff, nRead+nNumRead);

    return nRead+nNumRead;
}

qint32 Delayac3::getmpainfo(FILE* inputFile, FILEINFO *fileInfo)
{
    quint32 nubytes, i;
    Frame frame;
    quint32 rate, fsamp, layer, protection_bit;
    quint32 mode = 0;
    quint32 syncword, iID, padding_bit, private_bit;
//  quint32 mode_extension, copyright, original, emphasis;
//  quint32 crc_check;
#ifndef Q_OS_WIN
    struct stat64 statbuf;
#else
    struct _stati64 statbuf;
#endif
//  qreal fsample;
    qreal FrameDuration, FramesPerSecond, BytesPerFrame;
    qint64 nuframes, TimeLengthIni, rest;
//	double dactrate;
/*
    syncword			12	bits	bslbf
    ID					1	bit		bslbf
    layer				2	bits	bslbf
    protection_bit		1	bit		bslbf
    bitrate_index		4	bits	bslbf
    sampling_frequency	2	bits	bslbf
    padding_bit			1	bit		bslbf
    private_bit			1	bit		bslbf
    mode				2	bits	bslbf
    mode_extension		2	bits	bslbf
    copyright			1	bit		bslbf
    original/home		1	bit		bslbf
    emphasis			2	bits	bslbf
    if  (protection_bit==0)
         crc_check 	 16	bits	rpchof
*/
    // search for FFFX */
    for (i=0;!feof(inputFile) && i < 2 ;i++)
        frame[i]=fgetc(inputFile);

    while (frame[0] != 0xFF || ((frame[1] & 0xF0) != 0xF0 && !feof(inputFile)))
    {
            fseek(inputFile, -1L, SEEK_CUR);
                // search for 0xFFFX */
            for (i=0;!feof(inputFile) && i < 2 ;i++)
                    frame[i]=fgetc(inputFile);

    }

    if (frame[0] != 0xFF || (frame[1] & 0xF0 ) != 0xF0 ) return -1;

    fseek(inputFile, -2L, SEEK_CUR);

    nubytes=readmpaframe (inputFile, frame);

    syncword=      frame.getBits(12);
    iID=           frame.getBits(1);
    layer=         frame.getBits(2);
    protection_bit=frame.getBits(1);
    rate=          frame.getBits(4);
    fsamp=         frame.getBits(2);
    padding_bit=   frame.getBits(1);
    private_bit=   frame.getBits(1);
    mode=          frame.getBits(2);
/*  mode_extension=frame.getBits(2);
    copyright=     frame.getBits(1);
    original=      frame.getBits(1);
    emphasis=      frame.getBits(2);
*/

//	MpegVers= ((syncword & 0x1)<<1) + iID;
// FFFC--> MPeg 1
// FFF0--> MPeg 2
// FFE0--> MPeg 2.5
//      Case 0
//        AddInfo("2.5" + CR$)
//      Case 1
//        AddInfo("reserved" + CR$)
//      Case 2
//        AddInfo("2" + CR$)
//      Case 3
//        AddInfo("1" + CR$)

//    if (protection_bit==0)
//        crc_check=frame.getBits(16);

    if	    (layer==1) layer=3;
    else if (layer==3) layer=1;

    if      (layer==3) rate=layerIIIrate[rate];
    else if (layer==2) rate=layerIIrate[rate];
    else if (layer==1) rate=layerIrate[rate];
    else               rate=0;

    if (rate<8) rate=8;

    if      (fsamp==0) fsamp=44100;
    else if (fsamp==1) fsamp=48000;
    else if (fsamp==2) fsamp=32000;
    else               fsamp=44100;

//  fsample=(double)fsamp;

    if (layer==1)
    {
        BytesPerFrame=(12 * rate * 1000.0)/(double)fsamp;
//		if ( padding_bit) BytesPerFrame+=1;
    }
    else
    {
        BytesPerFrame=(144 * 1000.0 * rate)/(double)fsamp;
//		if ( padding_bit) BytesPerFrame++;
    }
    if (BytesPerFrame > MAXFRAMESIZE) BytesPerFrame = MAXFRAMESIZE;

#ifndef Q_OS_WIN
    fstat64(fileno(inputFile), &statbuf);
#else
    _fstati64(fileno(inputFile), &statbuf);
#endif

    nuframes=       (qint64) (statbuf.st_size/BytesPerFrame +0.5);
    rest=           statbuf.st_size%nuframes;
    FramesPerSecond=((double)(rate))*1000.0/(BytesPerFrame * 8);
    FrameDuration=  1000.0/FramesPerSecond; // in msecs
    TimeLengthIni=  statbuf.st_size/(rate/8); // in msecs

    fileInfo->csOriginalDuration = QString("%1:%2:%3.%4")
                                    .arg(TimeLengthIni / 3600000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 3600000) / 60000, 2, 10, QChar('0'))
                                    .arg((TimeLengthIni % 60000) / 1000, 2, 10, QChar('0'))
                                    .arg(TimeLengthIni % 1000, 3, 10, QChar('0'));
    fileInfo->layer=layer;
    fileInfo->fsample=fsamp;
    fileInfo->type = QString("mpeg 1 layer %1").arg(layer);
    fileInfo->dFrameduration=FrameDuration;
    fileInfo->i64Filesize=statbuf.st_size;
    fileInfo->i64TotalFrames=nuframes;
    fileInfo->bitrate=rate;
    fileInfo->dactrate=rate;
    fileInfo->dBytesperframe=BytesPerFrame;
    fileInfo->mode=mode;
    if      (mode==0) fileInfo->csMode="Stereo";
    else if (mode==1) fileInfo->csMode="Joint Stereo";
    else if (mode==2) fileInfo->csMode="Dual Channel (Dual Mono)";
    else if (mode==3) fileInfo->csMode="Single Channel (Mono)";
    fileInfo->csLFE="LFE: Not present";
    fileInfo->dFramesPerSecond=FramesPerSecond;
    fileInfo->i64rest=rest;
//	fileInfo->fscod=sfreq;
//	fileInfo->acmod=amode;
    if (protection_bit==0)
        fileInfo->cpf=1;
    else
        fileInfo->cpf=0;

//	fileInfo->i64frameinfo=1;


/////////////////////////////////////////////////////////////////////////////
// Fill silence frame, If bitrate is not included, use first frame
/////////////////////////////////////////////////////////////////////////////
    for (i=0; i<nubytes; i++)
        silence[i]=frame[i];

    if (layer==2 && fsamp==48000)
    {
        if (mode!=3) // Stereo (any mode)
        {
            if      (rate==32 ) silence.setData(mp2_32k_s_48 , sizeof(mp2_32k_s_48));
            else if (rate==48 ) silence.setData(mp2_48k_s_48 , sizeof(mp2_48k_s_48));
            else if (rate==56 ) silence.setData(mp2_56k_s_48 , sizeof(mp2_56k_s_48));
            else if (rate==64 ) silence.setData(mp2_64k_s_48 , sizeof(mp2_64k_s_48));
            else if (rate==80 ) silence.setData(mp2_80k_s_48 , sizeof(mp2_80k_s_48));
            else if (rate==96 ) silence.setData(mp2_96k_s_48 , sizeof(mp2_96k_s_48));
            else if (rate==112) silence.setData(mp2_112k_s_48, sizeof(mp2_112k_s_48));
            else if (rate==128) silence.setData(mp2_128k_s_48, sizeof(mp2_128k_s_48));
            else if (rate==160) silence.setData(mp2_160k_s_48, sizeof(mp2_160k_s_48));
            else if (rate==192) silence.setData(mp2_192k_s_48, sizeof(mp2_192k_s_48));
            else if (rate==224) silence.setData(mp2_224k_s_48, sizeof(mp2_224k_s_48));
            else if (rate==256) silence.setData(mp2_256k_s_48, sizeof(mp2_256k_s_48));
            else if (rate==320) silence.setData(mp2_320k_s_48, sizeof(mp2_320k_s_48));
            else if (rate==384) silence.setData(mp2_384k_s_48, sizeof(mp2_384k_s_48));

        }
        if (mode==3) // Mono
        {
            if      (rate==32 ) silence.setData(mp2_32k_m_48, sizeof(mp2_32k_m_48));
            else if (rate==48 ) silence.setData(mp2_48k_m_48, sizeof(mp2_48k_m_48));
            else if (rate==56 ) silence.setData(mp2_56k_m_48, sizeof(mp2_56k_m_48));
            else if (rate==64 ) silence.setData(mp2_64k_m_48, sizeof(mp2_64k_m_48));
            else if (rate==80 ) silence.setData(mp2_80k_m_48, sizeof(mp2_80k_m_48));
            else if (rate==96 ) silence.setData(mp2_96k_m_48, sizeof(mp2_96k_m_48));
            else if (rate==112) silence.setData(mp2_112k_m_48, sizeof(mp2_112k_m_48));
            else if (rate==128) silence.setData(mp2_128k_m_48, sizeof(mp2_128k_m_48));
            else if (rate==160) silence.setData(mp2_160k_m_48, sizeof(mp2_160k_m_48));
            else if (rate==192) silence.setData(mp2_192k_m_48, sizeof(mp2_192k_m_48));
            else if (rate==224) silence.setData(mp2_224k_m_48, sizeof(mp2_224k_m_48));
            else if (rate==256) silence.setData(mp2_256k_m_48, sizeof(mp2_256k_m_48));
            else if (rate==320) silence.setData(mp2_320k_m_48, sizeof(mp2_320k_m_48));
            else if (rate==384) silence.setData(mp2_384k_m_48, sizeof(mp2_384k_m_48));

        }
    }

    return 0;
}

uint Delayac3::readmpaframe (FILE *filein, Frame& frame)
{
    uint i;
    uchar buff[MAXFRAMESIZE];

    // search for FFF */
    for (i=0;!feof(filein) && i < 2 ;i++)
    {
        buff[i]=fgetc(filein);
    }

/*	while (buff[0] != 0xFF || (buff[1] & 0xF0 )!= 0xF0 )
    {
            fseek(filein, -1L, SEEK_CUR);
                // search for FFF
            for (i=0;!feof(filein) && i < 2 ;i++)
                    buff[i]=fgetc(filein);
    }
*/
    for (i=2;!feof(filein) && i < 4 ;i++)
    {
        buff[i]=fgetc(filein);
    }

    uint nNumRead=i;

    if (feof(filein))
        return i;

    uint layer=(buff[1] & 0x6)>>1;
    if (layer==3) layer=1;
    else if (layer==1) layer=3;
/*
    For Layer I the following equation is valid:
    N = 12 * bit_rate / sampling_frequency.
    For Layers II and III the equation becomes:
    N = 144 * bit_rate / sampling_frequency.
     384000 / 44100
*/
    uint rateidx=buff[2]>>4;

    uint bitrate=0;
    if (layer==3)
        bitrate=layerIIIrate[rateidx];
    else if (layer==2)
        bitrate=layerIIrate[rateidx];
    else if (layer==1)
        bitrate=layerIrate[rateidx];
    bitrate*=1000;

    uint fsample=(buff[2]>>2) & 0x3;

    if (fsample==0)      fsample=44100;
    else if (fsample==1) fsample=48000;
    else if (fsample==2) fsample=32000;
    else                 fsample=44100;

    uint padding_bit=(buff[2]>>1) & 0x1;

    uint BytesPerFrame;
    if (layer==1)
    {
        BytesPerFrame=(12 * bitrate)/fsample;
        if ( padding_bit) BytesPerFrame++;
    }
    else
    {
        BytesPerFrame=(144*bitrate)/fsample;
        if ( padding_bit) BytesPerFrame++;
    }

    if (BytesPerFrame > MAXFRAMESIZE)
        BytesPerFrame = MAXFRAMESIZE;

    uint nRead=0;
    if (BytesPerFrame==0)
        BytesPerFrame=4;
    else
        nRead=fread(&buff[nNumRead], sizeof(unsigned char), BytesPerFrame-nNumRead, filein);
    frame.setData(buff, nRead+nNumRead);

    return nRead+nNumRead;
}

qint32 Delayac3::gettargetinfo(FILEINFO *fileInfo, qreal startDelay, qreal endDelay, qreal startCut, qreal endCut, qreal startSilence, qreal lengthSilence)
{
    qreal frameDuration, notFixedDelay;
    qint64 startFrame, endFrame;
    qint64 numberOfFrames, estimatedTimeLength;

    frameDuration = fileInfo->dFrameduration;
    numberOfFrames = fileInfo->i64TotalFrames;

    /////////////////////////////////////////////////////////
    // Translate split and delay parameters to number of frames
    /////////////////////////////////////////////////////////
    if (startCut > 0)
    {
        startFrame = -round((startDelay - round(startCut * 1000.0)) / frameDuration);
    }
    else
    {
        startFrame= -round(startDelay / frameDuration);
    }

    fileInfo->i64StartFrame = startFrame;

    if (endCut > 0)
    {
        endFrame = round((-endDelay + round(endCut * 1000.0)) / frameDuration);
        if (endFrame > startFrame) endFrame = endFrame - 1;
    }
    else // endsplit = 0 go until the end of the file
    {
        endFrame = round((-endDelay + ((numberOfFrames - 1) * frameDuration)) / frameDuration);
    }

    fileInfo->i64EndFrame = endFrame;
   
    fileInfo->i64StartSilenceFrame = round(startSilence / frameDuration);
    fileInfo->i64LengthSilenceFrame = round(lengthSilence / frameDuration);

    estimatedTimeLength = (qint64)((endFrame - startFrame + fileInfo->i64LengthSilenceFrame + 1) * frameDuration);

    notFixedDelay = (startDelay + (startFrame * frameDuration)) - (startCut * 1000.0);

    fileInfo->csTimeLengthEst = QString("%1:%2:%3.%4")
                                .arg(estimatedTimeLength / (3600000), 2, 10, QChar('0'))
                                .arg((estimatedTimeLength % 3600000) / 60000, 2, 10, QChar('0'))
                                .arg((estimatedTimeLength % 60000) / 1000, 2, 10, QChar('0'))
                                .arg(estimatedTimeLength % 1000, 3, 10, QChar('0'));

    fileInfo->dNotFixedDelay = notFixedDelay;

    return 0;
}

////////////////////////////////////////////////////////////////////////
//      mul_poply and pow_poly: Operations with polynomials
////////////////////////////////////////////////////////////////////////
quint32 Delayac3::mul_poly(quint32 a, quint32 b, quint32 poly)
{
    quint32 c;

    c = 0;
    while (a) {
        if (a & 1)
            c ^= b;
        a = a >> 1;
        b = b << 1;
        if (b & (1 << 16))
            b ^= poly;
    }
    return c;
}

quint32 Delayac3::pow_poly(quint32 a, quint32 n, quint32 poly)
{
    quint32 r;
    r = 1;
    while (n) {
        if (n & 1)
            r = mul_poly(r, a, poly);
        a = mul_poly(a, a, poly);
        n >>= 1;
    }
    return r;
}

QString Delayac3::compute_time_string(qint64 i64, qreal dFrameduration)
{
    return QString("%1:%2:%3.%4")
        .arg((int) ((dFrameduration * i64) / 3600000), 2, 10, QChar('0'))
        .arg((((int) (dFrameduration * i64)) % 3600000) / 60000, 2, 10, QChar('0'))
        .arg((((int) (dFrameduration * i64)) % 60000) / 1000, 2, 10, QChar('0'))
        .arg((((int) (dFrameduration * i64)) % 1000), 3, 10, QChar('0'));
}

void Delayac3::printline(QString csLinea, bool writeConsole)
{
    static QString csLineaold;

    if (csLinea==csLineaold) return;

    csLineaold=csLinea;

    if (writeConsole)
        fprintf(stdout, "%s", QString(csLinea+"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b").toUtf8().constData());
}

void Delayac3::printlog(FILE *logFile, QString csLinea, bool isCLI, bool writeConsole)
{
    QString csAux;

    fprintf(logFile, "%s\n", csLinea.toUtf8().constData());

    if (isCLI)
    {
        if (writeConsole)
            fprintf(stdout, "%s\n", csLinea.toUtf8().constData());
    }
}

qint64 Delayac3::round(double value)
{
    double intpart;
    double decimal = modf(fabs(value), &intpart);

    if ((value < 0 && decimal < .5) || (value > 0 && decimal >= .5))
    {
        return (qint64) ceil(value);
    }
    else
    {
        return (qint64) floor(value);
    }
}
