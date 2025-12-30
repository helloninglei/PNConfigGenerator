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
    memset(m_targetMacBytes, 0, 6);
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
    m_phaseTimer->start(100); // Faster transitions for simulation
    
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
    m_phaseTimer->stop(); // Stop while processing to prevent overlaps
    switch (m_state) {
        case ArState::Connecting:
            if (sendRpcConnect()) {
                emit messageLogged("Phase 1: Connect request sent. Waiting for response...");
                int res = waitForResponse(0x0000); // 0x0000 for generic RPC/UDP response matching for now
                if (res >= 0) {
                    emit messageLogged("Phase 1: Connect response received.");
                    setState(ArState::Parameterizing);
                    m_phaseTimer->start(500); // Trigger next phase
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
            if (sendRpcControlParamEnd()) {
                emit messageLogged("Phase 2: ParamEnd sent. Waiting for response...");
                int res = waitForResponse(0x0001); // Special code to match RPC Control response
                if (res == 0) {
                    emit messageLogged("Phase 2: ParamEnd accepted by Slave.");
                    setState(ArState::AppReady);
                    startCyclicExchange(); // Start output data exchange immediately
                    m_phaseTimer->start(100);
                } else {
                    emit messageLogged(QString("Phase 2: ParamEnd rejected (Result: 0x%1).").arg(res, 8, 16, QChar('0')));
                    stop();
                }
            } else {
                emit messageLogged("Phase 2: Failed to send ParamEnd.");
                stop();
            }
            break;

        case ArState::AppReady:
            emit messageLogged("Phase 3: Waiting for ApplicationReady from Slave...");
            if (waitForResponse(0x0002) == 0) { // Special code to match CControl/AppReady request
                emit messageLogged("Phase 3: ApplicationReady received. AR established.");
                setState(ArState::Running);
                // Cyclic exchange already started in Phase 2
            } else {
                emit messageLogged("Phase 3: ApplicationReady timeout.");
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
    uint8_t packet[1024]; 
    memset(packet, 0, sizeof(packet));
    
    // Ethernet Header
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x08; packet[13] = 0x00; // IPv4
    
    // IP Header
    packet[14] = 0x45; // Version 4, IHL 5
    packet[18] = 0x12; packet[19] = 0x34; // Identification
    packet[22] = 0x40; packet[23] = 0x11; // TTL 64, Protocol UDP
    
    // IPs
    packet[26] = 192; packet[27] = 168; packet[28] = 0; packet[29] = 254; // Source
    QStringList ipParts = m_targetIp.split('.');
    if (ipParts.size() == 4) for (int i = 0; i < 4; ++i) packet[30+i] = (uint8_t)ipParts[i].toInt();
    
    // UDP Header
    packet[34] = 0x88; packet[35] = 0x94; // Source Port
    packet[36] = 0x88; packet[37] = 0x94; // Dest Port
    
    // DCE RPC Header (offset 42)
    uint8_t *rpc = packet + 42;
    rpc[0] = 4;        // RPC Version
    rpc[1] = 0;        // PDU Type: Request
    rpc[2] = 0x23;     // Flags1: Idempotent | Last Frag | First Frag
    rpc[3] = 0;        // Flags2
    rpc[4] = 0x00;     // Data Rep (Big Endian)
    rpc[5] = 0x00;
    rpc[6] = 0x00;
    rpc[7] = 0x00;
    memset(rpc + 8, 0, 16); // Object UUID (Zero for CM)
    
    // Interface UUID (DEA00001-6C97-11D1-8271)
    rpc[24] = 0xDE; rpc[25] = 0xA0; rpc[26] = 0x00; rpc[27] = 0x01;
    rpc[28] = 0x6C; rpc[29] = 0x97;
    rpc[30] = 0x11; rpc[31] = 0xD1;
    const uint8_t ifUuidTail[] = { 0x82, 0x71, 0x00, 0xA0, 0x24, 0x42, 0xDF, 0x7D };
    memcpy(rpc + 32, ifUuidTail, 8); 
    
    memset(rpc + 40, 0xAA, 16); // Activity UUID
    rpc[60] = 0x00; rpc[61] = 0x01; rpc[62] = 0x00; rpc[63] = 0x01; // Version 1.1
    static uint32_t seq = 100;
    rpc[64] = (seq >> 24); rpc[65] = (seq >> 16); rpc[66] = (seq >> 8); rpc[67] = (seq & 0xFF);
    rpc[68] = 0; rpc[69] = 0; // Opnum Connect
    rpc[70] = 0xFF; rpc[71] = 0xFF; // Hints
    rpc[72] = 0xFF; rpc[73] = 0xFF;
    rpc[76] = 0; rpc[77] = 0; // FragNum
    rpc[78] = 0; // Auth Proto
    rpc[79] = 0; // Serial Low

    // Body (offset 122)
    uint8_t *body = packet + 122;
    int offset = 0;

    // NDR Header (20 bytes)
    body[offset++] = 0; body[offset++] = 0; body[offset++] = 2; body[offset++] = 0; // Max (512)
    int ndrLenPos = offset; offset += 4;
    int arrayMaxPos = offset; offset += 4;
    body[offset++] = 0; body[offset++] = 0; body[offset++] = 0; body[offset++] = 0; // ArrayOffset
    int arrayActualPos = offset; offset += 4;

    int blocksStart = offset;

    // Block 1: ARBlockRequest (Type 0x0101)
    body[offset++] = 0x01; body[offset++] = 0x01; // Type
    int b1LenPos = offset; offset += 2;
    body[offset++] = 0x01; body[offset++] = 0x00; // Version
    body[offset++] = 0x00; body[offset++] = 0x01; // ARType
    static const uint8_t arUuid[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
    memcpy(body + offset, arUuid, 16); offset += 16;
    body[offset++] = 0x00; body[offset++] = 0x01; // Session Key
    memcpy(body + offset, m_sourceMac, 6); offset += 6;
    // Object UUID: DEA00000-6C97-11D1-8271-00A02442DF7D
    body[offset++] = 0xDE; body[offset++] = 0xA0; body[offset++] = 0x00; body[offset++] = 0x00;
    body[offset++] = 0x6C; body[offset++] = 0x97;
    body[offset++] = 0x11; body[offset++] = 0xD1;
    body[offset++] = 0x82; body[offset++] = 0x71;
    body[offset++] = 0x00; body[offset++] = 0xA0; body[offset++] = 0x24; body[offset++] = 0x42; body[offset++] = 0xDF; body[offset++] = 0x7D;
    body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x02; body[offset++] = 0x11; // Props
    body[offset++] = 0x00; body[offset++] = 0x64; // Timeout
    body[offset++] = 0x88; body[offset++] = 0x94; // Port
    QByteArray name = m_stationName.toUtf8();
    body[offset++] = (name.length() >> 8); body[offset++] = (name.length() & 0xFF);
    memcpy(body + offset, name.constData(), name.length()); offset += name.length();
    uint16_t b1Len = (uint16_t)(offset - b1LenPos - 2);
    body[b1LenPos] = (b1Len >> 8); body[b1LenPos+1] = (b1Len & 0xFF);

    // Block 2 & 3: IOCR (Input & Output)
    for (int i = 0; i < 2; ++i) {
        body[offset++] = 0x01; body[offset++] = 0x02;
        int bLenPos = offset; offset += 2;
        body[offset++] = 0x01; body[offset++] = 0x00; // Version
        body[offset++] = 0x00; body[offset++] = (i == 0 ? 0x01 : 0x02); // Type
        body[offset++] = 0x00; body[offset++] = (i == 0 ? 0x01 : 0x02); // Ref
        body[offset++] = 0x88; body[offset++] = 0x92; // LT
        body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x02; // Props (RT_CLASS_2)
        body[offset++] = 0x00; body[offset++] = 0x03; // DataLen 3 (1 data + 1 IOPS + 1 IOCS)
        body[offset++] = 0x80; body[offset++] = (i == 0 ? 0x01 : 0x02); // FrameID
        body[offset++] = 0x00; body[offset++] = 0x10; // Clock
        body[offset++] = 0x00; body[offset++] = 0x10; // RR
        body[offset++] = 0x00; body[offset++] = 0x01; // Phase
        body[offset++] = 0x00; body[offset++] = 0x00; // Sequence
        body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; // Offset
        body[offset++] = 0x00; body[offset++] = 0x03; // WD
        body[offset++] = 0x00; body[offset++] = 0x03; // DH
        body[offset++] = 0xC0; body[offset++] = 0x00; // Tag (Priority 6, VLAN 0)
        memset(body + offset, 0, 6); offset += 6; // Multicast
        body[offset++] = 0x00; body[offset++] = 0x01; // NbrAPIs
        body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; // API 0
        body[offset++] = 0x00; body[offset++] = 0x01; // NbrIOData
        body[offset++] = 0x00; body[offset++] = 0x02; // Slot 2
        body[offset++] = 0x00; body[offset++] = 0x01; // Subslot 1
        body[offset++] = 0x00; body[offset++] = 0x00; // FrameOffset 0
        body[offset++] = 0x00; body[offset++] = 0x01; // NbrIOCS
        body[offset++] = 0x00; body[offset++] = 0x02; // Slot 2
        body[offset++] = 0x00; body[offset++] = 0x01; // Subslot 1
        body[offset++] = 0x00; body[offset++] = 0x02; // FrameOffset 2 (Data 1 + IOPS 1)
        uint16_t bLen = (uint16_t)(offset - bLenPos - 2);
        body[bLenPos] = (bLen >> 8); body[bLenPos+1] = (bLen & 0xFF);
    }

    // Block 4: AlarmCR (Type 0x0103)
    body[offset++] = 0x01; body[offset++] = 0x03; // Type
    body[offset++] = 0x00; body[offset++] = 0x16; // Length 22 (2 bytes version + 20 bytes payload)
    body[offset++] = 0x01; body[offset++] = 0x00; // Version
    body[offset++] = 0x00; body[offset++] = 0x01; // AlarmCRType: TCP/UDP
    body[offset++] = 0x08; body[offset++] = 0x00; // LTField: 0x0800
    body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x02; // Properties: TransportUDP=1
    body[offset++] = 0x00; body[offset++] = 0x64; // TimeoutFactor: 100
    body[offset++] = 0x00; body[offset++] = 0x03; // Retries: 3
    body[offset++] = 0x00; body[offset++] = 0x01; // LocalAlarmReference
    body[offset++] = 0x00; body[offset++] = 0xC8; // MaxAlarmDataLength: 200
    body[offset++] = 0xC0; body[offset++] = 0x00; // TagHeaderHigh (Priority 6, VLAN 0)
    body[offset++] = 0xA0; body[offset++] = 0x00; // TagHeaderLow (Priority 5, VLAN 0)

    // Block 5: ExpectedSubmodule (Type 0x0104)
    body[offset++] = 0x01; body[offset++] = 0x04;
    int b5LenPos = offset; offset += 2;
    body[offset++] = 0x01; body[offset++] = 0x00; // Version
    body[offset++] = 0x00; body[offset++] = 0x01; // NbrAPIs
    body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; // API 0
    body[offset++] = 0x00; body[offset++] = 0x02; // Slot 2
    body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x32; // Ident 0x32 (Digital IO)
    body[offset++] = 0x00; body[offset++] = 0x00; // ModuleProps 0
    body[offset++] = 0x00; body[offset++] = 0x01; // NbrSubmodules 1
    body[offset++] = 0x00; body[offset++] = 0x01; // Subslot 1
    body[offset++] = 0x00; body[offset++] = 0x00; body[offset++] = 0x01; body[offset++] = 0x32; // Ident 0x132
    body[offset++] = 0x00; body[offset++] = 0x03; // SubmoduleProps (IO)
    body[offset++] = 0x00; body[offset++] = 0x01; // DataDirection (Descriptor 1: Input)
    body[offset++] = 0x00; body[offset++] = 0x01; // DataLength 1
    body[offset++] = 0x01; // LengthIOCS
    body[offset++] = 0x01; // LengthIOPS
    body[offset++] = 0x00; body[offset++] = 0x02; // DataDirection (Descriptor 2: Output)
    body[offset++] = 0x00; body[offset++] = 0x01; // DataLength 1
    body[offset++] = 0x01; // LengthIOCS
    body[offset++] = 0x01; // LengthIOPS
    uint16_t b5Len = (uint16_t)(offset - b5LenPos - 2);
    body[b5LenPos] = (b5Len >> 8); body[b5LenPos+1] = (b5Len & 0xFF);

    // Finalize NDR
    uint32_t totalBody = (uint32_t)offset;
    uint32_t totalBlocks = (uint32_t)(offset - blocksStart); // args_length should be blocks only
    
    // args_length (Position 4-7)
    body[ndrLenPos] = 0; body[ndrLenPos+1] = 0;
    body[ndrLenPos+2] = (totalBlocks >> 8); body[ndrLenPos+3] = (totalBlocks & 0xFF);
    
    // maximum_count (Position 8-11)
    body[arrayMaxPos] = 0; body[arrayMaxPos+1] = 0;
    body[arrayMaxPos+2] = (totalBlocks >> 8); body[arrayMaxPos+3] = (totalBlocks & 0xFF);
    
    // actual_count (Position 16-19)
    body[arrayActualPos] = 0; body[arrayActualPos+1] = 0;
    body[arrayActualPos+2] = (totalBlocks >> 8); body[arrayActualPos+3] = (totalBlocks & 0xFF);

    // Finalize RPC/UDP/IP
    rpc[74] = (offset >> 8); rpc[75] = (offset & 0xFF);
    uint16_t udpLen = (uint16_t)(8 + 80 + offset);
    packet[38] = (udpLen >> 8); packet[39] = (udpLen & 0xFF);
    uint16_t ipLen = (uint16_t)(20 + udpLen);
    packet[16] = (ipLen >> 8); packet[17] = (ipLen & 0xFF);
    uint16_t cksum = calculateIpChecksum(packet + 14, 20);
    packet[24] = (cksum >> 8); packet[25] = (cksum & 0xFF);

    return pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 14 + ipLen) == 0;
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

bool ArExchangeManager::sendRpcControlParamEnd() {
    uint8_t packet[1024]; 
    memset(packet, 0, sizeof(packet));
    
    // Ethernet Header
    QStringList macParts = m_targetMac.split(':');
    if (macParts.size() == 6) for (int i = 0; i < 6; ++i) packet[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(packet + 6, m_sourceMac, 6);
    packet[12] = 0x08; packet[13] = 0x00; // IPv4
    
    // IP Header
    packet[14] = 0x45; packet[18] = 0x55; packet[19] = 0x66; // Ident
    packet[22] = 0x40; packet[23] = 0x11; // TTL, UDP
    packet[26] = 192; packet[27] = 168; packet[28] = 0; packet[29] = 254; // Source
    QStringList ipParts = m_targetIp.split('.');
    if (ipParts.size() == 4) for (int i = 0; i < 4; ++i) packet[30+i] = (uint8_t)ipParts[i].toInt();
    
    // UDP Header
    packet[34] = 0x88; packet[35] = 0x94; // Source Port
    packet[36] = 0x88; packet[37] = 0x94; // Dest Port
    
    // DCE RPC Header (offset 42)
    uint8_t *rpc = packet + 42;
    rpc[0] = 4; rpc[1] = 0; // Request
    rpc[2] = 0x23; // Flags
    rpc[4] = 0x00; // Data Rep
    
    // Interface UUID (DEA00001-6C97-11D1-8271)
    rpc[24] = 0xDE; rpc[25] = 0xA0; rpc[26] = 0x00; rpc[27] = 0x01;
    rpc[28] = 0x6C; rpc[29] = 0x97;
    rpc[30] = 0x11; rpc[31] = 0xD1;
    const uint8_t ifUuidTail[] = { 0x82, 0x71, 0x00, 0xA0, 0x24, 0x42, 0xDF, 0x7D };
    memcpy(rpc + 32, ifUuidTail, 8); 
    
    memset(rpc + 40, 0xAA, 16); // Activity UUID
    rpc[60] = 0x00; rpc[61] = 0x01; rpc[62] = 0x00; rpc[63] = 0x01;
    rpc[64] = 0; rpc[65] = 0; rpc[66] = 0; rpc[67] = 101; // Seq
    rpc[68] = 0; rpc[69] = 4; // Opnum Control (0x0004)
    
    // Body (offset 122)
    uint8_t *body = packet + 122;
    int offset = 0;
    body[offset++] = 0; body[offset++] = 0; body[offset++] = 2; body[offset++] = 0; // NDR Max
    int ndrLenPos = offset; offset += 4;
    int arrayMaxPos = offset; offset += 4;
    body[offset++] = 0; body[offset++] = 0; body[offset++] = 0; body[offset++] = 0; // ArrayOffset
    int arrayActualPos = offset; offset += 4;

    int blocksStart = offset;

    // Block: ControlRequest (Type 0x0110)
    body[offset++] = 0x01; body[offset++] = 0x10; // ParamEnd
    body[offset++] = 0x00; body[offset++] = 0x1A; // Length 26
    body[offset++] = 0x01; body[offset++] = 0x00; // Version
    body[offset++] = 0x00; body[offset++] = 0x00; // Padding
    static const uint8_t arUuid[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
    memcpy(body + offset, arUuid, 16); offset += 16;
    body[offset++] = 0x00; body[offset++] = 0x01; // Session Key
    body[offset++] = 0x00; body[offset++] = 0x00; // Alarm Seq
    body[offset++] = 0x00; body[offset++] = 0x01; // Command: ParamEnd
    body[offset++] = 0x00; body[offset++] = 0x00; // Properties

    // Finalize NDR
    uint32_t totalBlocks = (uint32_t)(offset - blocksStart);
    body[ndrLenPos+2] = (totalBlocks >> 8); body[ndrLenPos+3] = (totalBlocks & 0xFF);
    body[arrayMaxPos+2] = (totalBlocks >> 8); body[arrayMaxPos+3] = (totalBlocks & 0xFF);
    body[arrayActualPos+2] = (totalBlocks >> 8); body[arrayActualPos+3] = (totalBlocks & 0xFF);

    rpc[74] = (offset >> 8); rpc[75] = (offset & 0xFF);
    uint16_t udpLen = (uint16_t)(8 + 80 + offset);
    packet[38] = (udpLen >> 8); packet[39] = (udpLen & 0xFF);
    uint16_t ipLen = (uint16_t)(20 + udpLen);
    packet[16] = (ipLen >> 8); packet[17] = (ipLen & 0xFF);
    uint16_t cksum = calculateIpChecksum(packet + 14, 20);
    packet[24] = (cksum >> 8); packet[25] = (cksum & 0xFF);

    return pcap_sendpacket((pcap_t*)m_pcapHandle, packet, 14 + ipLen) == 0;
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

    // PROFINET RT Header (FrameID 0x8002 for Master Output)
    packet[14] = 0x80; packet[15] = 0x02;

    // IO Data Object 1 (1 byte data + 1 byte IOPS)
    packet[16] = 0x00; // Data
    packet[17] = 0x80; // IOPS Good
    
    // IOCS for Slave Input (1 byte)
    packet[18] = 0x80; // IOCS Good

    // Footer (Positioned at the end of the 60-byte Ethernet packet)
    static uint16_t cycleCounter = 0;
    packet[56] = (cycleCounter >> 8) & 0xFF; // CycleCounter High
    packet[57] = cycleCounter & 0xFF;        // CycleCounter Low
    cycleCounter += 32;

    packet[58] = 0x35; // Data Status (Valid, Primary, OK)
    packet[59] = 0x00; // Transfer Status

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
                if (type == 0x0806) { // ARP
                    uint16_t op = (data[20] << 8 | data[21]);
                    if (op == 1) { // Request
                        // Target IP is at offset 38. Checking for 192.168.0.254
                        if (header->caplen >= 42 && data[38] == 192 && data[39] == 168 && data[40] == 0 && data[41] == 254) {
                            sendArpResponse(src, data + 28); // Requestor's IP is at 28
                        }
                    }
                }
                emit messageLogged(QString("  [Packet from Target] Dest: %1 Type: 0x%2")
                    .arg(macToString(dest)).arg(type, 4, 16, QChar('0')));
            }

            // Match based on destination MAC being US
            if (memcmp(dest, m_sourceMac, 6) != 0) continue;

            if (frameId == 0x0000 || frameId == 0x0001) { // Looking for RPC response
                if (type == 0x0800) { // IPv4
                    if (data[23] == 0x11) { // UDP
            if (header->caplen < 44) continue;
            uint8_t pduType = data[43]; // Offset 42+1 (PDU Type)
                        if (pduType == 2) { // Response
                            if (header->caplen >= 126) {
                                uint32_t retVal = (uint32_t(data[122]) << 24) | (uint32_t(data[123]) << 16) | (uint32_t(data[124]) << 8) | uint32_t(data[125]);
                                emit messageLogged(QString("  [RPC Response] Return Value: 0x%1").arg(retVal, 8, 16, QChar('0')));
                                if (retVal == 0) return 0; // Success
                                return (int)retVal; // Return error code
                            }
                        } else if (pduType == 3) {
                            emit messageLogged("  [RPC Fault] DCE RPC Fault received.");
                            return -4;
                        }
                    }
                }
            } else if (frameId == 0x0002) { // Looking for RPC Request (AppReady)
                if (type == 0x0800 && data[23] == 0x11) {
                    uint8_t pduType = data[43];
                    if (header->caplen >= 112) {
                        uint16_t opnum = (data[110] << 8) | data[111]; 
                        if (pduType == 0 && opnum == 4) { // Request, opnum Control (4)
                        emit messageLogged("  [RPC Request] Control Request (AppReady) received.");
                        // Send Control Response Pos
                        uint8_t resp[256];
                        memcpy(resp, data + 6, 6); // Dest = Original Src
                        memcpy(resp + 6, data, 6);   // Src = Original Dest
                        resp[12] = 0x08; resp[13] = 0x00;
                        memcpy(resp + 14, data + 14, 20); // Copy IP header template
                        resp[26] = data[30]; resp[27] = data[31]; resp[28] = data[32]; resp[29] = data[33];
                        resp[30] = data[26]; resp[31] = data[27]; resp[32] = data[28]; resp[33] = data[29];
                        resp[34] = data[36]; resp[35] = data[37]; resp[36] = data[34]; resp[37] = data[35];
                        uint8_t *r_rpc = resp + 42;
                        memcpy(r_rpc, data + 42, 80);
                        r_rpc[1] = 2; // Response
                        r_rpc[68] = 0; r_rpc[69] = 4; // Set Opnum 4 in response too
                        uint8_t *r_body = resp + 122;
                        memset(r_body, 0, 32); 
                        r_body[2] = 0x00; r_body[3] = 0x08; // NDR Len 8
                        
                        // Recalculate Body Len in RPC and IP Header
                        uint16_t r_bodyLen = 80 + 8; // RPC Header + Body
                        resp[38] = ((r_bodyLen + 8) >> 8); resp[39] = ((r_bodyLen + 8) & 0xFF); // UDP Len
                        uint16_t r_ipLen = r_bodyLen + 8 + 20;
                        resp[16] = (r_ipLen >> 8); resp[17] = (r_ipLen & 0xFF);
                        resp[24] = 0; resp[25] = 0;
                        uint16_t r_cksum = calculateIpChecksum(resp + 14, 20);
                        resp[24] = (r_cksum >> 8); resp[25] = (r_cksum & 0xFF);
                        
                        pcap_sendpacket((pcap_t*)m_pcapHandle, resp, 14 + r_ipLen);
                        return 0;
                    }
                }
            }
        } else if (frameId != 0xFEFD && type == 0x8892) { // PROFINET (not DCP matching branch)
                if (header->caplen >= 16) {
                    uint16_t capturedFrameId = (data[14] << 8) | data[15];
                    emit messageLogged(QString("  [PN Packet Captured] FrameID: 0x%1").arg(capturedFrameId, 4, 16, QChar('0')));
                    if (capturedFrameId == frameId) return 0;
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

bool ArExchangeManager::sendArpResponse(const uint8_t* targetMac, const uint8_t* targetIp) {
    if (!m_pcapHandle) return false;
    uint8_t packet[42];
    memset(packet, 0, sizeof(packet));

    // Ethernet Header
    memcpy(packet, targetMac, 6);       // Dest MAC
    memcpy(packet + 6, m_sourceMac, 6); // Source MAC
    packet[12] = 0x08; packet[13] = 0x06; // ARP type

    // ARP Header
    packet[14] = 0x00; packet[15] = 0x01; // Hardware: Ethernet
    packet[16] = 0x08; packet[17] = 0x00; // Protocol: IPv4
    packet[18] = 0x06;                    // Hardware Size
    packet[19] = 0x04;                    // Protocol Size
    packet[20] = 0x00; packet[21] = 0x02; // Opcode: Reply

    // Sender MAC/IP (Master)
    memcpy(packet + 22, m_sourceMac, 6);
    packet[28] = 192; packet[29] = 168; packet[30] = 0; packet[31] = 254;

    // Target MAC/IP (Slave)
    memcpy(packet + 32, targetMac, 6);
    memcpy(packet + 38, targetIp, 4);

    if (pcap_sendpacket((pcap_t*)m_pcapHandle, packet, sizeof(packet)) != 0) return false;
    emit messageLogged(QString("  [ARP Reply Out] To Slave for 192.168.0.254"));
    return true;
}

} // namespace PNConfigLib
