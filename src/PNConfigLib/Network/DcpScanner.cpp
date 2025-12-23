#include <QtEndian>
#include <cstring>
#include <QStringList>
#include "DcpScanner.h"
#include <QDebug>

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
    m_pcapHandle = pcap_open_live(interfaceName.toStdString().c_str(), 65536, 1, 100, errbuf);
    if (!m_pcapHandle) {
        qCritical() << "Error opening adapter:" << errbuf << "for interface:" << interfaceName;
        return false;
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
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFE);
    dcp->serviceId = 0x05;
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x12345678);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(4);

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0xFF;
    block->suboption = 0xFF;
    block->length = 0x0000;

    if (pcap_sendpacket(m_pcapHandle, packet, sizeof(packet)) != 0) {
        qCritical() << "Error sending DCP Identify request:" << pcap_geterr(m_pcapHandle);
        return devices;
    }

    struct pcap_pkthdr *header;
    const uint8_t *data;
    int res;
    
    for (int i = 0; i < 20; ++i) {
        res = pcap_next_ex(m_pcapHandle, &header, &data);
        if (res == 1) {
            parseDcpPacket(data, header->caplen, devices);
        } else if (res == -1) {
            qCritical() << "Error reading packet:" << pcap_geterr(m_pcapHandle);
            break;
        }
    }

    return devices;
}

void DcpScanner::parseDcpPacket(const uint8_t *data, int len, QList<DiscoveredDevice> &devices) {
    if (len < (int)(sizeof(EthernetHeader) + sizeof(DcpHeader))) return;

    EthernetHeader *eth = (EthernetHeader*)data;
    if (qFromBigEndian<uint16_t>(eth->type) != 0x8892) return;

    DcpHeader *dcp = (DcpHeader*)(data + sizeof(EthernetHeader));
    if (qFromBigEndian<uint16_t>(dcp->frameId) != 0xFEFF) return;
    if (dcp->serviceId != 0x05 || dcp->serviceType != 0x02) return;

    DiscoveredDevice device;
    device.macAddress = macToString(eth->src);

    int offset = sizeof(EthernetHeader) + sizeof(DcpHeader);
    int dcpDataLen = qFromBigEndian<uint16_t>(dcp->dcpDataLength);
    int remaining = dcpDataLen;

    while (remaining >= 4) {
        DcpBlockHeader *block = (DcpBlockHeader*)(data + offset);
        int blockLen = qFromBigEndian<uint16_t>(block->length);
        const uint8_t *val = data + offset + 4;

        if (block->option == 0x02) {
            if (block->suboption == 0x01 && blockLen >= 12) {
                device.ipAddress = QString("%1.%2.%3.%4").arg(val[2]).arg(val[3]).arg(val[4]).arg(val[5]);
                device.subnetMask = QString("%1.%2.%3.%4").arg(val[6]).arg(val[7]).arg(val[8]).arg(val[9]);
                device.gateway = QString("%1.%2.%3.%4").arg(val[10]).arg(val[11]).arg(val[12]).arg(val[13]);
            }
        } else if (block->option == 0x01) {
            if (block->suboption == 0x02) {
                device.deviceName = QString::fromUtf8((const char*)val, blockLen);
            } else if (block->suboption == 0x01) {
                 device.deviceType = QString::fromUtf8((const char*)val, blockLen);
            }
        }

        int advance = 4 + blockLen;
        if (advance % 2 != 0) advance++;
        offset += advance;
        remaining -= advance;
    }

    bool found = false;
    for (const auto &d : devices) {
        if (d.macAddress == device.macAddress) {
            found = true;
            break;
        }
    }
    if (!found) {
        devices.append(device);
    }
}

bool DcpScanner::setDeviceIp(const QString &mac, const QString &ip, const QString &mask, const QString &gw) {
    if (!m_isConnected || !m_pcapHandle) return false;

    QStringList macParts = mac.split(':');
    if (macParts.size() != 6) return false;

    uint8_t packet[80];
    memset(packet, 0, sizeof(packet));

    EthernetHeader *eth = (EthernetHeader*)packet;
    for (int i = 0; i < 6; ++i) eth->dest[i] = (uint8_t)macParts[i].toInt(nullptr, 16);
    eth->type = qToBigEndian<uint16_t>(0x8892);

    DcpHeader *dcp = (DcpHeader*)(packet + sizeof(EthernetHeader));
    dcp->frameId = qToBigEndian<uint16_t>(0xFEFD);
    dcp->serviceId = 0x03;
    dcp->serviceType = 0x01;
    dcp->xid = qToBigEndian<uint32_t>(0x87654321);
    dcp->responseDelay = qToBigEndian<uint16_t>(0x00FF);
    dcp->dcpDataLength = qToBigEndian<uint16_t>(16);

    DcpBlockHeader *block = (DcpBlockHeader*)(packet + sizeof(EthernetHeader) + sizeof(DcpHeader));
    block->option = 0x02;
    block->suboption = 0x01;
    block->length = qToBigEndian<uint16_t>(12);

    QStringList ipParts = ip.split('.');
    QStringList maskParts = mask.split('.');
    QStringList gwParts = gw.split('.');
    
    uint8_t *val = packet + sizeof(EthernetHeader) + sizeof(DcpHeader) + 4;
    val[0] = 0x00; val[1] = 0x01;
    for (int i = 0; i < 4; ++i) val[2+i] = (uint8_t)ipParts[i].toInt();
    for (int i = 0; i < 4; ++i) val[6+i] = (uint8_t)maskParts[i].toInt();
    for (int i = 0; i < 4; ++i) val[10+i] = (uint8_t)gwParts[i].toInt();

    if (pcap_sendpacket(m_pcapHandle, packet, sizeof(packet)) != 0) {
        qCritical() << "Error sending DCP Set IP request:" << pcap_geterr(m_pcapHandle);
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
