#ifndef DCPSCANNER_H
#define DCPSCANNER_H

#include <QString>
#include <QObject>
#include <QList>
#include <cstdint>
#include <pcap.h>

namespace PNConfigLib {

struct InterfaceInfo {
    QString name;
    QString description;
};

struct DiscoveredDevice {
    QString deviceName;
    QString deviceType;
    QString macAddress;
    QString ipAddress = "0.0.0.0";
    QString subnetMask = "0.0.0.0";
    QString gateway = "0.0.0.0";
    uint16_t vendorId = 0;
    uint16_t deviceId = 0;
};

#pragma pack(push, 1)
struct EthernetHeader {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
};

struct DcpHeader {
    uint16_t frameId;
    uint8_t serviceId;
    uint8_t serviceType;
    uint32_t xid;
    uint16_t responseDelay;
    uint16_t dcpDataLength;
};

struct DcpBlockHeader {
    uint8_t option;
    uint8_t suboption;
    uint16_t length;
};
#pragma pack(pop)

class DcpScanner : public QObject {
    Q_OBJECT
public:
    explicit DcpScanner(QObject *parent = nullptr);
    ~DcpScanner();

    static QList<InterfaceInfo> getAvailableInterfaces();

    bool connectToInterface(const QString &interfaceName);
    void disconnectFromInterface();
    bool isConnected() const { return m_isConnected; }

    QList<DiscoveredDevice> scan();
    bool setDeviceIp(const QString &mac, const QString &ip, const QString &mask, const QString &gw, bool permanent = false);
    bool setDeviceName(const QString &mac, const QString &name, bool permanent = false);
    bool resetFactory(const QString &mac);
    bool flashLed(const QString &mac);

private:
    void parseDcpPacket(const uint8_t *data, int len, QList<DiscoveredDevice> &devices);
    QString macToString(const uint8_t *mac);

    bool m_isConnected = false;
    QString m_interfaceName;
    uint8_t m_sourceMac[6] = {0};
    pcap_t *m_pcapHandle = nullptr;
};

} // namespace PNConfigLib

#endif // DCPSCANNER_H
