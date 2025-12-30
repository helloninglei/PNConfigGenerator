#include <QApplication>
#include <QCommandLineParser>
#include "MainWindow.h"
#include "SimulationController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application information
    QApplication::setApplicationName("PNConfigGenerator");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("PROFINET Tools");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("PROFINET Device Configuration Tool");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption simulateOption({"s", "simulate"}, "Run in headless simulation mode");
    parser.addOption(simulateOption);
    
    parser.process(app);
    
    if (parser.isSet(simulateOption)) {
        SimulationController controller;
        controller.start();
        return app.exec();
    }
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
