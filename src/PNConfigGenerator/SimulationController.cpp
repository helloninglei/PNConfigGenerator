#include "SimulationController.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>

SimulationController::SimulationController(QObject *parent) : QObject(parent)
{
    m_scanner = new PNConfigLib::DcpScanner(this);
    m_arManager = new PNConfigLib::ArExchangeManager(this);

    connect(m_arManager, &PNConfigLib::ArExchangeManager::stateChanged, this, &SimulationController::onArStateChanged);
    connect(m_arManager, &PNConfigLib::ArExchangeManager::messageLogged, [](const QString &msg) {
        qDebug() << "[AR]" << msg;
    });
}

void SimulationController::start()
{
    qDebug() << "Starting Headless Simulation...";

    auto interfaces = PNConfigLib::DcpScanner::getAvailableInterfaces();
    if (interfaces.isEmpty()) {
        qCritical() << "No network interfaces found!";
        QCoreApplication::exit(1);
        return;
    }

    // Try to find a working physical interface, prioritize Realtek
    bool connected = false;
    
    // Pass 1: Look for Realtek
    for (const auto &iface : interfaces) {
        if (iface.description.contains("Realtek", Qt::CaseInsensitive)) {
            qDebug() << "Found Realtek interface:" << iface.description << "(" << iface.name << ")";
            if (m_scanner->connectToInterface(iface.name)) {
                const uint8_t *mac = m_scanner->getSourceMac();
                if (mac[0] != 0 || mac[1] != 0 || mac[2] != 0 || mac[3] != 0 || mac[4] != 0 || mac[5] != 0) {
                    m_nicName = iface.name;
                    connected = true;
                    qDebug() << "Selected prioritized interface:" << iface.description;
                    break;
                }
                m_scanner->disconnectFromInterface();
            }
        }
    }

    // Pass 2: Try any other interface if Realtek not found or failed
    if (!connected) {
        for (const auto &iface : interfaces) {
            if (iface.description.contains("Realtek", Qt::CaseInsensitive)) continue; // Already tried
            
            qDebug() << "Checking alternate interface:" << iface.description << "(" << iface.name << ")";
            if (m_scanner->connectToInterface(iface.name)) {
                const uint8_t *mac = m_scanner->getSourceMac();
                if (mac[0] != 0 || mac[1] != 0 || mac[2] != 0 || mac[3] != 0 || mac[4] != 0 || mac[5] != 0) {
                    m_nicName = iface.name;
                    connected = true;
                    qDebug() << "Selected interface:" << iface.description;
                    break;
                }
                m_scanner->disconnectFromInterface();
            }
        }
    }

    if (!connected) {
        qCritical() << "Failed to find a physical network interface with a valid MAC address!";
        QCoreApplication::exit(1);
        return;
    }

    qDebug() << "Scanning for devices...";
    auto devices = m_scanner->scan();
    
    if (devices.isEmpty()) {
        qCritical() << "No PROFINET devices found on the network!";
        QCoreApplication::exit(1);
        return;
    }

    const auto &device = devices.first();
    qDebug() << "Found device:" << device.deviceName << "at" << device.ipAddress << "(" << device.macAddress << ")";

    qDebug() << "Initiating AR connection...";
    if (m_arManager->start(m_nicName, device.macAddress, device.ipAddress, device.deviceName)) {
        qDebug() << "AR connection process started.";
    } else {
        qCritical() << "Failed to start AR connection:" << m_arManager->lastError();
        QCoreApplication::exit(1);
    }
}

void SimulationController::onArStateChanged(PNConfigLib::ArState state)
{
    switch (state) {
        case PNConfigLib::ArState::Offline:
            qDebug() << "AR State: Offline";
            break;
        case PNConfigLib::ArState::Connecting:
            qDebug() << "AR State: Connecting (Phase 1)";
            break;
        case PNConfigLib::ArState::Parameterizing:
            qDebug() << "AR State: Parameterizing (Phase 2)";
            break;
        case PNConfigLib::ArState::AppReady:
            qDebug() << "AR State: AppReady (Waiting for Slave)";
            break;
        case PNConfigLib::ArState::Running:
            qDebug() << "AR State: Running (Cyclic Data Exchange)";
            // We can exit on success after a few cycles
            // QCoreApplication::exit(0);
            break;
        case PNConfigLib::ArState::Error:
            qCritical() << "AR State: Error -" << m_arManager->lastError();
            // Exit on error so we can see it in logs
            QCoreApplication::exit(1);
            break;
    }
}
