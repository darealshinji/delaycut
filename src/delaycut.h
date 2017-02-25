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

#ifndef DELAYCUT_H
#define DELAYCUT_H

#include <QMainWindow>

/* Reminder:
 * On a new release, don't forget to bump the version
 * in delaycut.ui and delaycut.1 too and update the ChangeLog.
 */
#define VERSION "1.4.3.9"

namespace Ui {
    class DelayCut;
}

class Delayac3;
class QValidator;
class QStringListModel;
class QFontMetrics;
struct FILEINFO;

class DelayCut : public QMainWindow
{
    Q_OBJECT

public slots:
    void onProcessingFinished(bool success);

public:
    explicit DelayCut(QWidget *parent = 0);
    ~DelayCut();
    void execCLI(int argc);

protected:
    void keyPressEvent(QKeyEvent* event);

private slots:
    void on_inputBrowseButton_clicked();
    void on_outputBrowseButton_clicked();
    void onFileDropped(QString);
    void on_ignoreRadioButton_toggled(bool checked);
    void on_fixRadioButton_toggled(bool checked);
    void on_silenceRadioButton_toggled(bool checked);
    void on_skipRadioButton_toggled(bool checked);
    void on_originalLengthCheckBox_toggled(bool checked);
    void on_startCuttingLineEdit_textEdited(const QString &startCut);
    void on_endCuttingLineEdit_textEdited(const QString &endCut);
    void on_startDelayLineEdit_textEdited(const QString &startDelay);
    void on_endDelayLineEdit_textEdited(const QString &endDelay);
    void on_cutFileCheckBox_toggled(bool checked);
    void on_processButton_clicked();
    void onUpdateProgress(qint32);
    void on_quitButton_clicked();
    void on_inputFileLineEdit_textChanged(const QString &arg1);
    void on_outputFileLineEdit_textChanged(const QString &arg1);
    void on_inputUnitsComboBox_activated(const QString &itemText);
    void on_fpsLineEdit_textChanged();
    void on_abortButton_clicked();

private:
    Ui::DelayCut *ui;
    Delayac3 *delayac3;
    QString lastOpenDir;
    QString lastSaveDir;
    QString crcMode;
    QValidator *cutValidator, *delayValidator, *fpsValidator;
    FILEINFO *fileInfo;
    QStringListModel *stringModel;
    FILE* inputFile;
    FILE* outputFile;
    FILE* logFile;
    bool abort, isCLI, writeConsole, isCut;
    QString currentInputMode;
    int tabWidth, sizeOfSpace;
    QFontMetrics *currentFont;
    qreal startCut, endCut, startDelay, endDelay, fps;
    QString inFileName, outFileName, logFileName;
    QThread *workerThread;
    QString versionString;

    void CalculateTarget();
    void execute();
    void GetInputFileInfo();
    qint32 logInputInfo(QString fileName, QString extension);
    qint32 logTargetInfo(QString fileName, QString extension);
    void Reset();
    void SetOutputPath(QString fileName);
    long double round(long double value);
    void Redirect_console();
    void printHelp();
};

#endif // DELAYCUT_H
