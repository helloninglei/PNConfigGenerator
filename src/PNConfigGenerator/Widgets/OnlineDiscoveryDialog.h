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
    
    QTimer *m_searchTimer;
};

#endif // ONLINEDISCOVERYDIALOG_H
