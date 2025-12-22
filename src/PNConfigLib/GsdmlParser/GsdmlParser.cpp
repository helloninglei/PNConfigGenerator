/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "GsdmlParser.h"
#include "tinyxml2/tinyxml2.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QMutexLocker>
#include <stdexcept>

using namespace tinyxml2;

namespace PNConfigLib {

// Static member initialization
QHash<QString, GsdmlInfo> GsdmlParser::s_gsdmlCache;
QMutex GsdmlParser::s_cacheMutex;

static QString getAttribute(XMLElement* element, const char* name, const QString& defaultValue = QString())
{
    if (!element) return defaultValue;
    const char* val = element->Attribute(name);
    return val ? QString(val) : defaultValue;
}

GsdmlInfo GsdmlParser::parseGSDML(const QString& gsdmlPath)
{
    if (gsdmlPath.isEmpty()) {
        throw std::runtime_error("GSDML path cannot be empty");
    }
    
    if (!QFile::exists(gsdmlPath)) {
        throw std::runtime_error(QString("GSDML file not found: %1").arg(gsdmlPath).toStdString());
    }
    
    // Check cache
    QMutexLocker locker(&s_cacheMutex);
    QString fileKey = getFileKey(gsdmlPath);
    QFileInfo fileInfo(gsdmlPath);
    
    if (s_gsdmlCache.contains(fileKey)) {
        const GsdmlInfo& cached = s_gsdmlCache[fileKey];
        if (cached.lastModified == fileInfo.lastModified()) {
            return cached;
        }
    }
    
    // Parse file
    locker.unlock();
    GsdmlInfo info = parseGSDMLFile(gsdmlPath);
    
    // Update cache
    locker.relock();
    s_gsdmlCache[fileKey] = info;
    
    return info;
}

QString GsdmlParser::getDeviceAccessPointID(const QString& gsdmlPath)
{
    try {
        GsdmlInfo info = parseGSDML(gsdmlPath);
        return info.deviceAccessPointId;
    } catch (...) {
        return extractDeviceAccessPointIDByRegex(gsdmlPath);
    }
}

QVector<ModuleInfo> GsdmlParser::parseModulesFromGSDML(const QString& gsdmlPath)
{
    try {
        GsdmlInfo info = parseGSDML(gsdmlPath);
        return info.modules;
    } catch (...) {
        return QVector<ModuleInfo>();
    }
}

void GsdmlParser::clearCache()
{
    QMutexLocker locker(&s_cacheMutex);
    s_gsdmlCache.clear();
}

GsdmlInfo GsdmlParser::parseGSDMLFile(const QString& gsdmlPath)
{
    GsdmlInfo info;
    info.filePath = gsdmlPath;
    info.lastModified = QFileInfo(gsdmlPath).lastModified();
    
    XMLDocument doc;
    XMLError err = doc.LoadFile(gsdmlPath.toStdString().c_str());
    
    if (err != XML_SUCCESS) {
        return fallbackParseGSDML(gsdmlPath);
    }
    
    XMLElement* root = doc.RootElement(); // ISO15745Profile usually
    if (!root) return fallbackParseGSDML(gsdmlPath);

    XMLElement* profileBody = root->FirstChildElement("ProfileBody");
    if (!profileBody) return fallbackParseGSDML(gsdmlPath);

    // 1. Parse ExternalTextList/PrimaryLanguage for TextId resolution
    QHash<QString, QString> textMap;
    XMLElement* appProcess = profileBody->FirstChildElement("ApplicationProcess");
    if (appProcess) {
        XMLElement* extTextList = appProcess->FirstChildElement("ExternalTextList");
        if (extTextList) {
            XMLElement* primLang = extTextList->FirstChildElement("PrimaryLanguage");
            if (primLang) {
                XMLElement* textItem = primLang->FirstChildElement("Text");
                while (textItem) {
                    QString id = getAttribute(textItem, "TextId");
                    QString val = getAttribute(textItem, "Value");
                    if (!id.isEmpty()) {
                        textMap[id] = val;
                    }
                    textItem = textItem->NextSiblingElement("Text");
                }
            }
        }
    }

    auto resolve = [&](const QString& textId) -> QString {
        if (textId.isEmpty()) return textId;
        return textMap.value(textId, textId); // Return original if not found
    };

    // 2. Extract Device Identity
    XMLElement* deviceIdent = profileBody->FirstChildElement("DeviceIdentity");
    if (deviceIdent) {
        // Extract VendorID and DeviceID hex values
        QString vendorIdStr = getAttribute(deviceIdent, "VendorID");
        QString deviceIdStr = getAttribute(deviceIdent, "DeviceID");
        
        if (!vendorIdStr.isEmpty()) {
            info.vendorId = vendorIdStr.startsWith("0x") ?
                vendorIdStr.mid(2).toUInt(nullptr, 16) : vendorIdStr.toUInt();
        }
        if (!deviceIdStr.isEmpty()) {
            info.deviceId = deviceIdStr.startsWith("0x") ?
                deviceIdStr.mid(2).toUInt(nullptr, 16) : deviceIdStr.toUInt();
        }
        
        // Extract vendor name
        XMLElement* vendorName = deviceIdent->FirstChildElement("VendorName");
        if (vendorName) {
            info.deviceVendor = resolve(getAttribute(vendorName, "Value"));
        }
    }

    // Extract Device Function (MainFamily and ProductFamily)
    XMLElement* deviceFunction = profileBody->FirstChildElement("DeviceFunction");
    if (deviceFunction) {
        XMLElement* family = deviceFunction->FirstChildElement("Family");
        if (family) {
            info.mainFamily = resolve(getAttribute(family, "MainFamily", "I/O"));
            info.productFamily = resolve(getAttribute(family, "ProductFamily", "P-Net Samples"));
        }
    }
    
    if (!appProcess) return fallbackParseGSDML(gsdmlPath);
    
    // Extract device access point
    XMLElement* dapList = appProcess->FirstChildElement("DeviceAccessPointList");
    if (dapList) {
        XMLElement* dapItem = dapList->FirstChildElement("DeviceAccessPointItem");
        if (dapItem) {
            info.deviceAccessPointId = getAttribute(dapItem, "ID");
            
            // Extract Device Name from ModuleInfo
            XMLElement* modInfo = dapItem->FirstChildElement("ModuleInfo");
            if (modInfo) {
                info.deviceName = resolve(getAttribute(modInfo, "Name"));
            }
            if (info.deviceName.isEmpty()) {
                info.deviceName = info.deviceAccessPointId;
            }

            // Extract DAP ModuleIdentNumber
            QString dapModuleIdStr = getAttribute(dapItem, "ModuleIdentNumber");
            if (!dapModuleIdStr.isEmpty()) {
                info.dapModuleId = dapModuleIdStr.startsWith("0x") ?
                    dapModuleIdStr.mid(2).toUInt(nullptr, 16) : dapModuleIdStr.toUInt();
            }
        }
    }
    
    // Extract modules
    XMLElement* moduleList = appProcess->FirstChildElement("ModuleList");
    if (moduleList) {
        XMLElement* moduleItem = moduleList->FirstChildElement("ModuleItem");
        while (moduleItem) {
            ModuleInfo module;
            module.id = getAttribute(moduleItem, "ID");
            
            QString identStr = getAttribute(moduleItem, "ModuleIdentNumber");
            if (!identStr.isEmpty()) {
                module.moduleIdentNumber = identStr.startsWith("0x") ? 
                    identStr.mid(2).toUInt(nullptr, 16) : identStr.toUInt();
            }
            
            // Module Info
            XMLElement* modInfo = moduleItem->FirstChildElement("ModuleInfo");
            if (modInfo) {
                module.name = resolve(getAttribute(modInfo, "Name"));
            }
            
            // Virtual Submodules
            XMLElement* vSubList = moduleItem->FirstChildElement("VirtualSubmoduleList");
            if (vSubList) {
                XMLElement* subItem = vSubList->FirstChildElement("VirtualSubmoduleItem");
                while (subItem) {
                    SubmoduleInfo submodule;
                    submodule.id = getAttribute(subItem, "ID");
                    
                    QString subIdentStr = getAttribute(subItem, "SubmoduleIdentNumber");
                    if (!subIdentStr.isEmpty()) {
                        submodule.submoduleIdentNumber = subIdentStr.startsWith("0x") ?
                            subIdentStr.mid(2).toUInt(nullptr, 16) : subIdentStr.toUInt();
                    }
                    
                    XMLElement* smInfo = subItem->FirstChildElement("ModuleInfo");
                    if (smInfo) {
                        submodule.name = resolve(getAttribute(smInfo, "Name"));
                    }
                    
                    // Parse IOData
                    XMLElement* ioData = subItem->FirstChildElement("IOData");
                    if (ioData) {
                        // Input
                        XMLElement* input = ioData->FirstChildElement("Input");
                        if (input) {
                            int length = 0;
                            XMLElement* dataItem = input->FirstChildElement("DataItem");
                            while (dataItem) {
                                length += getDataTypeLength(getAttribute(dataItem, "DataType"));
                                dataItem = dataItem->NextSiblingElement("DataItem");
                            }
                            submodule.inputDataLength = length;
                        }
                        
                        // Output
                        XMLElement* output = ioData->FirstChildElement("Output");
                        if (output) {
                            int length = 0;
                            XMLElement* dataItem = output->FirstChildElement("DataItem");
                            while (dataItem) {
                                length += getDataTypeLength(getAttribute(dataItem, "DataType"));
                                dataItem = dataItem->NextSiblingElement("DataItem");
                            }
                            submodule.outputDataLength = length;
                        }
                    }
                    
                    module.submodules.append(submodule);
                    subItem = subItem->NextSiblingElement("VirtualSubmoduleItem");
                }
            }
            
            info.modules.append(module);
            moduleItem = moduleItem->NextSiblingElement("ModuleItem");
        }
    }
    
    return info;
}

int GsdmlParser::getDataTypeLength(const QString& dataType)
{
    QString lower = dataType.toLower();
    
    if (lower.contains("unsigned8") || lower.contains("signed8") || lower.contains("binary8")) {
        return 1;
    }
    if (lower.contains("unsigned16") || lower.contains("signed16") || lower.contains("binary16")) {
        return 2;
    }
    if (lower.contains("unsigned32") || lower.contains("signed32") || 
        lower.contains("binary32") || lower.contains("float32")) {
        return 4;
    }
    if (lower.contains("unsigned64") || lower.contains("signed64") || 
        lower.contains("binary64") || lower.contains("float64")) {
        return 8;
    }
    
    return 0;
}

QString GsdmlParser::getFileKey(const QString& gsdmlPath)
{
    return QFileInfo(gsdmlPath).absoluteFilePath().toLower();
}

GsdmlInfo GsdmlParser::fallbackParseGSDML(const QString& gsdmlPath)
{
    GsdmlInfo info;
    info.filePath = gsdmlPath;
    info.deviceAccessPointId = extractDeviceAccessPointIDByRegex(gsdmlPath);
    info.lastModified = QFileInfo(gsdmlPath).lastModified();
    return info;
}

QString GsdmlParser::extractDeviceAccessPointIDByRegex(const QString& gsdmlPath)
{
    QFile file(gsdmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "UnknownDevice";
    }
    
    QString content = QString::fromUtf8(file.readAll());
    QRegularExpression regex("<DeviceAccessPointItem[^>]*ID\\s*=\\s*\"([^\"]+)\"");
    QRegularExpressionMatch match = regex.match(content);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return "UnknownDevice";
}

} // namespace PNConfigLib
