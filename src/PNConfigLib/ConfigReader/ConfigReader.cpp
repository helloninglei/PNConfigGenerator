/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ConfigReader.h"
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <stdexcept>

namespace PNConfigLib {

Configuration ConfigReader::parseConfiguration(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Failed to open configuration file: %1").arg(configPath).toStdString());
    }
    
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
        throw std::runtime_error(QString("XML parse error at line %1, column %2: %3")
            .arg(errorLine).arg(errorColumn).arg(errorMsg).toStdString());
    }
    
    Configuration config;
    
    QDomElement root = doc.documentElement();
    config.configurationID = root.attribute("ConfigurationID");
    config.configurationName = root.attribute("ConfigurationName");
    config.listOfNodesRefID = root.attribute("ListOfNodesRefID");
    config.schemaVersion = root.attribute("schemaVersion", "1.0");
    
    // Parse Devices element
    QDomElement devicesElem = root.firstChildElement("Devices");
    if (devicesElem.isNull()) {
        throw std::runtime_error("Missing Devices element in Configuration");
    }
    
    // Parse CentralDevice
    QDomElement centralElem = devicesElem.firstChildElement("CentralDevice");
    if (!centralElem.isNull()) {
        parseCentralDevice(centralElem, config.centralDevice);
    }
    
    // Parse DecentralDevices
    QDomElement decentralElem = devicesElem.firstChildElement("DecentralDevice");
    while (!decentralElem.isNull()) {
        DecentralDeviceType device;
        parseDecentralDevice(decentralElem, device);
        config.decentralDevices.append(device);
        decentralElem = decentralElem.nextSiblingElement("DecentralDevice");
    }
    
    return config;
}

ListOfNodes ConfigReader::parseListOfNodes(const QString& nodesPath)
{
    QFile file(nodesPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Failed to open ListOfNodes file: %1").arg(nodesPath).toStdString());
    }
    
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
        throw std::runtime_error(QString("XML parse error at line %1, column %2: %3")
            .arg(errorLine).arg(errorColumn).arg(errorMsg).toStdString());
    }
    
    ListOfNodes nodes;
    
    QDomElement root = doc.documentElement();
    nodes.listOfNodesID = root.attribute("ListOfNodesID");
    nodes.schemaVersion = root.attribute("schemaVersion", "1.0");
    
    // Parse PNDriver
    QDomElement pnDriverElem = root.firstChildElement("PNDriver");
    if (!pnDriverElem.isNull()) {
        parsePNDriver(pnDriverElem, nodes.pnDriver);
    }
    
    // Parse DecentralDevices
    QDomElement decentralElem = root.firstChildElement("DecentralDevice");
    while (!decentralElem.isNull()) {
        DecentralDeviceNode device;
        parseDecentralDeviceNode(decentralElem, device);
        nodes.decentralDevices.append(device);
        decentralElem = decentralElem.nextSiblingElement("DecentralDevice");
    }
    
    return nodes;
}

void ConfigReader::parseCentralDevice(QDomElement& element, CentralDeviceType& device)
{
    device.deviceRefID = element.attribute("DeviceRefID");
    
    QDomElement interfaceElem = element.firstChildElement("CentralDeviceInterface");
    if (!interfaceElem.isNull()) {
        device.interfaceRefID = interfaceElem.attribute("InterfaceRefID");
        
        QDomElement ethElem = interfaceElem.firstChildElement("EthernetAddresses");
        if (!ethElem.isNull()) {
            parseEthernetAddresses(ethElem, device.ethernetAddresses);
        }
        
        // Parse SendClock from AdvancedOptions
        QDomElement advElem = interfaceElem.firstChildElement("AdvancedOptions");
        if (!advElem.isNull()) {
            QDomElement rtElem = advElem.firstChildElement("RealTimeSettings");
            if (!rtElem.isNull()) {
                QDomElement ioCommElem = rtElem.firstChildElement("IOCommunication");
                if (!ioCommElem.isNull()) {
                    device.sendClock = ioCommElem.attribute("SendClock", "1").toInt();
                }
            }
        }
    }
}

void ConfigReader::parseDecentralDevice(QDomElement& element, DecentralDeviceType& device)
{
    device.deviceRefID = element.attribute("DeviceRefID");
    
    QDomElement interfaceElem = element.firstChildElement("DecentralDeviceInterface");
    if (!interfaceElem.isNull()) {
        device.interfaceRefID = interfaceElem.attribute("InterfaceRefID");
        
        QDomElement ethElem = interfaceElem.firstChildElement("EthernetAddresses");
        if (!ethElem.isNull()) {
            parseEthernetAddresses(ethElem, device.ethernetAddresses);
        }
    }
    
    // Parse Modules
    QDomElement moduleElem = element.firstChildElement("Module");
    while (!moduleElem.isNull()) {
        ModuleType module;
        parseModule(moduleElem, module);
        device.modules.append(module);
        moduleElem = moduleElem.nextSiblingElement("Module");
    }
}

void ConfigReader::parseEthernetAddresses(QDomElement& element, EthernetAddresses& addresses)
{
    QDomElement ipElem = element.firstChildElement("IPProtocol");
    if (!ipElem.isNull()) {
        QDomElement setInProjectElem = ipElem.firstChildElement("SetInTheProject");
        if (!setInProjectElem.isNull()) {
            addresses.ipAddress = setInProjectElem.attribute("IPAddress");
            addresses.subnetMask = setInProjectElem.attribute("SubnetMask", "255.255.255.0");
            addresses.routerAddress = setInProjectElem.attribute("RouterAddress");
        }
    }
    
    QDomElement pnNameElem = element.firstChildElement("PROFINETDeviceName");
    if (!pnNameElem.isNull()) {
        addresses.deviceNumber = pnNameElem.attribute("DeviceNumber", "0").toInt();
        
        QDomElement nameTextElem = pnNameElem.firstChildElement("PNDeviceName");
        if (!nameTextElem.isNull()) {
            addresses.deviceName = nameTextElem.text();
        }
    }
}

void ConfigReader::parseModule(QDomElement& element, ModuleType& module)
{
    module.moduleID = element.attribute("ModuleID");
    module.slotNumber = element.attribute("SlotNumber").toInt();
    module.gsdRefID = element.attribute("GSDRefID");
    
    // Parse Submodules
    QDomElement submoduleElem = element.firstChildElement("Submodule");
    while (!submoduleElem.isNull()) {
        SubmoduleType submodule;
        parseSubmodule(submoduleElem, submodule);
        module.submodules.append(submodule);
        submoduleElem = submoduleElem.nextSiblingElement("Submodule");
    }
}

void ConfigReader::parseSubmodule(QDomElement& element, SubmoduleType& submodule)
{
    submodule.submoduleID = element.attribute("SubmoduleID");
    submodule.subslotNumber = element.attribute("SubslotNumber").toInt();
    submodule.gsdRefID = element.attribute("GSDRefID");
    
    QDomElement ioElem = element.firstChildElement("IOAddresses");
    if (!ioElem.isNull()) {
        parseIOAddresses(ioElem, submodule.ioAddresses);
    }
}

void ConfigReader::parseIOAddresses(QDomElement& element, IOAddresses& addresses)
{
    QDomElement inputElem = element.firstChildElement("InputAddresses");
    if (!inputElem.isNull()) {
        addresses.inputStartAddress = inputElem.attribute("StartAddress").toInt();
        if (inputElem.hasAttribute("Length")) {
            addresses.inputLength = inputElem.attribute("Length").toInt();
        }
    }
    
    QDomElement outputElem = element.firstChildElement("OutputAddresses");
    if (!outputElem.isNull()) {
        addresses.outputStartAddress = outputElem.attribute("StartAddress").toInt();
        if (outputElem.hasAttribute("Length")) {
            addresses.outputLength = outputElem.attribute("Length").toInt();
        }
    }
}

void ConfigReader::parsePNDriver(QDomElement& element, PNDriverType& driver)
{
    driver.deviceID = element.attribute("DeviceID");
    driver.deviceName = element.attribute("DeviceName");
    driver.deviceVersion = element.attribute("DeviceVersion");
    
    QDomElement interfaceElem = element.firstChildElement("Interface");
    while (!interfaceElem.isNull()) {
        InterfaceType interface;
        parseInterface(interfaceElem, interface);
        driver.interfaces.append(interface);
        interfaceElem = interfaceElem.nextSiblingElement("Interface");
    }
}

void ConfigReader::parseDecentralDeviceNode(QDomElement& element, DecentralDeviceNode& device)
{
    device.deviceID = element.attribute("DeviceID");
    device.deviceName = element.attribute("DeviceName");
    device.gsdPath = element.attribute("GSDPath");
    device.gsdRefID = element.attribute("GSDRefID");
    
    QDomElement interfaceElem = element.firstChildElement("Interface");
    while (!interfaceElem.isNull()) {
        InterfaceType interface;
        parseInterface(interfaceElem, interface);
        device.interfaces.append(interface);
        interfaceElem = interfaceElem.nextSiblingElement("Interface");
    }
}

void ConfigReader::parseInterface(QDomElement& element, InterfaceType& interface)
{
    interface.interfaceID = element.attribute("InterfaceID");
    interface.interfaceName = element.attribute("InterfaceName");
    interface.interfaceType = element.attribute("InterfaceType");
}

} // namespace PNConfigLib
