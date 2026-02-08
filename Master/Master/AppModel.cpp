#include "AppModel.h"
#include <QDir>

AppModel::AppModel(QObject* parent) : 
    QObject(parent) 
{
    m_folder = QDir::currentPath() + "/outputs";
    m_slaveScriptName = "slave.py";
    
    connect(&m_processScanTimer, &QTimer::timeout, this, &AppModel::scanSlaveProcess);

    m_processScanTimer.start(1000);

    createSharedMemory();
}

AppModel::~AppModel()
{
#ifdef Q_OS_WIN
    if (m_pBuf)
    {
        UnmapViewOfFile(m_pBuf);
        m_pBuf = nullptr;
    }
    if (m_hMapFile)
    {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
    }
#endif
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

bool AppModel::createSharedMemory()
{
    static_assert(sizeof(SharedData) == EXPECTED_SHARED_DATA_SIZE);

#ifdef Q_OS_WIN
    // Fermer l'ancienne mémoire si elle existe
    if (m_pBuf)
    {
        UnmapViewOfFile(m_pBuf);
        m_pBuf = nullptr;
    }
    if (m_hMapFile)
    {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
    }

    // Créer la mémoire partagée avec l'API Windows native
    m_hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,    // utiliser le fichier de pagination
        nullptr,                 // sécurité par défaut
        PAGE_READWRITE,          // accès lecture/écriture
        0,                       // taille haute (32 bits hauts)
        sizeof(SharedData),      // taille basse (32 bits bas)
        L"Local\\ipc_masterslave_shm"  // nom de l'objet
    );

    if (m_hMapFile == nullptr)
    {
        DWORD error = GetLastError();
        qDebug() << "CreateFileMapping failed with error:" << error;
        return false;
    }

    // Mapper la vue
    m_pBuf = MapViewOfFile(
        m_hMapFile,              // handle du mapping
        FILE_MAP_ALL_ACCESS,     // accès lecture/écriture
        0,                       // offset haute
        0,                       // offset basse
        sizeof(SharedData)       // nombre d'octets
    );

    if (m_pBuf == nullptr)
    {
        DWORD error = GetLastError();
        qDebug() << "MapViewOfFile failed with error:" << error;
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
        return false;
    }

    // Initialisation
    SharedData* data = static_cast<SharedData*>(m_pBuf);
    memset(data, 0, sizeof(SharedData));

    data->magic = 0xDEADBEEF;
    data->version = 1;

    QByteArray folderBytes = m_folder.toUtf8();
    strncpy_s(data->resultsFolderPath, sizeof(data->resultsFolderPath),
        folderBytes.constData(), _TRUNCATE);

    data->startNumber = m_start;
    data->endNumber = m_end;
    data->requestCounter = m_requestCounter;
    data->responseCounter = m_requestCounter;

    strncpy_s(data->resultFilePath, sizeof(data->resultFilePath), "INIT", _TRUNCATE);

    data->codeResult = 1234;
    data->sumResult = 5678;
    data->flags = 0xFFFFFFFF;

    qDebug() << "---";
    qDebug() << "Shared memory created with native Windows API";
    qDebug() << "Name: " << IPC_NAME;
    qDebug() << "Size:" << sizeof(SharedData) << "bytes";
    qDebug() << "---";

    return true;
#else
    qDebug() << "Shared memory only supported on Windows";
    return false;
#endif
}

SharedData* AppModel::lockSharedMemory()
{
#ifdef Q_OS_WIN
    if (m_pBuf)
        return static_cast<SharedData*>(m_pBuf);
#endif
    return nullptr;
}

void AppModel::unlockSharedMemory()
{
    // Avec l'API Windows native, pas besoin de lock/unlock explicite
    // mais gardez cette méthode pour la compatibilité future
}

void AppModel::start()
{
    
}
