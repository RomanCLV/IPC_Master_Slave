#pragma once

#include "SharedData.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifndef IPC_NAME
#define IPC_NAME "ipc_masterslave_shm"
#endif

class WorkerThread;

class AppModel : public QObject
{
    Q_OBJECT

public:
    enum class MasterState
    {
        Idle,
        Starting,
        WaitingForSlave,
        Finished
    };

    enum class SlaveState
    {
        NotRunning,
        Idle,
        Processing,
        FinishedSuccess,
        FinishedError
    };

    static QString masterStateToString(MasterState state)
    {
        switch (state)
        {
        case MasterState::Idle:            return "Idle";
        case MasterState::Starting:        return "Starting";
        case MasterState::WaitingForSlave: return "Waiting for slave";
        case MasterState::Finished:        return "Finished";
        }
        return "Unknown";
    }

    static QString slaveStateToString(SlaveState state)
    {
        switch (state)
        {
        case SlaveState::NotRunning:      return "NotRunning";
        case SlaveState::Idle:            return "Idle";
        case SlaveState::Processing:      return "Processing";
        case SlaveState::FinishedSuccess: return "Finished (Success)";
        case SlaveState::FinishedError:   return "Finished (Error)";
        }
        return "Unknown";
    }

public:
    explicit AppModel(QObject* parent = nullptr);
    ~AppModel() override;

    // getters
    QString scriptName() const { return m_slaveScriptName; }
    bool slaveFound() const { return m_slaveFound; }
    int slavePid() const { return m_slavePid; }

    MasterState masterState() const { return m_masterState; }
    SlaveState slaveState() const { return m_slaveState; }

    int statusCode() const { return m_statusCode; }
    int sumResult() const { return m_sumResult; }
    QString fileContent() const { return m_fileContent; }

    quint64 elapsedMaster() const { return m_elapsedMaster; }
    quint64 elapsedSlave() const { return m_elapsedSlave; }

    QString folder() const { return m_folder; }
    int startValue() const { return m_start; }
    int endValue() const { return m_end; }

public slots:
    // setters (utilisés par le controller)

    void setFolder(const QString& folder);
    void setRange(int start, int end);

    void start();

private:
    void setElapsedMaster(quint64 ms);
    void setElapsedSlave(quint64 ms);

    void setSlaveFound(bool found);
    void setSlavePid(int pid);
    void setSlaveState(SlaveState state);
    void setMasterState(MasterState state);

    void setStatusCode(int code);
    void setSumResult(int result);
    void setFileContent(const QString& content);

    bool createSharedMemory();
    SharedData* lockSharedMemory();
    void unlockSharedMemory();
    bool tryExractSlaveElapsedFromFile(quint64& elapsedOut);

private slots:
    void scanSlaveProcess();
    void onScanProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onWorkerFinished(int errorCode, quint32 responseCounter, int result, const QString& filename, quint64 masterElapsed);
    void onWorkerSlaveStateChanged(SlaveState state);

signals:
    void processInfoChanged();
    void telemetryChanged();
    void inputsChanged();
    void outputsChanged();

private:
    bool m_slaveFound = false;
    int m_slavePid = -1;

    int m_start = 0;
    int m_end = 100;

    int m_requestCounter = 0;

    int m_statusCode = 0;
    int m_sumResult = 0;

    quint64 m_elapsedMaster = 0;
    quint64 m_elapsedSlave = 0;

    QTimer m_processScanTimer;

    MasterState m_masterState = MasterState::Idle;
    SlaveState m_slaveState = SlaveState::NotRunning;

    QString m_slaveScriptName;
    QString m_fileContent;
    QString m_folder;

    QProcess* m_scanProcess{ nullptr };
    WorkerThread* m_workerThread{ nullptr };

#ifdef Q_OS_WIN
    HANDLE m_hMapFile = nullptr;
    LPVOID m_pBuf = nullptr;
#endif
};

// Thread de travail pour ne pas bloquer l'UI
class WorkerThread : public QThread
{
    Q_OBJECT

public:
    WorkerThread(LPVOID sharedMemPtr, int start, int end,
        const QString& folder, uint32_t requestCounter, QObject* parent = nullptr);

protected:
    void run() override;

signals:
    void finished(int errorCode, uint32_t responseCounter, int result, const QString& filename, quint64 masterElapsed);
    void slaveStateChanged(AppModel::SlaveState state);

private:
    LPVOID m_pSharedMem;
    int m_start;
    int m_end;
    QString m_folder;
    uint32_t m_requestCounter;
};
