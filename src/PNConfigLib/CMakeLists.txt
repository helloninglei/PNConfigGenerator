# PNConfigLib - Core Configuration Library

add_library(PNConfigLib STATIC
    # Data Model
    DataModel/Catalog.h
    DataModel/Catalog.cpp
    DataModel/PCLObject.h
    DataModel/DeviceTypes.h
    DataModel/ModuleTypes.h
    
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

    # TinyXML2
    tinyxml2/tinyxml2.h
    tinyxml2/tinyxml2.cpp
)

target_link_libraries(PNConfigLib
    PUBLIC
        Qt6::Core
)

target_include_directories(PNConfigLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
