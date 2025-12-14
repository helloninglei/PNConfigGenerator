#include "DeviceCacheManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace PNConfigLib {

DeviceCacheManager& DeviceCacheManager::instance()
{
    static DeviceCacheManager instance;
    return instance;
}

DeviceCacheManager::DeviceCacheManager()
{
    // Default cache location
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataLocation.isEmpty()) {
        dataLocation = QDir::currentPath() + "/DeviceCache";
    } else {
        dataLocation += "/DeviceCache";
    }
    initialize(dataLocation);
}

bool DeviceCacheManager::initialize(const QString& cacheDir)
{
    m_cacheDir = cacheDir;
    QDir dir(m_cacheDir);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

QString DeviceCacheManager::getCacheDir() const
{
    return m_cacheDir;
}

QString DeviceCacheManager::importGSDML(const QString& sourcePath)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return QString();
    }

    QString destPath = m_cacheDir + "/" + sourceInfo.fileName();
    
    // If already exists, overwrite? Or skip?
    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    
    if (QFile::copy(sourcePath, destPath)) {
        // Also copy associated image files if they exist? 
        // GSDML usually refers to bitmaps. 
        // For simple requirements, just copy the XML.
        return destPath;
    }
    
    return QString();
}

QList<GsdmlInfo> DeviceCacheManager::getCachedDevices()
{
    QList<GsdmlInfo> devices;
    QDir dir(m_cacheDir);
    QStringList filters;
    filters << "*.xml";
    dir.setNameFilters(filters);
    
    QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo& fileInfo : list) {
        // Simple check if it's a GSDML file by name pattern
        if (fileInfo.fileName().toUpper().contains("GSDML")) {
            try {
                GsdmlInfo info = GsdmlParser::parseGSDML(fileInfo.absoluteFilePath());
                devices.append(info);
            } catch (...) {
                qDebug() << "Failed to parse cached file:" << fileInfo.fileName();
            }
        }
    }
    
    return devices;
}

bool DeviceCacheManager::isCached(const QString& fileName)
{
    QDir dir(m_cacheDir);
    return dir.exists(fileName);
}

} // namespace PNConfigLib
