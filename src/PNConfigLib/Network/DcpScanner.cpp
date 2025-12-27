#include <QtEndian>
#include <cstring>
#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QNetworkInterface>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif
#include "DcpScanner.h"

namespace PNConfigLib {

DcpScanner::DcpScanner(QObject *parent) : QObject(parent) {}

DcpScanner::~DcpScanner() {
    disconnectFromInterface();
}

QList<InterfaceInfo> DcpScanner::getAvailableInterfaces() {
    QList<InterfaceInfo> interfaces;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs;
    pcap_if_t *d;

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        qCritical() << "Error in pcap_findalldevs:" << errbuf;
        return interfaces;
    }

    for (d = alldevs; d; d = d->next) {
        InterfaceInfo info;
        info.name = QString::fromLocal8Bit(d->name);
        if (d->description) {
            info.description = QString::fromLocal8Bit(d->description);
        } else {
            info.description = info.name;
        }
        interfaces.append(info);
    }

    pcap_freealldevs(alldevs);
    return interfaces;
}



bool DcpScanner::connectToInterface(const QString &interfaceName) {
    if (m_isConnected) {
        disconnectFromInterface();
    }
    
    char errbuf[PCAP_ERRBUF_SIZE];
    m_pcapHandle = pcap_open_live(interfaceName.toStdString().c_str(), 65536, 1, 10, errbuf);
    if (!m_pcapHandle) {
        qCritical() << "Error opening adapter:" << errbuf << "for interface:" << interfaceName;
        return false;
    }

    // Resolve source MAC address using Windows API (most reliable for GUID matching)
    memset(m_sourceMac, 0, 6);
    QString targetGuid = interfaceName;
    QString targetDescription;
    int bStart = targetGuid.indexOf('{');
    if (bStart >= 0) {
        targetGuid = targetGuid.mid(bStart).toUpper();
    } else {
        targetGuid = targetGuid.toUpper();
    }

    // Get description from Npcap as a fallback search term
    char errbuf_devs[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs;
    if (pcap_findalldevs(&alldevs, errbuf_devs) != -1) {
        for (pcap_if_t *d = alldevs; d; d = d->next) {
            if (interfaceName == QString::fromLocal8Bit(d->name)) {
                if (d->description) targetDescription = QString::fromLocal8Bit(d->description).toUpper();
                break;
            }
        }
        pcap_freealldevs(alldevs);
    }
    
#if defined(_WIN32) || defined(WIN32) || defined(Q_OS_WIN)
    ULONG bufLen = 15000;
    PIP_ADAPTER_ADDRESSES addresses = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
    ULONG ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, addresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, addresses, &bufLen);
    }
    
    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = addresses; curr; curr = curr->Next) {
            QString adapterName = QString::fromLocal8Bit(curr->AdapterName).toUpper();
            QString adapterDesc = QString::fromWCharArray(curr->Description).toUpper();

            if (adapterName.contains(targetGuid) || targetGuid.contains(adapterName) ||
                (!targetDescription.isEmpty() && (adapterDesc.contains(targetDescription) || targetDescription.contains(adapterDesc)))) {
                
                if (curr->PhysicalAddressLength == 6) {
                    memcpy(m_sourceMac, curr->PhysicalAddress, 6);
                    qDebug() << "Resolved Source MAC via Windows API:" << macToString(m_sourceMac) 
                             << "(" << QString::fromWCharArray(curr->Description) << ")";
                    break;
                }
            }
        }
    }
    free(addresses);
#endif

    // Fallback to Qt matching
    if (m_sourceMac[0] == 0 && m_sourceMac[1] == 0 && m_sourceMac[2] == 0 &&
        m_sourceMac[3] == 0 && m_sourceMac[4] == 0 && m_sourceMac[5] == 0) {
        
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const auto &iface : interfaces) {
            QString ifName = iface.name().toUpper();
            QString ifHuman = iface.humanReadableName().toUpper();

            if (ifName.contains(targetGuid) || targetGuid.contains(ifName) ||
                (!targetDescription.isEmpty() && (ifHuman.contains(targetDescription) || targetDescription.contains(ifHuman)))) {
                
                QString macStr = iface.hardwareAddress();
                QString hexMac;
                for (const QChar &c : macStr) {
                    if (c.isDigit() || (c.toUpper() >= 'A' && c.toUpper() <= 'F')) hexMac.append(c);
                }
                if (hexMac.length() == 12) {
                    for (int i = 0; i < 6; ++i) m_sourceMac[i] = (uint8_t)hexMac.mid(i * 2, 2).toInt(nullptr, 16);
                    qDebug() << "Resolved Source MAC via Qt:" << macToString(m_sourceMac) 
                             << "(" << iface.humanReadableName() << ")";
                    break;
                }
            }
        }
    }

    if (m_sourceMac[0] == 0 && m_sourceMac[1] == 0 && m_sourceMac[2] == 0 &&
        m_sourceMac[3] == 0 && m_sourceMac[4] == 0 && m_sourceMac[5] == 0) {
        qWarning() << "CRITICAL: Could not resolve Source MAC for interface" << interfaceName 
                   << ". Please select a PHYSICAL adapter (not WAN Miniport) for PROFINET.";
    }

    // Set BPF filter to capture PROFINET (0x8892)
    struct bpf_program fcode;
    if (pcap_compile(m_pcapHandle, &fcode, "ether proto 0x8892", 1, PCAP_NETMASK_UNKNOWN) == -1) {
        qWarning() << "Error compiling BPF filter:" << pcap_geterr(m_pcapHandle);
    } else {
        if (pcap_setfilter(m_pcapHandle, &fcode) == -1) {
            qWarning() << "Error setting BPF filter:" << pcap_geterr(m_pcapHandle);
        }
        pcap_freecode(&fcode);
    }

    m_interfaceName = interfaceName;
    m_isConnected = true;
    qDebug() << "Connected to interface:" << interfaceName;
    return true;
}

void DcpScanner::disconnectFromInterface() {
    if (m_isConnected && m_pcapHandle) {
        pcap_close(m_pcapHandle);
        m_pcapHandle = nullptr;
        m_isConnected = false;
        qDebug() << "Disconnected from interface:" << m_interfaceName;
        m_interfaceName.clear();
        memset(m_sourceMac, 0, 6);
    }
}

QList<DiscoveredDevice> DcpScanner::scan() {
    QList<DiscoveredDevice> devices;
    if (!m_isConnected || !m_pcapHandle) {
        return devices;
    }

    uint8_t packet[60];
    memset(packet, 0, sizeof(packet));

    EthernetHeader *eth = (EthernetHeader*)packet;
    memcpy(eth->dest, "\x01\x0e\xcf\x00\x00\x00", 6);
    memcpy(eth->src, m_sourceMac, 6);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFE);
    dcp->serviceId = 0x05;
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x12345678);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4);

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0xFF; // All
    block->suboption = 0xFF; // All
    block->length = 0x0000;

    qDebug() << "Sending DCP Identify multicast request (Source MAC:" << macToString(m_sourceMac) << ")...";

    if (pcap_sendpacket(m_pcapHandle, packet, sizeof(packet)) != 0) {
        qCritical() << "Error sending DCP Identify request:" << pcap_geterr(m_pcapHandle);
        return devices;
    }

    struct pcap_pkthdr *header;
    const uint8_t *data;
    int res;
    
    QElapsedTimer timer;
    timer.start();
    
    int capturedCount = 0;
    // Capture for 5 seconds (to allow for ResponseDelay)
    while (timer.elapsed() < 5000) {
        res = pcap_next_ex(m_pcapHandle, &header, &data);
        if (res == 1) {
            capturedCount++;
            parseDcpPacket(data, header->caplen, devices);
        } else if (res == -1) {
            qCritical() << "Error reading packet:" << pcap_geterr(m_pcapHandle);
            break;
        } else if (res == 0) {
            // Timeout - this is expected if no packets arrive
        }
        
        QCoreApplication::processEvents(); 
    }

    qDebug() << "Scan completed. Received" << capturedCount << "PROFINET packets. Total unique devices found:" << devices.size();
    return devices;
}

void DcpScanner::parseDcpPacket(const uint8_t *data, int len, QList<DiscoveredDevice> &devices) {
    if (len < (int)(sizeof(EthernetHeader) + sizeof(DcpHeader))) return;

    EthernetHeader *eth = (EthernetHeader*)data;
    uint16_t type = qFromBigEndian<uint16_t>(eth->type);
    
    DcpHeader *dcp = (DcpHeader*)(data + sizeof(EthernetHeader));
    uint16_t frameId = qFromBigEndian<uint16_t>(dcp->frameId);
    
    // 0xFEFF is Identify Response
    if (frameId != 0xFEFF) {
        return;
    }

    // Some devices use ServiceType 0x01 (Success) instead of 0x02 (Response)
    if (dcp->serviceId != 0x05) {
        return;
    }

    DiscoveredDevice device;
    device.macAddress = macToString(eth->src);
    qDebug() << "Parsing DCP Identify Response from MAC:" << device.macAddress;

    // Check if we already have this device to merge info
    int deviceIndex = -1;
    for (int i = 0; i < devices.size(); ++i) {
        if (devices[i].macAddress == device.macAddress) {
            deviceIndex = i;
            break;
        }
    }

    int offset = sizeof(EthernetHeader) + sizeof(DcpHeader);
    int dcpDataLen = qFromBigEndian<uint16_t>(dcp->dcpDataLength);
    int remaining = dcpDataLen;

    while (remaining >= 4) {
        DcpBlockHeader *block = (DcpBlockHeader*)(data + offset);
        int blockLen = qFromBigEndian<uint16_t>(block->length);
        
        // Ensure we don't read past the packet
        if (offset + 4 + blockLen > len) break;

        // Pointer to data after Block Header (Option/Suboption/Length)
        const uint8_t *val = data + offset + 4;
        
        // PROFINET DCP Blocks always start with 2 bytes of BlockInfo (Status), skip them
        const uint8_t *payload = val + 2;
        int payloadLen = blockLen - 2;

        if (block->option == 0x01) { // IP Parameters
            // Suboption 1 (standard) or 2 (suite) both can contain IP if length >= 14
            if ((block->suboption == 0x01 || block->suboption == 0x02) && blockLen >= 14) {
                device.ipAddress = QString("%1.%2.%3.%4").arg(payload[0]).arg(payload[1]).arg(payload[2]).arg(payload[3]);
                device.subnetMask = QString("%1.%2.%3.%4").arg(payload[4]).arg(payload[5]).arg(payload[6]).arg(payload[7]);
                device.gateway = QString("%1.%2.%3.%4").arg(payload[8]).arg(payload[9]).arg(payload[10]).arg(payload[11]);
            }
        } else if (block->option == 0x02) { // Device Properties
            if (block->suboption == 0x01 && payloadLen > 0) { // Type of Station
                QString type = QString::fromUtf8((const char*)payload, payloadLen).trimmed();
                if (!type.isEmpty() && type[0] != '\0') device.deviceType = type;
            } else if (block->suboption == 0x02 && payloadLen > 0) { // Name of Station
                QString name = QString::fromUtf8((const char*)payload, payloadLen).trimmed();
                if (!name.isEmpty() && name[0] != '\0') device.deviceName = name;
            } else if (block->suboption == 0x03 && blockLen == 6) { // Vendor/Device ID
                device.vendorId = (payload[0] << 8) | payload[1];
                device.deviceId = (payload[2] << 8) | payload[3];
            }
        }

        int advance = 4 + blockLen;
        if (advance % 2 != 0) advance++;
        offset += advance;
        remaining -= advance;
    }

    if (deviceIndex >= 0) {
        // Merge into existing device
        auto &d = devices[deviceIndex];
        if (!device.deviceName.isEmpty()) d.deviceName = device.deviceName;
        if (!device.deviceType.isEmpty()) d.deviceType = device.deviceType;
        if (device.ipAddress != "0.0.0.0") d.ipAddress = device.ipAddress;
        if (device.subnetMask != "0.0.0.0") d.subnetMask = device.subnetMask;
        if (device.gateway != "0.0.0.0") d.gateway = device.gateway;
        if (device.vendorId != 0) d.vendorId = device.vendorId;
        if (device.deviceId != 0) d.deviceId = device.deviceId;
    } else {
        devices.append(device);
    }
}

bool DcpScanner::setDeviceIp(const QString &mac, const QString &ip, const QString &mask, const QString &gw, bool permanent) {
    if (!m_isConnected || !m_pcapHandle) return false;

    QStringList macParts = mac.split(':');
    if (macParts.size() != 6) return false;

    uint8_t packet[80];
    memset(packet, 0, sizeof(packet));

    EthernetHeader *eth = (EthernetHeader*)packet;
    for (int i = 0; i < 6; ++i) eth->dest[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(eth->src, m_sourceMac, 6);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFD);
    dcp->serviceId = 0x04; // Set Request (was 0x03 which is Get)
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x87654321);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4 + 14); // Header + Payload

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0x01;    // IP
    block->suboption = 0x02; // IP suite
    block->length = qToBigEndian<uint16_t>(14);

    QStringList ipParts = ip.split('.');
    QStringList maskParts = mask.split('.');
    QStringList gwParts = gw.split('.');
    
    uint8_t *val = packet + sizeof(EthernetHeader) + sizeof(DcpHeader) + 4;
    val[0] = 0x00; val[1] = permanent ? 0x02 : 0x01; // 0x01: Temporary, 0x02: Permanent
    if (ipParts.size() == 4) for (int i = 0; i < 4; ++i) val[2+i] = (uint8_t)ipParts[i].toInt();
    if (maskParts.size() == 4) for (int i = 0; i < 4; ++i) val[6+i] = (uint8_t)maskParts[i].toInt();
    if (gwParts.size() == 4) for (int i = 0; i < 4; ++i) val[10+i] = (uint8_t)gwParts[i].toInt();

    qDebug() << "Sending DCP Set IP request (Source MAC:" << macToString(m_sourceMac) << ")...";
    if (pcap_sendpacket(m_pcapHandle, packet, 60) != 0) {
        qCritical() << "Error sending DCP Set IP request:" << pcap_geterr(m_pcapHandle);
        return false;
    }

    return true;
}

bool DcpScanner::setDeviceName(const QString &mac, const QString &name, bool permanent) {
    if (!m_isConnected || !m_pcapHandle) return false;

    QStringList macParts = mac.split(':');
    if (macParts.size() != 6) return false;

    QByteArray nameData = name.toUtf8();
    int blockLen = nameData.size() + 2; // suboption + 0x00 reserved
    if (blockLen % 2 != 0) blockLen++; // Padding

    int totalLen = sizeof(EthernetHeader) + sizeof(DcpHeader) + 4 + blockLen;
    if (totalLen < 60) totalLen = 60; // Padding for minimum Ethernet frame size

    QByteArray packet(totalLen, 0);
    uint8_t *pktData = (uint8_t*)packet.data();

    EthernetHeader *eth = (EthernetHeader*)pktData;
    for (int i = 0; i < 6; ++i) eth->dest[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(eth->src, m_sourceMac, 6);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(pktData + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFD);
    dcp->serviceId = 0x04; // Set Request (was 0x03 which is Get)
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x12348765);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4 + blockLen);

    DcpBlockHeader *block = (DcpBlockHeader*)(pktData + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0x02;    // Device Properties
    block->suboption = 0x02; // Name of Station
    block->length = qToBigEndian<uint16_t>(nameData.size() + 2); 
    
    uint8_t *val = pktData + sizeof(EthernetHeader) + sizeof(DcpHeader) + 4;
    val[0] = 0x00; val[1] = permanent ? 0x02 : 0x00; // Block Qualifier: bit 1 is permanent, 0x00 is none (temporary)
    memcpy(val + 2, nameData.constData(), nameData.size());

    qDebug() << "Sending DCP Set Name request (Source MAC:" << macToString(m_sourceMac) << ", Name:" << name << ", Permanent:" << permanent << ")...";
    if (pcap_sendpacket(m_pcapHandle, (const uint8_t*)pktData, totalLen) != 0) {
        qCritical() << "Error sending DCP Set Name request:" << pcap_geterr(m_pcapHandle);
        return false;
    }

    return true;
}

bool DcpScanner::resetFactory(const QString &mac) {
    if (!m_isConnected || !m_pcapHandle) return false;

    QStringList macParts = mac.split(':');
    if (macParts.size() != 6) return false;

    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    EthernetHeader *eth = (EthernetHeader*)packet;
    for (int i = 0; i < 6; ++i) eth->dest[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(eth->src, m_sourceMac, 6);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFD);
    dcp->serviceId = 0x04; // Set Request (was 0x03 which is Get)
    dcp->serviceType = 0x01; // Request
    dcp->xid = qToBigEndian<uint32_t>(0x11223344);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4 + 4); // Block Header + Qualifier

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0x05; // Control Option
    block->suboption = 0x06; // Factory Reset
    block->length = qToBigEndian<uint16_t>(2); // Qualifier only

    uint8_t *val = packet + sizeof(EthernetHeader) + sizeof(DcpHeader) + 4;
    val[0] = 0x00; val[1] = 0x00; // Reserved/Qualifier for Factory Reset

    if (pcap_sendpacket(m_pcapHandle, packet, 60) != 0) {
        qCritical() << "Error sending DCP Factory Reset request:" << pcap_geterr(m_pcapHandle);
        return false;
    }

    return true;
}

bool DcpScanner::flashLed(const QString &mac) {
    if (!m_isConnected || !m_pcapHandle) return false;

    QStringList macParts = mac.split(':');
    if (macParts.size() != 6) return false;

    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    EthernetHeader *eth = (EthernetHeader*)packet;
    for (int i = 0; i < 6; ++i) eth->dest[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    memcpy(eth->src, m_sourceMac, 6);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFD);
    dcp->serviceId = 0x04; // Set Request (was 0x03 which is Get)
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x55AA55AA);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4 + 4); // Block header + payload

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0x05;    // Control (Corrected from 0x03)
    block->suboption = 0x03; // Signal
    block->length = qToBigEndian<uint16_t>(4); 
                                              // Payload: 2 bytes Reserved (0x0000) + 2 bytes Value

    uint8_t *val = packet + sizeof(EthernetHeader) + sizeof(DcpHeader) + 4;
    val[0] = 0x00; val[1] = 0x00; // Reserved
    val[2] = 0x01; val[3] = 0x00; // Signal value (Flash once)

    if (pcap_sendpacket(m_pcapHandle, packet, 60) != 0) { // Send at least 60 bytes
        qCritical() << "Error sending DCP Flash LED request:" << pcap_geterr(m_pcapHandle);
        return false;
    }

    return true;
}

QString DcpScanner::macToString(const uint8_t *mac) {
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QChar('0'))
        .arg(mac[1], 2, 16, QChar('0'))
        .arg(mac[2], 2, 16, QChar('0'))
        .arg(mac[3], 2, 16, QChar('0'))
        .arg(mac[4], 2, 16, QChar('0'))
        .arg(mac[5], 2, 16, QChar('0')).toUpper();
}

} // namespace PNConfigLib
