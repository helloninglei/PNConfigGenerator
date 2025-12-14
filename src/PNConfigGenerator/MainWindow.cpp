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
    
    QAction *newAction = fileMenu->addAction(tr("&New Project"), this, &MainWindow::onNewProject);
    newAction->setShortcut(QKeySequence::New);
    
    QAction *openAction = fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::onOpenProject);
    openAction->setShortcut(QKeySequence::Open);
    
    QAction *saveAction = fileMenu->addAction(tr("&Save Project"), this, &MainWindow::onSaveProject);
    saveAction->setShortcut(QKeySequence::Save);
    
    fileMenu->addSeparator();
    
    // Open Configuration
    QAction *openConfigAction = fileMenu->addAction(tr("Open &Configuration..."), this, &MainWindow::onOpenConfiguration);
    openConfigAction->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    
    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Quick Setup Wizard..."), this, &MainWindow::onQuickSetupWizard);
    toolsMenu->addAction(tr("&Generate Configuration..."), this, &MainWindow::onGenerateConfiguration);
    
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
    toolbar->addAction(tr("New"), this, &MainWindow::onNewProject);
    toolbar->addAction(tr("Open"), this, &MainWindow::onOpenProject);
    toolbar->addAction(tr("Save"), this, &MainWindow::onSaveProject);
    toolbar->addSeparator();
    toolbar->addAction(tr("Quick Setup"), this, &MainWindow::onQuickSetupWizard);
    toolbar->addAction(tr("Generate"), this, &MainWindow::onGenerateConfiguration);
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

void MainWindow::onNewProject()
{
    statusBar()->showMessage("Creating new project...", 2000);
    // TODO: Implement new project creation
}

void MainWindow::onOpenProject()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        "",
        tr("Configuration Files (*.xml);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        statusBar()->showMessage(QString("Opening project: %1").arg(fileName), 2000);
        // TODO: Implement project loading
    }
}

void MainWindow::onSaveProject()
{
    statusBar()->showMessage("Saving project...", 2000);
    // TODO: Implement project saving
}

void MainWindow::onImportGSD()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Import GSD File"),
        "",
        tr("GSDML Files (*.xml);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        statusBar()->showMessage(QString("Importing GSD: %1").arg(fileName), 2000);
        // TODO: Implement GSD import
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

void MainWindow::onGenerateConfiguration()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        statusBar()->showMessage(QString("Generating configuration to: %1").arg(dir), 2000);
        // TODO: Implement configuration generation
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

void MainWindow::onOpenConfiguration()
{
    QString configPath = QFileDialog::getOpenFileName(
        this,
        tr("Open Configuration File"),
        "",
        tr("Configuration Files (Configuration.xml);;XML Files (*.xml);;All Files (*)")
    );
    
    if (configPath.isEmpty()) {
        return;
    }
    
    QFileInfo configInfo(configPath);
    QString listOfNodesPath = configInfo.absolutePath() + "/ListOfNodes.xml";
    
    if (!QFile::exists(listOfNodesPath)) {
        listOfNodesPath = QFileDialog::getOpenFileName(
            this,
            tr("Select ListOfNodes File"),
            configInfo.absolutePath(),
            tr("ListOfNodes Files (ListOfNodes.xml);;XML Files (*.xml);;All Files (*)")
        );
        
        if (listOfNodesPath.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), 
                tr("ListOfNodes.xml is required to load configuration"));
            return;
        }
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
        
        statusBar()->showMessage(tr("Configuration loaded: %1").arg(configInfo.fileName()), 5000);
        
        QMessageBox::information(this, tr("Success"),
            tr("Configuration loaded successfully!\n\n"
               "Devices: %1 Central, %2 Decentralized\n\n"
               "Use Tools → Generate Output to compile configuration.")
               .arg(1).arg(config.decentralDevices.size()));
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load configuration:\n\n%1").arg(e.what()));
        m_configurationLoaded = false;
    }
}

void MainWindow::onGenerateOutput()
{
    if (!m_configurationLoaded) {
        QMessageBox::warning(this, tr("No Configuration"),
            tr("Please load a configuration first using File → Open Configuration"));
        return;
    }
    
    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Save Generated Output"),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/PROFINET_Output.xml",
        tr("XML Files (*.xml);;All Files (*)")
    );
    
    if (outputPath.isEmpty()) {
        return;
    }
    
    bool success = PNConfigLib::ProjectManager::runPNConfigLib(
        m_loadedConfigPath,
        m_loadedListOfNodesPath,
        outputPath
    );
    
    if (success) {
        statusBar()->showMessage(tr("Output generated successfully"), 5000);
        QMessageBox::information(this, tr("Success"),
            tr("Configuration compiled successfully!\n\nOutput: %1").arg(outputPath));
    } else {
        QString error = PNConfigLib::ProjectManager::getLastError();
        QMessageBox::critical(this, tr("Compilation Error"),
            tr("Failed to generate output:\n\n%1").arg(error));
        statusBar()->showMessage(tr("Compilation failed"), 5000);
    }
}
