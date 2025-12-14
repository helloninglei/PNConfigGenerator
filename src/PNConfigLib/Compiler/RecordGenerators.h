#ifndef RECORDGENERATORS_H
#define RECORDGENERATORS_H

#include "XmlEntities.h"
#include "../ConfigReader/ConfigurationSchema.h"
#include "../GsdmlParser/GsdmlParser.h"

namespace PNConfigLib {

class RecordGenerators {
public:
    // Generate DataRecordsConf for Driver/Interface
    static QList<XmlField> generateDriverInterfaceRecords(
        const QString& ip, 
        const QString& mask, 
        const QString& name,
        const QString& gateway = QString());

    // Generate NetworkParamConfig for Network Parameters Object
    static QList<XmlField> generateNetworkParameters(
        const QString& ip, 
        const QString& mask, 
        const QString& name);
        
    // Generate IODevParamConfig (Complex!)
    static QList<XmlField> generateIODevParamConfig(const DecentralDeviceType& device, const GsdmlInfo& gsdInfo);

    // Helpers
    static XmlField generateIpSuite(const QString& ip, const QString& mask, const QString& gateway);
    static XmlField generateNameOfStation(const QString& name);
    static XmlField generateSnmpControl();
    static XmlField generateExpectedConfig(const DecentralDeviceType& device);
};

} // namespace PNConfigLib

#endif // RECORDGENERATORS_H
