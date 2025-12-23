#ifndef DCPSCANNER_H
#define DCPSCANNER_H

#include <QString>
#include <QObject>
#include <QList>

namespace PNConfigLib {

struct DiscoveredDevice {
    QString deviceName;
    QString deviceType;
    QString macAddress;
    QString ipAddress;
    QString subnetMask;
    QString gateway;
};

class DcpScanner : public QObject {
    Q_OBJECT
public:
    explicit DcpScanner(QObject *parent = nullptr);
    ~DcpScanner();

    bool connectToInterface(const QString &interfaceName);
    void disconnectFromInterface();
    bool isConnected() const { return m_isConnected; }

    QList<DiscoveredDevice> scan();

private:
    bool m_isConnected = false;
    QString m_interfaceName;
};

} // namespace PNConfigLib

#endif // DCPSCANNER_H
