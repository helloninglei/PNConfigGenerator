#include "ArExchangeManager.h"
#include <QDebug>
#include <QtEndian>
#include <pcap.h>

namespace PNConfigLib {

ArExchangeManager::ArExchangeManager(QObject *parent) : QObject(parent) {
    m_phaseTimer = new QTimer(this);
    connect(m_phaseTimer, &QTimer::timeout, this, &ArExchangeManager::onPhaseTimerTick);
    
    m_cyclicTimer = new QTimer(this);
    connect(m_cyclicTimer, &QTimer::timeout, this, &ArExchangeManager::sendCyclicFrame);
}

ArExchangeManager::~ArExchangeManager() {
    stop();
}

bool ArExchangeManager::start(const QString &interfaceName, const QString &targetMac, const QString &targetIp) {
    if (m_state != ArState::Offline) return false;

    m_interfaceName = interfaceName;
    m_targetMac = targetMac;
    m_targetIp = targetIp;

    char errbuf[PCAP_ERRBUF_SIZE];
    m_pcapHandle = pcap_open_live(interfaceName.toStdString().c_str(), 65536, 1, 10, errbuf);
    if (!m_pcapHandle) {
        m_lastError = QString("Failed to open adapter: %1").arg(errbuf);
        setState(ArState::Error);
        return false;
    }

    // Capture source MAC (Simplified for simulation)
    // In a real app, we'd reuse the logic from DcpScanner
    memset(m_sourceMac, 0x00, 6); // Placeholder

    setState(ArState::Connecting);
    m_phaseStep = 0;
    m_phaseTimer->start(1000); 
    
    emit messageLogged("Starting IO Exchange simulation...");
    return true;
}

void ArExchangeManager::stop() {
    m_phaseTimer->stop();
    m_cyclicTimer->stop();
    
    if (m_pcapHandle) {
        pcap_close((pcap_t*)m_pcapHandle);
        m_pcapHandle = nullptr;
    }
    
    setState(ArState::Offline);
    emit messageLogged("IO Exchange stopped.");
}

void ArExchangeManager::setState(ArState newState) {
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(m_state);
    }
}

void ArExchangeManager::onPhaseTimerTick() {
    switch (m_state) {
        case ArState::Connecting:
            if (sendRpcConnect()) {
                emit messageLogged("Phase 1: DCE/RPC Connect sent.");
                setState(ArState::Parameterizing);
            } else {
                emit messageLogged("Phase 1: Failed to send Connect.");
                stop();
            }
            break;
            
        case ArState::Parameterizing:
            if (sendRecordData()) {
                emit messageLogged("Phase 2: Parameterization (Write Record) completed.");
                setState(ArState::Running);
                startCyclicExchange();
            } else {
                emit messageLogged("Phase 2: Parameterization failed.");
                stop();
            }
            break;
            
        default:
            m_phaseTimer->stop();
            break;
    }
}

bool ArExchangeManager::sendRpcConnect() {
    // Protocol Simulation: In a real Profinet stack, this would be a complex DCE/RPC call.
    // Here we simulate the successful initiation of the AR.
    emit messageLogged("Simulating DCE/RPC Connect to " + m_targetIp);
    return true;
}

bool ArExchangeManager::sendRecordData() {
    // Protocol Simulation: Writing GSDML records.
    emit messageLogged("Simulating Record Data Write to " + m_targetMac);
    return true;
}

void ArExchangeManager::startCyclicExchange() {
    emit messageLogged("Phase 3: Starting Cyclic Data Exchange (Ethertype 0x8892)");
    m_cyclicTimer->start(100); // 100ms cycle for simulation
}

void ArExchangeManager::sendCyclicFrame() {
    if (!m_pcapHandle) return;

    // Construct a basic PROFINET RT Frame (Ethertype 0x8892, FrameID 0x8000+)
    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    // Ethernet Header
    // Dest MAC (from target)
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) {
        for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    }
    
    // Source MAC (Simulated)
    memcpy(packet + 6, "\x00\x0C\x29\x01\x02\x03", 6);
    
    // Ethertype
    packet[12] = 0x88;
    packet[13] = 0x92;

    // PROFINET RT Header
    // FrameID (0x8000 for Cyclic Data)
    packet[14] = 0x80;
    packet[15] = 0x00;

    // IO Data (Simulated 4 bytes)
    packet[16] = 0xDE;
    packet[17] = 0xAD;
    packet[18] = 0xBE;
    packet[19] = 0xEF;

    // Cycle Counter (2 bytes)
    static uint16_t cycleCounter = 0;
    packet[20] = (cycleCounter >> 8) & 0xFF;
    packet[21] = cycleCounter & 0xFF;
    cycleCounter++;

    // Data Status (1 byte)
    packet[22] = 0x35; // Valid, OK, Primary

    // Transfer Status (1 byte)
    packet[23] = 0x00;

    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 60) != 0) {
        // qWarning() << "Error sending RT frame";
    }
}

} // namespace PNConfigLib
