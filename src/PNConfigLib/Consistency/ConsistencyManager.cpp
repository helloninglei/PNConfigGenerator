/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#include "ConsistencyManager.h"
#include "InputValidator.h"

namespace PNConfigLib {

ConsistencyManager::ConsistencyManager()
{
    reset();
}

void ConsistencyManager::reset()
{
    ConsistencyLogger::reset();
    m_topologyExists = false;
}

bool ConsistencyManager::validateInputPaths(const QString& configPath,
                                            const QString& listOfNodesPath,
                                            const QString& topologyPath)
{
    bool valid = true;
    
    // Check Configuration.xml
    if (!InputValidator::isConfigurationFileExist(configPath)) {
        valid = false;
    }
    
    // Check ListOfNodes.xml
    if (!InputValidator::isListOfNodesFileExist(listOfNodesPath)) {
        valid = false;
    }
    
    // Check Topology.xml (optional)
    if (!InputValidator::isTopologyFileExist(topologyPath, m_topologyExists)) {
        valid = false;
    }
    
    return valid;
}

bool ConsistencyManager::validateInputs(const Configuration& config,
                                        const ListOfNodes& nodes)
{
    bool valid = true;
    QStringList errors;
    
    // Validate Configuration content
    if (!InputValidator::validateConfiguration(config, errors)) {
        for (const QString& err : errors) {
            ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
                "Configuration", err);
        }
        valid = false;
    }
    
    errors.clear();
    
    // Validate ListOfNodes content
    if (!InputValidator::validateListOfNodes(nodes, errors)) {
        for (const QString& err : errors) {
            ConsistencyLogger::log(ConsistencyType::XML, LogSeverity::Error,
                "ListOfNodes", err);
        }
        valid = false;
    }
    
    errors.clear();
    
    // Validate cross-file references
    if (!InputValidator::validateReferences(config, nodes, errors)) {
        for (const QString& err : errors) {
            ConsistencyLogger::log(ConsistencyType::PN, LogSeverity::Error,
                "References", err);
        }
        valid = false;
    }
    
    return valid;
}

QList<ConsistencyLog> ConsistencyManager::getMessages() const
{
    return ConsistencyLogger::logs();
}

QString ConsistencyManager::getFormattedMessages() const
{
    return ConsistencyLogger::formatLogs();
}

bool ConsistencyManager::hasErrors() const
{
    return ConsistencyLogger::hasErrors();
}

} // namespace PNConfigLib
