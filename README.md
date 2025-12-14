# PNConfigGenerator - PROFINET Device Configuration Tool

A cross-platform QT6/C++17 application for configuring PROFINET devices, similar to EnTalk and TIA Portal configuration functions.

## Features

- **Quick Setup Wizard**: Fast single-device configuration from GSDML files
- **Advanced Tree View**: Multi-device project management
- **Device Catalog**: Browse and import GSDML device definitions
- **Configuration Editor**: Edit device parameters, IP addresses, and IO mappings
- **Automatic IO Allocation**: Smart IO address assignment based on module structure
- **XML Generation**: Create Configuration.xml and ListOfNodes.xml files

## Architecture

The project consists of two main components:

### 1. PNConfigLib (Core Library)
- **GsdmlParser**: Parse GSDML XML files to extract device information
- **ConfigGenerator**: Generate Configuration.xml and ListOfNodes.xml
- **ConfigReader**: Read existing configuration files
- **DataModel**: Catalog and device/module/submodule data structures
- **Compiler**: Generate final PROFINET driver configuration XML
- **ProjectManager**: High-level API orchestrating the workflow

### 2. PNConfigGenerator (GUI Application)
- **Main Window**: Menu bar, toolbar, dock widgets
- **Quick Setup Wizard**: 5-page wizard for fast configuration
- **Catalog Browser**: Browse imported GSD devices
- **Project Tree**: Hierarchical view of configured devices
- **Configuration Editors**: Edit device/module parameters

## Building

### Prerequisites

- **CMake** 3.16 or higher
- **Qt6** (Core, Widgets, Xml modules)
- **C++17** compatible compiler
  - Windows: Visual Studio 2019 or later / MinGW
  - Linux: GCC 7+ or Clang 5+

### Build Instructions

#### Windows

```powershell
# Configure
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"

# Build
cmake --build . --config Release

# Run
.\bin\Release\PNConfigGenerator.exe
```

#### Linux

```bash
# Configure
mkdir build
cd build
cmake ..

# Build
make -j$(nproc)

# Run
./bin/PNConfigGenerator
```

## Usage

### Quick Setup Wizard

1. Launch **PNConfigGenerator**
2. Select **Tools > Quick Setup Wizard**
3. Follow the wizard steps:
   - Select GSDML file for your IO device
   - Configure master device (name, IP address)
   - Configure slave device (name, IP address)
   - Set IO start addresses
   - Review and generate configuration files

Output: `Configuration.xml` and `ListOfNodes.xml` ready for use with PNConfigLib.

### Advanced Mode

1. **Import GSD Files**: File > Import GSD File
2. **Create Project**: File > New Project
3. **Add Devices**: Drag devices from catalog to project tree
4. **Configure**: Edit parameters in configuration editor
5. **Generate**: Tools > Generate Configuration

## Project Structure

```
PNConfigGenerator/
├── CMakeLists.txt           # Root CMake configuration
├── .gitignore               # Git ignore rules
├── README.md                # This file
├── example/                 # C# reference implementation
│   ├── PNConfigLib/         # Core library reference
│   ├── PNConfigLibRunner/   # CLI reference
│   └── PNconfigFileGenerator/ # Configuration generator reference
└── src/
    ├── PNConfigLib/         # Core library (C++)
    │   ├── GsdmlParser/     # GSDML XML parsing
    │   ├── ConfigGenerator/ # Configuration file generation
    │   ├── ConfigReader/    # XML configuration reading
    │   ├── DataModel/       # Data structures and catalog
    │   ├── Compiler/        # Output XML generation
    │   └── ProjectManager/  # High-level API
    └── PNConfigGenerator/   # GUI application (Qt)
        ├── Wizards/         # Setup wizards
        ├── Widgets/         # Custom widgets
        ├── MainWindow.h/cpp # Main application window
        └── main.cpp         # Entry point
```

## Development Status

### Completed
- [x] Project structure and CMake build system
- [x] Data model (Catalog class)
- [x] Main window skeleton
- [x] Quick Setup Wizard UI (5 pages)

### In Progress
- [ ] GSDML Parser implementation
- [ ] Configuration Generator
- [ ] Catalog browser widget
- [ ] Project tree widget
- [ ] Device configuration editor

### Planned
- [ ] Configuration Reader
- [ ] Compiler and XML output
- [ ] Advanced topology editor
- [ ] Consistency checking
- [ ] Unit tests
- [ ] Documentation

## Reference

This project is based on the C# PNConfigLib implementation (version 1.05), which provides:
- PNConfigLib: Core configuration library
- PNConfigLibRunner: Command-line runner
- PNconfigFileGenerator: Configuration file generator

## License

[Specify your license here]

## Contact

[Your contact information]
