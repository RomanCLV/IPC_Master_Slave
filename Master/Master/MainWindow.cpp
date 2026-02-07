#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent)
{
    ui.setupUi(this);

    connect(ui.startPushButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui.folderPushButton, &QPushButton::clicked, this, &MainWindow::onFolderClicked);
    connect(ui.startSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onStartSpinChanged);
    connect(ui.endSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onEndSpinChanged);
    connect(ui.scriptNameLineEdit, &QLineEdit::textEdited, this, &MainWindow::scriptNameChanged);

    updateStartButtonState();
}

MainWindow::~MainWindow() {}

void MainWindow::onStartClicked()
{
    emit startRequested();
}

void MainWindow::onFolderClicked()
{
    emit folderRequested();
}

void MainWindow::onStartSpinChanged(int value)
{
    ui.endSpinBox->setMinimum(value);
    emit rangeChanged(value, ui.endSpinBox->value());
    updateStartButtonState();
}

void MainWindow::onEndSpinChanged(int value)
{
    ui.startSpinBox->setMaximum(value);
    emit rangeChanged(ui.startSpinBox->value(), value);
    updateStartButtonState();
}

void MainWindow::updateStartButtonState()
{
    bool valid = !ui.folderLineEdit->text().isEmpty() && m_slaveProcessFound;
    ui.startPushButton->setEnabled(valid);
}

void MainWindow::updateProcessInfo(const QString& scriptName, bool found, int pid, const QString& state)
{
    m_slaveProcessFound = found;
    ui.scriptNameLineEdit->setText(scriptName);
    ui.slaveProcessLabel->setText(found ? "Found" : "Not found");
    ui.slaveProcessIdLabel->setText(QString::number(pid));
    ui.slaveStateLabel->setText(state);
 
    updateStartButtonState();
}

void MainWindow::updateTelemetry(qint64 masterMs, qint64 slaveMs)
{
    ui.elapsedTimeMasterLabel->setText(QString::number(masterMs));
    ui.elapsedTimeSlaveLabel->setText(QString::number(slaveMs));
}

void MainWindow::updateOutputs(int statusCode, int sumResult, const QString& fileContent)
{
    ui.statusCodeLabel->setText(QString::number(statusCode));
    ui.sumResultLabel->setText(QString::number(sumResult));
    ui.fileTextEdit->setPlainText(fileContent);
}

void MainWindow::updateInputs(const QString& folder, int start, int end)
{
    ui.folderLineEdit->blockSignals(true);
    ui.folderLineEdit->setText(folder);
    ui.folderLineEdit->blockSignals(false);

    ui.startSpinBox->blockSignals(true);
    ui.startSpinBox->setValue(start);
    ui.startSpinBox->blockSignals(false);

    ui.endSpinBox->blockSignals(true);
    ui.endSpinBox->setValue(end);
    ui.endSpinBox->blockSignals(false);

    updateStartButtonState();
}
