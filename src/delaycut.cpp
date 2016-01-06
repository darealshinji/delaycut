/*  Copyright (C) 2004 by jsoto
    2007-09-11 E-AC3 support added by www.madshi.net
    2009 Chumbo mod to autofill delay values from filename
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

#include "delaycut.h"
#include "ui_delaycut.h"

#include <QIntValidator>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QStringListModel>
#include <QMessageBox>
#include <QFontMetrics>
#include <QStringList>
#include <cmath>
#include <QThread>

#include <wchar.h>
#include "delayac3.h"
#include "dc_types.h"

#include <cstdio>

#ifdef Q_OS_WIN
#include "windows.h"
#include "io.h"
#include "fcntl.h"
#endif

DelayCut::DelayCut(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DelayCut),
    crcMode("IGNORED"),
    abort(false),
    isCLI(false),
    writeConsole(false),
    currentInputMode("milliseconds"),
    startCut(0),
    endCut(0),
    startDelay(0),
    endDelay(0),
    fps(29.97),
    inFileName(""),
    outFileName(""),
    logFileName(""),
    versionString("delaycut v" VERSION)
{
    ui->setupUi(this);
    delayac3 = new Delayac3;
    delayac3->ac3_crc_init();
    stringModel = new QStringListModel;
    fileInfo = new FILEINFO;
    this->setWindowTitle(versionString);
#ifdef Q_OS_WIN
    this->setWindowIcon(QIcon(":/delaycut.ico"));
#else
    this->setWindowIcon(QIcon(":/delaycut.png"));
#endif

    delayValidator = new QIntValidator(-9999999, 99999999, this);
    cutValidator = new QIntValidator(0, 99999999, this);
    ui->startCuttingLineEdit->setValidator(cutValidator);
    ui->startCuttingLineEdit->setText(QString::number((int) startCut));
    ui->endCuttingLineEdit->setValidator(cutValidator);
    ui->endCuttingLineEdit->setText(QString::number((int) endCut));
    ui->startDelayLineEdit->setValidator(delayValidator);
    ui->startDelayLineEdit->setText(QString::number((int) startDelay));
    ui->endDelayLineEdit->setValidator(delayValidator);
    ui->endDelayLineEdit->setText(QString::number((int) endDelay));

    fpsValidator = new QRegExpValidator(QRegExp("[0-9]{1,6}[/|.][0-9]{1,4}"), this);
    ui->fpsLineEdit->setValidator(fpsValidator);
    ui->fpsLineEdit->setText(QString::number(fps, 'f', 2));

    connect(ui->inputFileLineEdit, SIGNAL(onDropEvent(QString)), this, SLOT(onFileDropped(QString)));
    connect(ui->outputFileLineEdit, SIGNAL(onDropEvent(QString)), this, SLOT(onFileDropped(QString)));

    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    ui->infoTextEdit->setFont(font);

    workerThread = new QThread;

    connect(delayac3, SIGNAL(UpdateProgress(qint32)), this, SLOT(onUpdateProgress(qint32)));
    connect(delayac3, SIGNAL(ProcessingFinished(bool)), this, SLOT(onProcessingFinished(bool)));
}

DelayCut::~DelayCut()
{
    delete ui;
}


void DelayCut::Redirect_console()
{
#ifdef Q_OS_WIN
    int hCrtout,hCrterr;
    FILE *hfout, *hferr;

    AllocConsole();

    hCrtout=_open_osfhandle( (long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT );
    hfout=_fdopen(hCrtout, "w");
    *stdout= *hfout;
    setvbuf(stdout,NULL, _IONBF,0);

    hCrterr=_open_osfhandle( (long) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT );
    hferr=_fdopen(hCrterr, "w");
    *stderr= *hferr;
    setvbuf(stderr,NULL, _IONBF,0);
#endif
}

void DelayCut::printHelp()
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
    fprintf(stdout, "%s\n", "Return values");
    fprintf(stdout, "%s\n", "0: Success");
    fprintf(stdout, "%s\n", "1: Something went wrong.");
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
}

void DelayCut::execCLI(int argc)
{
    QStringList args = QApplication::arguments();

    Redirect_console();
    if (args.contains("-help") || args.contains("--help"))
    {
        printHelp();
        exit(EXIT_SUCCESS);
    }
    if (args.contains("-version") || args.contains("--version"))
    {
        fprintf(stdout, "%s\n", versionString.toStdString().c_str());
        exit(EXIT_SUCCESS);
    }

    writeConsole = true;
    isCLI = true;
    QString parameter;
    bool printInfoOnly = false, isSame = false; //isAuto = false;

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
                exit(EXIT_FAILURE);
            }
            if (!QFile(inFileName).exists())
            {
                fprintf(stderr, "No valid input file specified.\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (parameter == "-auto")
        {
            //isAuto = true;
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
                exit(EXIT_FAILURE);
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
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (sscanf(fpsParam.toAscii().constData(), "%lf", &fps) != 1)
                {
                    fprintf(stderr, "No valid fps value specified.\n");
                    exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }
            else
            {
                qDebug() << "Current input method =" << currentInputMode;
            }
        }
        else if (parameter == "-same")
        {
            isSame = true;
        }
        else if ((parameter == "-startdelay" || parameter == "-delay") && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toAscii().constData(), "%lf", &startDelay) != 1)
            {
                fprintf(stderr, "No valid start delay value specified.\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (parameter == "-enddelay" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toAscii().constData(), "%lf", &endDelay) != 1)
            {
                fprintf(stderr, "No valid end delay value specified.\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (parameter == "-startcut" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toAscii().constData(), "%lf", &startCut) != 1)
            {
                fprintf(stderr, "No valid start cut value specified.\n");
                exit(EXIT_FAILURE);
            }
            isCut = true;
        }
        else if (parameter == "-endcut" && i < (argc - 1))
        {
            i++;
            if (sscanf(args.at(i).toAscii().constData(), "%lf", &endCut) != 1)
            {
                fprintf(stderr, "No valid end cut value specified.\n");
                exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }
        }
        else if (parameter == "-o" && i < (argc - 1))
        {
            i++;
            outFileName = QFileInfo(args.at(i)).absoluteFilePath();
            if (outFileName.isEmpty() || outFileName.isNull())
            {
                fprintf(stderr, "No valid output file specified.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (parameter.at(0) == '-')
            {
                fprintf(stderr, "Unrecognized switch: %s.\n", parameter.toStdString().c_str());
                exit(EXIT_FAILURE);
            }

            if (i == (argc - 1) && QFile(parameter).exists())
            {
                inFileName = QFileInfo(parameter).absoluteFilePath();;
            }
            else if (i == (argc - 1) && !QFile(parameter).exists())
            {
                fprintf(stderr, "No valid input file specified.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (inFileName.isEmpty())
    {
        fprintf(stderr, "No valid input file specified.\n");
        exit(EXIT_FAILURE);
    }

    if (isSame)
    {
        endDelay = startDelay;
    }

    if (startCut != 0 || endCut != 0)
    {
        if (startCut < 0)
        {
            exit(EXIT_FAILURE);
        }

        if (endCut < 0)
        {
            exit(EXIT_FAILURE);
        }

        if (endCut != 0 && (endCut < startCut))
        {
            exit(EXIT_FAILURE);
        }
    }

    if (outFileName.isEmpty() || outFileName.isNull())
    {
        QString extension = QFileInfo(inFileName).suffix();
        outFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_fixed." + extension;
    }

    GetInputFileInfo();

    qreal length = (fileInfo->i64TotalFrames - 1) / fileInfo->dFramesPerSecond;
    if (currentInputMode == "videoframes")
    {
        qreal checkVal = startCut / fps;
        if (checkVal > length)
        {
            fprintf(stderr, "Start cut value is larger than length of file.\n");
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }
        if (endCut > (length * 1000.0))
        {
            endCut = delayac3->round(length * 1000.0);
            fprintf(stdout, "End cut value is larger than length of file.  Truncating to %d milliseconds.\n", (int) endCut);
        }
    }

    CalculateTarget();

    logFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_log.txt";
    logInputInfo(logFileName, QFileInfo(inFileName).suffix());

    if (printInfoOnly)
    {
        exit(EXIT_SUCCESS);
    }

    logTargetInfo(logFileName, QFileInfo(inFileName).suffix());

    execute();

    exit(EXIT_SUCCESS);
}

void DelayCut::keyPressEvent(QKeyEvent* event)
{
    if (ui->processButton->isEnabled() && event->key() == Qt::Key_Return)
    {
        on_processButton_clicked();
    }
    if (event->key() == Qt::Key_Escape)
    {
        on_quitButton_clicked();
    }
}

void DelayCut::execute()
{
#ifndef Q_OS_WIN
    outputFile = fopen(outFileName.toUtf8().constData(),"wb");
#else
    outputFile = _wfopen(outFileName.toStdWString().c_str(), L"wb");
#endif
    if (!outputFile)
    {
        onProcessingFinished(false);
        return;
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
        return;
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
        return;
    }

    QString extension = QFileInfo(inFileName).suffix();

    delayac3->SetDelayValues(inputFile, outputFile, logFile, fileInfo, isCLI, &abort, crcMode, writeConsole, extension);

    if (!isCLI)
    {
        delayac3->moveToThread(workerThread);
        connect(workerThread, SIGNAL(started()), delayac3, SLOT(delayFile()));
        connect(delayac3, SIGNAL(ProcessingFinished(bool)), workerThread, SLOT(quit()));

        workerThread->start();
    }
    else
    {
        delayac3->delayFile();
    }
}

void DelayCut::on_inputBrowseButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select input file"),
                                                    lastOpenDir == "" ? QDir::homePath() : lastOpenDir,
                                                    tr("All supported files (*.wav *.mpa *.ac3 *.eac3 *.ddp *.ec3 *.dd+ *.dts);;wave files (*.wav);;"
                                                       "mpeg1 files (*.mpa);;dolby files (*.ac3);;eac3 files (*.eac3 *.ddp *.ec3 *.dd+);;dts files (*.dts);;All Files (*.*)")
                                                    );

    if (fileName.isNull() || fileName.isEmpty()) return;

    inFileName = QDir::toNativeSeparators(fileName);
    ui->inputFileLineEdit->setText("");
    ui->inputFileLineEdit->setText(inFileName);
    if (ui->originalLengthCheckBox->isChecked())
    {
        ui->originalLengthCheckBox->setChecked(false);
        ui->originalLengthCheckBox->setChecked(true);
    }
}

void DelayCut::on_outputBrowseButton_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Select output file"),
                                                    lastSaveDir == "" ? lastOpenDir : lastSaveDir,
                                                    tr("ac3 files (*.ac3);;eac3 files (*.eac3 *.ddp *.ec3 *.dd+);;dts files (*.dts)")
                                                    );
    if (fileName.isNull() || fileName.isEmpty()) return;

    outFileName = QDir::toNativeSeparators(fileName);
    ui->outputFileLineEdit->setText(outFileName);
    lastSaveDir = QFileInfo(fileName).absolutePath();
}

void DelayCut::onFileDropped(QString fileName)
{
    ui->inputFileLineEdit->setText("");
    inFileName = QDir::toNativeSeparators(fileName);
    ui->inputFileLineEdit->setText(inFileName);
    if (ui->originalLengthCheckBox->isChecked())
    {
        ui->originalLengthCheckBox->setChecked(false);
        ui->originalLengthCheckBox->setChecked(true);
    }
}

void DelayCut::on_inputFileLineEdit_textChanged(const QString &fileName)
{
    if (!fileName.contains(".") || !QFile(fileName).exists())
    {
        inFileName = "";
        outFileName = "";
        Reset();
        return;
    }

    inFileName = fileName;
    QString fileNameWithoutPath = QFileInfo(inFileName).fileName().toLower();

    lastOpenDir = QFileInfo(inFileName).absolutePath();
    QString extension = QFileInfo(inFileName).suffix();
    outFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_fixed." + extension;
    SetOutputPath(outFileName);
    GetInputFileInfo();

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
                ui->startDelayLineEdit->setText(QString::number(startDelay, 'g', 3));
            }
            else if (currentInputMode == "videoframes")
            {
                if (ui->fpsLineEdit->text().contains("/"))
                {
                    QStringList fpsValues = ui->fpsLineEdit->text().split("/");
                    if (fpsValues.length() == 3)
                    {
                        fps = fpsValues.at(0).toDouble() / fpsValues.at(1).toDouble();
                    }
                }
                else
                {
                    fps = ui->fpsLineEdit->text().toDouble();
                }
                startDelay = delayac3->round((delay / 1000.0) * fps);
                ui->startDelayLineEdit->setText(QString::number(startDelay));
            }
            else if (currentInputMode == "audioframes")
            {
                startDelay = delayac3->round(delay / fileInfo->dFrameduration);
                ui->startDelayLineEdit->setText(QString::number((int) startDelay));
            }
            else
            {
                startDelay = delay;
                ui->startDelayLineEdit->setText(QString::number((int) delay));
            }
        }
    }

    CalculateTarget();
}

void DelayCut::Reset()
{
    ui->outputBrowseButton->setEnabled(false);
    ui->infoTextEdit->clear();
    ui->ignoreRadioButton->setChecked(true);
    ui->ignoreRadioButton->setEnabled(false);
    ui->fixRadioButton->setEnabled(false);
    ui->silenceRadioButton->setEnabled(false);
    ui->skipRadioButton->setEnabled(false);
    ui->processButton->setEnabled(false);
}

void DelayCut::SetOutputPath(QString fileName)
{
    ui->outputFileLineEdit->setText(QDir::toNativeSeparators(fileName));
    ui->outputFileLineEdit->setCursorPosition(0);
}

void DelayCut::GetInputFileInfo()
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

    ui->infoTextEdit->clear();
    ui->infoTextEdit->insertPlainText(QString("====== INPUT FILE INFO ===============\n"));
    ui->infoTextEdit->insertPlainText(QString("File is            %1\n") .arg(fileInfo->type));
    ui->infoTextEdit->insertPlainText(QString("Bitrate  (kbit/s)  %1\n").arg(fileInfo->bitrate));
    ui->infoTextEdit->insertPlainText(QString("Act rate (kbit/s)  %1\n").arg(fileInfo->dactrate, 5, 'f', 3));
    ui->infoTextEdit->insertPlainText(QString("File size (bytes)  %1\n").arg(fileInfo->i64Filesize));
    ui->infoTextEdit->insertPlainText(QString("Channels mode      %1\n").arg(fileInfo->csMode));
    ui->infoTextEdit->insertPlainText(QString("Sampling Frec      %1\n").arg(fileInfo->fsample));

    if (fileInfo->type != "wav")
    {
        ui->infoTextEdit->insertPlainText(QString("Low Frec Effects   %1\n").arg(fileInfo->csLFE));
    }
    else
    {
        ui->infoTextEdit->insertPlainText(QString("Byte rate (kbit/s) %1\n").arg(fileInfo->byterate));
        ui->infoTextEdit->insertPlainText(QString("Bits of Prec.      %1\n").arg(fileInfo->iBits));
    }

    ui->infoTextEdit->insertPlainText(QString("Duration           %1\n").arg(fileInfo->csOriginalDuration));
    ui->infoTextEdit->insertPlainText(QString("%1 length (ms)%2 %3\n").arg(fileInfo->type == "wav" ? "Sample" : "Frame")
                                                                      .arg(fileInfo->type == "wav" ? "" : " ")
                                                                      .arg(fileInfo->dFrameduration, 8, 'f', 6));

    if (fileInfo->type != "wav")
    {
        ui->infoTextEdit->insertPlainText(QString("Frames/second      %1\n").arg(fileInfo->dFramesPerSecond, 8, 'L', 6));
    }

    ui->infoTextEdit->insertPlainText(QString("Num of %1     %2\n").arg(fileInfo->type == "wav" ? "samples" : "frames ")
                                                                   .arg(fileInfo->i64TotalFrames));

    if (fileInfo->type != "wav")
    {
        ui->infoTextEdit->insertPlainText(QString("Bytes per Frame    %1\n").arg(fileInfo->dBytesperframe, 9, 'L', 4));
    }
    else
    {
        ui->infoTextEdit->insertPlainText(QString("Bytes per Sample   %1\n").arg(fileInfo->dBytesperframe, 1, 'f', 0));
    }
    ui->infoTextEdit->insertPlainText(QString("Size % %1 size%2 %3\n").arg(fileInfo->type == "wav" ? "Sample" : "Frame")
                                                                      .arg(fileInfo->type == "wav" ? "" : " ")
                                                                      .arg(fileInfo->i64rest));

    if (fileInfo -> type != "wav")
    {
        if (fileInfo->cpf)
        {
            ui->infoTextEdit->insertPlainText(QString("CRC present:       YES\n"));
        }
        else
        {
            ui->infoTextEdit->insertPlainText(QString("CRC present:       NO\n"));
        }
    }
    ui->infoTextEdit->insertPlainText(QString("======================================\n"));
}

void DelayCut::CalculateTarget()
{
    if (fileInfo->type == "unknown") return;

    qreal calcStartCut, calcEndCut, calcStartDelay, calcEndDelay;

    calcStartCut = calcEndCut = calcStartDelay = calcEndDelay = 0.0;
    QString extension = QFileInfo(inFileName).suffix().toLower();
    if (extension == "ac3" || ((extension == "mpa" || extension == "mp2" || extension == "mp3") &&
        (fileInfo->layer == 2 && fileInfo->cpf != 0)))
    {
        ui->ignoreRadioButton->setEnabled(true);
        ui->fixRadioButton->setEnabled(true);
        ui->silenceRadioButton->setEnabled(true);
        ui->silenceRadioButton->setChecked(true);
        ui->skipRadioButton->setEnabled(true);
    }
    else
    {
        ui->ignoreRadioButton->setChecked(true);
        ui->ignoreRadioButton->setEnabled(false);
        ui->fixRadioButton->setEnabled(false);
        ui->silenceRadioButton->setEnabled(false);
        ui->skipRadioButton->setEnabled(false);
    }
    ui->outputBrowseButton->setEnabled(true);
    ui->processButton->setEnabled(true);

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

    ui->infoTextEdit->insertPlainText(QString("====== TARGET FILE INFO ==============\n"));
    ui->infoTextEdit->insertPlainText(QString("Start %1       %2\n").arg(fileInfo->type == "wav" ? "Sample" : "Frame ").arg(fileInfo->i64StartFrame));
    ui->infoTextEdit->insertPlainText(QString("End %1         %2\n").arg(fileInfo->type == "wav" ? "Sample" : "Frame ").arg(fileInfo->i64EndFrame));
    ui->infoTextEdit->insertPlainText(QString("Num of %1     %2\n").arg(fileInfo->type == "wav" ? "Samples" : "Frames ").arg((fileInfo->i64EndFrame - fileInfo->i64StartFrame) + 1));
    ui->infoTextEdit->insertPlainText(QString("Duration           %1\n").arg(fileInfo->csTimeLengthEst));
    ui->infoTextEdit->insertPlainText(QString("NotFixedDelay      %1\n").arg(fileInfo->dNotFixedDelay, 5, 'f', 4));
    ui->infoTextEdit->insertPlainText("======================================\n");
}

void DelayCut::on_ignoreRadioButton_toggled(bool checked)
{
    if (checked)
    {
        ui->fixRadioButton->setChecked(false);
        ui->silenceRadioButton->setChecked(false);
        ui->skipRadioButton->setChecked(false);
        crcMode = "IGNORED";
    }
}

void DelayCut::on_fixRadioButton_toggled(bool checked)
{
    if (checked)
    {
        ui->ignoreRadioButton->setChecked(false);
        ui->silenceRadioButton->setChecked(false);
        ui->skipRadioButton->setChecked(false);
        crcMode = "FIXED";
    }
}

void DelayCut::on_silenceRadioButton_toggled(bool checked)
{
    if (checked)
    {
        ui->ignoreRadioButton->setChecked(false);
        ui->fixRadioButton->setChecked(false);
        ui->skipRadioButton->setChecked(false);
        crcMode = "SILENCED";
    }
}

void DelayCut::on_skipRadioButton_toggled(bool checked)
{
    if (checked)
    {
        ui->ignoreRadioButton->setChecked(false);
        ui->fixRadioButton->setChecked(false);
        ui->silenceRadioButton->setChecked(false);
        crcMode = "SKIPPED";
    }
}

void DelayCut::on_originalLengthCheckBox_toggled(bool checked)
{
    if (checked)
    {
        ui->endDelayLineEdit->setText(ui->startDelayLineEdit->text());
    }
    ui->endDelayLineEdit->setEnabled(!checked);
    ui->endDelayLineEdit->setEnabled(!checked);
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_fpsLineEdit_textChanged()
{
    if (ui->fpsLineEdit->text().isEmpty())
    {
        ui->fpsLineEdit->setText("29.97");
        fps = 29.97;
    }
    if (ui->fpsLineEdit->text().contains("/"))
    {
        QStringList fpsValues = ui->fpsLineEdit->text().split("/");
        if (fpsValues.length() != 3) return;
        else
        {
            fps = fpsValues.at(0).toDouble() / fpsValues.at(1).toDouble();
        }
    }
    else
    {
        fps = ui->fpsLineEdit->text().toDouble();
    }
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_startCuttingLineEdit_textEdited(const QString &inStartCut)
{
    if (ui->startCuttingLineEdit->text().isEmpty())
    {
        ui->startCuttingLineEdit->setText("0");
        startCut = 0;
    }
    else
    {
        startCut = inStartCut.toDouble();
    }
    if (!inFileName.isEmpty() && isCut)
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_endCuttingLineEdit_textEdited(const QString &inEndCut)
{
    if (ui->endCuttingLineEdit->text().isEmpty())
    {
        ui->endCuttingLineEdit->setText("0");
        endCut = 0;
    }
    else
    {
        endCut = inEndCut.toDouble();
    }
    if (!inFileName.isEmpty() && isCut)
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_startDelayLineEdit_textEdited(const QString &inStartDelay)
{
    if (ui->startDelayLineEdit->text().isEmpty())
    {
        ui->startDelayLineEdit->setText("0");
        startDelay = 0;
    }
    else
    {
        startDelay = inStartDelay.toDouble();
    }
    if (ui->originalLengthCheckBox->isChecked())
    {
        ui->endDelayLineEdit->setText(inStartDelay);
        endDelay = inStartDelay.toDouble();
    }
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_endDelayLineEdit_textEdited(const QString &inEndDelay)
{
    if (ui->endDelayLineEdit->text().isEmpty())
    {
        ui->endDelayLineEdit->setText("0");
        endDelay = 0;
    }
    else
    {
        endDelay = inEndDelay.toDouble();
    }
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_cutFileCheckBox_toggled(bool checked)
{
    isCut = checked;
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}

void DelayCut::on_quitButton_clicked()
{
    this->close();
}

void DelayCut::on_processButton_clicked()
{
    if (inFileName.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Fill input file path!");
        return;
    }
    if (outFileName.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Fill output file path!");
        return;
    }
    if (endCut < startCut && (endCut != 0))
    {
        QMessageBox::warning(this, "Error", "Start cut value is larger than ending cut value.");
        return;
    }
    if (startCut > (fileInfo->i64TotalFrames / fileInfo->dFramesPerSecond))
    {
        QMessageBox::warning(this, "Error", "Start cut value is larger than length of file.");
    }

    if (isCut)
    {
        startCut = startCut / 1000.0;
        endCut = endCut / 1000.0;
    }
    else
    {
        endCut = startCut = 0.0;
    }

    ui->processButton->setEnabled(false);
    ui->quitButton->setEnabled(false);
    ui->abortButton->setEnabled(true);

    logFileName = inFileName.mid(0, inFileName.lastIndexOf(".")) + "_log.txt";
    logInputInfo(logFileName, QFileInfo(inFileName).suffix());
    logTargetInfo(logFileName, QFileInfo(inFileName).suffix());

    execute();
}

void DelayCut::on_abortButton_clicked()
{
    abort = true;
}

void DelayCut::onProcessingFinished(bool success)
{
    if (abort)
    {
        if (isCLI)
        {
            fprintf(stderr, "%s\n", "Processing aborted.");
            exit(EXIT_FAILURE);
        }
        else
        {
            QMessageBox::information(this, "Info", "Aborted!");
            abort = false;
        }
    }
    else if (success)
    {
        if (isCLI)
        {
            fprintf(stdout, "%s\n", "Processing finished successfully.");
            exit(EXIT_SUCCESS);
        }
        else
        {
            QMessageBox::information(this, "Info", "Finished OK!");
        }
    }
    else
    {
        if (isCLI)
        {
            fprintf(stdout, "%s\n", "Processing failed.");
            exit(EXIT_FAILURE);
        }
        else
        {
            QMessageBox::information(this, "Info", "Finished with error!");
        }
    }

    ui->progressLabel->clear();
    ui->progressBar->setValue(0);

    ui->processButton->setEnabled(true);
    ui->quitButton->setEnabled(true);
    ui->abortButton->setEnabled(false);
}

void DelayCut::onUpdateProgress(qint32 value)
{
    if (value < 0)
    {
        value = 0;
    }
    if (value > 100)
    {
        value = 100;
    }
    ui->progressBar->setValue(value);
    ui->progressLabel->setText(QString("Processing... %1 %").arg(value));
}

qint32 DelayCut::logInputInfo(QString fileName, QString extension)
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
        if (extension!="wav")
        {
            fprintf(logFile, "[Input info]\n");
            fprintf(logFile, "%s", QString("Bitrate=%1\n").arg(fileInfo->bitrate).toUtf8().constData());
            fprintf(logFile, "%s", QString("Actual rate=%1\n").arg(fileInfo->dactrate).toUtf8().constData());
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
            fprintf(logFile, "[Input info]\n");
            fprintf(logFile, "%s", QString("Bitrate=%1\n").arg(fileInfo->bitrate).toUtf8().constData());
            fprintf(logFile, "%s", QString("Actual rate=%1\n").arg(fileInfo->dactrate).toUtf8().constData());
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

qint32 DelayCut::logTargetInfo(QString fileName, QString extension)
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
        if (extension!="wav")
        {
            fprintf(logFile, "[Target info]\n");
            fprintf(logFile, "%s", QString("StartFrame=%1\n").arg(fileInfo->i64StartFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("EndFrame=%1\n").arg(fileInfo->i64EndFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("NotFixedDelay=%1\n").arg(fileInfo->dNotFixedDelay, 8, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Duration=%1\n").arg(fileInfo->csTimeLengthEst).toUtf8().constData());
        }
        else
        {
            fprintf(logFile, "[Target info]\n");
            fprintf(logFile, "%s", QString("StartSample=%1\n").arg(fileInfo->i64StartFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("EndSample=%1\n").arg(fileInfo->i64EndFrame).toUtf8().constData());
            fprintf(logFile, "%s", QString("NotFixedDelay=%1\n").arg(fileInfo->dNotFixedDelay, 8, 'f', 4).toUtf8().constData());
            fprintf(logFile, "%s", QString("Duration=%1\n").arg(fileInfo->csTimeLengthEst).toUtf8().constData());
        }
        fclose(logFile);
        return 0;
    }
}

void DelayCut::on_inputUnitsComboBox_activated(const QString &itemText)
{
    QString startLabel, endLabel;
    qint32 maxLength;

    if (ui->fpsLineEdit->text().contains("/"))
    {
        QStringList fpsValues = ui->fpsLineEdit->text().split("/");
        fps = fpsValues.at(0).toDouble() / fpsValues.at(1).toDouble();
    }
    else
    {
        fps = ui->fpsLineEdit->text().toDouble();
    }

    if (currentInputMode == "seconds")
    {
        startCut = delayac3->round(startCut * 1000.0);
        endCut = delayac3->round(endCut * 1000.0);
        startDelay = delayac3->round(startDelay * 1000.0);
        endDelay = delayac3->round(endDelay * 1000.0);
    }
    else if (currentInputMode == "videoframes")
    {
        startCut = delayac3->round((startCut / fps) * 1000.0);
        endCut = delayac3->round((endCut / fps) * 1000.0);
        startDelay = delayac3->round((startDelay / fps) * 1000.0);
        endDelay = delayac3->round((endDelay / fps) * 1000.0);
    }
    else if (currentInputMode == "audioframes")
    {
        if (startCut != 0)
        {
            startCut = delayac3->round(startCut * fileInfo->dFrameduration);
        }
        if (endCut != 0)
        {
            endCut = delayac3->round(endCut * fileInfo->dFrameduration);
        }
        if (startDelay != 0)
        {
            startDelay = delayac3->round(startDelay * fileInfo->dFrameduration);
        }
        if (endDelay != 0)
        {
            endDelay = delayac3->round(endDelay * fileInfo->dFrameduration);
        }
    }

    ui->fpsLineEdit->setEnabled(itemText == "Video frames");

    if (itemText == "Seconds")
    {
        delayValidator = new QDoubleValidator(-99999.999, 99999.999, 3, this);
        cutValidator = new QDoubleValidator(-99999.999, 99999.999, 3, this);
        startLabel = "Start (sec)";
        endLabel = "End (sec)";
        maxLength = 10;
        currentInputMode = "seconds";
        startCut /= 1000.0;
        endCut /= 1000.0;
        startDelay /= 1000.0;
        endDelay /= 1000.0;
    }
    else
    {
        maxLength = 8;
        cutValidator = new QIntValidator(0, 99999999, this);
        delayValidator = new QIntValidator(-9999999, 99999999, this);

        if (itemText == "Video frames")
        {
            startLabel = "Start (frames)";
            endLabel = "End (frames)";
            currentInputMode = "videoframes";
            startCut = delayac3->round(fps * (startCut / 1000.0));
            endCut = delayac3->round(fps * (endCut / 1000.0));
            startDelay = delayac3->round(fps * (startDelay / 1000.0));
            endDelay = delayac3->round(fps * (endDelay / 1000.0));
        }
        else if (itemText == "Audio frames")
        {
            startLabel = "Start (frames)";
            endLabel = "End (frames)";
            currentInputMode = "audioframes";
            if (inFileName.isEmpty())
            {
                startCut = endCut = startDelay = endDelay = 0;
            }
            else
            {
                startCut = delayac3->round(startCut / fileInfo->dFrameduration);
                endCut = delayac3->round(endCut / fileInfo->dFrameduration);
                startDelay = delayac3->round(startDelay / fileInfo->dFrameduration);
                endDelay = delayac3->round(endDelay / fileInfo->dFrameduration);
            }
        }
        else if (itemText == "Milliseconds")
        {
            startLabel = "Start (msec)";
            endLabel = "End (msec)";
            currentInputMode = "milliseconds";
        }
    }

    ui->startCuttingLabel->setText(startLabel);
    ui->startCuttingLineEdit->setMaxLength(maxLength);
    ui->startCuttingLineEdit->setValidator(cutValidator);
    ui->startCuttingLineEdit->setText(itemText == "Video frames" || itemText == "Milliseconds" || itemText == "Audio frames" ? QString::number((int) startCut)
                                                                                                                             : QString::number(startCut, 'f', 3));
    ui->endCuttingLabel->setText(endLabel);
    ui->endCuttingLineEdit->setMaxLength(maxLength);
    ui->endCuttingLineEdit->setValidator(cutValidator);
    ui->endCuttingLineEdit->setText(itemText == "Video frames" || itemText == "Milliseconds" || itemText == "Audio frames" ? QString::number((int) endCut)
                                                                                                                           : QString::number(endCut, 'f', 3));
    ui->startDelayLabel->setText(startLabel);
    ui->startDelayLineEdit->setMaxLength(maxLength);
    ui->startDelayLineEdit->setValidator(delayValidator);
    ui->startDelayLineEdit->setText(itemText == "Video frames" || itemText == "Milliseconds" || itemText == "Audio frames" ? QString::number((int) startDelay)
                                                                                                                           : QString::number(startDelay, 'f', 3));
    ui->endDelayLabel->setText(endLabel);
    ui->endDelayLineEdit->setMaxLength(maxLength);
    ui->endDelayLineEdit->setValidator(delayValidator);
    ui->endDelayLineEdit->setText(itemText == "Video frames" || itemText == "Milliseconds" || itemText == "Audio frames" ? QString::number((int) endDelay)
                                                                                                                         : QString::number(endDelay, 'f', 3));
    if (!inFileName.isEmpty())
    {
        GetInputFileInfo();
        CalculateTarget();
    }
}
