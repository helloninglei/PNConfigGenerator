#include <PNConfigLib/Compiler/Compiler.h>
#include <PNConfigLib/ConfigReader/ConfigReader.h>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Hardcoded paths for verification in this specific environment
    QString configPath = "f:/workspaces/PNConfigGenerator/example/config_files/01_Basic_Configuration/Configuration.xml";
    QString listPath = "f:/workspaces/PNConfigGenerator/example/config_files/01_Basic_Configuration/ListOfNodes.xml";
    QString outPath = "f:/workspaces/PNConfigGenerator/verifier_output.xml";
    
    // Check files
    if (!QFileInfo::exists(configPath)) {
        qDebug() << "Config not found at" << configPath;
        return 1;
    }
    
    qDebug() << "Loading config:" << configPath;
    try {
        auto config = PNConfigLib::ConfigReader::parseConfiguration(configPath);
        auto nodes = PNConfigLib::ConfigReader::parseListOfNodes(listPath);
        
        qDebug() << "Generating output to:" << outPath;
        bool res = PNConfigLib::Compiler::compile(config, nodes, outPath);
        
        qDebug() << "Result:" << res;
        return res ? 0 : 1;
    } catch (const std::exception& e) {
        qDebug() << "Exception:" << e.what();
        return 1;
    }
}
