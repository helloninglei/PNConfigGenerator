#include "DcpScanner.h"
#include <QDebug>

namespace PNConfigLib {

DcpScanner::DcpScanner(QObject *parent) : QObject(parent) {}

DcpScanner::~DcpScanner() {
    disconnectFromInterface();
}

bool DcpScanner::connectToInterface(const QString &interfaceName) {
    if (m_isConnected) {
        disconnectFromInterface();
    }
    
    m_interfaceName = interfaceName;
    m_isConnected = true;
    qDebug() << "Connected to interface:" << interfaceName;
    return true;
}

void DcpScanner::disconnectFromInterface() {
    if (m_isConnected) {
        qDebug() << "Disconnected from interface:" << m_interfaceName;
        m_isConnected = false;
        m_interfaceName.clear();
    }
}

QList<DiscoveredDevice> DcpScanner::scan() {
    QList<DiscoveredDevice> devices;
    if (!m_isConnected) {
        return devices;
    }

    // Mock scanning logic for now
    qDebug() << "Scanning for devices on" << m_interfaceName;
    
    DiscoveredDevice dev1;
    dev1.deviceName = "rt-labs-dev";
    dev1.deviceType = "P-Net sample app";
    dev1.macAddress = "00:11:22:33:44:55";
    dev1.ipAddress = "192.168.0.253";
    devices.append(dev1);

    return devices;
}

} // namespace PNConfigLib
