/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef LISTOFNODESBUILDER_H
#define LISTOFNODESBUILDER_H

#include <QString>

namespace PNConfigLib {

/**
 * @brief Builds ListOfNodes.xml from GSDML and parameters
 */
class ListOfNodesBuilder {
public:
    struct DeviceNode {
        QString name;
        QString ipAddress;
        QString routerIpAddress;
    };
    
    /**
     * @brief Generate ListOfNodes XML string
     * @param gsdmlPath Path to GSDML file
     * @param masterNode Master device node
     * @param slaveNode Slave device node
     * @return XML string
     */
    static QString generateListOfNodesXml(
        const QString& gsdmlPath,
        const DeviceNode& masterNode,
        const DeviceNode& slaveNode);
    
    /**
     * @brief Save ListOfNodes XML to file
     * @param gsdmlPath Path to GSDML file
     * @param masterNode Master device node
     * @param slaveNode Slave device node
     * @param outputPath Output file path
     * @return true if successful
     */
    static bool saveListOfNodesXml(
        const QString& gsdmlPath,
        const DeviceNode& masterNode,
        const DeviceNode& slaveNode,
        const QString& outputPath);
};

} // namespace PNConfigLib

#endif // LISTOFNODESBUILDER_H
