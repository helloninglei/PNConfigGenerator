# PNConfigLib - Core Configuration Library

add_library(PNConfigLib STATIC
    # Data Model
    DataModel/Catalog.h
    DataModel/Catalog.cpp
    DataModel/PCLObject.h
    DataModel/DeviceTypes.h
    DataModel/ModuleTypes.h
    DataModel/DeviceCacheManager.h
    DataModel/DeviceCacheManager.cpp
    
    # GSDML Parser
    GsdmlParser/GsdmlParser.h
    GsdmlParser/GsdmlParser.cpp
    
    # Config Reader
    ConfigReader/ConfigurationSchema.h
    ConfigReader/ConfigReader.h
    ConfigReader/ConfigReader.cpp
    
    # Configuration Generator
    ConfigGenerator/ConfigurationBuilder.h
    ConfigGenerator/ConfigurationBuilder.cpp
    ConfigGenerator/ListOfNodesBuilder.h
    ConfigGenerator/ListOfNodesBuilder.cpp
    
    # Compiler (stub)
    Compiler/Compiler.h
    Compiler/Compiler.cpp
    Compiler/XmlSerializer.h
    Compiler/XmlSerializer.cpp
    Compiler/BlobBuilder.h
    Compiler/BlobBuilder.cpp
    Compiler/XmlEntities.h
    Compiler/XmlEntities.cpp
    Compiler/RecordGenerators.h
    Compiler/RecordGenerators.cpp
    Compiler/CompilerConstants.h
    
    # Project Manager (stub)
    ProjectManager/ProjectManager.h
    ProjectManager/ProjectManager.cpp
    
    # Consistency Validation
    Consistency/ConsistencyLogger.h
    Consistency/ConsistencyLogger.cpp
    Consistency/InputValidator.h
    Consistency/InputValidator.cpp
    Consistency/ConsistencyManager.h
    Consistency/ConsistencyManager.cpp

    # Network
    Network/DcpScanner.h
    Network/DcpScanner.cpp

    # TinyXML2
    tinyxml2/tinyxml2.h
    tinyxml2/tinyxml2.cpp
)

# Npcap/pcap paths
set(NPCAP_DIR ${CMAKE_SOURCE_DIR}/example/pn_driver/src/driver/Npcap)
set(NPCAP_INCLUDE_DIR ${NPCAP_DIR}/Include)
set(NPCAP_LIB_DIR ${NPCAP_DIR}/Lib/x64)

target_include_directories(PNConfigLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NPCAP_INCLUDE_DIR}
)

target_link_directories(PNConfigLib
    PUBLIC
        ${NPCAP_LIB_DIR}
)

target_link_libraries(PNConfigLib
    PUBLIC
        Qt6::Core
        wpcap
        Packet
)

target_compile_definitions(PNConfigLib
    PUBLIC
        WPCAP
        HAVE_REMOTE
)
