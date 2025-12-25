#ifndef ONLINEDISCOVERYDIALOG_H
#define ONLINEDISCOVERYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QProgressBar>
#include <QTabWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QFormLayout>
#include "../../PNConfigLib/Network/DcpScanner.h"

namespace PNConfigLib {
    class DcpScanner;
}

class OnlineDiscoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OnlineDiscoveryDialog(PNConfigLib::DcpScanner *scanner, QWidget *parent = nullptr);
    ~OnlineDiscoveryDialog();

private slots:
    void onStartSearch();
    void onStopSearch();
    void onFlashLed();
    void onAssignIp();
    void onAssignName();
    void onUpdateResults();
    void onTableSelectionChanged();

private:
    void setupUi();
    void updateInterfaceList();

    PNConfigLib::DcpScanner *m_scanner;
    
    QComboBox *nicComboBox;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QTableWidget *deviceTable;
    QProgressBar *progressBar;
    
    QPushButton *flashBtn;
    QPushButton *assignIpBtn;
    QPushButton *assignNameBtn;
    
    // Properties view
    QTabWidget *detailsTab;
    QLabel *propName;
    QLabel *propDeviceId;
    QLabel *propVendorId;
    QLabel *propType;
    QLabel *propIp;
    QLabel *propMask;
    QLabel *propGw;
    QLabel *propMac;
    
    QTimer *m_searchTimer;
    QList<PNConfigLib::DiscoveredDevice> m_discoveredDevices;
};

#endif // ONLINEDISCOVERYDIALOG_H
