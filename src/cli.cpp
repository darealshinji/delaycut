/*  Copyright (C) 2004 by jsoto
    2007-09-11 E-AC3 support added by www.madshi.net
    2009 Chumbo mod to autofill delay values from filename
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

#include <wchar.h>
#include "cli.h"
#include "delayac3.h"
#include "dc_types.h"

#include <cstdio>
#include <cinttypes>

#ifdef Q_OS_WIN
#include "windows.h"
#include "io.h"
#include "fcntl.h"
#endif

void printHelp()
{
    fprintf(stdout, "%s\n", "Command Line Instructions");
    fprintf(stdout, "%s\n", "Output and log files will be in the same path than the input file.");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Options:");
    fprintf(stdout, "%s\n", "-help:                 List options.");
    fprintf(stdout, "%s\n", "-version:              Get current version.");
    fprintf(stdout, "%s\n", "-info:                 Outputs info about input file in log file");
    fprintf(stdout, "%s\n", "-inputtype <string>:   Input type of delay/cut values. (default millseconds");
    fprintf(stdout, "%s\n", "                       when not specified) [milliseconds, seconds, videoframes]");
    fprintf(stdout, "%s\n", "-fps <float|rational>: Specify frame rate.");
    fprintf(stdout, "%s\n", "                       Needed when inputtype is set to frames.");
    fprintf(stdout, "%s\n", "-fixcrc <string>:      Specify action to take in the case of crc errors");
    fprintf(stdout, "%s\n", "                       [ignore, skip, fix, silence]");
    fprintf(stdout, "%s\n", "-startdelay <integer>: Specify the needed frames added at the beginning of the file");
    fprintf(stdout, "%s\n", "-delay <integer>:      Alias for -startdelay");
    fprintf(stdout, "%s\n", "-enddelay <integer>:   Specify the needed frames added at the end of the file");
    fprintf(stdout, "%s\n", "-same:                 file length will be the same after adding delay");
    fprintf(stdout, "%s\n", "-auto:                 detect start delay in filename (assuming DVD2AVI style)");
    fprintf(stdout, "%s\n", "-startcut <integer>:   Specify cut start point");
    fprintf(stdout, "%s\n", "-endcut <integer>:     Specify cut end point");
    fprintf(stdout, "%s\n", "-o <string>:           specify output file");
    fprintf(stdout, "%s\n", "-i <string>:           specify inputfile");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Examples:");
    fprintf(stdout, "%s\n", "Get info: Log file will be myfile_log.txt");
    fprintf(stdout, "%s\n", "    delaycut -info -i myfile.ac3");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Adds 100 msec of silence at the begining. File lenght will be the same");
    fprintf(stdout, "%s\n", "    delaycut -startdelay 100 -same -i myfile.ac3");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Adds 100 msec of silence at the begining. File lenght will be 100 msecs more");
    fprintf(stdout, "%s\n", "    delaycut -startdelay 100 -i myfile.ac3");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Cuts start at 10.32 sec and ends at 15.20 sec.");
    fprintf(stdout, "%s\n", "    delaycut -inputtype seconds -startcut 10.32 -endcut 15.20 -i myfile.ac3");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Cuts start at 10320 msec and ends at 15200 msec. Delay correction of 100 msec.");
    fprintf(stdout, "%s\n", "    delaycut -delay 100 -startcut 10320 -endcut 15200 -i myfile.ac3");
    fprintf(stdout, "%s\n", "");
    fprintf(stdout, "%s\n", "Automatic delay correction based on the filename (-500 msec in this case)");
    fprintf(stdout, "%s\n", "    delaycut -auto \"myfile DELAY -500ms.ac3\"");
}

int main(int argc, char *argv[])
{
    QStringList args;

    for (int i = 0; i <= argc; ++i)
    {
        args << argv[i];
    }

#ifdef Q_OS_WIN
/* Redirect_console() */
    int hCrtout,hCrterr;
    FILE *hfout, *hferr;

    AllocConsole();

    hCrtout=_open_osfhandle( (intptr_t) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT );
    hfout=_fdopen(hCrtout, "w");
    *stdout= *hfout;
    setvbuf(stdout,NULL, _IONBF,0);

    hCrterr=_open_osfhandle( (intptr_t) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT );
    hferr=_fdopen(hCrterr, "w");
    *stderr= *hferr;
    setvbuf(stderr,NULL, _IONBF,0);
#endif

    if (args.contains("-help") || args.contains("--help"))
    {
        printHelp();
        return 0;
    }
    if (args.contains("-version") || args.contains("--version"))
    {
        fprintf(stdout, VERSION "\n");
        return 0;
    }

    writeConsole = true;
    isCLI = true;
    QString parameter;
    bool printInfoOnly = false, isSame = false, isAuto = false;

    fps = 29997/1001;

    startDelay = endDelay = startCut = endCut = 0;
    for (int i = 1; i < argc; ++i)
    {
        parameter = args.at(i).toLower();

        if (parameter == "-i" && i < (argc - 1))
        {
            i++;
            inFileName = QFileInfo(args.at(i)).absoluteFilePath();
            if (inFileName.isEmpty() || inFileName.isNull())
            {
                fprintf(stderr, "No valid input file specified.\n");
                return 1;
            }
            if (!QFile(inFileName).exists())
            {
                fprintf(stderr, "No valid input file specified.\n");
                return 1;
            }
        }
        else if (parameter == "-auto")
        {
            isAuto = true;
        }
        else if (parameter == "-info")
        {
            printInfoOnly = true;
        }
        else if (parameter == "-fps" && i < (argc - 1))
        {
            i++;
            int pos = 0;
            QString fpsParam = args.at(i);
            if (fpsValidator->validate(fpsParam, pos) == QValidator::Invalid)
            {
                fprintf(stderr, "No valid fps value specified.\n");
                return 1;
            }
            if (fpsParam.contains("/"))
            {
                QStringList fpsList = fpsParam.split('/', QString::SkipEmptyParts);
                if (fpsList.count() == 2 && fpsList.at(1) != "0")
                {
                    fps = (fpsList.at(0).toDouble() / fpsList.at(1).toDouble());
                }
                else
                {
                    fprintf(stderr, "No valid fps value specified.\n");
                    return 1;
                }
            }
            else
            {
                if (sscanf(fpsParam.toUtf8().constData(), "%lf", &fps) != 1)
                {
                    fprintf(stderr, "No valid fps value specified.\n");
                    return 1;
                }
            }
        }
        else if (parameter == "-inputtype" && i < (argc - 1))
        {
            ++i;
            currentInputMode = args.at(i).toLower();

            if (currentInputMode != "milliseconds" && currentInputMode != "seconds" && currentInputMode != "videoframes" && currentInputMode != "audioframes")
            {
                fprintf(stderr, "No valid input type specified.\n");
                return 1;
            }
            else
            {
                printf("Current input method = %s\n", currentInputMode.toUtf8().constData());
            }
        }
        else if (parameter == "-same")
        {
            isSame = true;
        }
        else if ((parameter == "-startdelay" || parameter == "-delay") && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toUtf8().constData(), "%lf", &startDelay) != 1)
            {
                fprintf(stderr, "No valid start delay value specified.\n");
                return 1;
            }
        }
        else if (parameter == "-enddelay" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toUtf8().constData(), "%lf", &endDelay) != 1)
            {
                fprintf(stderr, "No valid end delay value specified.\n");
                return 1;
            }
        }
        else if (parameter == "-startcut" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toUtf8().constData(), "%lf", &startCut) != 1)
            {
                fprintf(stderr, "No valid start cut value specified.\n");
                return 1;
            }
            isCut = true;
        }
        else if (parameter == "-endcut" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toUtf8().constData(), "%lf", &endCut) != 1)
            {
                fprintf(stderr, "No valid end cut value specified.\n");
                return 1;
            }
            isCut = true;
        }
        else if (parameter == "-fixcrc" && i < (argc - 1))
        {
            i++;
            QString crcOption = args.at(i).toLower();

            if (crcOption == "ignore")
            {
                crcMode = "IGNORED";
            }
            else if (crcOption == "fix")
            {
                crcMode = "FIXED";
            }
            else if (crcOption == "skip")
            {
                crcMode = "SKIPPED";
            }
            else if (crcOption == "silence")
            {
                crcMode = "SILENCED";
            }
            else
            {
                fprintf(stderr, "No valid fixcrc option specified.\n");
                return 1;
            }
        }
        else if (parameter == "-o" && i < (argc - 1))
        {
            i++;
            outFileName = QFileInfo(args.at(i)).absoluteFilePath();
            if (outFileName.isEmpty() || outFileName.isNull())
            {
                fprintf(stderr, "No valid output file specified.\n");
                return 1;
            }
        }
        else
        {
            if (parameter.at(0) == '-')
            {
                fprintf(stderr, "Unrecognized switch: %s.\n", parameter.toStdString().c_str());
                return 1;
            }

            if (i == (argc - 1) && QFile(parameter).exists())
            {
                inFileName = QFileInfo(parameter).absoluteFilePath();;
            }
            else if (i == (argc - 1) && !QFile(parameter).exists())
            {
                fprintf(stderr, "No valid input file specified.\n");
                return 1;
            }
        }
    }

    if (inFileName.isEmpty())
    {
        fprintf(stderr, "No valid input file specified.\n");
        return 1;
    }

    if (isSame)
    {
        endDelay = startDelay;
    }

    if (startCut != 0 || endCut != 0)
    {
        if (startCut < 0)
        {
            return 1;
        }

        if (endCut < 0)
        {
            return 1;
        }

        if (endCut != 0 && (endCut < startCut))
        {
            return 1;
        }
    }

    if (outFileName.isEmpty() || outFileName.isNull())
    {
        QString extension = QFileInfo(inFileName).suffix();
        outFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_fixed." + extension;
    }

    GetInputFileInfo();

    if (isAuto == true)
    {
        QString fileNameWithoutPath = QFileInfo(inFileName).fileName().toLower();
        qreal delay;

        if (fileNameWithoutPath.contains(QString("delay")))
        {
            qint32 startIndex = fileNameWithoutPath.toLower().indexOf("delay") + 6;

            if (startIndex != -1)
            {
                qint32 endIndex = fileNameWithoutPath.toLower().indexOf("ms", startIndex);
                delay = fileNameWithoutPath.mid(startIndex, endIndex - startIndex).toDouble();

                if (currentInputMode == "seconds")
                {
                    startDelay = delay / 1000.0;
                }
                else if (currentInputMode == "videoframes")
                {
                    startDelay = delayac3->round((delay / 1000.0) * fps);
                }
                else if (currentInputMode == "audioframes")
                {
                    startDelay = delayac3->round(delay / fileInfo->dFrameduration);
                }
                else
                {
                    startDelay = delay;
                }
            }
        }
    }
    else
    {
        qreal length = (fileInfo->i64TotalFrames - 1) / fileInfo->dFramesPerSecond;
        if (currentInputMode == "videoframes")
        {
            qreal checkVal = startCut / fps;
            if (checkVal > length)
            {
                fprintf(stderr, "Start cut value is larger than length of file.\n");
                return 1;
            }
            checkVal = endCut * fps;
            if (checkVal > length)
            {
                endCut = delayac3->round(length * fps);
                fprintf(stdout, "End cut value is larger than length of file.  Truncating to %d frames.\n", (int) endCut);
            }
        }
        if (currentInputMode == "audioframes")
        {
            if (startCut > (fileInfo->i64TotalFrames - 1))
            {
                fprintf(stderr, "Start cut value is larger than length of file.\n");
                return 1;
            }
            if (endCut > (fileInfo->i64TotalFrames - 1))
            {
                endCut = (fileInfo->i64TotalFrames - 1);
                fprintf(stdout, "End cut value is larger than length of file.  Truncating to %d frames.\n", (int) endCut);
            }
        }
        else if (currentInputMode == "seconds")
        {
            if (startCut > length)
            {
                fprintf(stderr, "Start cut value is larger than length of file.\n");
                return 1;
            }
            if (endCut > length)
            {
                endCut = length;
                fprintf(stdout, "End cut value is larger than length of file.  Truncating to %f seconds.\n", endCut);
            }
        }
        else
        {
            if (startCut > (length * 1000.0))
            {
                fprintf(stderr, "Start cut value is larger than length of file.\n");
                return 1;
            }
            if (endCut > (length * 1000.0))
            {
                endCut = delayac3->round(length * 1000.0);
                fprintf(stdout, "End cut value is larger than length of file.  Truncating to %d milliseconds.\n", (int) endCut);
            }
        }
    }

    CalculateTarget();

    logFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_log.txt";
    logInputInfo(logFileName, QFileInfo(inFileName).suffix());

    if (printInfoOnly)
    {
        return 0;
    }

    logTargetInfo(logFileName, QFileInfo(inFileName).suffix());

#ifndef Q_OS_WIN
    outputFile = fopen(outFileName.toUtf8().constData(),"wb");
#else
    outputFile = _wfopen(outFileName.toStdWString().c_str(), L"wb");
#endif
    if (!outputFile)
    {
        onProcessingFinished(false);
        return 1;
    }

#ifndef Q_OS_WIN
    inputFile = fopen(inFileName.toUtf8().constData(),"rb");
#else
    inputFile = _wfopen(inFileName.toStdWString().c_str(), L"rb");
#endif
    if (!inputFile)
    {
        fclose(outputFile);
        onProcessingFinished(false);
        return 1;
    }

#ifndef Q_OS_WIN
    logFile = fopen(logFileName.toUtf8().constData(),"a");
#else
    logFile = _wfopen(logFileName.toStdWString().c_str(), L"a");
#endif
    if (!logFile)
    {
        fclose(outputFile);
        fclose(inputFile);
        onProcessingFinished(false);
        return 1;
    }

    QString extension = QFileInfo(inFileName).suffix();

    delayac3->SetDelayValues(inputFile, outputFile, logFile, fileInfo, isCLI, &_abort, crcMode, writeConsole, extension);
    delayac3->delayFile();

    return 0;
}

void GetInputFileInfo()
{
    if (fileInfo->type == "unknown") return;
#ifndef Q_OS_WIN
    inputFile = fopen(inFileName.toUtf8().constData(),"rb");
#else
    inputFile = _wfopen(inFileName.toStdWString().c_str(), L"rb");
#endif
    QString extension = QFileInfo(inFileName).suffix().toLower();

    if (extension == "wav")
    {
        delayac3->getwavinfo(inputFile, fileInfo);
    }
    if (extension == "ac3")
    {
        delayac3->getac3info(inputFile, fileInfo);
    }
    if (extension == "eac3" || extension == "ddp" || extension == "ec3" || extension == "dd+")
    {
        delayac3->geteac3info(inputFile, fileInfo);
    }
    if (extension == "dts")
    {
        delayac3->getdtsinfo(inputFile, fileInfo);
    }
    if (extension == "mpa" || extension == "mp2" || extension == "mp3")
    {
        delayac3->getmpainfo(inputFile, fileInfo);
    }
    fclose(inputFile);
}

void CalculateTarget()
{
    if (fileInfo->type == "unknown") return;

    qreal calcStartCut, calcEndCut, calcStartDelay, calcEndDelay;

    calcStartCut = calcEndCut = calcStartDelay = calcEndDelay = 0.0;

    if (isCut)
    {
        if (currentInputMode == "videoframes")
        {
            calcStartCut = startCut / fps;
            calcEndCut = endCut / fps;
        }
        else if (currentInputMode == "milliseconds")
        {
            calcStartCut = ((int) startCut) / 1000.0;
            calcEndCut = ((int) endCut) / 1000.0;
        }
        else if (currentInputMode == "audioframes")
        {
            if (startCut != 0)
            {
                calcStartCut = (startCut * fileInfo->dFrameduration) / 1000.0;
            }
            if (endCut != 0)
            {
                calcEndCut = (endCut * fileInfo->dFrameduration) / 1000.0;
            }
        }
        else
        {
            calcStartCut = startCut;
            calcEndCut = endCut;
        }

        qreal length = ((fileInfo->i64TotalFrames - 1) * fileInfo->dFrameduration) / 1000.0;
        if (calcStartCut > length)
        {
            calcStartCut = length;
        }
        if (calcEndCut > length)
        {
            calcEndCut = length;
        }
    }
    else
    {
        calcEndCut = calcStartCut = 0.0;
    }

    if (currentInputMode == "videoframes")
    {
        calcStartDelay = delayac3->round((startDelay / fps) * 1000.0);
        calcEndDelay = delayac3->round((endDelay / fps) * 1000.0);
    }
    else if (currentInputMode == "seconds")
    {
        calcStartDelay = startDelay * 1000.0;
        calcEndDelay = endDelay * 1000.0;
    }
    else if (currentInputMode == "audioframes")
    {
        calcStartDelay = delayac3->round(startDelay * fileInfo->dFrameduration);
        calcEndDelay = delayac3->round(endDelay * fileInfo->dFrameduration);
    }
    else
    {
        calcStartDelay = startDelay;
        calcEndDelay = endDelay;
    }

    delayac3->gettargetinfo(fileInfo, calcStartDelay, calcEndDelay, calcStartCut, calcEndCut);
}

void onProcessingFinished(bool success)
{
    if (_abort)
    {
        fprintf(stderr, "%s\n", "Processing aborted.");
    }
    else if (success)
    {
        fprintf(stdout, "%s\n", "Processing finished successfully.");
    }
    else
    {
        fprintf(stdout, "%s\n", "Processing failed.");
    }
}

qint32 logInputInfo(QString fileName, QString extension)
{
#ifndef Q_OS_WIN
    logFile = fopen(fileName.toUtf8().constData(),"w");
#else
    logFile = _wfopen(fileName.toStdWString().c_str(), L"w");
#endif
    if (!logFile)
    {
        return -1;
    }
    else
    {
        fprintf(logFile, "[Input info]\n");
        fprintf(logFile, "%s", QString("Bitrate=%1\n").arg(fileInfo->bitrate).toUtf8().constData());
        fprintf(logFile, "%s", QString("Actual rate=%1\n").arg(fileInfo->dactrate).toUtf8().constData());
        if (extension!="wav")
        {
            fprintf(logFile, "%s", QString("Sampling Frec=%1\n").arg(fileInfo->fsample).toUtf8().constData());
            fprintf(logFile, "%s", QString("TotalFrames=%1\n").arg(fileInfo->i64TotalFrames).toUtf8().constData());
            fprintf(logFile, "%s", QString("Bytesperframe=%1\n").arg(fileInfo->dBytesperframe, 9, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Filesize=%1\n").arg(fileInfo->i64Filesize).toUtf8().constData());
            fprintf(logFile, "%s", QString("FrameDuration=%1\n").arg(fileInfo->dFrameduration, 8, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Framespersecond=%1\n").arg(fileInfo->dFramesPerSecond, 8, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Duration=%1\n").arg(fileInfo->csOriginalDuration).toUtf8().constData());
            fprintf(logFile, "%s", QString("Channels mode=%1\n").arg(fileInfo->csMode).toUtf8().constData());
            fprintf(logFile, "%s", QString("LFE=%1\n").arg(fileInfo->csLFE).toUtf8().constData());
        }
        else
        {
            fprintf(logFile, "%s", QString("Byte rate=%1\n").arg(fileInfo->byterate).toUtf8().constData());
            fprintf(logFile, "%s", QString("Sampling Frec=%1\n").arg(fileInfo->fsample).toUtf8().constData());
            fprintf(logFile, "%s", QString("Bits of Prec=%1\n").arg(fileInfo->iBits).toUtf8().constData());
            fprintf(logFile, "%s", QString("TotalSamples=%1\n").arg(fileInfo->i64TotalFrames).toUtf8().constData());
            fprintf(logFile, "%s", QString("Bytespersample=%1\n").arg(fileInfo->dBytesperframe, 9, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Filesize=%1\n").arg(fileInfo->i64Filesize).toUtf8().constData());
            fprintf(logFile, "%s", QString("Duration=%1\n").arg(fileInfo->csOriginalDuration).toUtf8().constData());
            fprintf(logFile, "%s", QString("Channels mode=%1\n").arg(fileInfo->csMode).toUtf8().constData());
        }
        fclose(logFile);
        return 0;
    }
}

qint32 logTargetInfo(QString fileName, QString extension)
{
#ifndef Q_OS_WIN
    logFile = fopen(fileName.toUtf8().constData(),"a");
#else
    logFile = _wfopen(fileName.toStdWString().c_str(), L"a");
#endif
    if (!logFile)
    {
        return -1;
    }
    else
    {
        fprintf(logFile, "[Target info]\n");
        if (extension!="wav")
        {
            fprintf(logFile, "%s", QString("StartFrame=%1\n").arg(fileInfo->i64StartFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("EndFrame=%1\n").arg(fileInfo->i64EndFrame).toUtf8().constData());
        }
        else
        {
            fprintf(logFile, "%s", QString("StartSample=%1\n").arg(fileInfo->i64StartFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("EndSample=%1\n").arg(fileInfo->i64EndFrame).toUtf8().constData());
        }
        fprintf(logFile, "%s", QString("NotFixedDelay=%1\n").arg(fileInfo->dNotFixedDelay, 8, 'f', 4).toUtf8().constData());
        fprintf(logFile, "%s", QString("Duration=%1\n").arg(fileInfo->csTimeLengthEst).toUtf8().constData());
        fclose(logFile);
        return 0;
    }
}
