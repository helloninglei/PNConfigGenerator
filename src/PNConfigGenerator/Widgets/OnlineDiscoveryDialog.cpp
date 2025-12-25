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

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(splitter);

    // Table
    deviceTable = new QTableWidget(0, 5, this);
    deviceTable->setHorizontalHeaderLabels({"设备名称", "设备类型", "MAC 地址", "IP 地址", "子网掩码"});
    deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(deviceTable, &QTableWidget::itemSelectionChanged, this, &OnlineDiscoveryDialog::onTableSelectionChanged);
    splitter->addWidget(deviceTable);

    // Properties View
    detailsTab = new QTabWidget(this);
    
    QWidget *propWidget = new QWidget();
    QVBoxLayout *propLayout = new QVBoxLayout(propWidget);
    
    QGroupBox *infoGroup = new QGroupBox("属性", propWidget);
    QFormLayout *form = new QFormLayout(infoGroup);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    propName = new QLabel("-");
    propDeviceId = new QLabel("-");
    propVendorId = new QLabel("-");
    propType = new QLabel("-");
    propIp = new QLabel("-");
    propMask = new QLabel("-");
    propGw = new QLabel("-");
    propMac = new QLabel("-");
    
    form->addRow("站名称:", propName);
    form->addRow("设备ID:", propDeviceId);
    form->addRow("厂商ID:", propVendorId);
    form->addRow("设备类型:", propType);
    
    // Address sub-group in form
    QWidget *addrWidget = new QWidget();
    QFormLayout *addrForm = new QFormLayout(addrWidget);
    addrForm->setContentsMargins(0, 0, 0, 0);
    addrForm->addRow("IP 地址:", propIp);
    addrForm->addRow("子网掩码:", propMask);
    addrForm->addRow("网关:", propGw);
    form->addRow("地址:", addrWidget);
    
    form->addRow("MAC:", propMac);
    form->addRow("职能:", new QLabel("设备"));
    form->addRow("GSDML:", new QLabel("P-Net multi-module sample app (GSDML-V2.4)"));
    
    propLayout->addWidget(infoGroup);
    propLayout->addStretch();
    
    detailsTab->addTab(propWidget, "属性");
    detailsTab->addTab(new QWidget(), "设备设置");
    
    splitter->addWidget(detailsTab);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

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
    m_discoveredDevices = m_scanner->scan();
    QApplication::restoreOverrideCursor();

    deviceTable->setRowCount(0);
    for (int i = 0; i < m_discoveredDevices.size(); ++i) {
        const auto &device = m_discoveredDevices[i];
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);
        
        QTableWidgetItem *nameItem = new QTableWidgetItem(device.deviceName);
        nameItem->setData(Qt::UserRole, i); // Store index
        deviceTable->setItem(row, 0, nameItem);
        
        deviceTable->setItem(row, 1, new QTableWidgetItem(device.deviceType));
        deviceTable->setItem(row, 2, new QTableWidgetItem(device.macAddress));
        deviceTable->setItem(row, 3, new QTableWidgetItem(device.ipAddress));
        deviceTable->setItem(row, 4, new QTableWidgetItem(device.subnetMask));
    }

    onStopSearch();
}

void OnlineDiscoveryDialog::onTableSelectionChanged()
{
    int row = deviceTable->currentRow();
    bool hasSelection = (row >= 0);
    
    flashBtn->setEnabled(hasSelection);
    assignIpBtn->setEnabled(hasSelection);
    assignNameBtn->setEnabled(hasSelection);
    
    if (hasSelection) {
        int index = deviceTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (index >= 0 && index < m_discoveredDevices.size()) {
            const auto &d = m_discoveredDevices[index];
            propName->setText(d.deviceName);
            propDeviceId->setText(QString("16#%1").arg(d.deviceId, 4, 16, QChar('0')).toUpper());
            propVendorId->setText(QString("16#%1").arg(d.vendorId, 4, 16, QChar('0')).toUpper());
            propType->setText(d.deviceType);
            propIp->setText(d.ipAddress);
            propMask->setText(d.subnetMask);
            propGw->setText(d.gateway);
            propMac->setText(d.macAddress.toUpper().remove(':').remove('-'));
        }
    } else {
        propName->setText("-");
        propDeviceId->setText("-");
        propVendorId->setText("-");
        propType->setText("-");
        propIp->setText("-");
        propMask->setText("-");
        propGw->setText("-");
        propMac->setText("-");
    }
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
