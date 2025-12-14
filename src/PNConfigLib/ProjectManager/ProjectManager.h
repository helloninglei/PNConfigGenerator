/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QString>

namespace PNConfigLib {

/**
 * @brief Project manager for orchestrating configuration workflow
 */
class ProjectManager {
public:
    /**
     * @brief Run complete PNConfigLib workflow
     * @param configPath Path to Configuration.xml
     * @param listOfNodesPath Path to ListOfNodes.xml
     * @param outputPath Output file path for compiled configuration
     * @return true if successful
     */
    static bool runPNConfigLib(
        const QString& configPath,
        const QString& listOfNodesPath,
        const QString& outputPath);
        
    /**
     * @brief Get last error message
     * @return Error description
     */
    static QString getLastError();
    
private:
    static QString s_lastError;
};

} // namespace PNConfigLib

#endif // PROJECTMANAGER_H
