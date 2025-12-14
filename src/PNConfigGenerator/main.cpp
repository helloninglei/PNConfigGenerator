/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application information
    QApplication::setApplicationName("PNConfigGenerator");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("PROFINET Tools");
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
