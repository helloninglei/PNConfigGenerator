/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ListOfNodesBuilder.h"
#include "../GsdmlParser/GsdmlParser.h"
#include <QFile>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>

namespace PNConfigLib {

QString ListOfNodesBuilder::generateListOfNodesXml(
    const QString& gsdmlPath,
    const DeviceNode& masterNode,
    const DeviceNode& slaveNode)
{
    // Get GSDML info for GSD Reference ID
    QString gsdRefId;
    try {
        auto gsdmlInfo = GsdmlParser::parseGSDML(gsdmlPath);
        gsdRefId = gsdmlInfo.deviceAccessPointId.isEmpty() ? "IDD_1" : "IDD_1";
    } catch (...) {
        gsdRefId = "IDD_1";
    }
    
    QDomDocument doc;
    
    // XML declaration
    QDomProcessingInstruction xmlDecl = doc.createProcessingInstruction(
        "xml", "version=\"1.0\" encoding=\"utf-8\"");
    doc.appendChild(xmlDecl);
    
    // Root element with namespaces
    QDomElement root = doc.createElement("ListOfNodes");
    root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root.setAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root.setAttribute("ListOfNodesID", "ListOfNodesID");
    root.setAttribute("schemaVersion", "1.0");
    root.setAttribute("xmlns", "http://www.siemens.com/Automation/PNConfigLib/ListOfNodes");
    doc.appendChild(root);
    
    // PNDriver element (not CentralDevice!)
    QDomElement pnDriver = doc.createElement("PNDriver");
    pnDriver.setAttribute("DeviceID", masterNode.name + "_ID");
    pnDriver.setAttribute("DeviceName", masterNode.name);
    pnDriver.setAttribute("DeviceVersion", "v3.1");
    root.appendChild(pnDriver);
    
    QDomElement masterInterface = doc.createElement("Interface");
    masterInterface.setAttribute("InterfaceID", masterNode.name + "_Interface");
    masterInterface.setAttribute("InterfaceName", masterNode.name + "_Interface");
    masterInterface.setAttribute("InterfaceType", "Linux Native");
    pnDriver.appendChild(masterInterface);
    
    // DecentralDevice element
    QDomElement decentralDevice = doc.createElement("DecentralDevice");
    decentralDevice.setAttribute("DeviceID", slaveNode.name + "_ID");
    decentralDevice.setAttribute("GSDPath", QFileInfo(gsdmlPath).absoluteFilePath());
    decentralDevice.setAttribute("GSDRefID", gsdRefId);
    decentralDevice.setAttribute("DeviceName", slaveNode.name);
    root.appendChild(decentralDevice);
    
    QDomElement slaveInterface = doc.createElement("Interface");
    slaveInterface.setAttribute("InterfaceID", slaveNode.name + "_Interface");
    slaveInterface.setAttribute("InterfaceName", slaveNode.name + "_Interface");
    decentralDevice.appendChild(slaveInterface);
    
    return doc.toString(2); // Indent with 2 spaces
}

bool ListOfNodesBuilder::saveListOfNodesXml(
    const QString& gsdmlPath,
    const DeviceNode& masterNode,
    const DeviceNode& slaveNode,
    const QString& outputPath)
{
    QString xml = generateListOfNodesXml(gsdmlPath, masterNode, slaveNode);
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
