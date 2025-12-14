/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#ifndef INPUTVALIDATOR_H
#define INPUTVALIDATOR_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "../ConfigReader/ConfigReader.h"

namespace PNConfigLib {

class InputValidator {
public:
    // File existence checks
    static bool isConfigurationFileExist(const QString& configPath);
    static bool isListOfNodesFileExist(const QString& nodesPath);
    static bool isTopologyFileExist(const QString& topologyPath, bool& exists);
    
    // Content validation
    static bool validateConfiguration(const Configuration& config, QStringList& errors);
    static bool validateListOfNodes(const ListOfNodes& nodes, QStringList& errors);
    static bool validateReferences(const Configuration& config, const ListOfNodes& nodes, QStringList& errors);
    
    // PROFINET name validation
    static bool isValidPNDeviceName(const QString& name, QString& error);
    
    // IP address validation
    static bool isValidIPAddress(const QString& ip);
};

} // namespace PNConfigLib

#endif // INPUTVALIDATOR_H
