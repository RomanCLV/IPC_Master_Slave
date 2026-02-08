#include "AppModel.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

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

	if (m_workerThread)
	{
		m_workerThread->quit();
		m_workerThread->wait();
		delete m_workerThread;
	}
}

void AppModel::setMasterState(MasterState state)
{
	if (m_masterState == state)
		return;

	m_masterState = state;
	emit processInfoChanged();
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
	if (m_slaveState != state)
	{
		m_slaveState = state;
		emit processInfoChanged();
	}
}

void AppModel::setStatusCode(int code)
{
	if (m_statusCode != code)
	{
		m_statusCode = code;
		emit outputsChanged();
	}
}

void AppModel::setSumResult(int result)
{
	if (m_sumResult != result)
	{
		m_sumResult = result;
		emit outputsChanged();
	}
}

void AppModel::setFileContent(const QString& content)
{
	m_fileContent = content;
	emit outputsChanged();
}

void AppModel::setElapsedMaster(quint64 ms)
{
	if (m_elapsedMaster != ms)
	{
		m_elapsedMaster = ms;
		emit telemetryChanged();
	}
}

void AppModel::setElapsedSlave(quint64 ms)
{
	if (m_elapsedSlave != ms)
	{
		m_elapsedSlave = ms;
		emit telemetryChanged();
	}
}

void AppModel::setFolder(const QString& folder)
{
	m_folder = folder;
	emit inputsChanged();
}

void AppModel::setRange(int start, int end)
{
	if (m_start != start || m_end != end)
	{
		m_start = start;
		m_end = end;
		emit inputsChanged();
	}
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

	connect(m_scanProcess, &QProcess::finished, this, &AppModel::onScanProcessFinished);

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
	strncpy_s(data->resultsFolderPath, sizeof(data->resultsFolderPath), folderBytes.constData(), _TRUNCATE);

	data->startNumber = m_start;
	data->endNumber = m_end;
	data->requestCounter = m_requestCounter;
	data->responseCounter = m_requestCounter;

	strncpy_s(data->resultFileName, sizeof(data->resultFileName), "", _TRUNCATE);

	data->codeResult = 0;
	data->sumResult = 0;
	data->flags = IPCFlags::IDLE;

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
	if (m_masterState != MasterState::Idle && m_masterState != MasterState::Finished)
	{
		qDebug() << "Cannot start: master not idle";
		return;
	}

	if (!m_slaveFound)
	{
		qDebug() << "Cannot start: slave not found";
		return;
	}

	if (m_workerThread)
	{
		m_workerThread->quit();
		m_workerThread->wait();
		delete m_workerThread;
		m_workerThread = nullptr;
	}

	setMasterState(MasterState::Starting);

	m_requestCounter++;

	m_workerThread = new WorkerThread(m_pBuf, m_start, m_end, m_folder, m_requestCounter, this);

	connect(m_workerThread, &WorkerThread::finished, this, &AppModel::onWorkerFinished);
	connect(m_workerThread, &WorkerThread::slaveStateChanged, this, &AppModel::onWorkerSlaveStateChanged);
	connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

	m_workerThread->start();
}

void AppModel::onWorkerFinished(int errorCode, quint32 responseCounter, int result, const QString& filename, quint64 masterElapsed)
{
	if (m_requestCounter == responseCounter)
	{
		setStatusCode(errorCode);
		setSumResult(result);
		setElapsedMaster(masterElapsed);

		quint64 elapsedTime = 0;

		// Lire le contenu du fichier
		if (errorCode == IPCErrorCode::SUCCESS && !filename.isEmpty())
		{
			QString filePath = m_folder + "/" + filename;
			QFile file(filePath);
			if (file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				QTextStream in(&file);
				setFileContent(in.readAll());
				file.close();

				tryExractSlaveElapsedFromFile(elapsedTime);
			}
			else
			{
				setFileContent("Error: Could not read file");
			}
		}
		else
		{
			setFileContent("");
		}
		setElapsedSlave(elapsedTime);
	}
	else
	{
		setStatusCode(IPCErrorCode::INVALID_RESPONSE_COUNTER);
		setSumResult(0);
		setElapsedMaster(0);
		setElapsedSlave(0);
		setFileContent("");
	}

	setMasterState(MasterState::Finished);

	m_workerThread = nullptr;
}

void AppModel::onWorkerSlaveStateChanged(SlaveState state)
{
	setSlaveState(state);
}

bool AppModel::tryExractSlaveElapsedFromFile(quint64& elapsedOut)
{
	if (m_fileContent.isEmpty())
		return false;

	QStringList lines = m_fileContent.split("\n");
	bool ok = false;

	for (const auto& line : lines)
	{
		if (line.startsWith("Duration:"))
		{
			QString value = line.right(line.length() - 10);

			int v = value.toInt(&ok);
			if (ok)
				elapsedOut = v;
		}
	}
	return ok;
}

// ============================================================================
// WorkerThread Implementation
// ============================================================================

WorkerThread::WorkerThread(LPVOID sharedMemPtr, int start, int end, const QString& folder, uint32_t requestCounter, QObject* parent) :
	QThread(parent),
	m_pSharedMem(sharedMemPtr),
	m_start(start),
	m_end(end),
	m_folder(folder),
	m_requestCounter(requestCounter)
{
}

void WorkerThread::run()
{
	if (!m_pSharedMem)
	{
		emit finished(IPCErrorCode::UNKNOWN_ERROR, m_requestCounter, 0, "", 0);
		return;
	}

	QElapsedTimer masterTimer;
	masterTimer.start();

	SharedData* data = static_cast<SharedData*>(m_pSharedMem);

	// Écrire les inputs et effacer les outputs
	data->startNumber = m_start;
	data->endNumber = m_end;

	QByteArray folderBytes = m_folder.toUtf8();
	strncpy_s(data->resultsFolderPath, sizeof(data->resultsFolderPath), folderBytes.constData(), _TRUNCATE);

	data->requestCounter = m_requestCounter;

	// Effacer les outputs
	data->codeResult = 0;
	data->sumResult = 0;
	memset(data->resultFileName, 0, sizeof(data->resultFileName));

	// Signaler au slave qu'il peut commencer
	data->flags = IPCFlags::MASTER_READY;

	qDebug() << "Master: MASTER_READY flag set, waiting for slave...";

	// Attendre que le slave démarre
	const int timeout = 30000; // 30 secondes
	int elapsed = 0;

	while (data->flags == IPCFlags::MASTER_READY && elapsed < timeout)
	{
		QThread::msleep(10);
		elapsed += 10;
	}

	if (elapsed >= timeout)
	{
		qDebug() << "Master: Timeout waiting for slave to start";
		data->flags = IPCFlags::IDLE;
		emit finished(IPCErrorCode::UNKNOWN_ERROR, m_requestCounter, 0, "", masterTimer.elapsed());
		return;
	}

	qDebug() << "Master: Slave started processing";
	emit slaveStateChanged(AppModel::SlaveState::Processing);

	// Attendre que le slave termine
	while (data->flags != IPCFlags::SLAVE_FINISHED)
	{
		QThread::msleep(10);
	}

	qDebug() << "Master: Slave finished";

	// Lire les résultats
	int errorCode = data->codeResult;
	int result = data->sumResult;
	QString filename = QString::fromUtf8(data->resultFileName);

	// Lire le responseCounter
	uint32_t responseCounter = data->responseCounter;

	quint64 masterElapsed = masterTimer.elapsed();

	// Remettre le flag à IDLE
	data->flags = IPCFlags::IDLE;

	qDebug() << "Master: Process complete. Error code:" << errorCode << "Result:" << result << "File:" << filename;

	if (errorCode == IPCErrorCode::SUCCESS)
	{
		emit slaveStateChanged(AppModel::SlaveState::FinishedSuccess);
	}
	else
	{
		emit slaveStateChanged(AppModel::SlaveState::FinishedError);
	}

	emit finished(errorCode, responseCounter, result, filename, masterElapsed);
}
