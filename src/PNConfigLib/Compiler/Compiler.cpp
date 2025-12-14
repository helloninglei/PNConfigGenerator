/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "Compiler.h"
#include "CompilerConstants.h"
#include "RecordGenerators.h"
#include "XmlEntities.h"
#include "BlobBuilder.h"
#include <QFile>
#include <QTextStream>
#include <QDomDocument>
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
    XmlField keyField; 
    keyField.key = aidKey; 
    // Key is technically a variable here in ref logic? 
    // Ref: <Object Name="Port 1"><Key AID="2">32769</Key> ...
    // NOTE: In our XmlEntities, Key is probably not represented as a standard variable?
    // Wait, ref: <Key AID="2">32769</Key>. This is special.
    // Our XmlObject doesn't have explicit Key support except QHash keys?
    // Let's treat Key as a variable with specific name "Key" and AID=2?
    // Actually, Ref XML: <Key AID="2">32769</Key>. This looks like a Variable named "Key" with AID=2.
    // Let's try that.
    port.addScalar("Key", CompilerConstants::AID_Key, XmlDataType::UINT32, aidKey); // 32769 = 0x8001 (Port 1)
    
    // LADDR
    port.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, laddr);
    
    // DataRecordsConf
    QList<XmlField> emptyRecords;
    port.addBlobVariable("DataRecordsConf", CompilerConstants::AID_DataRecordsConf, emptyRecords);
    
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
    
    QDomDocument doc;
    
    // XML Declaration
    QDomProcessingInstruction xmlDecl = doc.createProcessingInstruction(
        "xml", "version=\"1.0\" encoding=\"utf-8\"");
    doc.appendChild(xmlDecl);
    
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
    
    // Driver DataRecordsConf (Records 4101, 45169 etc) - simplified for now
    QList<XmlField> driverRecords;
    // Add dummy or calculated records here if needed
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
    XmlVariable linkVar;
    linkVar.name = "Link";
    linkVar.aid = CompilerConstants::AID_Link;
    XmlField linkField;
    linkField.key = 0; // Unused for Link?
    linkField.length = 0;
    // Link uses TargetRID in ref: <Link><TargetRID>...</TargetRID></Link>
    // Our XmlVariable/XmlEntity might need extension for "Link" type if it's special.
    // A Link in Ref looks like an Object? <Link><AID>16</AID><TargetRID>...</TargetRID></Link>
    // It has AID=16. Let's treat it as a child object for now or custom variable serialization.
    // For now, let's implement it as a special case in XmlEntities or just a child object named Link.
    // Let's add it as a child object to simplify.
    XmlObject linkObj;
    linkObj.name = "Link";
    
    // Actually Ref: <Link><AID>16</AID><TargetRID>2296447237</TargetRID></Link>
    // AID is added explicitly below as a variable.
    
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
    
    // Network Parameters Object
    XmlObject netParams;
    netParams.name = "Network Parameters";
    netParams.classRid = CompilerConstants::ClassRID_NetworkParameters;
    
    QList<XmlField> netRecs = RecordGenerators::generateNetworkParameters(
        config.centralDevice.ethernetAddresses.ipAddress,
        config.centralDevice.ethernetAddresses.subnetMask,
        config.centralDevice.ethernetAddresses.deviceName
    );
    netParams.addBlobVariable("NetworkParamConfig", CompilerConstants::AID_NetworkParamConfig, netRecs);
    ioSystem.children.append(netParams);
    
    // -------------------------------------------------------------------------
    // 3. Decentralized Devices
    // -------------------------------------------------------------------------
    int currentLaddr = 264; // Start address for devices
    
    for (const DecentralDeviceType& dev : config.decentralDevices) {
        XmlObject devObj;
        devObj.name = dev.ethernetAddresses.deviceName; // Or "PNet_Device"
        devObj.name = "PNet_Device"; // The Class name seems to be PNet_Device in ref
        
        // Add GSDML file reference (Variable? Or just implied?)
        // Ref: <GSDMLFile>...</GSDMLFile>. Special Element.
        // We can't support this easily with generic Object/Variable serializer.
        // But let's proceed without it for a moment.
        
        devObj.classRid = CompilerConstants::ClassRID_Device;
        
        // Key 3:1 (Device ID?)
        devObj.addScalar("Key", 3, XmlDataType::UINT32, 1); 
        
        devObj.addScalar("DeactivatedConfig", CompilerConstants::AID_DeactivatedConfig, XmlDataType::BOOL, false);
        devObj.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, currentLaddr++);
        
        // IODevParamConfig (The big one)
        XmlField emptyBigField; emptyBigField.key=12546; emptyBigField.length=0; 
        QList<XmlField> ioDevRecords;
        ioDevRecords.append(emptyBigField);
        // Note: Real implementation would generate this
        devObj.addBlobVariable("IODevParamConfig", CompilerConstants::AID_IODevParamConfig, ioDevRecords);
        
        // Device Interface & Ports
        // Simplified hierarchy: Device -> Device Interface -> Ports
        
        // PNet_Device_Interface
        XmlObject devInterface;
        devInterface.name = "PNet_Device_Interface";
        devInterface.classRid = CompilerConstants::ClassRID_Device_Interface;
        devInterface.addScalar("LADDR", CompilerConstants::AID_LADDR, XmlDataType::UINT16, currentLaddr++);
        devObj.children.append(devInterface);
        
        ioSystem.children.append(devObj);
        
        // Add Modules/Submodules here if we had full logic
    }
    
    root.children.append(ioSystem);
    
    // Serialize Root
    doc.appendChild(XmlEntitySerializer::serializeObject(doc, root));
    
    // Post-process to fix "Key" variables to <Key> elements?
    // The current XmlEntitySerializer outputs: <Variable Name="Key"><Value ...>...</Value></Variable>
    // The Ref has: <Key AID="2">32768</Key>
    // This is a semantic difference. We might need to adjust the serializer to handle "Key" named scalar variables specially.
    
    return doc.toString(2);
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
