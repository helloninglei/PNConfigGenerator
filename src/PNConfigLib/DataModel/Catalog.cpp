/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "Catalog.h"

namespace PNConfigLib {

Catalog& Catalog::instance()
{
    static Catalog instance;
    return instance;
}

void Catalog::reset()
{
    m_devices.clear();
    m_modules.clear();
    m_submodules.clear();
    m_interfaces.clear();
    m_ports.clear();
    m_centralDevices.clear();
    m_importedGSDMLFiles.clear();
}

// Device catalog management
void Catalog::addDevice(const QString& key, std::shared_ptr<DecentralDeviceCatalog> device)
{
    m_devices[key] = device;
}

std::shared_ptr<DecentralDeviceCatalog> Catalog::getDevice(const QString& key) const
{
    return m_devices.value(key, nullptr);
}

QList<QString> Catalog::getDeviceKeys() const
{
    return m_devices.keys();
}

// Module catalog management
void Catalog::addModule(const QString& key, std::shared_ptr<ModuleCatalog> module)
{
    m_modules[key] = module;
}

std::shared_ptr<ModuleCatalog> Catalog::getModule(const QString& key) const
{
    return m_modules.value(key, nullptr);
}

QList<QString> Catalog::getModuleKeys() const
{
    return m_modules.keys();
}

// Submodule catalog management
void Catalog::addSubmodule(const QString& key, std::shared_ptr<SubmoduleCatalog> submodule)
{
    m_submodules[key] = submodule;
}

std::shared_ptr<SubmoduleCatalog> Catalog::getSubmodule(const QString& key) const
{
    return m_submodules.value(key, nullptr);
}

QList<QString> Catalog::getSubmoduleKeys() const
{
    return m_submodules.keys();
}

// Interface catalog management
void Catalog::addInterface(const QString& key, std::shared_ptr<InterfaceCatalog> interface)
{
    m_interfaces[key] = interface;
}

std::shared_ptr<InterfaceCatalog> Catalog::getInterface(const QString& key) const
{
    return m_interfaces.value(key, nullptr);
}

// Port catalog management
void Catalog::addPort(const QString& key, std::shared_ptr<PortCatalog> port)
{
    m_ports[key] = port;
}

std::shared_ptr<PortCatalog> Catalog::getPort(const QString& key) const
{
    return m_ports.value(key, nullptr);
}

// Central device catalog
std::shared_ptr<CentralDeviceCatalog> Catalog::getCentralDeviceCatalog(
    const QString& interfaceType,
    const QString& customInterfacePath,
    const QString& version)
{
    QString catalogKey = QString("%1_%2_%3").arg(interfaceType, version, customInterfacePath);
    
    if (!m_centralDevices.contains(catalogKey)) {
        // Create central device catalog on demand
        // This would be implemented based on the C# CentralDeviceCatalogObjectReader
        // For now, return nullptr
        return nullptr;
    }
    
    return m_centralDevices[catalogKey];
}

// GSDML file tracking
void Catalog::addImportedGSDML(const QString& fileName)
{
    QString upperFileName = fileName.toUpper();
    if (!m_importedGSDMLFiles.contains(upperFileName)) {
        m_importedGSDMLFiles.append(upperFileName);
    }
}

bool Catalog::isGSDMLImported(const QString& fileName) const
{
    return m_importedGSDMLFiles.contains(fileName.toUpper());
}

QList<QString> Catalog::getImportedGSDMLFiles() const
{
    return m_importedGSDMLFiles;
}

} // namespace PNConfigLib
