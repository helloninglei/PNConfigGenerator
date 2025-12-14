/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include "ConfigurationSchema.h"
#include <QString>

namespace tinyxml2 {
class XMLElement;
}

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
    static void parseCentralDevice(tinyxml2::XMLElement* element, CentralDeviceType& device);
    static void parseDecentralDevice(tinyxml2::XMLElement* element, DecentralDeviceType& device);
    static void parseEthernetAddresses(tinyxml2::XMLElement* element, EthernetAddresses& addresses);
    static void parseModule(tinyxml2::XMLElement* element, ModuleType& module);
    static void parseSubmodule(tinyxml2::XMLElement* element, SubmoduleType& submodule);
    static void parseIOAddresses(tinyxml2::XMLElement* element, IOAddresses& addresses);
    static void parsePNDriver(tinyxml2::XMLElement* element, PNDriverType& driver);
    static void parseDecentralDeviceNode(tinyxml2::XMLElement* element, DecentralDeviceNode& device);
    static void parseInterface(tinyxml2::XMLElement* element, InterfaceType& interface);
};

} // namespace PNConfigLib

#endif // CONFIGREADER_H
