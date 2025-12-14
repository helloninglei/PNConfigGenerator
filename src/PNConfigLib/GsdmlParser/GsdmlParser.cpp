/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "GsdmlParser.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QMutexLocker>
#include <stdexcept>

namespace PNConfigLib {

// Static member initialization
QHash<QString, GsdmlInfo> GsdmlParser::s_gsdmlCache;
QMutex GsdmlParser::s_cacheMutex;

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
    
    QFile file(gsdmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fallbackParseGSDML(gsdmlPath);
    }
    
    QXmlStreamReader xml(&file);
    
    // Parse XML structure
    while (!xml.atEnd()) {
        xml.readNext();
        
        if (xml.isStartElement()) {
            QString name = xml.name().toString();
            
            // Extract device information
            if (name == "DeviceIdentity" || name == "Device") {
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("DeviceName")) {
                    info.deviceName = attrs.value("DeviceName").toString();
                }
                if (attrs.hasAttribute("VendorName")) {
                    info.deviceVendor = attrs.value("VendorName").toString();
                }
                if (attrs.hasAttribute("ID")) {
                    info.deviceID = attrs.value("ID").toString();
                }
            }
            
            // Extract device access point
            else if (name == "DeviceAccessPointItem") {
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("ID")) {
                    info.deviceAccessPointId = attrs.value("ID").toString();
                }
            }
            
            // Extract modules
            else if (name == "ModuleItem") {
                ModuleInfo module;
                QXmlStreamAttributes attrs = xml.attributes();
                
                if (attrs.hasAttribute("ID")) {
                    module.id = attrs.value("ID").toString();
                }
                if (attrs.hasAttribute("ModuleIdentNumber")) {
                    QString identStr = attrs.value("ModuleIdentNumber").toString();
                    module.moduleIdentNumber = identStr.startsWith("0x") ? 
                        identStr.mid(2).toUInt(nullptr, 16) : identStr.toUInt();
                }
                
                // Parse module contents
                while (!(xml.isEndElement() && xml.name() == "ModuleItem")) {
                    xml.readNext();
                    
                    if (xml.isStartElement()) {
                        if (xml.name() == "ModuleInfo") {
                            QXmlStreamAttributes modAttrs = xml.attributes();
                            if (modAttrs.hasAttribute("Name")) {
                                module.name = modAttrs.value("Name").toString();
                            }
                        }
                        else if (xml.name() == "VirtualSubmoduleItem") {
                            SubmoduleInfo submodule;
                            QXmlStreamAttributes subAttrs = xml.attributes();
                            
                            if (subAttrs.hasAttribute("ID")) {
                                submodule.id = subAttrs.value("ID").toString();
                            }
                            if (subAttrs.hasAttribute("SubmoduleIdentNumber")) {
                                QString identStr = subAttrs.value("SubmoduleIdentNumber").toString();
                                submodule.submoduleIdentNumber = identStr.startsWith("0x") ?
                                    identStr.mid(2).toUInt(nullptr, 16) : identStr.toUInt();
                            }
                            
                            // Parse submodule IOData
                            while (!(xml.isEndElement() && xml.name() == "VirtualSubmoduleItem")) {
                                xml.readNext();
                                
                                if (xml.isStartElement()) {
                                    if (xml.name() == "ModuleInfo") {
                                        QXmlStreamAttributes smAttrs = xml.attributes();
                                        if (smAttrs.hasAttribute("Name")) {
                                            submodule.name = smAttrs.value("Name").toString();
                                        }
                                    }
                                    else if (xml.name() == "IOData") {
                                        // Parse Input/Output data lengths
                                        while (!(xml.isEndElement() && xml.name() == "IOData")) {
                                            xml.readNext();
                                            
                                            if (xml.isStartElement()) {
                                                if (xml.name() == "Input") {
                                                    int length = 0;
                                                    while (!(xml.isEndElement() && xml.name() == "Input")) {
                                                        xml.readNext();
                                                        if (xml.isStartElement() && xml.name() == "DataItem") {
                                                            QXmlStreamAttributes dataAttrs = xml.attributes();
                                                            if (dataAttrs.hasAttribute("DataType")) {
                                                                QString dataType = dataAttrs.value("DataType").toString();
                                                                length += getDataTypeLength(dataType);
                                                            }
                                                        }
                                                    }
                                                    submodule.inputDataLength = length;
                                                }
                                                else if (xml.name() == "Output") {
                                                    int length = 0;
                                                    while (!(xml.isEndElement() && xml.name() == "Output")) {
                                                        xml.readNext();
                                                        if (xml.isStartElement() && xml.name() == "DataItem") {
                                                            QXmlStreamAttributes dataAttrs = xml.attributes();
                                                            if (dataAttrs.hasAttribute("DataType")) {
                                                                QString dataType = dataAttrs.value("DataType").toString();
                                                                length += getDataTypeLength(dataType);
                                                            }
                                                        }
                                                    }
                                                    submodule.outputDataLength = length;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            
                            module.submodules.append(submodule);
                        }
                    }
                }
                
                info.modules.append(module);
            }
        }
    }
    
    if (xml.hasError()) {
        return fallbackParseGSDML(gsdmlPath);
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
