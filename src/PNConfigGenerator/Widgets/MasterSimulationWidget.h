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
#include <QPoint>
#include <QCheckBox>
#include <QLineEdit>
#include "../PNConfigLib/GsdmlParser/GsdmlParser.h"
#include "../PNConfigLib/Network/DcpScanner.h"
#include "../PNConfigLib/Network/ArExchangeManager.h"

namespace PNConfigLib {
    class GsdmlInfo;
    class DcpScanner;
    class ArExchangeManager;
    struct DiscoveredDevice;
}

class MasterSimulationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MasterSimulationWidget(QWidget *parent = nullptr);
    ~MasterSimulationWidget();
    void refreshCatalog();

private slots:
    void onScanClicked();
    void onConnectClicked();
    void onImportGsdml();
    void onCatalogSelectionChanged();
    void onCatalogContextMenu(const QPoint &pos);
    void onAddToConfiguration();
    void onProjectTreeContextMenu(const QPoint &pos);
    void onProjectTreeSelectionChanged();
    void onRemoveFromConfiguration();
    void onProjectTreeDoubleClicked(QTreeWidgetItem *item, int column);
    void onSlotClicked(int slotIndex);
    void onModuleContextMenu(const QPoint &pos);
    void onInsertModule();
    void onOnlineContextMenu(const QPoint &pos);
    void onAddOnlineToConfiguration();
    void onSetIp();
    void onOnlineTreeSelectionChanged();
    
    // Device Setup slots
    void onGetStationName();
    void onSetStationName();
    void onGetIpConfig();
    void onSetIpConfig();
    void onResetToFactory();
    void onFlashLed();
    void onFlashTimerTick();
    void onStartCommunication();
    void onArStateChanged(PNConfigLib::ArState state);
    void onArLogMessage(const QString &msg);

private:
    void setupUi();
    void createToolbar();
    void createLeftPanel(QSplitter *splitter);
    void createCenterPanel(QSplitter *splitter);
    void createRightPanel(QSplitter *splitter);
    void addSlot(QVBoxLayout *layout, int slotIndex, const QString &slotName, const QString &description, const QStringList &subslots = {});
    
    void updateDeviceDetail(const PNConfigLib::GsdmlInfo &info);
    void displayDeviceSlots(const PNConfigLib::GsdmlInfo &info);
    void showBasicConfig(const PNConfigLib::GsdmlInfo &info, QTreeWidgetItem *item = nullptr);

    enum TreeItemRoles {
        RoleGsdmlIndex = Qt::UserRole,
        RoleIpAddress = Qt::UserRole + 10,
        RoleSubnetMask = Qt::UserRole + 11,
        RoleGateway = Qt::UserRole + 12
    };
    void showModuleConfig(int slotIndex);
    void updateModuleList(const PNConfigLib::GsdmlInfo &info);
    QString formatIdent(uint32_t val);

    QToolBar *toolbar;
    QComboBox *nicComboBox;
    QPushButton *btnConnect;
    QPushButton *btnScan;
    QPushButton *btnStart;
    bool m_isConnected = false;
    bool m_isArRunning = false;
    
    QTreeWidget *projectTree;
    QTreeWidgetItem *stationsItem;
    
    QTreeWidget *onlineTree;
    PNConfigLib::DcpScanner *m_scanner;
    PNConfigLib::ArExchangeManager *m_arManager;
    
    // Center panel
    QSplitter *centerSplitter;
    QScrollArea *slotScrollArea;
    QWidget *slotContainer;
    QVBoxLayout *slotLayout;
    QScrollArea *configArea;
    QWidget *configWidget;
    QVBoxLayout *configLayout;
    
    PNConfigLib::GsdmlInfo m_currentStationInfo;
    int m_selectedSlotIndex = -1;
    QMap<int, PNConfigLib::ModuleInfo> m_assignedModules;
    
    // Right panel
    QTabWidget *rightTabWidget;
    QTreeWidget *moduleTree;
    QTreeWidget *catalogTree;
    QScrollArea *catalogDetailArea;
    QWidget *catalogDetailContent;
    QVBoxLayout *catalogDetailLayout;
    
    QList<PNConfigLib::GsdmlInfo> m_cachedDevices;
    QLabel *statusLabel;

    // Project Configuration widgets (basic)
    QLineEdit *editProjectName;
    QLineEdit *editProjectIp;
    QLineEdit *editProjectMask;
    QLineEdit *editProjectGw;
    QComboBox *comboProjectIoCycle;
    QLineEdit *editProjectWatchdog;

    // Online Properties view
    QList<PNConfigLib::DiscoveredDevice> m_onlineDevices;
    QGroupBox *onlinePropGroup;
    QLabel *onlinePropName;
    QLabel *onlinePropDeviceId;
    QLabel *onlinePropVendorId;
    QLabel *onlinePropType;
    QLabel *onlinePropIp;
    QLabel *onlinePropMask;
    QLabel *onlinePropGw;
    QLabel *onlinePropMac;
    QLabel *onlinePropRole;
    QLabel *onlinePropGsdml;

    // Device Setup UI members
    QLineEdit *editOnlineName;
    QCheckBox *chkNamePermanent;
    QLineEdit *editOnlineIp;
    QLineEdit *editOnlineMask;
    QLineEdit *editOnlineGw;
    QCheckBox *chkIpPermanent;
    QLabel *statusLed;
    QTimer *flashTimer;
    int flashRemaining;
    bool flashState;
};

#include <QMetaType>
Q_DECLARE_METATYPE(PNConfigLib::DiscoveredDevice)
Q_DECLARE_METATYPE(QList<PNConfigLib::DiscoveredDevice>)

#endif // MASTERSIMULATIONWIDGET_H
