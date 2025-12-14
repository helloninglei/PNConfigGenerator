/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#include "InputValidator.h"
#include "ConsistencyLogger.h"
#include <QDir>

namespace PNConfigLib {

bool InputValidator::isConfigurationFileExist(const QString& configPath)
{
    if (configPath.isEmpty()) {
        ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
            "Configuration", "配置文件路径为空");
        return false;
    }
    
    if (!QFile::exists(configPath)) {
        ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
            "Configuration", QString("配置文件不存在: %1").arg(configPath));
        return false;
    }
    
    return true;
}

bool InputValidator::isListOfNodesFileExist(const QString& nodesPath)
{
    if (nodesPath.isEmpty()) {
        ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
            "ListOfNodes", "节点列表文件路径为空");
        return false;
    }
    
    if (!QFile::exists(nodesPath)) {
        ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
            "ListOfNodes", QString("节点列表文件不存在: %1").arg(nodesPath));
        return false;
    }
    
    return true;
}

bool InputValidator::isTopologyFileExist(const QString& topologyPath, bool& exists)
{
    exists = false;
    
    if (topologyPath.isEmpty()) {
        // Topology is optional
        return true;
    }
    
    if (QFile::exists(topologyPath)) {
        exists = true;
    }
    
    return true;
}

bool InputValidator::validateConfiguration(const Configuration& config, QStringList& errors)
{
    bool valid = true;
    
    // Check ConfigurationID
    if (config.configurationID.isEmpty()) {
        errors.append("配置ID (ConfigurationID) 为空");
        valid = false;
    }
    
    // Check ListOfNodesRefID
    if (config.listOfNodesRefID.isEmpty()) {
        errors.append("节点列表引用ID (ListOfNodesRefID) 为空");
        valid = false;
    }
    
    // Check Central Device
    if (config.centralDevice.deviceRefID.isEmpty()) {
        errors.append("中心设备引用ID (DeviceRefID) 为空");
        valid = false;
    }
    
    if (config.centralDevice.ethernetAddresses.ipAddress.isEmpty()) {
        errors.append("中心设备IP地址为空");
        valid = false;
    } else if (!isValidIPAddress(config.centralDevice.ethernetAddresses.ipAddress)) {
        errors.append(QString("中心设备IP地址格式无效: %1").arg(config.centralDevice.ethernetAddresses.ipAddress));
        valid = false;
    }
    
    if (config.centralDevice.ethernetAddresses.deviceName.isEmpty()) {
        errors.append("中心设备名称 (PNDeviceName) 为空");
        valid = false;
    }
    
    // Check Decentralized Devices
    for (int i = 0; i < config.decentralDevices.size(); i++) {
        const auto& dev = config.decentralDevices[i];
        QString prefix = QString("从站设备 %1: ").arg(i + 1);
        
        if (dev.deviceRefID.isEmpty()) {
            errors.append(prefix + "设备引用ID为空");
            valid = false;
        }
        
        if (dev.ethernetAddresses.ipAddress.isEmpty()) {
            errors.append(prefix + "IP地址为空");
            valid = false;
        } else if (!isValidIPAddress(dev.ethernetAddresses.ipAddress)) {
            errors.append(prefix + QString("IP地址格式无效: %1").arg(dev.ethernetAddresses.ipAddress));
            valid = false;
        }
        
        if (dev.ethernetAddresses.deviceName.isEmpty()) {
            errors.append(prefix + "设备名称为空");
            valid = false;
        } else {
            QString nameError;
            if (!isValidPNDeviceName(dev.ethernetAddresses.deviceName, nameError)) {
                errors.append(prefix + nameError);
                valid = false;
            }
        }
    }
    
    return valid;
}

bool InputValidator::validateListOfNodes(const ListOfNodes& nodes, QStringList& errors)
{
    bool valid = true;
    
    // Check ListOfNodesID
    if (nodes.listOfNodesID.isEmpty()) {
        errors.append("节点列表ID (ListOfNodesID) 为空");
        valid = false;
    }
    
    // Check PNDriver (Central Device Node)
    if (nodes.pnDriver.deviceID.isEmpty()) {
        errors.append("主站设备节点ID为空");
        valid = false;
    }
    
    // Check Decentralized Device Nodes
    for (int i = 0; i < nodes.decentralDevices.size(); i++) {
        const auto& dev = nodes.decentralDevices[i];
        QString prefix = QString("从站节点 %1: ").arg(i + 1);
        
        if (dev.deviceID.isEmpty()) {
            errors.append(prefix + "设备ID为空");
            valid = false;
        }
        
        if (dev.gsdPath.isEmpty()) {
            errors.append(prefix + "GSDML路径为空");
            valid = false;
        } else if (!QFile::exists(dev.gsdPath)) {
            // Try relative path from list of nodes directory
            errors.append(prefix + QString("GSDML文件不存在: %1").arg(dev.gsdPath));
            valid = false;
        }
    }
    
    return valid;
}

bool InputValidator::validateReferences(const Configuration& config, const ListOfNodes& nodes, QStringList& errors)
{
    bool valid = true;
    
    // Check ListOfNodesRefID matches ListOfNodesID
    if (config.listOfNodesRefID != nodes.listOfNodesID) {
        errors.append(QString("配置文件引用的节点列表ID (%1) 与节点列表文件中的ID (%2) 不匹配")
            .arg(config.listOfNodesRefID, nodes.listOfNodesID));
        valid = false;
    }
    
    // Check device count matches
    if (config.decentralDevices.size() != nodes.decentralDevices.size()) {
        errors.append(QString("配置文件中的从站设备数量 (%1) 与节点列表文件中的数量 (%2) 不匹配")
            .arg(config.decentralDevices.size())
            .arg(nodes.decentralDevices.size()));
        valid = false;
    }
    
    // Check device references match
    for (const auto& configDev : config.decentralDevices) {
        bool found = false;
        for (const auto& nodeDev : nodes.decentralDevices) {
            if (configDev.deviceRefID == nodeDev.deviceID) {
                found = true;
                break;
            }
        }
        if (!found) {
            errors.append(QString("配置文件中的设备引用 (%1) 在节点列表中不存在")
                .arg(configDev.deviceRefID));
            valid = false;
        }
    }
    
    return valid;
}

bool InputValidator::isValidPNDeviceName(const QString& name, QString& error)
{
    // PROFINET device name rules:
    // - Length: 1-240 characters
    // - Only lowercase letters, digits, hyphens, dots
    // - Cannot start or end with hyphen or dot
    // - Cannot contain "port-xxx" pattern at start
    // - Cannot be an IP address format
    
    if (name.isEmpty()) {
        error = "设备名称为空";
        return false;
    }
    
    if (name.length() > 240) {
        error = QString("设备名称过长 (%1 > 240 字符)").arg(name.length());
        return false;
    }
    
    QString lowerName = name.toLower();
    
    // Check for forbidden port pattern
    QRegularExpression portRegex("^port-\\d{3}");
    if (portRegex.match(lowerName).hasMatch()) {
        error = QString("设备名称不能以 'port-xxx' 开头: %1").arg(name);
        return false;
    }
    
    // Check for IP address format
    QRegularExpression ipRegex("\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b");
    if (ipRegex.match(lowerName).hasMatch()) {
        error = QString("设备名称不能包含IP地址格式: %1").arg(name);
        return false;
    }
    
    // Check valid characters (allow uppercase, lowercase, digits, hyphens, dots, underscores)
    QRegularExpression validChars("^[a-zA-Z0-9][a-zA-Z0-9._-]*[a-zA-Z0-9]$|^[a-zA-Z0-9]$");
    if (!validChars.match(name).hasMatch()) {
        error = QString("设备名称包含无效字符或格式错误: %1").arg(name);
        return false;
    }
    
    return true;
}

bool InputValidator::isValidIPAddress(const QString& ip)
{
    QRegularExpression ipRegex("^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$");
    return ipRegex.match(ip).hasMatch();
}

} // namespace PNConfigLib
