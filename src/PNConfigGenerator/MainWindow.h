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
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onImportGSD();
    void onQuickSetupWizard();
    void onGenerateConfiguration();
    void onAbout();

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupDockWidgets();
    void setupConnections();
    
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
