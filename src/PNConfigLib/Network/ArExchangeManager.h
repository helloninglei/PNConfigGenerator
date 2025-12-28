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

signals:
    void stateChanged(ArState newState);
    void messageLogged(const QString &message);

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
    
    // Phase 2: Parameterize
    bool sendRecordData();
    
    // Phase 3: Cyclic Data
    void startCyclicExchange();

    ArState m_state = ArState::Offline;
    QString m_lastError;
    QString m_interfaceName;
    QString m_targetMac;
    QString m_targetIp;
    QString m_stationName;
    
    uint8_t m_sourceMac[6];
    uint8_t m_targetMacBytes[6];
    void* m_pcapHandle = nullptr;
    
    QTimer *m_phaseTimer;
    QTimer *m_cyclicTimer;
    int m_phaseStep = 0;
    uint32_t m_lastXid = 0;
};

} // namespace PNConfigLib

#endif // AREXCHANGEMANAGER_H
