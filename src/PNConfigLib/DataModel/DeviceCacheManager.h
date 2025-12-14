#ifndef DEVICECACHEMANAGER_H
#define DEVICECACHEMANAGER_H

#include <QString>
#include <QList>
#include <QFileInfo>
#include "../GsdmlParser/GsdmlParser.h"

namespace PNConfigLib {

class DeviceCacheManager
{
public:
    static DeviceCacheManager& instance();

    bool initialize(const QString& cacheDir);
    QString getCacheDir() const;

    // Returns the path to the cached file
    QString importGSDML(const QString& sourcePath);
    
    // Returns list of GsdmlInfo for all cached files
    QList<GsdmlInfo> getCachedDevices();
    
    // Check if file is already in cache
    bool isCached(const QString& fileName);

private:
    DeviceCacheManager();
    ~DeviceCacheManager() = default;
    
    QString m_cacheDir;
};

} // namespace PNConfigLib

#endif // DEVICECACHEMANAGER_H
