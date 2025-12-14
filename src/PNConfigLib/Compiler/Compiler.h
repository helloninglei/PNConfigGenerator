/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef COMPILER_H
#define COMPILER_H

#include "../ConfigReader/ConfigurationSchema.h"
#include "../GsdmlParser/GsdmlParser.h"
#include <QString>
#include <QHash>

namespace PNConfigLib {

/**
 * @brief PROFINET configuration compiler
 * 
 * Compiles Configuration + ListOfNodes + GSDML data into final output format
 */
class Compiler {
public:
    /**
     * @brief Compile configuration to output XML
     * @param config Configuration structure
     * @param nodes ListOfNodes structure  
     * @param outputPath Output file path
     * @return true if successful
     */
    static bool compile(
        const Configuration& config,
        const ListOfNodes& nodes,
        const QString& outputPath);
    
    /**
     * @brief Generate output XML from configuration
     * @param config Configuration structure
     * @param nodes ListOfNodes structure
     * @return Generated XML string
     */
    static QString generateOutputXml(
        const Configuration& config,
        const ListOfNodes& nodes);
        
private:
    static QHash<QString, GsdmlInfo> loadGsdmlData(const ListOfNodes& nodes);
    static QString generateDeviceSection(
        const DecentralDeviceType& device,
        const QHash<QString, GsdmlInfo>& gsdmlData);
};

} // namespace PNConfigLib

#endif // COMPILER_H
