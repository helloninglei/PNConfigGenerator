/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ConfigReader.h"
#include "tinyxml2/tinyxml2.h"
#include <QFile>
#include <stdexcept>

using namespace tinyxml2;

namespace PNConfigLib {

// Helper to safely get attribute as QString
static QString getAttribute(XMLElement* element, const char* name, const QString& defaultValue = QString())
{
    if (!element) return defaultValue;
    const char* val = element->Attribute(name);
    return val ? QString(val) : defaultValue;
}

Configuration ConfigReader::parseConfiguration(const QString& configPath)
{
    XMLDocument doc;
    XMLError err = doc.LoadFile(configPath.toStdString().c_str());
    
    if (err != XML_SUCCESS) {
        throw std::runtime_error(QString("Failed to load configuration file: %1. Error: %2")
            .arg(configPath).arg(doc.ErrorStr()).toStdString());
    }
    
    Configuration config;
    
    XMLElement* root = doc.FirstChildElement("Configuration");
    if (!root) {
        throw std::runtime_error("Invalid configuration file: Missing root 'Configuration' element");
    }

    config.configurationID = getAttribute(root, "ConfigurationID");
    config.configurationName = getAttribute(root, "ConfigurationName");
    config.listOfNodesRefID = getAttribute(root, "ListOfNodesRefID");
    config.schemaVersion = getAttribute(root, "schemaVersion", "1.0");
    
    // Parse Devices element
    XMLElement* devicesElem = root->FirstChildElement("Devices");
    if (!devicesElem) {
        throw std::runtime_error("Missing Devices element in Configuration");
    }
    
    // Parse CentralDevice
    XMLElement* centralElem = devicesElem->FirstChildElement("CentralDevice");
    if (centralElem) {
        parseCentralDevice(centralElem, config.centralDevice);
    }
    
    // Parse DecentralDevices
    XMLElement* decentralElem = devicesElem->FirstChildElement("DecentralDevice");
    while (decentralElem) {
        DecentralDeviceType device;
        parseDecentralDevice(decentralElem, device);
        config.decentralDevices.append(device);
        decentralElem = decentralElem->NextSiblingElement("DecentralDevice");
    }
    
    return config;
}

ListOfNodes ConfigReader::parseListOfNodes(const QString& nodesPath)
{
    XMLDocument doc;
    XMLError err = doc.LoadFile(nodesPath.toStdString().c_str());
    
    if (err != XML_SUCCESS) {
        throw std::runtime_error(QString("Failed to load ListOfNodes file: %1. Error: %2")
            .arg(nodesPath).arg(doc.ErrorStr()).toStdString());
    }
    
    ListOfNodes nodes;
    
    XMLElement* root = doc.FirstChildElement("ListOfNodes");
    if (!root) {
        throw std::runtime_error("Invalid ListOfNodes file: Missing root 'ListOfNodes' element");
    }

    nodes.listOfNodesID = getAttribute(root, "ListOfNodesID");
    nodes.schemaVersion = getAttribute(root, "schemaVersion", "1.0");
    
    // Parse PNDriver
    XMLElement* pnDriverElem = root->FirstChildElement("PNDriver");
    if (pnDriverElem) {
        parsePNDriver(pnDriverElem, nodes.pnDriver);
    }
    
    // Parse DecentralDevices
    XMLElement* decentralElem = root->FirstChildElement("DecentralDevice");
    while (decentralElem) {
        DecentralDeviceNode device;
        parseDecentralDeviceNode(decentralElem, device);
        nodes.decentralDevices.append(device);
        decentralElem = decentralElem->NextSiblingElement("DecentralDevice");
    }
    
    return nodes;
}

void ConfigReader::parseCentralDevice(XMLElement* element, CentralDeviceType& device)
{
    device.deviceRefID = getAttribute(element, "DeviceRefID");
    
    XMLElement* interfaceElem = element->FirstChildElement("CentralDeviceInterface");
    if (interfaceElem) {
        device.interfaceRefID = getAttribute(interfaceElem, "InterfaceRefID");
        
        XMLElement* ethElem = interfaceElem->FirstChildElement("EthernetAddresses");
        if (ethElem) {
            parseEthernetAddresses(ethElem, device.ethernetAddresses);
        }
        
        // Parse SendClock from AdvancedOptions
        XMLElement* advElem = interfaceElem->FirstChildElement("AdvancedOptions");
        if (advElem) {
            XMLElement* rtElem = advElem->FirstChildElement("RealTimeSettings");
            if (rtElem) {
                XMLElement* ioCommElem = rtElem->FirstChildElement("IOCommunication");
                if (ioCommElem) {
                    device.sendClock = getAttribute(ioCommElem, "SendClock", "1").toInt();
                }
            }
        }
    }
}

void ConfigReader::parseDecentralDevice(XMLElement* element, DecentralDeviceType& device)
{
    device.deviceRefID = getAttribute(element, "DeviceRefID");
    
    XMLElement* interfaceElem = element->FirstChildElement("DecentralDeviceInterface");
    if (interfaceElem) {
        device.interfaceRefID = getAttribute(interfaceElem, "InterfaceRefID");
        
        XMLElement* ethElem = interfaceElem->FirstChildElement("EthernetAddresses");
        if (ethElem) {
            parseEthernetAddresses(ethElem, device.ethernetAddresses);
        }
    }
    
    // Parse Modules
    XMLElement* moduleElem = element->FirstChildElement("Module");
    while (moduleElem) {
        ModuleType module;
        parseModule(moduleElem, module);
        device.modules.append(module);
        moduleElem = moduleElem->NextSiblingElement("Module");
    }
}

void ConfigReader::parseEthernetAddresses(XMLElement* element, EthernetAddresses& addresses)
{
    XMLElement* ipElem = element->FirstChildElement("IPProtocol");
    if (ipElem) {
        XMLElement* setInProjectElem = ipElem->FirstChildElement("SetInTheProject");
        if (setInProjectElem) {
            addresses.ipAddress = getAttribute(setInProjectElem, "IPAddress");
            addresses.subnetMask = getAttribute(setInProjectElem, "SubnetMask", "255.255.255.0");
            addresses.routerAddress = getAttribute(setInProjectElem, "RouterAddress");
        }
    }
    
    XMLElement* pnNameElem = element->FirstChildElement("PROFINETDeviceName");
    if (pnNameElem) {
        addresses.deviceNumber = getAttribute(pnNameElem, "DeviceNumber", "0").toInt();
        
        XMLElement* nameTextElem = pnNameElem->FirstChildElement("PNDeviceName");
        if (nameTextElem && nameTextElem->GetText()) {
            addresses.deviceName = QString(nameTextElem->GetText());
        }
    }
}

void ConfigReader::parseModule(XMLElement* element, ModuleType& module)
{
    module.moduleID = getAttribute(element, "ModuleID");
    module.slotNumber = getAttribute(element, "SlotNumber").toInt();
    module.gsdRefID = getAttribute(element, "GSDRefID");
    
    // Parse Submodules
    XMLElement* submoduleElem = element->FirstChildElement("Submodule");
    while (submoduleElem) {
        SubmoduleType submodule;
        parseSubmodule(submoduleElem, submodule);
        module.submodules.append(submodule);
        submoduleElem = submoduleElem->NextSiblingElement("Submodule");
    }
}

void ConfigReader::parseSubmodule(XMLElement* element, SubmoduleType& submodule)
{
    submodule.submoduleID = getAttribute(element, "SubmoduleID");
    submodule.subslotNumber = getAttribute(element, "SubslotNumber").toInt();
    submodule.gsdRefID = getAttribute(element, "GSDRefID");
    
    XMLElement* ioElem = element->FirstChildElement("IOAddresses");
    if (ioElem) {
        parseIOAddresses(ioElem, submodule.ioAddresses);
    }
}

void ConfigReader::parseIOAddresses(XMLElement* element, IOAddresses& addresses)
{
    XMLElement* inputElem = element->FirstChildElement("InputAddresses");
    if (inputElem) {
        addresses.inputStartAddress = getAttribute(inputElem, "StartAddress").toInt();
        if (inputElem->Attribute("Length")) {
            addresses.inputLength = getAttribute(inputElem, "Length").toInt();
        }
    }
    
    XMLElement* outputElem = element->FirstChildElement("OutputAddresses");
    if (outputElem) {
        addresses.outputStartAddress = getAttribute(outputElem, "StartAddress").toInt();
        if (outputElem->Attribute("Length")) {
            addresses.outputLength = getAttribute(outputElem, "Length").toInt();
        }
    }
}

void ConfigReader::parsePNDriver(XMLElement* element, PNDriverType& driver)
{
    driver.deviceID = getAttribute(element, "DeviceID");
    driver.deviceName = getAttribute(element, "DeviceName");
    driver.deviceVersion = getAttribute(element, "DeviceVersion");
    
    XMLElement* interfaceElem = element->FirstChildElement("Interface");
    while (interfaceElem) {
        InterfaceType interface;
        parseInterface(interfaceElem, interface);
        driver.interfaces.append(interface);
        interfaceElem = interfaceElem->NextSiblingElement("Interface");
    }
}

void ConfigReader::parseDecentralDeviceNode(XMLElement* element, DecentralDeviceNode& device)
{
    device.deviceID = getAttribute(element, "DeviceID");
    device.deviceName = getAttribute(element, "DeviceName");
    device.gsdPath = getAttribute(element, "GSDPath");
    device.gsdRefID = getAttribute(element, "GSDRefID");
    
    XMLElement* interfaceElem = element->FirstChildElement("Interface");
    while (interfaceElem) {
        InterfaceType interface;
        parseInterface(interfaceElem, interface);
        device.interfaces.append(interface);
        interfaceElem = interfaceElem->NextSiblingElement("Interface");
    }
}

void ConfigReader::parseInterface(XMLElement* element, InterfaceType& interface)
{
    interface.interfaceID = getAttribute(element, "InterfaceID");
    interface.interfaceName = getAttribute(element, "InterfaceName");
    interface.interfaceType = getAttribute(element, "InterfaceType");
}

} // namespace PNConfigLib
