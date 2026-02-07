#pragma once

#include <QObject>
#include "AppModel.h"
#include "MainWindow.h"

class AppController : public QObject
{
    Q_OBJECT

public:
    AppController(AppModel* model, MainWindow* view, QObject* parent = nullptr);

private slots:
    void onFolderRequested();
    void onRangeChanged(int start, int end);

    void refreshView();
    void refreshProcessInfo();
    void refreshTelemetry();
    void refreshInputs();
    void refreshOutputs();

private:
    AppModel* m_model;
    MainWindow* m_view;
};
