#ifndef COMPILERCONSTANTS_H
#define COMPILERCONSTANTS_H

namespace PNConfigLib {
namespace CompilerConstants {

// Class RIDs
const uint32_t ClassRID_HWConfiguration = 1;
const uint32_t ClassRID_Windows_PNDriver = 2;
const uint32_t ClassRID_PN_Driver_Windows_Interface = 3;
const uint32_t ClassRID_Port = 4; // Used for driver ports
const uint32_t ClassRID_IOSystem = 5;
const uint32_t ClassRID_DeactivatedDevice = 6;
const uint32_t ClassRID_Device = 7;
const uint32_t ClassRID_Device_Interface = 9; // Often 9 for interface and ports in device
const uint32_t ClassRID_Module = 8; // Sometimes 8 or variable
const uint32_t ClassRID_NetworkParameters = 11;

// Attribute IDs (AIDs)
const uint32_t AID_Key = 2; // Key identifier
const uint32_t AID_DeactivatedConfig = 4;
const uint32_t AID_LADDR = 10;
const uint32_t AID_DataRecordsConf = 11;
const uint32_t AID_NetworkParamConfig = 12; // For Network Parameters object
const uint32_t AID_IODevParamConfig = 13;
const uint32_t AID_DataRecordsTransferSequence = 14;
const uint32_t AID_IOsysParamConfig = 15;
const uint32_t AID_Link = 16;
const uint32_t AID_DataRecordsConf_Driver = 17; // For driver itself

// Record Keys (Big Endian)
const uint32_t Key_IpV4Suite = 4096; // 0x1000
const uint32_t Key_Unknown4097 = 4097; // 0x1001
const uint32_t Key_NameOfStation = 4099; // 0x1003
const uint32_t Key_Unknown4100 = 4100; // 0x1004
const uint32_t Key_Unknown4101 = 4101; // 0x1005
const uint32_t Key_Unknown4103 = 4103; // 0x1007

const uint32_t Key_IOParam_12352 = 12352; // 0x3040
const uint32_t Key_IOParam_12384 = 12384; // 0x3060
const uint32_t Key_IOParam_12544 = 12544; // 0x3100
const uint32_t Key_IOParam_12545 = 12545; // 0x3101
const uint32_t Key_IOParam_12546 = 12546; // 0x3102
const uint32_t Key_IOParam_12548 = 12548; // 0x3104
const uint32_t Key_IOParam_12551 = 12551; // 0x3107

const uint32_t Key_SnmpControl = 32881; // 0x8071
const uint32_t Key_Unknown45169 = 45169; // 0xB071
const uint32_t Key_ExpectedConfig = 65536; // 0x10000

// Fixed Values
const uint32_t RID_IOSystem = 2296447237;

} // namespace CompilerConstants
} // namespace PNConfigLib

#endif // COMPILERCONSTANTS_H
