#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>

class AppModel : public QObject
{
    Q_OBJECT

public:
    enum class SlaveState
    {
        NotRunning,
        Idle,
        Started,
        WaitingForSlave,
        Processing,
        Finished
    };

    static QString stateToString(SlaveState state) 
    {
        switch (state)
        {
        case SlaveState::NotRunning:      return "Not running";
        case SlaveState::Idle:            return "Idle";
        case SlaveState::Started:         return "Started";
        case SlaveState::WaitingForSlave: return "Waiting for slave";
        case SlaveState::Processing:      return "Processing";
        case SlaveState::Finished:        return "Finished";
        }
        return "Unknown";
    }

public:
    explicit AppModel(QObject* parent = nullptr);

    // getters
    QString scriptName() const { return m_slaveScriptName; }
    bool slaveFound() const { return m_slaveFound; }
    int slavePid() const { return m_slavePid; }
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

private slots:
    void setSlaveFound(bool found);
    void setSlavePid(int pid);
    void setSlaveState(SlaveState state);

    void scanSlaveProcess();

    void setStatusCode(int code);
    void setSumResult(int result);
    void setFileContent(const QString& content);

    void onScanProcessFinished(int exitCode, QProcess::ExitStatus status);

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

    int m_statusCode = 0;
    int m_sumResult = 0;

    quint64 m_elapsedMaster = 0;
    quint64 m_elapsedSlave = 0;

    QTimer m_processScanTimer;

    SlaveState m_slaveState = SlaveState::NotRunning;
    QString m_slaveScriptName;
    QString m_fileContent;
    QString m_folder;

    QProcess* m_scanProcess{ nullptr };
};
