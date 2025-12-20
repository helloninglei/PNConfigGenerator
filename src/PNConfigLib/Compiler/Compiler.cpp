/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "Compiler.h"
#include "CompilerConstants.h"
#include "RecordGenerators.h"
#include "XmlEntities.h"
#include "BlobBuilder.h"
#include "../tinyxml2/tinyxml2.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <stdexcept>

using namespace tinyxml2;

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

// -----------------------------------------------------------------------------
// Structure Builders
// -----------------------------------------------------------------------------

XmlObject buildPortObject(
    int portNum, 
    uint32_t aidKey, 
    uint16_t laddr, 
    uint32_t classRid)
{
    XmlObject port;
    port.name = QString("Port %1").arg(portNum);
    port.classRid = classRid;
    
    // Key
    port.addScalar("Key", CompilerConstants::AID_Key, XmlDataType::UINT32, aidKey); // 32769 = 0x8001 (Port 1)
    
    // LADDR
    port.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, laddr);
    
    // DataRecordsConf
    QList<XmlField> emptyRecords;
    port.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf, emptyRecords);
    
    // DataRecordsTransferSequence (Scalar)
    port.addScalar("DataRecordsTransferSequence", CompilerConstants::AID_DataRecordsTransferSequence, XmlDataType::BLOB, QByteArray());
    
    return port;
}

// -----------------------------------------------------------------------------
// Main Generation Logic
// -----------------------------------------------------------------------------

QString Compiler::generateOutputXml(
    const Configuration& config,
    const ListOfNodes& nodes)
{
    // Load GSDML data
    QHash<QString, GsdmlInfo> gsdmlData = loadGsdmlData(nodes);
    
    XMLDocument doc;
    
    // XML Declaration
    XMLDeclaration* decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\"");
    doc.InsertEndChild(decl);
    
    // Root: HWConfiguration
    XmlObject root;
    root.name = "HWConfiguration";
    root.classRid = CompilerConstants::ClassRID_HWConfiguration;
    
    // -------------------------------------------------------------------------
    // 1. Windows_PNDriver (Controller)
    // -------------------------------------------------------------------------
    XmlObject driver;
    driver.name = "Windows_PNDriver";
    driver.classRid = CompilerConstants::ClassRID_Windows_PNDriver;
    
    // Driver DataRecordsConf (Records 4101, 45169 etc)
    QList<XmlField> driverRecords = RecordGenerators::generateWindowsPNDriverRecords();
    driver.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf_Driver, driverRecords);
    
    // Driver Interface
    XmlObject driverInterface;
    driverInterface.name = "PN_Driver_Windows_Interface";
    driverInterface.classRid = CompilerConstants::ClassRID_PN_Driver_Windows_Interface;
    
    // Interface Key (AID=2, Value=32768/0x8000)
    driverInterface.addScalar("Key", CompilerConstants::AID_Key, XmlDataType::UINT32, 32768);
    
    // LADDR (e.g. 259)
    driverInterface.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, 259);
    
    // Interface DataRecordsConf (IP, Name, Check, etc)
    QList<XmlField> ifRecords = RecordGenerators::generateDriverInterfaceRecords(
        config.centralDevice.ethernetAddresses.ipAddress,
        config.centralDevice.ethernetAddresses.subnetMask,
        config.centralDevice.ethernetAddresses.deviceName,
        config.centralDevice.ethernetAddresses.routerAddress
    );
    driverInterface.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf, ifRecords);
    
    // Link to IO System
    XmlObject linkObj;
    linkObj.name = "Link";
    
    XmlVariable targetRidVar; 
    targetRidVar.name = "TargetRID";
    targetRidVar.valueType = XmlValueType::Scalar;
    targetRidVar.value = QVariant(static_cast<qulonglong>(CompilerConstants::RID_IOSystem));
    linkObj.variables.append(targetRidVar);
    // Add AID variable to Link object
    XmlVariable linkAidVar;
    linkAidVar.name = "AID";
    linkAidVar.valueType = XmlValueType::Scalar;
    linkAidVar.value = 16;
    linkObj.variables.append(linkAidVar);
    
    driverInterface.children.append(linkObj); 
    driver.children.append(driverInterface);
    
    // Ports (e.g. Port 1)
    driver.children.append(buildPortObject(1, 32769, 260, CompilerConstants::ClassRID_Port));
    
    root.children.append(driver);
    
    // -------------------------------------------------------------------------
    // 2. PROFINET IO-System
    // -------------------------------------------------------------------------
    XmlObject ioSystem;
    ioSystem.name = "PROFINET IO-System";
    ioSystem.rid = CompilerConstants::RID_IOSystem;
    ioSystem.classRid = CompilerConstants::ClassRID_IOSystem;
    
    ioSystem.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, 261);
    
    // IOsysParamConfig
    QList<XmlField> ioParamRecords; 
    // Fill with ControllerProperties (Index 12352)
    // Ref: 3040 0010 0101 0000 002A 0008 0064 0258 012C 0000
    BlobBuilder bProp;
    bProp.appendUint16(0x3040); bProp.appendUint16(0x0010); bProp.appendUint16(0x0101);
    bProp.appendUint16(0x0000); bProp.appendUint16(0x002A); bProp.appendUint16(0x0008);
    bProp.appendUint16(0x0064); bProp.appendUint16(0x0258); bProp.appendUint16(0x012C);
    bProp.appendUint16(0x0000);
    XmlField fProp; fProp.key = 12352; fProp.value = bProp.toByteArray(); fProp.length = fProp.value.size();
    ioParamRecords.append(fProp);
    
    ioSystem.addBlobVariable("IOsysParamConfig", CompilerConstants::AID_IOsysParamConfig, ioParamRecords);
    
    // -------------------------------------------------------------------------
    // 3. Decentralized Devices
    // -------------------------------------------------------------------------
    int currentLaddr = 264; // Start address for devices
    
    for (const DecentralDeviceType& dev : config.decentralDevices) {
        XmlObject devObj;
        devObj.name = "PNet_Device"; // The Class name seems to be PNet_Device in ref
        
        devObj.classRid = CompilerConstants::ClassRID_Device;
        
        // Set GSDML filename from nodes if available
        for (const DecentralDeviceNode& node : nodes.decentralDevices) {
            if (node.deviceID == dev.deviceRefID && !node.gsdPath.isEmpty()) {
                devObj.gsdmlFile = QFileInfo(node.gsdPath).fileName();
                break;
            }
        }
        
        // Key 3:1 (Device ID?)
        devObj.addScalar("Key", 3, XmlDataType::UINT32, 1); 
        
        devObj.addScalar("DeactivatedConfig", CompilerConstants::AID_DeactivatedConfig, XmlDataType::BOOL, false);
        devObj.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, currentLaddr++);
        
        // IODevParamConfig (PROFINET device parameter records)
        GsdmlInfo gsdInfo;
        if (gsdmlData.contains(dev.deviceRefID)) {
            gsdInfo = gsdmlData[dev.deviceRefID];
        }
        
        QList<XmlField> ioDevRecords = RecordGenerators::generateIODevParamConfig(
            dev,
            gsdInfo,
            dev.ethernetAddresses.deviceName,
            dev.ethernetAddresses.ipAddress
        );
        devObj.addBlobVariable("IODevParamConfig", CompilerConstants::AID_IODevParamConfig, ioDevRecords);
        
        // Device Interface & Ports
        // Simplified hierarchy: Device -> Device Interface -> Ports
        
        // Network Parameters Object (Child of PNet_Device)
        XmlObject netParams;
        netParams.name = "Network Parameters";
        netParams.classRid = CompilerConstants::ClassRID_NetworkParameters;
        
        QList<XmlField> netRecs = RecordGenerators::generateNetworkParameters(
            dev.ethernetAddresses.ipAddress,
            dev.ethernetAddresses.subnetMask,
            dev.ethernetAddresses.deviceName
        );
        netParams.addBlobVariable("NetworkParamConfig", CompilerConstants::AID_NetworkParamConfig, netRecs);
        devObj.children.append(netParams);

        // Nested PNet_Device (Level 2)
        XmlObject subDevObj;
        subDevObj.name = "PNet_Device";
        subDevObj.classRid = CompilerConstants::ClassRID_Device;
        subDevObj.addScalar("Key", 3, XmlDataType::UINT32, 1);

        // 1. Dual Nested PNet_Device (Level 3)
        for (int i = 0; i < 2; ++i) {
            XmlObject subDevObj3;
            subDevObj3.name = "PNet_Device";
            subDevObj3.classRid = CompilerConstants::ClassRID_Device;
            subDevObj3.addScalar("Key", CompilerConstants::AID_Key, XmlDataType::UINT32, i + 1);
            
            // Add required variables
            subDevObj3.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, currentLaddr++);
            
            QList<XmlField> emptyRecords;
            subDevObj3.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf, emptyRecords);
            
            // DataRecordsTransferSequence (Scalar)
            subDevObj3.addScalar("DataRecordsTransferSequence", CompilerConstants::AID_DataRecordsTransferSequence, XmlDataType::BLOB, QByteArray());
            
            subDevObj.children.append(subDevObj3);
        }

        // 2. PNet_Device_Interface (Child of Level 2 PNet_Device)
        XmlObject devInterface;
        devInterface.name = "PNet_Device_Interface";
        devInterface.classRid = CompilerConstants::ClassRID_Device_Interface;
        devInterface.addScalar("Key", CompilerConstants::AID_Key, XmlDataType::UINT32, 32768);
        devInterface.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, currentLaddr++);
        
        QList<XmlField> emptyVars;
        devInterface.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf, emptyVars);
        devInterface.addScalar("DataRecordsTransferSequence", CompilerConstants::AID_DataRecordsTransferSequence, XmlDataType::BLOB, QByteArray());
        subDevObj.children.append(devInterface);

        // 3. Port 1 (Child of Level 2 PNet_Device)
        XmlObject port1 = buildPortObject(1, 0x8001, currentLaddr++, CompilerConstants::ClassRID_Port);
        subDevObj.children.append(port1);

        devObj.children.append(subDevObj);
        
        ioSystem.children.append(devObj);
        
        // Add Modules/Submodules here if we had full logic
    }
    
    root.children.append(ioSystem);
    
    // Serialize Root
    doc.InsertEndChild(XmlEntitySerializer::serializeObject(&doc, root));
    
    XMLPrinter printer;
    doc.Print(&printer);
    return QString::fromUtf8(printer.CStr());
}

// -----------------------------------------------------------------------------
// Unused Stub for now
// -----------------------------------------------------------------------------
QString Compiler::generateDeviceSection(
    const DecentralDeviceType& device,
    const QHash<QString, GsdmlInfo>& gsdmlData)
{
    return QString();
}

} // namespace PNConfigLib
