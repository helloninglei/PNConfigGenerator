#ifndef MASTERSIMULATIONWIDGET_H
#define MASTERSIMULATIONWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QTreeWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QScrollArea>

class MasterSimulationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MasterSimulationWidget(QWidget *parent = nullptr);
    void refreshCatalog();

private slots:
    void onScanClicked();
    void onConnectClicked();
    void onImportGsdml();

private:
    void setupUi();
    void createToolbar();
    void createLeftPanel(QSplitter *splitter);
    void createCenterPanel(QSplitter *splitter);
    void createRightPanel(QSplitter *splitter);
    
    void addSlot(QVBoxLayout *layout, const QString &slotName, const QString &description, const QStringList &subslots = {});

    QToolBar *toolbar;
    QComboBox *nicComboBox;
    
    QTreeWidget *projectTree;
    QWidget *centerWidget;
    QVBoxLayout *slotLayout;
    QTabWidget *rightTabWidget;
    
    QLabel *statusLabel;
};

#endif // MASTERSIMULATIONWIDGET_H
