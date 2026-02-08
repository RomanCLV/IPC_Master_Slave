#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

signals:
    void startRequested();
    void folderRequested();
    void rangeChanged(int start, int end);
    void folderChanged(const QString& folder);
    void scriptNameChanged(const QString& scriptName);

public slots:
    void updateProcessInfo(const QString& scriptName, bool found, int pid, const QString& masterState, const QString& slaveState);
    void updateTelemetry(qint64 masterMs, qint64 slaveMs);
    void updateOutputs(int statusCode, int sumResult, const QString& fileContent);
    void updateInputs(const QString& folder, int start, int end);

private slots:
    void onStartClicked();
    void onFolderClicked();
    void onStartSpinChanged(int value);
    void onEndSpinChanged(int value);

private:
    void updateStartButtonState();

    Ui::MainWindowClass ui;
    bool m_slaveProcessFound = false;
};
