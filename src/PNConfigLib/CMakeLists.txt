# PNConfigLib - Core Configuration Library

add_library(PNConfigLib STATIC
    # Data Model
    DataModel/Catalog.cpp
    DataModel/Catalog.h
    DataModel/PCLObject.h
    DataModel/DeviceTypes.h
    DataModel/ModuleTypes.h
    
    # GSDML Parser
    GsdmlParser/GsdmlParser.cpp
    GsdmlParser/GsdmlParser.h
    
    # Configuration Generator
    ConfigGenerator/ConfigurationBuilder.cpp
    ConfigGenerator/ConfigurationBuilder.h
    ConfigGenerator/ListOfNodesBuilder.cpp
    ConfigGenerator/ListOfNodesBuilder.h
    
    # Configuration Reader
    ConfigReader/ConfigReader.cpp
    ConfigReader/ConfigReader.h
    ConfigReader/ConfigurationSchema.h
    
    # Compiler
    Compiler/Compiler.cpp
    Compiler/Compiler.h
    Compiler/XmlSerializer.cpp
    Compiler/XmlSerializer.h
    
    # Project Manager
    ProjectManager/ProjectManager.cpp
    ProjectManager/ProjectManager.h
)

target_link_libraries(PNConfigLib
    PUBLIC
        Qt6::Core
        Qt6::Xml
)

target_include_directories(PNConfigLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
