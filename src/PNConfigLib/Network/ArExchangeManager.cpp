#line 1 "f:/workspaces/PNConfigGenerator/src/PNConfigLib/Network/ArExchangeManager.cpp"
#include "ArExchangeManager.h"
#include <QDebug>
#include <QtEndian>
#include <QNetworkInterface>
#include <QStringList>
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

    // Capture source MAC - Clean up target MAC and resolve source
    QString cleanTargetMac = m_targetMac;
    cleanTargetMac.remove(':').remove('-');
    m_targetMac = "";
    for (int i = 0; i < 6 && i*2+1 < cleanTargetMac.length(); ++i) {
        if (!m_targetMac.isEmpty()) m_targetMac += ":";
        m_targetMac += cleanTargetMac.mid(i*2, 2).toUpper();
    }

    // Resolve source MAC (using Qt for robustness)
    memset(m_sourceMac, 0, 6);
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        if (interfaceName.contains(iface.name()) || interfaceName.contains(iface.humanReadableName())) {
            QString macStr = iface.hardwareAddress();
            QString hexMac;
            for (const QChar &c : macStr) if (c.isDigit() || (c.toUpper() >= 'A' && c.toUpper() <= 'F')) hexMac.append(c);
            if (hexMac.length() == 12) {
                for (int i = 0; i < 6; ++i) m_sourceMac[i] = (uint8_t)hexMac.mid(i * 2, 2).toInt(nullptr, 16);
                emit messageLogged(QString("Resolved Source MAC: %1 (%2)")
                    .arg(iface.hardwareAddress(), iface.humanReadableName()));
                break;
            }
        }
    }

    if (m_sourceMac[0] == 0 && m_sourceMac[1] == 0 && m_sourceMac[2] == 0) {
        emit messageLogged("Warning: Failed to resolve source MAC. Simulation might fail.");
    }

    setState(ArState::Connecting);
    m_phaseStep = 0;
    m_phaseTimer->start(500); // Faster transitions for simulation
    
    emit messageLogged("Starting IO Exchange simulation...");
    return true;
}

void ArExchangeManager::stop() {
    if (m_cyclicTimer) m_cyclicTimer->stop();
    if (m_phaseTimer) m_phaseTimer->stop();
    
    if (m_pcapHandle) {
        pcap_close((pcap_t*)m_pcapHandle);
        m_pcapHandle = nullptr;
    }
    
    if (m_state != ArState::Offline) {
        setState(ArState::Offline);
        emit messageLogged("IO Exchange stopped.");
    }
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
                emit messageLogged("Phase 1: Connect request sent.");
                setState(ArState::Parameterizing);
            } else {
                emit messageLogged("Phase 1: Failed to send Connect.");
                stop();
            }
            break;
            
        case ArState::Parameterizing:
            if (sendRecordData()) {
                emit messageLogged("Phase 2: Configuration (DCP Signal) sent.");
                setState(ArState::Running);
                startCyclicExchange();
            } else {
                emit messageLogged("Phase 2: Configuration failed.");
                stop();
            }
            break;
            
        case ArState::Running:
            m_phaseTimer->stop();
            break;
            
        default:
            m_phaseTimer->stop();
            break;
    }
}

bool ArExchangeManager::sendRpcConnect() {
    // Send a real UDP multicast or unicast to trigger slave (PROFINET Port 34964)
    uint8_t packet[100];
    memset(packet, 0, sizeof(packet));
    
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x08; packet[13] = 0x00; // IPv4
    
    // IP Header
    packet[14] = 0x45; packet[23] = 0x11; // UDP
    memcpy(packet + 26, "\xC0\xA8\x01\x01", 4); // Source IP (Simulated)
    QStringList ipParts = m_targetIp.split('.');
    if (ipParts.size() == 4) for (int i = 0; i < 4; ++i) packet[30+i] = (uint8_t)ipParts[i].toInt();
    packet[24] = 0x00; packet[25] = 0x24; // Tot Len
    
    // UDP Header
    packet[34] = 0x88; packet[35] = 0x94; // Source 34964
    packet[36] = 0x88; packet[37] = 0x94; // Dest 34964
    packet[38] = 0x00; packet[39] = 0x10; // Length
    
    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 64) != 0) return false;
    return true;
}

bool ArExchangeManager::sendRecordData() {
    // Send a real DCP Signal payload to satisfy slave "Get/Set" expectation
    uint8_t packet[100];
    memset(packet, 0, sizeof(packet));
    
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x88; packet[13] = 0x92; // PN
    
    // DCP Header
    packet[14] = 0xFE; packet[15] = 0xFD; // Get/Set
    packet[16] = 0x04; // Set
    packet[17] = 0x01; // Request
    packet[18] = 0x11; packet[19] = 0x22; packet[20] = 0x33; packet[21] = 0x44; // XID
    packet[24] = 0x00; packet[25] = 0x08; // Data Length (Block header + 4 bytes)

    // Option 0x05 (Control), Suboption 0x03 (Signal)
    packet[26] = 0x05; packet[27] = 0x03; 
    packet[28] = 0x00; packet[29] = 0x04; // Block length
    packet[30] = 0x00; packet[31] = 0x00; // Reserved
    packet[32] = 0x01; packet[33] = 0x00; // Flash once
    
    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 60) != 0) return false;
    return true;
}

void ArExchangeManager::startCyclicExchange() {
    emit messageLogged("Phase 3: Starting Cyclic Data Exchange (Ethertype 0x8892)");
    m_cyclicTimer->start(10); // 10ms for more continuous interaction
}

void ArExchangeManager::sendCyclicFrame() {
    if (!m_pcapHandle) return;

    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    // Ethernet Header
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    
    // Ethertype 0x8892
    packet[12] = 0x88; packet[13] = 0x92;

    // PROFINET RT Header (FrameID 0x8000)
    packet[14] = 0x80; packet[15] = 0x00;

    // IO Data (Simulated 4 bytes)
    static uint8_t val = 0;
    packet[16] = val++; packet[17] = 0x00; packet[18] = 0x00; packet[19] = 0x00;

    // Cycle Counter (Standard PN style)
    static uint16_t cycleCounter = 0;
    packet[20] = (cycleCounter >> 8) & 0xFF;
    packet[21] = cycleCounter & 0xFF;
    cycleCounter += 32; // Increment by 31-32 as per RT class 1/3 typical

    // Data Status (0x35: Valid, OK, Primary)
    packet[22] = 0x35;
    packet[23] = 0x00; // Transfer Status

    pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 60);
}

} // namespace PNConfigLib
