/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ConfigurationBuilder.h"
#include "../GsdmlParser/GsdmlParser.h"
#include "../tinyxml2/tinyxml2.h"
#include <QFile>
#include <QTextStream>

using namespace tinyxml2;

namespace PNConfigLib {

QString ConfigurationBuilder::generateConfigurationXml(
    const QString& gsdmlPath,
    const DeviceConfig& masterConfig,
    const DeviceConfig& slaveConfig)
{
    // Parse GSDML to get device information
    GsdmlInfo gsdmlInfo;
    try {
        gsdmlInfo = GsdmlParser::parseGSDML(gsdmlPath);
    } catch (...) {
        return QString();
    }
    
    XMLDocument doc;
    
    // XML declaration
    XMLDeclaration* decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\"");
    doc.InsertEndChild(decl);
    
    // Root element with namespaces
    XMLElement* root = doc.NewElement("Configuration");
    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root->SetAttribute("ConfigurationID", "ConfigurationID");
    root->SetAttribute("ConfigurationName", "ConfigurationName");
    root->SetAttribute("ListOfNodesRefID", "ListOfNodesID");
    root->SetAttribute("schemaVersion", "1.0");
    root->SetAttribute("xmlns", "http://www.siemens.com/Automation/PNConfigLib/Configuration");
    doc.InsertEndChild(root);
    
    // Devices container
    XMLElement* devices = doc.NewElement("Devices");
    root->InsertEndChild(devices);
    
    // Central Device (Master/Controller)
    XMLElement* centralDevice = doc.NewElement("CentralDevice");
    centralDevice->SetAttribute("DeviceRefID", (masterConfig.name + "_ID").toStdString().c_str());
    devices->InsertEndChild(centralDevice);
    
    XMLElement* centralInterface = doc.NewElement("CentralDeviceInterface");
    centralInterface->SetAttribute("InterfaceRefID", (masterConfig.name + "_Interface").toStdString().c_str());
    centralDevice->InsertEndChild(centralInterface);
    
    // Ethernet Addresses
    XMLElement* ethAddresses = doc.NewElement("EthernetAddresses");
    centralInterface->InsertEndChild(ethAddresses);
    
    XMLElement* ipProtocol = doc.NewElement("IPProtocol");
    ethAddresses->InsertEndChild(ipProtocol);
    
    XMLElement* setInProject = doc.NewElement("SetInTheProject");
    setInProject->SetAttribute("IPAddress", masterConfig.ipAddress.toStdString().c_str());
    setInProject->SetAttribute("SubnetMask", "255.255.255.0");
    if (!masterConfig.routerIpAddress.isEmpty() && masterConfig.routerIpAddress != "0.0.0.0") {
        setInProject->SetAttribute("RouterAddress", masterConfig.routerIpAddress.toStdString().c_str());
    }
    ipProtocol->InsertEndChild(setInProject);
    
    XMLElement* pnDeviceName = doc.NewElement("PROFINETDeviceName");
    ethAddresses->InsertEndChild(pnDeviceName);
    
    XMLElement* deviceNameText = doc.NewElement("PNDeviceName");
    deviceNameText->SetText(masterConfig.name.toStdString().c_str());
    pnDeviceName->InsertEndChild(deviceNameText);
    
    // Advanced Options for Central Device
    XMLElement* advancedOptions = doc.NewElement("AdvancedOptions");
    centralInterface->InsertEndChild(advancedOptions);
    
    XMLElement* realTimeSettings = doc.NewElement("RealTimeSettings");
    advancedOptions->InsertEndChild(realTimeSettings);
    
    XMLElement* ioCommunication = doc.NewElement("IOCommunication");
    ioCommunication->SetAttribute("SendClock", "1");
    realTimeSettings->InsertEndChild(ioCommunication);
    
    // Decentralized Device (Slave/IO Device)
    XMLElement* decentralDevice = doc.NewElement("DecentralDevice");
    decentralDevice->SetAttribute("DeviceRefID", (slaveConfig.name + "_ID").toStdString().c_str());
    devices->InsertEndChild(decentralDevice);
    
    XMLElement* decentralInterface = doc.NewElement("DecentralDeviceInterface");
    decentralInterface->SetAttribute("InterfaceRefID", (slaveConfig.name + "_Interface").toStdString().c_str());
    decentralDevice->InsertEndChild(decentralInterface);
    
    // Ethernet Addresses for Slave
    XMLElement* slaveEthAddresses = doc.NewElement("EthernetAddresses");
    decentralInterface->InsertEndChild(slaveEthAddresses);
    
    XMLElement* slaveIpProtocol = doc.NewElement("IPProtocol");
    slaveEthAddresses->InsertEndChild(slaveIpProtocol);
    
    XMLElement* slaveSetInProject = doc.NewElement("SetInTheProject");
    slaveSetInProject->SetAttribute("IPAddress", slaveConfig.ipAddress.toStdString().c_str());
    slaveSetInProject->SetAttribute("SubnetMask", "255.255.255.0");
    if (!slaveConfig.routerIpAddress.isEmpty() && slaveConfig.routerIpAddress != "0.0.0.0") {
        slaveSetInProject->SetAttribute("RouterAddress", slaveConfig.routerIpAddress.toStdString().c_str());
    }
    slaveIpProtocol->InsertEndChild(slaveSetInProject);
    
    XMLElement* slavePnDeviceName = doc.NewElement("PROFINETDeviceName");
    slavePnDeviceName->SetAttribute("DeviceNumber", "1");
    slaveEthAddresses->InsertEndChild(slavePnDeviceName);
    
    XMLElement* slaveDeviceNameText = doc.NewElement("PNDeviceName");
    slaveDeviceNameText->SetText(slaveConfig.name.toStdString().c_str());
    slavePnDeviceName->InsertEndChild(slaveDeviceNameText);
    
    // Advanced Options for Slave Device
    XMLElement* slaveAdvancedOptions = doc.NewElement("AdvancedOptions");
    decentralInterface->InsertEndChild(slaveAdvancedOptions);
    
    XMLElement* interfaceOptions = doc.NewElement("InterfaceOptions");
    slaveAdvancedOptions->InsertEndChild(interfaceOptions);
    
    XMLElement* ports = doc.NewElement("Ports");
    slaveAdvancedOptions->InsertEndChild(ports);
    
    XMLElement* mediaRedundancy = doc.NewElement("MediaRedundancy");
    slaveAdvancedOptions->InsertEndChild(mediaRedundancy);
    
    XMLElement* slaveRealTimeSettings = doc.NewElement("RealTimeSettings");
    slaveAdvancedOptions->InsertEndChild(slaveRealTimeSettings);
    
    XMLElement* ioCycle = doc.NewElement("IOCycle");
    slaveRealTimeSettings->InsertEndChild(ioCycle);
    
    XMLElement* sharedDevicePart = doc.NewElement("SharedDevicePart");
    ioCycle->InsertEndChild(sharedDevicePart);
    
    XMLElement* updateTime = doc.NewElement("UpdateTime");
    ioCycle->InsertEndChild(updateTime);
    
    XMLElement* synchronization = doc.NewElement("Synchronization");
    slaveRealTimeSettings->InsertEndChild(synchronization);
    
    // Add modules and submodules
    int currentInputAddress = slaveConfig.inputStartAddress;
    int currentOutputAddress = slaveConfig.outputStartAddress;
    int moduleIndex = 48; // Starting module ID
    int submoduleIndex = 304; // Starting submodule ID
    int slotNumber = 1;
    
    for (const ModuleInfo& module : gsdmlInfo.modules) {
        XMLElement* moduleElem = doc.NewElement("Module");
        moduleElem->SetAttribute("ModuleID", QString("Module_%1").arg(moduleIndex++).toStdString().c_str());
        moduleElem->SetAttribute("SlotNumber", slotNumber++);
        moduleElem->SetAttribute("GSDRefID", module.id.toStdString().c_str());
        decentralDevice->InsertEndChild(moduleElem);
        
        XMLElement* moduleIOAddresses = doc.NewElement("IOAddresses");
        moduleElem->InsertEndChild(moduleIOAddresses);
        
        int subslotNumber = 1;
        for (const SubmoduleInfo& submodule : module.submodules) {
            XMLElement* submoduleElem = doc.NewElement("Submodule");
            submoduleElem->SetAttribute("SubmoduleID", QString("Submodule_%1").arg(submoduleIndex++).toStdString().c_str());
            submoduleElem->SetAttribute("SubslotNumber", subslotNumber++);
            submoduleElem->SetAttribute("GSDRefID", submodule.id.toStdString().c_str());
            moduleElem->InsertEndChild(submoduleElem);
            
            XMLElement* submoduleIOAddresses = doc.NewElement("IOAddresses");
            submoduleElem->InsertEndChild(submoduleIOAddresses);
            
            // Add input addresses if needed
            if (submodule.inputDataLength > 0) {
                XMLElement* inputAddresses = doc.NewElement("InputAddresses");
                inputAddresses->SetAttribute("StartAddress", currentInputAddress);
                submoduleIOAddresses->InsertEndChild(inputAddresses);
                currentInputAddress += submodule.inputDataLength;
            }
            
            // Add output addresses if needed
            if (submodule.outputDataLength > 0) {
                XMLElement* outputAddresses = doc.NewElement("OutputAddresses");
                outputAddresses->SetAttribute("StartAddress", currentOutputAddress);
                submoduleIOAddresses->InsertEndChild(outputAddresses);
                currentOutputAddress += submodule.outputDataLength;
            }
        }
    }
    
    // SharedDevice element
    XMLElement* sharedDevice = doc.NewElement("SharedDevice");
    decentralDevice->InsertEndChild(sharedDevice);
    
    // AdvancedConfiguration
    XMLElement* advancedConfig = doc.NewElement("AdvancedConfiguration");
    decentralDevice->InsertEndChild(advancedConfig);
    
    XMLElement* snmp = doc.NewElement("Snmp");
    advancedConfig->InsertEndChild(snmp);
    
    XMLElement* dcp = doc.NewElement("Dcp");
    advancedConfig->InsertEndChild(dcp);
    
    XMLPrinter printer;
    doc.Print(&printer);
    return QString::fromUtf8(printer.CStr());
}

bool ConfigurationBuilder::saveConfigurationXml(
    const QString& gsdmlPath,
    const DeviceConfig& masterConfig,
    const DeviceConfig& slaveConfig,
    const QString& outputPath)
{
    QString xml = generateConfigurationXml(gsdmlPath, masterConfig, slaveConfig);
    if (xml.isEmpty()) {
        return false;
    }
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << xml;
    file.close();
    
    return true;
}

} // namespace PNConfigLib
