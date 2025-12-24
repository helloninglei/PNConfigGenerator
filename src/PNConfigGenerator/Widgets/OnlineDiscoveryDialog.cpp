#include "OnlineDiscoveryDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>

OnlineDiscoveryDialog::OnlineDiscoveryDialog(PNConfigLib::DcpScanner *scanner, QWidget *parent)
    : QDialog(parent), m_scanner(scanner)
{
    setupUi();
    updateInterfaceList();
    
    m_searchTimer = new QTimer(this);
    connect(m_searchTimer, &QTimer::timeout, this, &OnlineDiscoveryDialog::onUpdateResults);
    
    setWindowTitle("可访问设备 (Online Discovery)");
    resize(800, 500);
}

OnlineDiscoveryDialog::~OnlineDiscoveryDialog()
{
}

void OnlineDiscoveryDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top: Filter / Interface
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("网卡:"));
    nicComboBox = new QComboBox(this);
    nicComboBox->setMinimumWidth(300);
    topLayout->addWidget(nicComboBox);
    
    startBtn = new QPushButton("开始搜索", this);
    startBtn->setIcon(qApp->style()->standardIcon(QStyle::SP_BrowserReload));
    connect(startBtn, &QPushButton::clicked, this, &OnlineDiscoveryDialog::onStartSearch);
    topLayout->addWidget(startBtn);

    stopBtn = new QPushButton("停止", this);
    stopBtn->setEnabled(false);
    connect(stopBtn, &QPushButton::clicked, this, &OnlineDiscoveryDialog::onStopSearch);
    topLayout->addWidget(stopBtn);
    
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // Table
    deviceTable = new QTableWidget(0, 5, this);
    deviceTable->setHorizontalHeaderLabels({"设备名称", "设备类型", "MAC 地址", "IP 地址", "子网掩码"});
    deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(deviceTable, &QTableWidget::itemSelectionChanged, this, &OnlineDiscoveryDialog::onTableSelectionChanged);
    mainLayout->addWidget(deviceTable);

    // Progress Bar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    mainLayout->addWidget(progressBar);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    flashBtn = new QPushButton("闪烁 LED", this);
    flashBtn->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogYesButton));
    flashBtn->setEnabled(false);
    connect(flashBtn, &QPushButton::clicked, this, &OnlineDiscoveryDialog::onFlashLed);
    btnLayout->addWidget(flashBtn);

    assignIpBtn = new QPushButton("修改 IP 地址", this);
    assignIpBtn->setEnabled(false);
    connect(assignIpBtn, &QPushButton::clicked, this, &OnlineDiscoveryDialog::onAssignIp);
    btnLayout->addWidget(assignIpBtn);

    assignNameBtn = new QPushButton("分配设备名称", this);
    assignNameBtn->setEnabled(false);
    connect(assignNameBtn, &QPushButton::clicked, this, &OnlineDiscoveryDialog::onAssignName);
    btnLayout->addWidget(assignNameBtn);

    btnLayout->addStretch();
    
    QPushButton *closeBtn = new QPushButton("关闭", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    mainLayout->addLayout(btnLayout);
}

void OnlineDiscoveryDialog::updateInterfaceList()
{
    QList<PNConfigLib::InterfaceInfo> interfaces = PNConfigLib::DcpScanner::getAvailableInterfaces();
    nicComboBox->clear();
    for (const auto &iface : interfaces) {
        nicComboBox->addItem(iface.description, iface.name);
    }
}

void OnlineDiscoveryDialog::onStartSearch()
{
    QString ifaceName = nicComboBox->currentData().toString();
    if (ifaceName.isEmpty()) return;

    if (!m_scanner->connectToInterface(ifaceName)) {
        QMessageBox::critical(this, "错误", "无法打开网卡。");
        return;
    }

    startBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    deviceTable->setRowCount(0);
    progressBar->setValue(0);

    // Progress bar simulation
    QTimer *pbTimer = new QTimer(this);
    connect(pbTimer, &QTimer::timeout, this, [this, pbTimer]() {
        int val = progressBar->value() + 5;
        if (val >= 100) {
            progressBar->setValue(100);
            pbTimer->stop();
            pbTimer->deleteLater();
        } else {
            progressBar->setValue(val);
        }
    });
    pbTimer->start(100);

    onUpdateResults();
}

void OnlineDiscoveryDialog::onStopSearch()
{
    startBtn->setEnabled(true);
    stopBtn->setEnabled(false);
}

void OnlineDiscoveryDialog::onUpdateResults()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<PNConfigLib::DiscoveredDevice> devices = m_scanner->scan();
    QApplication::restoreOverrideCursor();

    deviceTable->setRowCount(0);
    for (const auto &device : devices) {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);
        deviceTable->setItem(row, 0, new QTableWidgetItem(device.deviceName));
        deviceTable->setItem(row, 1, new QTableWidgetItem(device.deviceType));
        deviceTable->setItem(row, 2, new QTableWidgetItem(device.macAddress));
        deviceTable->setItem(row, 3, new QTableWidgetItem(device.ipAddress));
        deviceTable->setItem(row, 4, new QTableWidgetItem(device.subnetMask));
        
        // Store gateway in UserRole of MAC column
        deviceTable->item(row, 2)->setData(Qt::UserRole, device.gateway);
    }

    onStopSearch();
}

void OnlineDiscoveryDialog::onTableSelectionChanged()
{
    bool hasSelection = !deviceTable->selectedItems().isEmpty();
    flashBtn->setEnabled(hasSelection);
    assignIpBtn->setEnabled(hasSelection);
    assignNameBtn->setEnabled(hasSelection);
}

void OnlineDiscoveryDialog::onFlashLed()
{
    int row = deviceTable->currentRow();
    if (row < 0) return;
    QString mac = deviceTable->item(row, 2)->text();
    
    if (m_scanner->flashLed(mac)) {
        // Show a brief feedback or status bar would be nice, but QMessageBox for now
        // Actually, TIA Portal doesn't show a box, maybe just status hint
    } else {
        QMessageBox::warning(this, "结果", "无法发送 Flash LED 指令。");
    }
}

void OnlineDiscoveryDialog::onAssignIp()
{
    int row = deviceTable->currentRow();
    if (row < 0) return;
    QString mac = deviceTable->item(row, 2)->text();
    QString currentIp = deviceTable->item(row, 3)->text();
    QString currentMask = deviceTable->item(row, 4)->text();
    QString currentGw = deviceTable->item(row, 2)->data(Qt::UserRole).toString();

    bool ok;
    QString newIp = QInputDialog::getText(this, "修改 IP", "IP 地址:", QLineEdit::Normal, currentIp, &ok);
    if (!ok || newIp.isEmpty()) return;

    if (m_scanner->setDeviceIp(mac, newIp, currentMask, currentGw)) {
        QMessageBox::information(this, "完成", "IP 修改指令已发送。");
        onUpdateResults();
    } else {
        QMessageBox::warning(this, "错误", "发送失败。");
    }
}

void OnlineDiscoveryDialog::onAssignName()
{
    int row = deviceTable->currentRow();
    if (row < 0) return;
    QString mac = deviceTable->item(row, 2)->text();
    QString currentName = deviceTable->item(row, 0)->text();

    bool ok;
    QString newName = QInputDialog::getText(this, "分配名称", "设备名称:", QLineEdit::Normal, currentName, &ok);
    if (!ok || newName.isEmpty()) return;

    if (m_scanner->setDeviceName(mac, newName)) {
        QMessageBox::information(this, "完成", "名称分配指令已发送。");
        onUpdateResults();
    } else {
        QMessageBox::warning(this, "错误", "发送失败。");
    }
}
