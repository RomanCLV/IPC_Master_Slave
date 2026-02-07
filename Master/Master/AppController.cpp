#include "AppController.h"
#include <QFileDialog>

AppController::AppController(AppModel* model, MainWindow* view, QObject* parent) : QObject(parent), m_model(model), m_view(view)
{
    // View -> Controller
    connect(view, &MainWindow::startRequested, model, &AppModel::start);
    connect(view, &MainWindow::folderRequested, this, &AppController::onFolderRequested);
    connect(view, &MainWindow::rangeChanged, this, &AppController::onRangeChanged);

    // Model -> Controller -> View
    connect(model, &AppModel::processInfoChanged, this, &AppController::refreshProcessInfo);
    connect(model, &AppModel::telemetryChanged, this, &AppController::refreshTelemetry);
    connect(model, &AppModel::inputsChanged, this, &AppController::refreshInputs);
    connect(model, &AppModel::outputsChanged, this, &AppController::refreshOutputs);

    refreshView();
}

void AppController::refreshView()
{
    refreshProcessInfo();
    refreshTelemetry();
    refreshInputs();
    refreshOutputs();
}

void AppController::refreshProcessInfo()
{
    m_view->updateProcessInfo(
        m_model->scriptName(),
        m_model->slaveFound(),
        m_model->slavePid(),
        AppModel::stateToString(m_model->slaveState())
    );
}

void AppController::refreshTelemetry()
{
    m_view->updateTelemetry(
        m_model->elapsedMaster(),
        m_model->elapsedSlave()
    );
}

void AppController::refreshInputs()
{
    m_view->updateInputs(
        m_model->folder(),
        m_model->startValue(),
        m_model->endValue()
    );
}

void AppController::refreshOutputs()
{
    m_view->updateOutputs(
        m_model->statusCode(),
        m_model->sumResult(),
        m_model->fileContent()
    );
}

void AppController::onFolderRequested()
{
    QString folder = QFileDialog::getExistingDirectory(m_view, "Select folder");
    if (!folder.isEmpty())
        m_model->setFolder(folder);
}

void AppController::onRangeChanged(int start, int end)
{
    m_model->setRange(start, end);
}
