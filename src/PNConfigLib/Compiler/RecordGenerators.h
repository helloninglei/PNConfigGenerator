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

    // Generate DataRecordsConf for Windows_PNDriver (Records 4101, 45169)
    static QList<XmlField> generateWindowsPNDriverRecords();

    // Generate NetworkParamConfig for Network Parameters Object
    static QList<XmlField> generateNetworkParameters(
        const QString& ip, 
        const QString& mask, 
        const QString& name);
        
    // Generate IODevParamConfig records (all required PROFINET device parameters)
    static QList<XmlField> generateIODevParamConfig(
        const DecentralDeviceType& device, 
        const GsdmlInfo& gsdInfo,
        const QString& deviceName,
        const QString& ipAddress);

    // Helpers
    static XmlField generateIpSuite(const QString& ip, const QString& mask, const QString& gateway);
    static XmlField generateNameOfStation(const QString& name);
    static XmlField generateSnmpControl();
    static XmlField generateExpectedConfig(const DecentralDeviceType& device);
};

} // namespace PNConfigLib

#endif // RECORDGENERATORS_H
