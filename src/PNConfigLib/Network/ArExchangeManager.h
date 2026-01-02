#ifndef AREXCHANGEMANAGER_H
#define AREXCHANGEMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <vector>
#include <cstdint>

#include "DcpScanner.h"

namespace PNConfigLib {

enum class ArState {
    Offline,
    Connecting,
    Parameterizing,
    AppReady,
    Running,
    Error
};

class ArExchangeManager : public QObject {
    Q_OBJECT
public:
    explicit ArExchangeManager(QObject *parent = nullptr);
    ~ArExchangeManager();

    bool start(const QString &interfaceName, const QString &targetMac, const QString &targetIp, const QString &stationName);
    void stop();

    ArState state() const { return m_state; }
    QString lastError() const { return m_lastError; }

    void setOutputData(uint8_t val) { m_outputData = val; }
    uint8_t inputData() const { return m_inputData; }

signals:
    void stateChanged(ArState newState);
    void messageLogged(const QString &message);
    void inputDataReceived(uint8_t value);

private slots:
    void onPhaseTimerTick();
    void sendCyclicFrame();

private:
    void setState(ArState newState);
    
    // Response verification
    int waitForResponse(uint16_t frameId, uint32_t xid = 0, int timeoutMs = 2000);
    QString macToString(const uint8_t *mac);
    uint16_t calculateIpChecksum(const uint8_t* ipHeader, int len);
    
    // Phase 1: Connect
    bool sendRpcConnect();
    bool sendArpResponse(const uint8_t* targetMac, const uint8_t* targetIp);
    
    // Phase 2: Parameterize
    bool sendRecordData();
    bool sendRpcControlParamEnd();
    
    // Phase 3: AppReady & Cyclic Data
    void startCyclicExchange();

    ArState m_state = ArState::Offline;
    QString m_lastError;
    QString m_interfaceName;
    QString m_targetMac;
    QString m_targetIp;
    QString m_stationName;
    
    uint8_t m_sourceMac[6];
    uint8_t m_inputData = 0;
    uint8_t m_outputData = 0;
    uint8_t m_targetMacBytes[6];
    void* m_pcapHandle = nullptr;
    
    QTimer *m_phaseTimer;
    QTimer *m_cyclicTimer;
    int m_phaseStep = 0;
    uint32_t m_lastXid = 0;
    bool m_inPhaseTick = false;
    bool m_isDestroying = false;
};

} // namespace PNConfigLib

#endif // AREXCHANGEMANAGER_H
