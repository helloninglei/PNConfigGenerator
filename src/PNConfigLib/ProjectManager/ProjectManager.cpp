/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "ProjectManager.h"
#include "../ConfigReader/ConfigReader.h"
#include "../Compiler/Compiler.h"
#include <QFileInfo>

namespace PNConfigLib {

QString ProjectManager::s_lastError;

bool ProjectManager::runPNConfigLib(
    const QString& configPath,
    const QString& listOfNodesPath,
    const QString& outputPath)
{
    s_lastError.clear();
    
    try {
        // Validate input files exist
        if (!QFileInfo::exists(configPath)) {
            s_lastError = QString("Configuration file not found: %1").arg(configPath);
            return false;
        }
        
        if (!QFileInfo::exists(listOfNodesPath)) {
            s_lastError = QString("ListOfNodes file not found: %1").arg(listOfNodesPath);
            return false;
        }
        
        // Parse Configuration.xml
        Configuration config = ConfigReader::parseConfiguration(configPath);
        
        // Parse ListOfNodes.xml
        ListOfNodes nodes = ConfigReader::parseListOfNodes(listOfNodesPath);
        
        // Validate consistency
        if (config.listOfNodesRefID != nodes.listOfNodesID) {
            s_lastError = QString("Configuration references ListOfNodesID '%1' but actual ID is '%2'")
                .arg(config.listOfNodesRefID).arg(nodes.listOfNodesID);
            return false;
        }
        
        // Compile to output
        bool success = Compiler::compile(config, nodes, outputPath);
        
        if (!success) {
            s_lastError = "Failed to write output file";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        s_lastError = QString("Error: %1").arg(e.what());
        return false;
    } catch (...) {
        s_lastError = "Unknown error occurred";
        return false;
    }
}

QString ProjectManager::getLastError()
{
    return s_lastError;
}

} // namespace PNConfigLib
