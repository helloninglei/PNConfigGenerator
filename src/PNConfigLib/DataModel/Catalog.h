/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef CATALOG_H
#define CATALOG_H

#include <QString>
#include <QHash>
#include <QList>
#include <memory>

namespace PNConfigLib {

// Forward declarations
class DecentralDeviceCatalog;
class ModuleCatalog;
class SubmoduleCatalog;
class InterfaceCatalog;
class PortCatalog;
class CentralDeviceCatalog;

/**
 * @brief Main catalog containing hardware types (devices, modules, submodules, ports)
 * 
 * This singleton class stores catalog objects imported from GSDML files.
 * It provides fast lookup and management of device catalog information.
 */
class Catalog
{
public:
    /**
     * @brief Get the singleton instance of the Catalog
     */
    static Catalog& instance();
    
    /**
     * @brief Clear all catalog contents
     */
    void reset();
    
    // Device catalog management
    void addDevice(const QString& key, std::shared_ptr<DecentralDeviceCatalog> device);
    std::shared_ptr<DecentralDeviceCatalog> getDevice(const QString& key) const;
    QList<QString> getDeviceKeys() const;
    
    // Module catalog management
    void addModule(const QString& key, std::shared_ptr<ModuleCatalog> module);
    std::shared_ptr<ModuleCatalog> getModule(const QString& key) const;
    QList<QString> getModuleKeys() const;
    
    // Submodule catalog management
    void addSubmodule(const QString& key, std::shared_ptr<SubmoduleCatalog> submodule);
    std::shared_ptr<SubmoduleCatalog> getSubmodule(const QString& key) const;
    QList<QString> getSubmoduleKeys() const;
    
    // Interface catalog management
    void addInterface(const QString& key, std::shared_ptr<InterfaceCatalog> interface);
    std::shared_ptr<InterfaceCatalog> getInterface(const QString& key) const;
    
    // Port catalog management
    void addPort(const QString& key, std::shared_ptr<PortCatalog> port);
    std::shared_ptr<PortCatalog> getPort(const QString& key) const;
    
    // Central device catalog
    std::shared_ptr<CentralDeviceCatalog> getCentralDeviceCatalog(
        const QString& interfaceType,
        const QString& customInterfacePath,
        const QString& version);
    
    // GSDML file tracking
    void addImportedGSDML(const QString& fileName);
    bool isGSDMLImported(const QString& fileName) const;
    QList<QString> getImportedGSDMLFiles() const;
    
private:
    Catalog() = default;
    ~Catalog() = default;
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;
    
    QHash<QString, std::shared_ptr<DecentralDeviceCatalog>> m_devices;
    QHash<QString, std::shared_ptr<ModuleCatalog>> m_modules;
    QHash<QString, std::shared_ptr<SubmoduleCatalog>> m_submodules;
    QHash<QString, std::shared_ptr<InterfaceCatalog>> m_interfaces;
    QHash<QString, std::shared_ptr<PortCatalog>> m_ports;
    QHash<QString, std::shared_ptr<CentralDeviceCatalog>> m_centralDevices;
    QList<QString> m_importedGSDMLFiles;
};

} // namespace PNConfigLib

#endif // CATALOG_H
