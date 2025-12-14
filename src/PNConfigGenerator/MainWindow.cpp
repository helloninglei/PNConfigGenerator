/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Wizards/QuickSetupWizard.h"
#include "../PNConfigLib/ProjectManager/ProjectManager.h"
#include "../PNConfigLib/ConfigReader/ConfigReader.h"
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
    resize(1200, 800);
    
    // Status bar
    statusBar()->showMessage("Ready");
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
    toolbar->addAction(tr("Generate"), this, &MainWindow::onGenerateOutput);
    
    // Find generate action in toolbar to enable/disable it later if needed, 
    // or we can just rely on the menu action state if they share the action.
    // Ideally we should create QActions as members and share them between menu and toolbar.
    // For now we just add them as separate actions which simplifies things but doesn't sync state perfectly.
    // However, the menu action has an object name "generateOutputAction" which we use to find it.
    // The toolbar action is separate here. To sync them properly we'd need to refactor to use QAction members.
    // Given the request to just "clean up", let's keep it simple.
    // Actually, onOpenConfiguration enables "generateOutputAction", which is the menu action.
    // The toolbar button won't be enabled/disabled automatically unless we share the action object.
    // Let's improve this slightly by using the same logic or just accepting it for now.
    // User asked to "remove unused", not "implement perfect state sync".
    // I'll stick to removing unused ones.
}

void MainWindow::setupDockWidgets()
{
    // TODO: Add dock widgets for:
    // - Device catalog browser (left)
    // - Project tree (left)
    // - Configuration editor (right)
    // - Properties panel (right)
}

void MainWindow::setupConnections()
{
    // TODO: Connect signals and slots
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
