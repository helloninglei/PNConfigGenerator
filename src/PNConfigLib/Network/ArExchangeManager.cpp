#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "ArExchangeManager.h"
#include "DcpScanner.h"
#include <QDebug>
#include <QtEndian>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QStringList>
#include <QThread>
#include <cstring>
#include <cstdlib>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif
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

bool ArExchangeManager::start(const QString &interfaceName, const QString &targetMac, const QString &targetIp, const QString &stationName) {
    if (m_state != ArState::Offline) return false;

    m_interfaceName = interfaceName;
    m_targetMac = targetMac;
    m_targetIp = targetIp;
    m_stationName = stationName;

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

    // Step 1: Configure device via DCP
    DcpScanner scanner;
    if (!scanner.connectToInterface(interfaceName)) {
        m_lastError = "Failed to connect DCP scanner to interface";
        setState(ArState::Error);
        return false;
    }

    // Capture the source MAC discovered by the scanner
    memcpy(m_sourceMac, scanner.getSourceMac(), 6);
    if (m_sourceMac[0] == 0 && m_sourceMac[1] == 0 && m_sourceMac[2] == 0) {
        emit messageLogged("Error: Source MAC resolution failed.");
        scanner.disconnectFromInterface();
        return false;
    }
    emit messageLogged(QString("Resolved Source MAC: %1").arg(macToString(m_sourceMac)));

    emit messageLogged(QString("Setting station name to: %1 (Permanent: true)").arg(stationName));
    if (!scanner.setDeviceName(targetMac, stationName, true)) {
        m_lastError = "Failed to set station name via DCP";
        scanner.disconnectFromInterface();
        setState(ArState::Error);
        return false;
    }
    emit messageLogged("Station name set successfully. Waiting 2s for Slave to commit...");

    // Increased delay to ensure the Slave has fully processed the name set and updated its identity
    QThread::msleep(2000);

    // Calculate default network parameters
    QStringList ipParts = targetIp.split('.');
    QString mask = "255.255.255.0"; // Default subnet mask
    QString gw = ipParts[0] + "." + ipParts[1] + "." + ipParts[2] + ".1"; // Default gateway

    emit messageLogged(QString("Setting IP to: %1, Mask: %2, Gateway: %3 (Permanent: true)").arg(targetIp, mask, gw));
    if (!scanner.setDeviceIp(targetMac, targetIp, mask, gw, true)) {
        m_lastError = "Failed to set IP address via DCP. (Is the device locked or still applying the name?)";
        scanner.disconnectFromInterface();
        setState(ArState::Error);
        return false;
    }
    emit messageLogged("IP address set successfully. Waiting 1s before starting AR...");

    scanner.disconnectFromInterface();

    // Small delay before starting AR to let the IP stack settle
    QThread::msleep(1000);
    
    emit messageLogged("Step 2: Starting AR connection...");
    setState(ArState::Connecting);
    m_phaseStep = 0;
    m_phaseTimer->start(500); // Faster transitions for simulation
    
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
                emit messageLogged("Phase 1: Connect request sent. Waiting for response...");
                int res = waitForResponse(0x0000); // 0x0000 for generic RPC/UDP response matching for now
                if (res >= 0) {
                    emit messageLogged("Phase 1: Connect response received.");
                    setState(ArState::Parameterizing);
                } else {
                    emit messageLogged("Phase 1: Connect timeout / failed.");
                    stop();
                }
            } else {
                emit messageLogged("Phase 1: Failed to send Connect.");
                stop();
            }
            break;
            
        case ArState::Parameterizing:
            if (sendRecordData()) {
                emit messageLogged("Phase 2: Configuration (DCP Signal) sent. Waiting for response...");
                int res = waitForResponse(0xFEFD, 0x11223344); // DCP Get/Set with specific XID
                if (res == 0 || res == 5) {
                    emit messageLogged("Phase 2: Configuration successful.");
                    setState(ArState::Running);
                    startCyclicExchange();
                } else {
                    emit messageLogged(QString("Phase 2: Configuration failed (Result: %1).").arg(res));
                    stop();
                }
            } else {
                emit messageLogged("Phase 2: Configuration failed to send.");
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
    uint8_t packet[80];
    memset(packet, 0, sizeof(packet));
    
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x08; packet[13] = 0x00; // IPv4
    
    packet[14] = 0x45; // Ver 4, IHL 5
    packet[16] = 0x00; packet[17] = sizeof(packet) - 14; 
    packet[18] = 0xAB; packet[19] = 0xCD;
    packet[22] = 0x40; packet[23] = 0x11; // UDP
    
    QStringList ipParts = m_targetIp.split('.');
    if (ipParts.size() == 4) {
        for (int i = 0; i < 3; ++i) packet[26+i] = (uint8_t)ipParts[i].toInt();
        packet[29] = 254; 
        for (int i = 0; i < 4; ++i) packet[30+i] = (uint8_t)ipParts[i].toInt();
    }
    
    uint16_t cksum = calculateIpChecksum(packet + 14, 20);
    packet[24] = (cksum >> 8); packet[25] = (cksum & 0xFF);
    
    packet[34] = 0x88; packet[35] = 0x94;
    packet[36] = 0x88; packet[37] = 0x94;
    uint16_t udpLen = sizeof(packet) - 14 - 20;
    packet[38] = (udpLen >> 8); packet[39] = (udpLen & 0xFF);
    
    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, sizeof(packet)) != 0) return false;
    return true;
}

bool ArExchangeManager::sendRecordData() {
    uint8_t packet[100];
    memset(packet, 0, sizeof(packet));
    
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x88; packet[13] = 0x92; // PN
    
    packet[14] = 0xFE; packet[15] = 0xFD; // Get/Set
    packet[16] = 0x04; packet[17] = 0x01; // Set Req
    packet[18] = 0x11; packet[19] = 0x22; packet[20] = 0x33; packet[21] = 0x44; // XID
    packet[24] = 0x00; packet[25] = 0x08;
    packet[26] = 0x05; packet[27] = 0x03; // Control/Signal
    packet[28] = 0x00; packet[29] = 0x04;
    packet[32] = 0x01; // Flash
    
    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 60) != 0) return false;
    return true;
}

uint16_t ArExchangeManager::calculateIpChecksum(const uint8_t* ipHeader, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2) sum += (ipHeader[i] << 8) | ipHeader[i + 1];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~((uint16_t)sum);
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

int ArExchangeManager::waitForResponse(uint16_t frameId, uint32_t xid, int timeoutMs) {
    if (!m_pcapHandle) return -1;

    struct pcap_pkthdr *header;
    const uint8_t *data;
    QElapsedTimer timer;
    timer.start();

    // Mapping for FrameIDs:
    // 0xFEFD: DCP Get/Set
    // 0x0000: Generic RPC (matching by IP/Port in sendRpcConnect)

    while (timer.elapsed() < timeoutMs) {
        int res = pcap_next_ex((pcap_t*)m_pcapHandle, &header, &data);
        if (res == 1) {
            if (header->caplen < 14) continue;

            const uint8_t *dest = data;
            const uint8_t *src = data + 6;
            uint16_t type = (data[12] << 8) | data[13];

            // Match based on destination MAC being US
            if (memcmp(dest, m_sourceMac, 6) != 0) continue;

            if (frameId == 0x0000) { // Looking for RPC response
                if (type == 0x0800) { // IPv4
                    // Simple check for UDP to our port 34964
                    if (data[23] == 0x11 && (data[36] << 8 | data[37]) == 34964) {
                        return 0; // Success
                    }
                }
            } else if (type == 0x8892) { // PROFINET
                uint16_t capturedFrameId = (data[14] << 8) | data[15];
                if (capturedFrameId == frameId) {
                    if (frameId == 0xFEFD) { // DCP
                        uint32_t capturedXid = (data[18] << 24) | (data[19] << 16) | (data[20] << 8) | data[21];
                        if (capturedXid == xid) {
                            // Logic similar to DcpScanner::waitForSetResponse
                            uint16_t dataLen = (data[24] << 8) | data[25];
                            if (dataLen >= 7) {
                                return data[32]; // BlockResult
                            }
                            return 0;
                        }
                    }
                }
            }
        }
        QCoreApplication::processEvents();
    }
    return -2; // Timeout
}

QString ArExchangeManager::macToString(const uint8_t *mac) {
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QChar('0'))
        .arg(mac[1], 2, 16, QChar('0'))
        .arg(mac[2], 2, 16, QChar('0'))
        .arg(mac[3], 2, 16, QChar('0'))
        .arg(mac[4], 2, 16, QChar('0'))
        .arg(mac[5], 2, 16, QChar('0')).toUpper();
}

} // namespace PNConfigLib
