/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Wizards/QuickSetupWizard.h"
#include "../PNConfigLib/ProjectManager/ProjectManager.h"
#include "../PNConfigLib/ConfigReader/ConfigReader.h"
#include "../PNConfigLib/ConfigGenerator/ConfigurationBuilder.h"
#include "../PNConfigLib/ConfigGenerator/ListOfNodesBuilder.h"
#include "../PNConfigLib/Compiler/Compiler.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUi();
    setupMenus();
    setupToolbar();
    setupDockWidgets();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUi()
{
    setWindowTitle("PNConfigGenerator - PROFINET Device Configuration Tool");
    resize(900, 700);
    
    // Set default output path to project root/results with timestamp subdirectory
    QString appDir = QCoreApplication::applicationDirPath();
    // Navigate from bin folder to project root
    QDir projectDir(appDir);
    projectDir.cdUp(); // Go up from bin to build or project root
    if (projectDir.dirName() == "bin") {
        projectDir.cdUp();
    }
    if (projectDir.dirName() == "build") {
        projectDir.cdUp();
    }
    
    // Create results directory with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString defaultOutput = projectDir.absolutePath() + "/results/" + timestamp;
    ui->outputPathEdit->setText(defaultOutput);
    
    // Status bar
    statusBar()->showMessage("Ready - Configure devices and click 'Generate Configuration'");
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    
    // Open Project Folder
    QAction *openConfigAction = fileMenu->addAction(tr("Open Project &Folder..."), this, &MainWindow::onOpenProjectFolder);
    openConfigAction->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    
    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Quick Setup Wizard..."), this, &MainWindow::onQuickSetupWizard);
    
    toolsMenu->addSeparator();
    
    // Generate Output (enabled when configuration is loaded)
    QAction *generateAction = toolsMenu->addAction(tr("Generate &Output..."), this, &MainWindow::onGenerateOutput);
    generateAction->setObjectName("generateOutputAction");
    generateAction->setShortcut(QKeySequence(tr("Ctrl+G")));
    generateAction->setEnabled(false);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);
}

void MainWindow::setupToolbar()
{
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->addAction(tr("Open"), this, &MainWindow::onOpenProjectFolder);
    toolbar->addSeparator();
    toolbar->addAction(tr("Quick Setup"), this, &MainWindow::onQuickSetupWizard);
}

void MainWindow::setupDockWidgets()
{
    // Reserved for future dock widgets
}

void MainWindow::setupConnections()
{
    // Connect UI buttons to slots
    connect(ui->browseGsdmlButton, &QPushButton::clicked, this, &MainWindow::onBrowseGsdml);
    connect(ui->browseOutputButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(ui->generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateConfiguration);
}

void MainWindow::onBrowseGsdml()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Select GSDML File"),
        "",
        tr("GSDML Files (*.xml);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        ui->gsdmlPathEdit->setText(fileName);
        statusBar()->showMessage(tr("GSDML file selected: %1").arg(QFileInfo(fileName).fileName()), 3000);
    }
}

void MainWindow::onBrowseOutput()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        ui->outputPathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        ui->outputPathEdit->setText(dir);
    }
}

void MainWindow::onGenerateConfiguration()
{
    // Validate inputs
    QString masterName = ui->masterNameEdit->text().trimmed();
    QString masterIP = ui->masterIPEdit->text().trimmed();
    QString slaveName = ui->slaveNameEdit->text().trimmed();
    QString slaveIP = ui->slaveIPEdit->text().trimmed();
    QString gsdmlPath = ui->gsdmlPathEdit->text().trimmed();
    QString outputPath = ui->outputPathEdit->text().trimmed();
    
    if (masterName.isEmpty() || masterIP.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Please enter Master Station name and IP address."));
        return;
    }
    
    if (slaveName.isEmpty() || slaveIP.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Please enter Slave Station name and IP address."));
        return;
    }
    
    if (gsdmlPath.isEmpty() || !QFile::exists(gsdmlPath)) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Please select a valid GSDML file."));
        return;
    }
    
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Please select an output directory."));
        return;
    }
    
    // Create output directory if it doesn't exist
    QDir dir;
    if (!dir.exists(outputPath)) {
        if (!dir.mkpath(outputPath)) {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to create output directory: %1").arg(outputPath));
            return;
        }
    }
    
    statusBar()->showMessage(tr("Generating configuration files..."));
    
    // Prepare configuration paths
    QString configPath = QDir(outputPath).filePath("Configuration.xml");
    QString nodesPath = QDir(outputPath).filePath("ListOfNodes.xml");
    QString finalOutputPath = QDir(outputPath).filePath("PROFINET_Driver_Output.xml");
    
    try {
        // Step 1: Generate Configuration.xml
        PNConfigLib::ConfigurationBuilder::DeviceConfig masterConfig;
        masterConfig.name = masterName;
        masterConfig.ipAddress = masterIP;
        masterConfig.routerIpAddress = ui->masterRouterEdit->text().trimmed();
        masterConfig.inputStartAddress = 0;
        masterConfig.outputStartAddress = 0;
        
        PNConfigLib::ConfigurationBuilder::DeviceConfig slaveConfig;
        slaveConfig.name = slaveName;
        slaveConfig.ipAddress = slaveIP;
        slaveConfig.routerIpAddress = ui->slaveRouterEdit->text().trimmed();
        slaveConfig.inputStartAddress = ui->inputStartEdit->text().trimmed().toInt();
        slaveConfig.outputStartAddress = ui->outputStartEdit->text().trimmed().toInt();
        
        bool success = PNConfigLib::ConfigurationBuilder::saveConfigurationXml(
            gsdmlPath, masterConfig, slaveConfig, configPath);
        
        if (!success) {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to generate Configuration.xml"));
            statusBar()->showMessage(tr("Generation failed"), 5000);
            return;
        }
        
        // Step 2: Generate ListOfNodes.xml
        PNConfigLib::ListOfNodesBuilder::DeviceNode masterNode;
        masterNode.name = masterName;
        masterNode.ipAddress = masterIP;
        masterNode.routerIpAddress = ui->masterRouterEdit->text().trimmed();
        
        PNConfigLib::ListOfNodesBuilder::DeviceNode slaveNode;
        slaveNode.name = slaveName;
        slaveNode.ipAddress = slaveIP;
        slaveNode.routerIpAddress = ui->slaveRouterEdit->text().trimmed();
        
        success = PNConfigLib::ListOfNodesBuilder::saveListOfNodesXml(
            gsdmlPath, masterNode, slaveNode, nodesPath);
        
        if (!success) {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to generate ListOfNodes.xml"));
            statusBar()->showMessage(tr("Generation failed"), 5000);
            return;
        }
        
        // Step 3: Read and compile to final output
        auto config = PNConfigLib::ConfigReader::parseConfiguration(configPath);
        auto nodes = PNConfigLib::ConfigReader::parseListOfNodes(nodesPath);
        
        success = PNConfigLib::Compiler::compile(config, nodes, finalOutputPath);
        
        if (!success) {
            QMessageBox::critical(this, tr("Compilation Error"),
                tr("Failed to compile final output file."));
            statusBar()->showMessage(tr("Compilation failed"), 5000);
            return;
        }
        
        // Success!
        statusBar()->showMessage(tr("Configuration generated successfully!"), 5000);
        
        QMessageBox::information(this, tr("Success"),
            tr("Configuration files generated successfully!\n\n"
               "Output Directory:\n%1\n\n"
               "Generated Files:\n"
               "• Configuration.xml\n"
               "• ListOfNodes.xml\n"
               "• PROFINET_Driver_Output.xml").arg(outputPath));
        
        // Open folder in explorer
        #ifdef Q_OS_WIN
        QString explorer = "explorer.exe";
        QString param = "/select," + QDir::toNativeSeparators(finalOutputPath);
        QProcess::startDetached(explorer, QStringList() << param);
        #endif
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Error during generation:\n%1").arg(e.what()));
        statusBar()->showMessage(tr("Generation failed"), 5000);
    }
}

void MainWindow::onQuickSetupWizard()
{
    QuickSetupWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted) {
        // Generate configuration files
        if (wizard.generateConfiguration()) {
            statusBar()->showMessage("Configuration generated successfully", 5000);
        } else {
            statusBar()->showMessage("Configuration generation failed", 5000);
        }
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        tr("About PNConfigGenerator"),
        tr("<h3>PNConfigGenerator 1.0.0</h3>"
           "<p>PROFINET Device Configuration Tool</p>"
           "<p>A cross-platform tool for configuring PROFINET devices, "
           "similar to EnTalk and TIA Portal configuration functions.</p>")
    );
}

void MainWindow::onOpenProjectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Open Project Directory"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (dir.isEmpty()) {
        return;
    }
    
    QString configPath = dir + "/Configuration.xml";
    QString listOfNodesPath = dir + "/ListOfNodes.xml";
    
    if (!QFile::exists(configPath)) {
        QMessageBox::warning(this, tr("File Missing"), 
            tr("Configuration.xml not found in directory:\n%1").arg(dir));
        return;
    }
    
    if (!QFile::exists(listOfNodesPath)) {
        QMessageBox::warning(this, tr("File Missing"), 
            tr("ListOfNodes.xml not found in directory:\n%1").arg(dir));
        return;
    }
    
    try {
        PNConfigLib::Configuration config = PNConfigLib::ConfigReader::parseConfiguration(configPath);
        PNConfigLib::ListOfNodes nodes = PNConfigLib::ConfigReader::parseListOfNodes(listOfNodesPath);
        
        if (config.listOfNodesRefID != nodes.listOfNodesID) {
            QMessageBox::warning(this, tr("Validation Warning"),
                tr("Configuration references ListOfNodesID '%1' but file has ID '%2'.\n\n"
                   "Files may be inconsistent.").arg(config.listOfNodesRefID, nodes.listOfNodesID));
        }
        
        m_loadedConfigPath = configPath;
        m_loadedListOfNodesPath = listOfNodesPath;
        m_configurationLoaded = true;
        
        QAction *generateAction = findChild<QAction*>("generateOutputAction");
        if (generateAction) {
            generateAction->setEnabled(true);
        }
        
        statusBar()->showMessage(tr("Project loaded from: %1").arg(dir), 5000);
        
        QMessageBox::information(this, tr("Success"),
            tr("Project loaded successfully!\n\n"
               "Directory: %1\n"
               "Devices: %2 Central, %3 Decentralized\n\n"
               "Ready to generate output.")
               .arg(dir).arg(1).arg(config.decentralDevices.size()));
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load project:\n\n%1").arg(e.what()));
        m_configurationLoaded = false;
    }
}

void MainWindow::onGenerateOutput()
{
    if (!m_configurationLoaded) {
        QMessageBox::warning(this, tr("No Project Loaded"),
            tr("Please load a project folder first using File → Open Project Folder"));
        return;
    }
    
    QFileInfo configInfo(m_loadedConfigPath);
    QString projectDir = configInfo.absolutePath();
    QDir dir(projectDir);
    
    // Create output directory
    QString outputDirName = "PNConfigLib_Output";
    if (!dir.exists(outputDirName)) {
        if (!dir.mkdir(outputDirName)) {
            QMessageBox::critical(this, tr("Error"), 
                tr("Failed to create output directory:\n%1/%2").arg(projectDir, outputDirName));
            return;
        }
    }
    
    QString outputPath = dir.absoluteFilePath(outputDirName + "/PROFINET Driver_PN_Driver_Windows.xml");
    
    bool success = PNConfigLib::ProjectManager::runPNConfigLib(
        m_loadedConfigPath,
        m_loadedListOfNodesPath,
        outputPath
    );
    
    if (success) {
        statusBar()->showMessage(tr("Output generated successfully"), 5000);
        QMessageBox::information(this, tr("Success"),
            tr("Configuration compiled successfully!\n\n"
               "Output File:\n%1").arg(outputPath));
        
        // Open folder in explorer
        #ifdef Q_OS_WIN
        QString explorer = "explorer.exe";
        QString param = "/select," + QDir::toNativeSeparators(outputPath);
        QProcess::startDetached(explorer, QStringList() << param);
        #endif
        
    } else {
        QString error = PNConfigLib::ProjectManager::getLastError();
        QMessageBox::critical(this, tr("Compilation Error"),
            tr("Failed to generate output:\n\n%1").arg(error));
        statusBar()->showMessage(tr("Compilation failed"), 5000);
    }
}
