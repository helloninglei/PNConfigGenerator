/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Widgets/MasterSimulationWidget.h"

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
    void onBrowseGsdml();
    void onBrowseOutput();
    void onGenerateConfiguration();
    void onImportGsdmlGlobal();

private:
    void setupUi();
    void setupConnections();
    
    Ui::MainWindow *ui;
    MasterSimulationWidget *masterSimWidget;
};

#endif // MAINWINDOW_H
