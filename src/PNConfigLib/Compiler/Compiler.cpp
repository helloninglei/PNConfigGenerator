/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "Compiler.h"
#include <QFile>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <stdexcept>

namespace PNConfigLib {

bool Compiler::compile(
    const Configuration& config,
    const ListOfNodes& nodes,
    const QString& outputPath)
{
    QString xml = generateOutputXml(config, nodes);
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

QString Compiler::generateOutputXml(
    const Configuration& config,
    const ListOfNodes& nodes)
{
    // Load GSDML data for all devices
    QHash<QString, GsdmlInfo> gsdmlData = loadGsdmlData(nodes);
    
    QDomDocument doc;
    
    // XML declaration
    QDomProcessingInstruction xmlDecl = doc.createProcessingInstruction(
        "xml", "version=\"1.0\" encoding=\"utf-8\"");
    doc.appendChild(xmlDecl);
    
    // Root element - using generic "PROFINETConfiguration" 
    // (actual schema depends on target runtime)
    QDomElement root = doc.createElement("PROFINETConfiguration");
    root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root.setAttribute("ConfigurationName", config.configurationName);
    doc.appendChild(root);
    
    // Network Settings Section
    QDomElement networkSettings = doc.createElement("NetworkSettings");
    root.appendChild(networkSettings);
    
    QDomElement masterDevice = doc.createElement("MasterDevice");
    masterDevice.setAttribute("Name", config.centralDevice.ethernetAddresses.deviceName);
    masterDevice.setAttribute("IPAddress", config.centralDevice.ethernetAddresses.ipAddress);
    masterDevice.setAttribute("SubnetMask", config.centralDevice.ethernetAddresses.subnetMask);
    if (!config.centralDevice.ethernetAddresses.routerAddress.isEmpty()) {
        masterDevice.setAttribute("RouterAddress", config.centralDevice.ethernetAddresses.routerAddress);
    }
    networkSettings.appendChild(masterDevice);
    
    // Devices Section
    QDomElement devicesSection = doc.createElement("Devices");
    root.appendChild(devicesSection);
    
    // Add each decentralized device
    for (const DecentralDeviceType& device : config.decentralDevices) {
        QString deviceXml = generateDeviceSection(device, gsdmlData);
        if (!deviceXml.isEmpty()) {
            QDomDocument tempDoc;
            if (tempDoc.setContent(deviceXml)) {
                QDomElement deviceElem = tempDoc.documentElement();
                QDomNode importedNode = doc.importNode(deviceElem, true);
                devicesSection.appendChild(importedNode);
            }
        }
    }
    
    return doc.toString(2);
}

QHash<QString, GsdmlInfo> Compiler::loadGsdmlData(const ListOfNodes& nodes)
{
    QHash<QString, GsdmlInfo> result;
    
    for (const DecentralDeviceNode& device : nodes.decentralDevices) {
        if (!device.gsdPath.isEmpty()) {
            try {
                GsdmlInfo info = GsdmlParser::parseGSDML(device.gsdPath);
                result[device.deviceID] = info;
            } catch (...) {
                // Skip devices with parse errors
            }
        }
    }
    
    return result;
}

QString Compiler::generateDeviceSection(
    const DecentralDeviceType& device,
    const QHash<QString, GsdmlInfo>& gsdmlData)
{
    QDomDocument doc;
    
    QDomElement deviceElem = doc.createElement("Device");
    deviceElem.setAttribute("ID", device.deviceRefID);
    deviceElem.setAttribute("Name", device.ethernetAddresses.deviceName);
    deviceElem.setAttribute("IPAddress", device.ethernetAddresses.ipAddress);
    deviceElem.setAttribute("SubnetMask", device.ethernetAddresses.subnetMask);
    doc.appendChild(deviceElem);
    
    // Modules Section
    QDomElement modulesElem = doc.createElement("Modules");
    deviceElem.appendChild(modulesElem);
    
    for (const ModuleType& module : device.modules) {
        QDomElement moduleElem = doc.createElement("Module");
        moduleElem.setAttribute("ID", module.moduleID);
        moduleElem.setAttribute("SlotNumber", module.slotNumber);
        moduleElem.setAttribute("GSDRefID", module.gsdRefID);
        modulesElem.appendChild(moduleElem);
        
        // Submodules
        for (const SubmoduleType& submodule : module.submodules) {
            QDomElement submoduleElem = doc.createElement("Submodule");
            submoduleElem.setAttribute("ID", submodule.submoduleID);
            submoduleElem.setAttribute("SubslotNumber", submodule.subslotNumber);
            submoduleElem.setAttribute("GSDRefID", submodule.gsdRefID);
            moduleElem.appendChild(submoduleElem);
            
            // IO Addresses
            if (submodule.ioAddresses.inputStartAddress >= 0 || 
                submodule.ioAddresses.outputStartAddress >= 0) {
                QDomElement ioElem = doc.createElement("IOMapping");
                
                if (submodule.ioAddresses.inputStartAddress >= 0) {
                    ioElem.setAttribute("InputStart", submodule.ioAddresses.inputStartAddress);
                    if (submodule.ioAddresses.inputLength > 0) {
                        ioElem.setAttribute("InputLength", submodule.ioAddresses.inputLength);
                    }
                }
                
                if (submodule.ioAddresses.outputStartAddress >= 0) {
                    ioElem.setAttribute("OutputStart", submodule.ioAddresses.outputStartAddress);
                    if (submodule.ioAddresses.outputLength > 0) {
                        ioElem.setAttribute("OutputLength", submodule.ioAddresses.outputLength);
                    }
                }
                
                submoduleElem.appendChild(ioElem);
            }
        }
    }
    
    return doc.toString(2);
}

} // namespace PNConfigLib
