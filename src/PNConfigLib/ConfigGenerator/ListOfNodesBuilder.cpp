/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ListOfNodesBuilder.h"
#include "../GsdmlParser/GsdmlParser.h"
#include "../tinyxml2/tinyxml2.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

using namespace tinyxml2;

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
    
    XMLDocument doc;
    
    // XML declaration
    XMLDeclaration* decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\"");
    doc.InsertEndChild(decl);
    
    // Root element with namespaces
    XMLElement* root = doc.NewElement("ListOfNodes");
    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root->SetAttribute("ListOfNodesID", "ListOfNodesID");
    root->SetAttribute("schemaVersion", "1.0");
    root->SetAttribute("xmlns", "http://www.siemens.com/Automation/PNConfigLib/ListOfNodes");
    doc.InsertEndChild(root);
    
    // PNDriver element (not CentralDevice!)
    XMLElement* pnDriver = doc.NewElement("PNDriver");
    pnDriver->SetAttribute("DeviceID", (masterNode.name + "_ID").toStdString().c_str());
    pnDriver->SetAttribute("DeviceName", masterNode.name.toStdString().c_str());
    pnDriver->SetAttribute("DeviceVersion", "v3.1");
    root->InsertEndChild(pnDriver);
    
    XMLElement* masterInterface = doc.NewElement("Interface");
    masterInterface->SetAttribute("InterfaceID", (masterNode.name + "_Interface").toStdString().c_str());
    masterInterface->SetAttribute("InterfaceName", (masterNode.name + "_Interface").toStdString().c_str());
    masterInterface->SetAttribute("InterfaceType", "Linux Native");
    pnDriver->InsertEndChild(masterInterface);
    
    // DecentralDevice element
    XMLElement* decentralDevice = doc.NewElement("DecentralDevice");
    decentralDevice->SetAttribute("DeviceID", (slaveNode.name + "_ID").toStdString().c_str());
    decentralDevice->SetAttribute("GSDPath", QFileInfo(gsdmlPath).absoluteFilePath().toStdString().c_str());
    decentralDevice->SetAttribute("GSDRefID", gsdRefId.toStdString().c_str());
    decentralDevice->SetAttribute("DeviceName", slaveNode.name.toStdString().c_str());
    root->InsertEndChild(decentralDevice);
    
    XMLElement* slaveInterface = doc.NewElement("Interface");
    slaveInterface->SetAttribute("InterfaceID", (slaveNode.name + "_Interface").toStdString().c_str());
    slaveInterface->SetAttribute("InterfaceName", (slaveNode.name + "_Interface").toStdString().c_str());
    decentralDevice->InsertEndChild(slaveInterface);
    
    XMLPrinter printer;
    doc.Print(&printer);
    return QString::fromUtf8(printer.CStr());
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
