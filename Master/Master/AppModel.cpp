#include "AppModel.h"
#include <QDir>

AppModel::AppModel(QObject* parent) : 
    QObject(parent) 
{
    m_folder = QDir::currentPath() + "/outputs";
    m_slaveScriptName = "main.py";
    
    connect(&m_processScanTimer, &QTimer::timeout, this, &AppModel::scanSlaveProcess);

    m_processScanTimer.start(1000);
}

void AppModel::setSlaveFound(bool found)
{
    m_slaveFound = found;
    emit processInfoChanged();
}

void AppModel::setSlavePid(int pid)
{
    m_slavePid = pid;
    emit processInfoChanged();
}

void AppModel::setSlaveState(SlaveState state)
{
    if (m_slaveState == state)
        return;

    m_slaveState = state;
    emit processInfoChanged();
}

void AppModel::setStatusCode(int code)
{
    m_statusCode = code;
    emit outputsChanged();
}

void AppModel::setSumResult(int result)
{
    m_sumResult = result;
    emit outputsChanged();
}

void AppModel::setFileContent(const QString& content)
{
    m_fileContent = content;
    emit outputsChanged();
}

void AppModel::setElapsedMaster(quint64 ms)
{
    m_elapsedMaster = ms;
    emit telemetryChanged();
}

void AppModel::setElapsedSlave(quint64 ms)
{
    m_elapsedSlave = ms;
    emit telemetryChanged();
}

void AppModel::setFolder(const QString& folder)
{
    m_folder = folder;
    emit inputsChanged();
}

void AppModel::setRange(int start, int end)
{
    m_start = start;
    m_end = end;
    emit inputsChanged();
}

void AppModel::scanSlaveProcess()
{
#ifdef Q_OS_WIN

    if (m_scanProcess) // éviter scans simultanés
        return;

    m_scanProcess = new QProcess(this);

    QString command =
        "Get-CimInstance Win32_Process | "
        "Where-Object {$_.Name -like \"python*\"} | "
        "Select-Object ProcessId, Name, CommandLine | "
        "ConvertTo-Csv -NoTypeInformation";

    connect(m_scanProcess, &QProcess::finished,
        this, &AppModel::onScanProcessFinished);

    m_scanProcess->start("powershell", QStringList() << "-Command" << command);

#endif
}

void AppModel::onScanProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QString output = m_scanProcess->readAllStandardOutput();

    bool found = false;
    int pid = -1;

    QStringList lines = output.split('\n');

    for (const QString& line : lines)
    {
        if (line.contains("python", Qt::CaseInsensitive) &&
            line.contains(m_slaveScriptName, Qt::CaseInsensitive))
        {
            found = true;

            QString colPid = line.split(',')[0];
            colPid = colPid.mid(1, colPid.length() - 2);
            pid = colPid.toInt();

            break;
        }
    }

    if (found != m_slaveFound || pid != m_slavePid)
    {
        m_slaveFound = found;
        m_slavePid = pid;
        m_slaveState = found ? SlaveState::Idle : SlaveState::NotRunning;
        emit processInfoChanged();  // SAFE: toujours thread UI
    }

    m_scanProcess->deleteLater();
    m_scanProcess = nullptr;
}

void AppModel::start()
{

}