/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include "ConfigurationSchema.h"
#include <QString>
#include <QDomElement>

namespace PNConfigLib {

/**
 * @brief Configuration file reader
 * 
 * Parses Configuration.xml and ListOfNodes.xml files into C++ data structures
 */
class ConfigReader {
public:
    /**
     * @brief Parse Configuration.xml file
     * @param configPath Path to Configuration.xml
     * @return Parsed configuration structure
     * @throws std::runtime_error on parse failure
     */
    static Configuration parseConfiguration(const QString& configPath);
    
    /**
     * @brief Parse ListOfNodes.xml file
     * @param nodesPath Path to ListOfNodes.xml
     * @return Parsed ListOfNodes structure
     * @throws std::runtime_error on parse failure
     */
    static ListOfNodes parseListOfNodes(const QString& nodesPath);
    
private:
    static void parseCentralDevice(QDomElement& element, CentralDeviceType& device);
    static void parseDecentralDevice(QDomElement& element, DecentralDeviceType& device);
    static void parseEthernetAddresses(QDomElement& element, EthernetAddresses& addresses);
    static void parseModule(QDomElement& element, ModuleType& module);
    static void parseSubmodule(QDomElement& element, SubmoduleType& submodule);
    static void parseIOAddresses(QDomElement& element, IOAddresses& addresses);
    static void parsePNDriver(QDomElement& element, PNDriverType& driver);
    static void parseDecentralDeviceNode(QDomElement& element, DecentralDeviceNode& device);
    static void parseInterface(QDomElement& element, InterfaceType& interface);
};

} // namespace PNConfigLib

#endif // CONFIGREADER_H
