/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef CONFIGURATIONBUILDER_H
#define CONFIGURATIONBUILDER_H

#include <QString>
#include <QVector>
#include "../GsdmlParser/GsdmlParser.h"

namespace PNConfigLib {

/**
 * @brief Builds Configuration.xml from GSDML and parameters
 */
class ConfigurationBuilder {
public:
    struct DeviceConfig {
        QString name;
        QString ipAddress;
        QString routerIpAddress;
        int inputStartAddress;
        int outputStartAddress;
    };
    
    /**
     * @brief Generate Configuration XML string
     * @param gsdmlPath Path to GSDML file
     * @param masterConfig Master device configuration
     * @param slaveConfig Slave device configuration
     * @return XML string
     */
    static QString generateConfigurationXml(
        const QString& gsdmlPath,
        const DeviceConfig& masterConfig,
        const DeviceConfig& slaveConfig);
    
    /**
     * @brief Save Configuration XML to file
     * @param gsdmlPath Path to GSDML file
     * @param masterConfig Master device configuration
     * @param slaveConfig Slave device configuration
     * @param outputPath Output file path
     * @return true if successful
     */
    static bool saveConfigurationXml(
        const QString& gsdmlPath,
        const DeviceConfig& masterConfig,
        const DeviceConfig& slaveConfig,
        const QString& outputPath);

private:
    static QString buildCentralDevice(const DeviceConfig& masterConfig);
    static QString buildDecentralDevice(
        const GsdmlInfo& gsdmlInfo,
        const DeviceConfig& slaveConfig);
    static QString buildModules(const QVector<ModuleInfo>& modules, int& currentSlot);
    static QString buildSubmodules(const QVector<SubmoduleInfo>& submodules, int slot, int& currentSubslot);
};

} // namespace PNConfigLib

#endif // CONFIGURATIONBUILDER_H
