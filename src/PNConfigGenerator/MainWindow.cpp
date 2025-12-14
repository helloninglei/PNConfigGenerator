/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Wizards/QuickSetupWizard.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>

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
    
    fileMenu->addAction(tr("&Import GSD File..."), this, &MainWindow::onImportGSD);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    
    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Quick Setup Wizard..."), this, &MainWindow::onQuickSetupWizard);
    toolsMenu->addAction(tr("&Generate Configuration..."), this, &MainWindow::onGenerateConfiguration);
    
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
