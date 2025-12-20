/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef GSDMLPARSER_H
#define GSDMLPARSER_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QDateTime>
#include <QMutex>

namespace PNConfigLib {

/**
 * @brief Submodule information extracted from GSDML
 */
struct SubmoduleInfo {
    QString id;
    QString name;
    uint32_t submoduleIdentNumber;
    int inputDataLength;
    int outputDataLength;
    
    SubmoduleInfo() : submoduleIdentNumber(0), inputDataLength(0), outputDataLength(0) {}
};

/**
 * @brief Module information extracted from GSDML
 */
struct ModuleInfo {
    QString id;
    QString name;
    uint32_t moduleIdentNumber;
    QVector<SubmoduleInfo> submodules;
    
    ModuleInfo() : moduleIdentNumber(0) {}
};

/**
 * @brief Complete GSDML file information
 */
struct GsdmlInfo {
    QString filePath;
    QString deviceName;
    QString deviceVendor;
    QString deviceID;
    QString deviceAccessPointId;
    uint32_t vendorId = 0;      // Numeric vendor ID from GSDML
    uint32_t deviceId = 0;      // Numeric device ID from GSDML
    uint32_t dapModuleId = 0;   // DAP ModuleIdentNumber
    QVector<ModuleInfo> modules;
    QDateTime lastModified;
};

/**
 * @brief GSDML XML parser with caching
 * 
 * Parses GSDML files to extract device, module, and submodule information.
 * Implements caching for performance.
 */
class GsdmlParser {
public:
    /**
     * @brief Parse GSDML file and extract device information
     * @param gsdmlPath Path to GSDML file
     * @return Parsed GSDML information
     * @throws std::runtime_error if file doesn't exist or parsing fails
     */
    static GsdmlInfo parseGSDML(const QString& gsdmlPath);
    
    /**
     * @brief Get device access point ID from GSDML file
     * @param gsdmlPath Path to GSDML file
     * @return Device access point ID
     */
    static QString getDeviceAccessPointID(const QString& gsdmlPath);
    
    /**
     * @brief Parse modules from GSDML file
     * @param gsdmlPath Path to GSDML file
     * @return List of modules
     */
    static QVector<ModuleInfo> parseModulesFromGSDML(const QString& gsdmlPath);
    
    /**
     * @brief Clear the parser cache
     */
    static void clearCache();

private:
    static GsdmlInfo parseGSDMLFile(const QString& gsdmlPath);
    static int getDataTypeLength(const QString& dataType);
    static QString getFileKey(const QString& gsdmlPath);
    static GsdmlInfo fallbackParseGSDML(const QString& gsdmlPath);
    static QString extractDeviceAccessPointIDByRegex(const QString& gsdmlPath);
    
    static QHash<QString, GsdmlInfo> s_gsdmlCache;
    static QMutex s_cacheMutex;
};

} // namespace PNConfigLib

#endif // GSDMLPARSER_H
