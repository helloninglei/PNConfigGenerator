#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <cstdio>
#include "MainWindow.h"
#include "SimulationController.h"

// Message handler for logging to file
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    
    QFile outFile("pn_controller.log");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream ts(&outFile);
        QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        
        QString typeStr;
        switch (type) {
            case QtDebugMsg:    typeStr = "DEBUG"; break;
            case QtWarningMsg:  typeStr = "WARN "; break;
            case QtCriticalMsg: typeStr = "CRIT "; break;
            case QtFatalMsg:    typeStr = "FATAL"; break;
            case QtInfoMsg:     typeStr = "INFO "; break;
        }
        
        ts << timeStr << " [" << typeStr << "] " << msg << "\n";
        ts.flush();
    }
    
    // Also output to stderr so it's still visible in the terminal
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    // Install the message handler
    qInstallMessageHandler(messageHandler);

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

