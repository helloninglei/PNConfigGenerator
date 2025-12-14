/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#ifndef CONSISTENCYMANAGER_H
#define CONSISTENCYMANAGER_H

#include <QString>
#include <QStringList>
#include "ConsistencyLogger.h"
#include "../ConfigReader/ConfigReader.h"

namespace PNConfigLib {

class ConsistencyManager {
public:
    ConsistencyManager();
    
    // Validate file paths exist
    bool validateInputPaths(const QString& configPath, 
                           const QString& listOfNodesPath,
                           const QString& topologyPath = QString());
    
    // Validate XML content (structural checks without XSD)
    bool validateInputs(const Configuration& config, 
                       const ListOfNodes& nodes);
    
    // Get validation messages
    QList<ConsistencyLog> getMessages() const;
    QString getFormattedMessages() const;
    bool hasErrors() const;
    
    // Reset logger
    void reset();
    
private:
    bool m_topologyExists = false;
};

} // namespace PNConfigLib

#endif // CONSISTENCYMANAGER_H
