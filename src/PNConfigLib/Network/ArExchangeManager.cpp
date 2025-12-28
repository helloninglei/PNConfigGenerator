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

    // Initialize target MAC bytes for fast matching
    QStringList parts = m_targetMac.split(':');
    if (parts.size() == 6) {
        for (int i = 0; i < 6; ++i) m_targetMacBytes[i] = (uint8_t)parts[i].toInt(nullptr, 16);
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

    // Open local pcap handle AFTER DCP is done to avoid interference
    char errbuf[PCAP_ERRBUF_SIZE];
    m_pcapHandle = pcap_open_live(interfaceName.toStdString().c_str(), 65536, 1, 10, errbuf);
    if (!m_pcapHandle) {
        m_lastError = QString("Failed to open adapter after DCP: %1").arg(errbuf);
        setState(ArState::Error);
        return false;
    }

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
    uint8_t packet[160]; 
    memset(packet, 0, sizeof(packet));
    
    // Ethernet & IP Headers (same as before)
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x08; packet[13] = 0x00;
    
    packet[14] = 0x45;
    uint16_t ipTotLen = sizeof(packet) - 14;
    packet[16] = (ipTotLen >> 8); packet[17] = (ipTotLen & 0xFF);
    packet[18] = 0xAB; packet[19] = 0xCD;
    packet[22] = 0x40; packet[23] = 0x11;
    
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
    
    // --- DCE RPC Header (starts at 42) ---
    uint8_t *rpc = packet + 42;
    rpc[0] = 4;        // RPC Version
    rpc[1] = 0;        // PDU Type: Request
    rpc[2] = 0x20;     // Flags1: Idempotent
    rpc[3] = 0;        // Flags2
    rpc[4] = 0x10;     // Data Rep (Little Endian, IEEE Float, ASCII)
    rpc[8] = 0;        // Serial High
    
    // Interface UUID: DEA00001-6C97-11D1-8271-00A02442DF7D 
    // PROFINET standard: first three fields are LITTLE ENDIAN in the wire packet.
    // DEA00001 -> 01 00 A0 DE (Wait, DE A0 00 01)
    // Actually, DCE RPC defines UUID as: 4-2-2-2-6
    // DEA00001 (4 bytes) - 6C97 (2 bytes) - 11D1 (2 bytes) ...
    // In p-net: 0x01, 0x00, 0xA0, 0xDE, 0x97, 0x6C, 0xD1, 0x11 ...
    const uint8_t ifUuid[] = { 0x01, 0x00, 0xA0, 0xDE, 0x97, 0x6C, 0xD1, 0x11, 0x82, 0x71, 0x00, 0xA0, 0x24, 0x42, 0xDF, 0x7D };
    
    // Interface UUID starts at offset 24 in RPC packet (42 + 24 = 66 in Ethernet)
    memcpy(rpc + 24, ifUuid, 16); 
    // Activity UUID (often all 0s or random) starts at offset 8
    memset(rpc + 8, 0xAA, 16); // Random activity ID
    
    // Sequence Number starts at offset 64
    static uint32_t seq = 100;
    rpc[64] = (seq & 0xFF); rpc[65] = ((seq >> 8) & 0xFF); rpc[66] = 0; rpc[67] = 0;
    
    // Opnum starts at offset 68
    rpc[68] = 0; rpc[69] = 0; // Opnum 0 (Connect)
    
    // Interface Version starts at offset 52
    rpc[52] = 0x01; rpc[53] = 0x00; // Ver 1.0

    // Body Length is at offset 74
    // We'll send a 64-byte body containing an ARBlockRequest
    uint16_t bodyLen = 64; 
    rpc[74] = (bodyLen & 0xFF); rpc[75] = ((bodyLen >> 8) & 0xFF);

    // Fragment Number is at offset 76
    rpc[76] = 0; rpc[77] = 0;

    // --- DCE RPC Body (starts at 42 + 80 = 122) ---
    uint8_t *body = packet + 122;
    // ARBlockRequest (Type 0x0101)
    body[0] = 0x01; body[1] = 0x01; // Type
    body[2] = 0x00; body[3] = 0x3C; // Length (60 bytes following)
    body[4] = 0x01; body[5] = 0x00; // Version 1.0
    body[6] = 0x00; body[7] = 0x01; // ARType: IO-AR
    // ARUUID (16 bytes)
    static const uint8_t arUuid[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    memcpy(body + 8, arUuid, 16);
    // SessionKey, CM-Initiator-MAC etc. (rest is just filler for now)
    memcpy(body + 24, m_sourceMac, 6);

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

            // MATCH ALL PACKETS FROM TARGET FOR LOGGING
            if (memcmp(src, m_targetMacBytes, 6) == 0) {
                emit messageLogged(QString("  [Packet from Target] Dest: %1 Type: 0x%2")
                    .arg(macToString(dest)).arg(type, 4, 16, QChar('0')));
            }

            // Match based on destination MAC being US
            if (memcmp(dest, m_sourceMac, 6) != 0) continue;

            if (frameId == 0x0000) { // Looking for RPC response
                if (type == 0x0800) { // IPv4
                    uint16_t destPort = (data[36] << 8 | data[37]);
                    if (data[23] == 0x11) { // UDP
                        emit messageLogged(QString("  [RPC Packet Captured] DestPort: %1 Len: %2").arg(destPort).arg(header->caplen));
                        if (destPort == 34964) return 0; // Success
                    }
                }
            } else if (type == 0x8892) { // PROFINET
                uint16_t capturedFrameId = (data[14] << 8) | data[15];
                emit messageLogged(QString("  [PN Packet Captured] FrameID: 0x%1").arg(capturedFrameId, 4, 16, QChar('0')));
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
