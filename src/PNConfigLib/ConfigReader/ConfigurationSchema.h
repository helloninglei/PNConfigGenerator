/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef CONFIGURATIONSCHEMA_H
#define CONFIGURATIONSCHEMA_H

#include <QString>
#include <QVector>
#include <QHash>

namespace PNConfigLib {

/**
 * @brief IO Address information
 */
struct IOAddresses {
    int inputStartAddress = -1;
    int inputLength = 0;
    int outputStartAddress = -1;
    int outputLength = 0;
};

/**
 * @brief Submodule configuration
 */
struct SubmoduleType {
    QString submoduleID;
    int subslotNumber = 0;
    QString gsdRefID;
    IOAddresses ioAddresses;
};

/**
 * @brief Module configuration  
 */
struct ModuleType {
    QString moduleID;
    int slotNumber = 0;
    QString gsdRefID;
    QVector<SubmoduleType> submodules;
};

/**
 * @brief Ethernet address configuration
 */
struct EthernetAddresses {
    QString ipAddress;
    QString subnetMask = "255.255.255.0";
    QString routerAddress;
    QString deviceName;
    int deviceNumber = 0;
};

/**
 * @brief Decentralized device configuration
 */
struct DecentralDeviceType {
    QString deviceRefID;
    QString interfaceRefID;
    EthernetAddresses ethernetAddresses;
    QVector<ModuleType> modules;
};

/**
 * @brief Central device configuration
 */
struct CentralDeviceType {
    QString deviceRefID;
    QString interfaceRefID;
    EthernetAddresses ethernetAddresses;
    int sendClock = 1;
};

/**
 * @brief Main configuration structure
 */
struct Configuration {
    QString configurationID;
    QString configurationName;
    QString listOfNodesRefID;
    QString schemaVersion = "1.0";
    
    CentralDeviceType centralDevice;
    QVector<DecentralDeviceType> decentralDevices;
};

/**
 * @brief Interface information in ListOfNodes
 */
struct InterfaceType {
    QString interfaceID;
    QString interfaceName;
    QString interfaceType;
};

/**
 * @brief PNDriver device in ListOfNodes
 */
struct PNDriverType {
    QString deviceID;
    QString deviceName;
    QString deviceVersion;
    QVector<InterfaceType> interfaces;
};

/**
 * @brief Decentralized device in ListOfNodes
 */
struct DecentralDeviceNode {
    QString deviceID;
    QString deviceName;
    QString gsdPath;
    QString gsdRefID;
    QVector<InterfaceType> interfaces;
};

/**
 * @brief ListOfNodes structure
 */
struct ListOfNodes {
    QString listOfNodesID;
    QString schemaVersion = "1.0";
    
    PNDriverType pnDriver;
    QVector<DecentralDeviceNode> decentralDevices;
};

} // namespace PNConfigLib

#endif // CONFIGURATIONSCHEMA_H
