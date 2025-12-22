#ifndef MASTERSIMULATIONWIDGET_H
#define MASTERSIMULATIONWIDGET_H

#include <QWidget>
#include <cstdint>
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
#include "../PNConfigLib/GsdmlParser/GsdmlParser.h"

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
    void onCatalogSelectionChanged();
    void onCatalogContextMenu(const QPoint &pos);
    void onAddToConfiguration();
    void onProjectTreeContextMenu(const QPoint &pos);
    void onRemoveFromConfiguration();
    void onProjectTreeDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void setupUi();
    void createToolbar();
    void createLeftPanel(QSplitter *splitter);
    void createCenterPanel(QSplitter *splitter);
    void createRightPanel(QSplitter *splitter);
    void addSlot(QVBoxLayout *layout, const QString &slotName, const QString &description, const QStringList &subslots = {});
    
    void updateDeviceDetail(const PNConfigLib::GsdmlInfo &info);
    void displayDeviceSlots(const PNConfigLib::GsdmlInfo &info);
    QString formatIdent(uint32_t val);

    QToolBar *toolbar;
    QComboBox *nicComboBox;
    
    QTreeWidget *projectTree;
    QTreeWidgetItem *stationsItem;
    QWidget *centerWidget;
    QVBoxLayout *slotLayout;
    
    // Right panel
    QTabWidget *rightTabWidget;
    QTreeWidget *catalogTree;
    QScrollArea *catalogDetailArea;
    QWidget *catalogDetailContent;
    QVBoxLayout *catalogDetailLayout;
    
    QList<PNConfigLib::GsdmlInfo> m_cachedDevices;
    QLabel *statusLabel;
};

#endif // MASTERSIMULATIONWIDGET_H
