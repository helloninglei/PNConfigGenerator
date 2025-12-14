/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File menu actions
    void onOpenProjectFolder();
    
    // Tools menu actions
    void onQuickSetupWizard();
    void onGenerateOutput();
    
    // Help menu actions
    void onAbout();
    
    // Configuration UI actions
    void onBrowseGsdml();
    void onBrowseOutput();
    void onGenerateConfiguration();

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupDockWidgets();
    void setupConnections();
    
    Ui::MainWindow *ui;
    
    // Configuration loading state (for Open Project Folder workflow)
    QString m_loadedConfigPath;
    QString m_loadedListOfNodesPath;
    bool m_configurationLoaded = false;
};

#endif // MAINWINDOW_H
