/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ConfigurationBuilder.h"
#include "../GsdmlParser/GsdmlParser.h"
#include <QFile>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>

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
    
    QDomDocument doc;
    
    // XML declaration is handled by QDomDocument
    QDomProcessingInstruction xmlDecl = doc.createProcessingInstruction(
        "xml", "version=\"1.0\" encoding=\"utf-8\"");
    doc.appendChild(xmlDecl);
    
    // Root element with namespaces
    QDomElement root = doc.createElement("Configuration");
    root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root.setAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root.setAttribute("ConfigurationID", "ConfigurationID");
    root.setAttribute("ConfigurationName", "ConfigurationName");
    root.setAttribute("ListOfNodesRefID", "ListOfNodesID");
    root.setAttribute("schemaVersion", "1.0");
    root.setAttribute("xmlns", "http://www.siemens.com/Automation/PNConfigLib/Configuration");
    doc.appendChild(root);
    
    // Devices container
    QDomElement devices = doc.createElement("Devices");
    root.appendChild(devices);
    
    // Central Device (Master/Controller)
    QDomElement centralDevice = doc.createElement("CentralDevice");
    centralDevice.setAttribute("DeviceRefID", masterConfig.name + "_ID");
    devices.appendChild(centralDevice);
    
    QDomElement centralInterface = doc.createElement("CentralDeviceInterface");
    centralInterface.setAttribute("InterfaceRefID", masterConfig.name + "_Interface");
    centralDevice.appendChild(centralInterface);
    
    // Ethernet Addresses
    QDomElement ethAddresses = doc.createElement("EthernetAddresses");
    centralInterface.appendChild(ethAddresses);
    
    QDomElement ipProtocol = doc.createElement("IPProtocol");
    ethAddresses.appendChild(ipProtocol);
    
    QDomElement setInProject = doc.createElement("SetInTheProject");
    setInProject.setAttribute("IPAddress", masterConfig.ipAddress);
    setInProject.setAttribute("SubnetMask", "255.255.255.0");
    if (!masterConfig.routerIpAddress.isEmpty() && masterConfig.routerIpAddress != "0.0.0.0") {
        setInProject.setAttribute("RouterAddress", masterConfig.routerIpAddress);
    }
    ipProtocol.appendChild(setInProject);
    
    QDomElement pnDeviceName = doc.createElement("PROFINETDeviceName");
    ethAddresses.appendChild(pnDeviceName);
    
    QDomElement deviceNameText = doc.createElement("PNDeviceName");
    deviceNameText.appendChild(doc.createTextNode(masterConfig.name));
    pnDeviceName.appendChild(deviceNameText);
    
    // Advanced Options for Central Device
    QDomElement advancedOptions = doc.createElement("AdvancedOptions");
    centralInterface.appendChild(advancedOptions);
    
    QDomElement realTimeSettings = doc.createElement("RealTimeSettings");
    advancedOptions.appendChild(realTimeSettings);
    
    QDomElement ioCommunication = doc.createElement("IOCommunication");
    ioCommunication.setAttribute("SendClock", "1");
    realTimeSettings.appendChild(ioCommunication);
    
    // Decentralized Device (Slave/IO Device)
    QDomElement decentralDevice = doc.createElement("DecentralDevice");
    decentralDevice.setAttribute("DeviceRefID", slaveConfig.name + "_ID");
    devices.appendChild(decentralDevice);
    
    QDomElement decentralInterface = doc.createElement("DecentralDeviceInterface");
    decentralInterface.setAttribute("InterfaceRefID", slaveConfig.name + "_Interface");
    decentralDevice.appendChild(decentralInterface);
    
    // Ethernet Addresses for Slave
    QDomElement slaveEthAddresses = doc.createElement("EthernetAddresses");
    decentralInterface.appendChild(slaveEthAddresses);
    
    QDomElement slaveIpProtocol = doc.createElement("IPProtocol");
    slaveEthAddresses.appendChild(slaveIpProtocol);
    
    QDomElement slaveSetInProject = doc.createElement("SetInTheProject");
    slaveSetInProject.setAttribute("IPAddress", slaveConfig.ipAddress);
    slaveSetInProject.setAttribute("SubnetMask", "255.255.255.0");
    if (!slaveConfig.routerIpAddress.isEmpty() && slaveConfig.routerIpAddress != "0.0.0.0") {
        slaveSetInProject.setAttribute("RouterAddress", slaveConfig.routerIpAddress);
    }
    slaveIpProtocol.appendChild(slaveSetInProject);
    
    QDomElement slavePnDeviceName = doc.createElement("PROFINETDeviceName");
    slavePnDeviceName.setAttribute("DeviceNumber", "1");
    slaveEthAddresses.appendChild(slavePnDeviceName);
    
    QDomElement slaveDeviceNameText = doc.createElement("PNDeviceName");
    slaveDeviceNameText.appendChild(doc.createTextNode(slaveConfig.name));
    slavePnDeviceName.appendChild(slaveDeviceNameText);
    
    // Advanced Options for Slave Device
    QDomElement slaveAdvancedOptions = doc.createElement("AdvancedOptions");
    decentralInterface.appendChild(slaveAdvancedOptions);
    
    QDomElement interfaceOptions = doc.createElement("InterfaceOptions");
    slaveAdvancedOptions.appendChild(interfaceOptions);
    
    QDomElement ports = doc.createElement("Ports");
    slaveAdvancedOptions.appendChild(ports);
    
    QDomElement mediaRedundancy = doc.createElement("MediaRedundancy");
    slaveAdvancedOptions.appendChild(mediaRedundancy);
    
    QDomElement slaveRealTimeSettings = doc.createElement("RealTimeSettings");
    slaveAdvancedOptions.appendChild(slaveRealTimeSettings);
    
    QDomElement ioCycle = doc.createElement("IOCycle");
    slaveRealTimeSettings.appendChild(ioCycle);
    
    QDomElement sharedDevicePart = doc.createElement("SharedDevicePart");
    ioCycle.appendChild(sharedDevicePart);
    
    QDomElement updateTime = doc.createElement("UpdateTime");
    ioCycle.appendChild(updateTime);
    
    QDomElement synchronization = doc.createElement("Synchronization");
    slaveRealTimeSettings.appendChild(synchronization);
    
    // Add modules and submodules
    int currentInputAddress = slaveConfig.inputStartAddress;
    int currentOutputAddress = slaveConfig.outputStartAddress;
    int moduleIndex = 48; // Starting module ID
    int submoduleIndex = 304; // Starting submodule ID
    int slotNumber = 1;
    
    for (const ModuleInfo& module : gsdmlInfo.modules) {
        QDomElement moduleElem = doc.createElement("Module");
        moduleElem.setAttribute("ModuleID", QString("Module_%1").arg(moduleIndex++));
        moduleElem.setAttribute("SlotNumber", QString::number(slotNumber++));
        moduleElem.setAttribute("GSDRefID", module.id);
        decentralDevice.appendChild(moduleElem);
        
        QDomElement moduleIOAddresses = doc.createElement("IOAddresses");
        moduleElem.appendChild(moduleIOAddresses);
        
        int subslotNumber = 1;
        for (const SubmoduleInfo& submodule : module.submodules) {
            QDomElement submoduleElem = doc.createElement("Submodule");
            submoduleElem.setAttribute("SubmoduleID", QString("Submodule_%1").arg(submoduleIndex++));
            submoduleElem.setAttribute("SubslotNumber", QString::number(subslotNumber++));
            submoduleElem.setAttribute("GSDRefID", submodule.id);
            moduleElem.appendChild(submoduleElem);
            
            QDomElement submoduleIOAddresses = doc.createElement("IOAddresses");
            submoduleElem.appendChild(submoduleIOAddresses);
            
            // Add input addresses if needed
            if (submodule.inputDataLength > 0) {
                QDomElement inputAddresses = doc.createElement("InputAddresses");
                inputAddresses.setAttribute("StartAddress", QString::number(currentInputAddress));
                submoduleIOAddresses.appendChild(inputAddresses);
                currentInputAddress += submodule.inputDataLength;
            }
            
            // Add output addresses if needed
            if (submodule.outputDataLength > 0) {
                QDomElement outputAddresses = doc.createElement("OutputAddresses");
                outputAddresses.setAttribute("StartAddress", QString::number(currentOutputAddress));
                submoduleIOAddresses.appendChild(outputAddresses);
                currentOutputAddress += submodule.outputDataLength;
            }
        }
    }
    
    // SharedDevice element
    QDomElement sharedDevice = doc.createElement("SharedDevice");
    decentralDevice.appendChild(sharedDevice);
    
    // AdvancedConfiguration
    QDomElement advancedConfig = doc.createElement("AdvancedConfiguration");
    decentralDevice.appendChild(advancedConfig);
    
    QDomElement snmp = doc.createElement("Snmp");
    advancedConfig.appendChild(snmp);
    
    QDomElement dcp = doc.createElement("Dcp");
    advancedConfig.appendChild(dcp);
    
    return doc.toString(2); // Indent with 2 spaces
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
