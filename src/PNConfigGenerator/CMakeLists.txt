# PNConfigGenerator - GUI Application

set(SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    
    # Wizards
    Wizards/QuickSetupWizard.cpp
    Wizards/QuickSetupWizard.h
    
    # Widgets
    Widgets/CatalogBrowser.cpp
    Widgets/CatalogBrowser.h
    Widgets/ProjectTree.cpp
    Widgets/ProjectTree.h
    Widgets/DeviceConfigEditor.cpp
    Widgets/DeviceConfigEditor.h
    Widgets/TopologyEditor.cpp
    Widgets/TopologyEditor.h
    Widgets/MasterSimulationWidget.cpp
    Widgets/MasterSimulationWidget.h
)

add_executable(PNConfigGenerator ${SOURCES})

target_link_libraries(PNConfigGenerator
    PRIVATE
        PNConfigLib
        Qt6::Core
        Qt6::Widgets
        Qt6::Xml
        Qt6::Network
)

target_include_directories(PNConfigGenerator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
