#include "MasterSimulationWidget.h"
#include "Network/DcpScanner.h"
#include "Network/ArExchangeManager.h"
#include <QHeaderView>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QMenu>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QFormLayout>
#include <QListWidget>
#include <QInputDialog>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>

static QIcon createConnectIcon() {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(Qt::black, 2);
    painter.setPen(pen);
    
    // Draw two horizontal line/plugs meeting in the middle
    painter.drawLine(2, 12, 8, 12);   // Left cable
    painter.drawLine(16, 12, 22, 12); // Right cable
    
    painter.setBrush(Qt::black);
    painter.drawRect(8, 9, 3, 6);     // Left plug head
    painter.drawRect(13, 9, 3, 6);    // Right plug head
    
    // Draw the "pins" meeting
    painter.drawLine(11, 10, 13, 10);
    painter.drawLine(11, 14, 13, 14);
    
    return QIcon(pix);
}

static QIcon createDisconnectIcon() {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(Qt::black, 2);
    painter.setPen(pen);
    
    // Draw two horizontal line/plugs separated
    painter.drawLine(2, 12, 7, 12);   // Left cable
    painter.drawLine(17, 12, 22, 12); // Right cable
    
    painter.setBrush(Qt::black);
    painter.drawRect(7, 9, 3, 6);     // Left plug head
    painter.drawRect(14, 9, 3, 6);    // Right plug head
    
    // Draw the "pins" NOT meeting
    painter.drawLine(10, 10, 11, 10);
    painter.drawLine(10, 14, 11, 14);
    
    painter.drawLine(13, 10, 14, 10);
    painter.drawLine(13, 14, 14, 14);
    
    return QIcon(pix);
}

static QIcon createSearchIcon() {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(Qt::black, 1.5);
    painter.setPen(pen);
    
    // Draw magnifying glass
    painter.drawEllipse(6, 6, 10, 10);
    painter.drawLine(14, 14, 20, 20);
    
    return QIcon(pix);
}

static QIcon createPlayIcon() {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(Qt::black, 1.5);
    painter.setPen(pen);
    painter.setBrush(QColor("#008000")); // Dark green
    
    QPolygonF triangle;
    triangle << QPointF(8, 6) << QPointF(18, 12) << QPointF(8, 18);
    painter.drawPolygon(triangle);
    
    return QIcon(pix);
}

static QIcon createStopIcon() {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(Qt::black, 1.5);
    painter.setPen(pen);
    painter.setBrush(QColor("#cc0000")); // Red
    
    painter.drawRect(7, 7, 10, 10);
    
    return QIcon(pix);
}

MasterSimulationWidget::MasterSimulationWidget(QWidget *parent)
    : QWidget(parent)
{
    m_scanner = new PNConfigLib::DcpScanner(this);
    m_arManager = new PNConfigLib::ArExchangeManager(this);
    connect(m_arManager, &PNConfigLib::ArExchangeManager::stateChanged, this, &MasterSimulationWidget::onArStateChanged);
    connect(m_arManager, &PNConfigLib::ArExchangeManager::messageLogged, this, &MasterSimulationWidget::onArLogMessage);
    
    // Initialize project widgets
    editProjectName = nullptr;
    editProjectIp = nullptr;
    editProjectMask = nullptr;
    editProjectGw = nullptr;
    comboProjectIoCycle = nullptr;
    editProjectWatchdog = nullptr;

    setupUi();
    refreshCatalog();
}

MasterSimulationWidget::~MasterSimulationWidget()
{
    if (m_arManager) m_arManager->stop();
    if (flashTimer) flashTimer->stop();
}

void MasterSimulationWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Toolbar
    createToolbar();
    mainLayout->addWidget(toolbar);

    // 2. Main Content Splitter
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    createLeftPanel(mainSplitter);
    createCenterPanel(mainSplitter);
    createRightPanel(mainSplitter);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setStretchFactor(2, 2);
    mainSplitter->setSizes({250, 600, 350});
    
    mainLayout->addWidget(mainSplitter, 1);

    // 3. Status Bar
    statusLabel = new QLabel(" 就绪", this);
    statusLabel->setStyleSheet("background-color: #f0f0f0; border-top: 1px solid #ccc; padding: 2px;");
    mainLayout->addWidget(statusLabel);
}

#include <QNetworkInterface>

void MasterSimulationWidget::createToolbar()
{
    toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setStyleSheet("QToolBar { spacing: 0px; border-bottom: 1px solid #c0c0c0; }");

    // Container for the network group
    QWidget *netGroup = new QWidget(this);
    netGroup->setStyleSheet(
        "QWidget#netGroup { border: 1px solid #a0a0a0; background: #f8f8f8; }"
        "QComboBox { border: none; padding-left: 5px; background: white; }"
        "QPushButton { border: none; border-left: 1px solid #c0c0c0; padding: 2px; background: transparent; }"
        "QPushButton:hover { background: #e0e0e0; }"
        "QPushButton:checked { background: #c0c0c0; }"
    );
    netGroup->setObjectName("netGroup");
    
    QHBoxLayout *netLayout = new QHBoxLayout(netGroup);
    netLayout->setContentsMargins(0, 0, 0, 0);
    netLayout->setSpacing(0);

    nicComboBox = new QComboBox(netGroup);
    nicComboBox->setMinimumWidth(180);
    
    QList<PNConfigLib::InterfaceInfo> interfaces = PNConfigLib::DcpScanner::getAvailableInterfaces();
    for (const auto &iface : interfaces) {
        nicComboBox->addItem(iface.description, iface.name);
    }
    if (nicComboBox->count() == 0) nicComboBox->addItem("未找到可用网卡", "");
    netLayout->addWidget(nicComboBox);

    btnConnect = new QPushButton(createConnectIcon(), "", netGroup);
    btnConnect->setCheckable(true);
    btnConnect->setFixedSize(28, 24);
    btnConnect->setToolTip("建立连接");
    connect(btnConnect, &QPushButton::clicked, this, &MasterSimulationWidget::onConnectClicked);
    netLayout->addWidget(btnConnect);
    
    btnScan = new QPushButton(createSearchIcon(), "", netGroup);
    btnScan->setEnabled(false);
    btnScan->setFixedSize(28, 24);
    btnScan->setToolTip("搜索设备");
    btnScan->setObjectName("btnScan");
    connect(btnScan, &QPushButton::clicked, this, &MasterSimulationWidget::onScanClicked);
    netLayout->addWidget(btnScan);

    btnStart = new QPushButton(createPlayIcon(), "", netGroup);
    btnStart->setEnabled(false);
    btnStart->setFixedSize(28, 24);
    btnStart->setToolTip("开始 cyclic 数据交换 (AR)");
    btnStart->setObjectName("btnStart");
    connect(btnStart, &QPushButton::clicked, this, &MasterSimulationWidget::onStartCommunication);
    netLayout->addWidget(btnStart);

    toolbar->addWidget(netGroup);
}

void MasterSimulationWidget::createLeftPanel(QSplitter *splitter)
{
    projectTree = new QTreeWidget(this);
    projectTree->setHeaderLabel("工程");
    
    QTreeWidgetItem *projectItem = new QTreeWidgetItem(projectTree, QStringList() << "Project");
    new QTreeWidgetItem(projectItem, QStringList() << "Logic");
    stationsItem = new QTreeWidgetItem(projectItem, QStringList() << "从站列表");
    
    projectTree->expandAll();
    
    projectTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(projectTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onProjectTreeContextMenu);
    connect(projectTree, &QTreeWidget::itemDoubleClicked, this, &MasterSimulationWidget::onProjectTreeDoubleClicked);
    connect(projectTree, &QTreeWidget::itemSelectionChanged, this, &MasterSimulationWidget::onProjectTreeSelectionChanged);

    splitter->addWidget(projectTree);
}

void MasterSimulationWidget::createCenterPanel(QSplitter *splitter)
{
    centerSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side: Slot List
    slotScrollArea = new QScrollArea(this);
    slotScrollArea->setWidgetResizable(true);
    slotScrollArea->setFrameShape(QFrame::NoFrame);
    
    slotContainer = new QWidget();
    slotLayout = new QVBoxLayout(slotContainer);
    slotLayout->setAlignment(Qt::AlignTop);
    
    // Start with empty slots
    
    slotScrollArea->setWidget(slotContainer);
    centerSplitter->addWidget(slotScrollArea);
    
    // Right side: Basic Config Panel
    configArea = new QScrollArea(this);
    configArea->setWidgetResizable(true);
    configArea->setFrameShape(QFrame::NoFrame);
    configArea->setStyleSheet("background-color: white; border-left: 1px solid #ccc;");
    
    configWidget = new QWidget();
    configLayout = new QVBoxLayout(configWidget);
    configLayout->setAlignment(Qt::AlignTop);
    
    configArea->setWidget(configWidget);
    centerSplitter->addWidget(configArea);
    
    // Set initial splitter sizes
    centerSplitter->setStretchFactor(0, 2);
    centerSplitter->setStretchFactor(1, 3);
    
    splitter->addWidget(centerSplitter);
}

void MasterSimulationWidget::createRightPanel(QSplitter *splitter)
{
    rightTabWidget = new QTabWidget(this);
    
    // 1. Catalog Tab
    QWidget *catalogTab = new QWidget(this);
    QVBoxLayout *catalogTabLayout = new QVBoxLayout(catalogTab);
    catalogTabLayout->setContentsMargins(0, 0, 0, 0);
    
    QSplitter *vSplitter = new QSplitter(Qt::Vertical, catalogTab);
    
    catalogTree = new QTreeWidget(this);
    catalogTree->setHeaderLabel("设备列表");
    vSplitter->addWidget(catalogTree);
    
    catalogDetailArea = new QScrollArea(this);
    catalogDetailArea->setWidgetResizable(true);
    catalogDetailArea->setFrameShape(QFrame::NoFrame);
    catalogDetailArea->setStyleSheet("background-color: white;");
    
    catalogDetailContent = new QWidget();
    catalogDetailLayout = new QVBoxLayout(catalogDetailContent);
    catalogDetailLayout->setAlignment(Qt::AlignTop);
    catalogDetailLayout->setContentsMargins(10, 10, 10, 10);
    
    catalogDetailArea->setWidget(catalogDetailContent);
    vSplitter->addWidget(catalogDetailArea);
    
    catalogTabLayout->addWidget(vSplitter);
    
    // 2. Module Tab
    QWidget *moduleTab = new QWidget(this);
    QVBoxLayout *moduleTabLayout = new QVBoxLayout(moduleTab);
    moduleTabLayout->setContentsMargins(0, 0, 0, 0);
    
    moduleTree = new QTreeWidget(this);
    moduleTree->setHeaderLabel("子模块列表");
    moduleTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(moduleTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onModuleContextMenu);
    
    moduleTabLayout->addWidget(moduleTree);
    
    QWidget *onlineTab = new QWidget(this);
    QVBoxLayout *onlineTabLayout = new QVBoxLayout(onlineTab);
    onlineTabLayout->setContentsMargins(0, 0, 0, 0);

    QSplitter *onlineSplitter = new QSplitter(Qt::Vertical, onlineTab);
    
    onlineTree = new QTreeWidget(onlineTab);
    onlineTree->setHeaderLabels({"设备名称", "IP 地址"});
    onlineTree->setColumnWidth(0, 200);
    onlineTree->setColumnWidth(1, 150);
    onlineTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(onlineTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onOnlineContextMenu);
    connect(onlineTree, &QTreeWidget::itemSelectionChanged, this, &MasterSimulationWidget::onOnlineTreeSelectionChanged);

    onlineSplitter->addWidget(onlineTree);
    
    // Detailed Properties (TIA Style)
    QTabWidget *onlinePropTab = new QTabWidget(onlineTab);
    QWidget *propContent = new QWidget();
    QVBoxLayout *propVBox = new QVBoxLayout(propContent);
    
    onlinePropGroup = new QGroupBox("属性", propContent);
    onlinePropGroup->setVisible(false);
    QFormLayout *form = new QFormLayout(onlinePropGroup);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    onlinePropName = new QLabel("-");
    onlinePropDeviceId = new QLabel("-");
    onlinePropVendorId = new QLabel("-");
    onlinePropType = new QLabel("-");
    onlinePropIp = new QLabel("-");
    onlinePropMask = new QLabel("-");
    onlinePropGw = new QLabel("-");
    onlinePropMac = new QLabel("-");
    
    form->addRow("站名称:", onlinePropName);
    form->addRow("设备ID:", onlinePropDeviceId);
    form->addRow("厂商ID:", onlinePropVendorId);
    form->addRow("设备类型:", onlinePropType);
    
    QWidget *addrWidget = new QWidget();
    QFormLayout *addrForm = new QFormLayout(addrWidget);
    addrForm->setContentsMargins(0, 0, 0, 0);
    addrForm->addRow("IP 地址:", onlinePropIp);
    addrForm->addRow("子网掩码:", onlinePropMask);
    addrForm->addRow("网关:", onlinePropGw);
    form->addRow("地址:", addrWidget);
    
    onlinePropRole = new QLabel("设备");
    onlinePropGsdml = new QLabel("P-Net multi-module sample app (GSDML-V2.4)");
    
    form->addRow("MAC:", onlinePropMac);
    form->addRow("职能:", onlinePropRole);
    form->addRow("GSDML:", onlinePropGsdml);
    
    propVBox->addWidget(onlinePropGroup);
    propVBox->addStretch();
    
    onlinePropTab->addTab(propContent, "属性");
    
    // 3.2 Device Setup Tab
    QWidget *setupContent = new QWidget();
    QVBoxLayout *setupVBox = new QVBoxLayout(setupContent);
    setupVBox->setAlignment(Qt::AlignTop);
    
    // Group: Station Name
    QGroupBox *nameGroup = new QGroupBox("站名称", setupContent);
    QGridLayout *nameGrid = new QGridLayout(nameGroup);
    nameGrid->addWidget(new QLabel("名称"), 0, 0);
    editOnlineName = new QLineEdit("-");
    nameGrid->addWidget(editOnlineName, 0, 1);
    
    QPushButton *btnGetName = new QPushButton("获取");
    QPushButton *btnSetName = new QPushButton("设置");
    nameGrid->addWidget(btnGetName, 0, 2);
    nameGrid->addWidget(btnSetName, 0, 3);
    
    chkNamePermanent = new QCheckBox("永久保存");
    chkNamePermanent->setChecked(true);
    nameGrid->addWidget(chkNamePermanent, 1, 1);
    setupVBox->addWidget(nameGroup);
    
    // Group: Network
    QGroupBox *netGroupSetup = new QGroupBox("网络", setupContent);
    QGridLayout *netGrid = new QGridLayout(netGroupSetup);
    netGrid->addWidget(new QLabel("IP"), 0, 0);
    editOnlineIp = new QLineEdit("0.0.0.0");
    netGrid->addWidget(editOnlineIp, 0, 1);
    
    netGrid->addWidget(new QLabel("网关"), 1, 0);
    editOnlineGw = new QLineEdit("0.0.0.0");
    netGrid->addWidget(editOnlineGw, 1, 1);
    
    netGrid->addWidget(new QLabel("子网掩码"), 2, 0);
    editOnlineMask = new QLineEdit("0.0.0.0");
    netGrid->addWidget(editOnlineMask, 2, 1);
    
    QPushButton *btnGetIp = new QPushButton("获取");
    QPushButton *btnSetIpSetup = new QPushButton("设置");
    netGrid->addWidget(btnGetIp, 2, 2);
    netGrid->addWidget(btnSetIpSetup, 2, 3);
    
    chkIpPermanent = new QCheckBox("永久保存");
    chkIpPermanent->setChecked(true);
    netGrid->addWidget(chkIpPermanent, 3, 1);
    setupVBox->addWidget(netGroupSetup);
    
    // Group: Factory Reset
    QGroupBox *resetGroup = new QGroupBox("重置为工厂设置", setupContent);
    QVBoxLayout *resetLayout = new QVBoxLayout(resetGroup);
    QPushButton *btnReset = new QPushButton("重置");
    btnReset->setFixedWidth(120);
    resetLayout->addWidget(btnReset, 0, Qt::AlignCenter);
    setupVBox->addWidget(resetGroup);
    
    // Group: Identification
    QGroupBox *identGroup = new QGroupBox("设备识别", setupContent);
    QHBoxLayout *identLayout = new QHBoxLayout(identGroup);
    
    QPushButton *btnFlash = new QPushButton("闪烁 LED");
    btnFlash->setFixedWidth(120);
    identLayout->addWidget(btnFlash);
    
    statusLed = new QLabel();
    statusLed->setFixedSize(20, 20);
    statusLed->setStyleSheet("background-color: #f0f0f0; border: 2px solid #ccc; border-radius: 10px;");
    identLayout->addWidget(statusLed);
    identLayout->addStretch();
    
    setupVBox->addWidget(identGroup);
    
    flashTimer = new QTimer(this);
    connect(flashTimer, &QTimer::timeout, this, &MasterSimulationWidget::onFlashTimerTick);
    flashRemaining = 0;
    flashState = false;
    
    setupVBox->addStretch();
    
    onlinePropTab->addTab(setupContent, "设备设置");
    
    onlineSplitter->addWidget(onlinePropTab);
    
    // Connect Device Setup signals
    connect(btnGetName, &QPushButton::clicked, this, &MasterSimulationWidget::onGetStationName);
    connect(btnSetName, &QPushButton::clicked, this, &MasterSimulationWidget::onSetStationName);
    connect(btnGetIp, &QPushButton::clicked, this, &MasterSimulationWidget::onGetIpConfig);
    connect(btnSetIpSetup, &QPushButton::clicked, this, &MasterSimulationWidget::onSetIpConfig);
    connect(btnReset, &QPushButton::clicked, this, &MasterSimulationWidget::onResetToFactory);
    connect(btnFlash, &QPushButton::clicked, this, &MasterSimulationWidget::onFlashLed);
    onlineSplitter->setStretchFactor(0, 2);
    onlineSplitter->setStretchFactor(1, 1);
    
    onlineTabLayout->addWidget(onlineSplitter);
    
    rightTabWidget->addTab(catalogTab, "设备列表");
    rightTabWidget->addTab(moduleTab, "子模块列表");
    rightTabWidget->addTab(onlineTab, "在线");
    
    splitter->addWidget(rightTabWidget);

    connect(catalogTree, &QTreeWidget::itemSelectionChanged, this, &MasterSimulationWidget::onCatalogSelectionChanged);
    
    catalogTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(catalogTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onCatalogContextMenu);
}

void MasterSimulationWidget::addSlot(QVBoxLayout *layout, int slotIndex, const QString &slotName, const QString &description, const QStringList &subslots)
{
    QFrame *slotFrame = new QFrame(this);
    slotFrame->setFrameShape(QFrame::StyledPanel);
    slotFrame->setProperty("slotIndex", slotIndex);
    
    // Selection styling (rough draft)
    if (m_selectedSlotIndex == slotIndex) {
        slotFrame->setStyleSheet("background-color: #f0f7ff; border: 2px solid #0078d7; margin-bottom: 5px;");
    } else {
        slotFrame->setStyleSheet("background-color: #f7f7f7; border: 1px solid #ccc; margin-bottom: 5px;");
    }
    
    QVBoxLayout *vbox = new QVBoxLayout(slotFrame);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    
    QPushButton *headerBtn = new QPushButton(QString(" %1 - %2").arg(slotName, description), this);
    headerBtn->setStyleSheet("text-align: left; background-color: #e0e0e0; padding: 5px; border: none; font-weight: bold;");
    headerBtn->setCursor(Qt::PointingHandCursor);
    vbox->addWidget(headerBtn);
    
    connect(headerBtn, &QPushButton::clicked, [this, slotIndex]() {
        onSlotClicked(slotIndex);
    });
    
    if (!subslots.isEmpty()) {
        QWidget *subContainer = new QWidget();
        QVBoxLayout *subVbox = new QVBoxLayout(subContainer);
        subVbox->setContentsMargins(5, 5, 5, 5);
        for (const QString &subslot : subslots) {
            QLabel *subLabel = new QLabel(subslot, this);
            subLabel->setStyleSheet("padding: 3px; padding-left: 15px; background-color: white; border: 1px solid #eee; margin-top: 2px;");
            subVbox->addWidget(subLabel);
        }
        vbox->addWidget(subContainer);
    }
    
    layout->addWidget(slotFrame);
}

#include <QFileDialog>
#include <QMessageBox>
#include "../PNConfigLib/DataModel/DeviceCacheManager.h"

void MasterSimulationWidget::refreshCatalog()
{
    if (!catalogTree) return;

    catalogTree->clear();
    
    // Helper to find or create a child item with a specific name
    auto findOrCreate = [](QTreeWidgetItem* parent, const QString& name) {
        if (name.isEmpty()) return parent;
        for (int i = 0; i < parent->childCount(); ++i) {
            if (parent->child(i)->text(0) == name) {
                return parent->child(i);
            }
        }
        return new QTreeWidgetItem(parent, QStringList() << name);
    };

    // Load from DeviceCacheManager
    m_cachedDevices = PNConfigLib::DeviceCacheManager::instance().getCachedDevices();
    
    for (int i = 0; i < m_cachedDevices.size(); ++i) {
        const auto& device = m_cachedDevices[i];
        
        // 1. Vendor Name (Top Level)
        QTreeWidgetItem* vendorItem = nullptr;
        for (int j = 0; j < catalogTree->topLevelItemCount(); ++j) {
            if (catalogTree->topLevelItem(j)->text(0) == device.deviceVendor) {
                vendorItem = catalogTree->topLevelItem(j);
                break;
            }
        }
        if (!vendorItem) {
            vendorItem = new QTreeWidgetItem(catalogTree, QStringList() << device.deviceVendor);
        }

        // 2. Main Family
        QString mainFam = device.mainFamily.isEmpty() ? "I/O" : device.mainFamily;
        QTreeWidgetItem* mainFamilyItem = findOrCreate(vendorItem, mainFam);

        // 3. Product Family
        QString prodFam = device.productFamily.isEmpty() ? "P-Net Samples" : device.productFamily;
        QTreeWidgetItem* productFamilyItem = findOrCreate(mainFamilyItem, prodFam);

        // 4. Device Name (Leaf)
        QTreeWidgetItem *deviceItem = new QTreeWidgetItem(productFamilyItem, QStringList() << device.deviceName);
        deviceItem->setData(0, Qt::UserRole, i);
        deviceItem->setIcon(0, qApp->style()->standardIcon(QStyle::SP_DriveHDIcon));
    }
    
    // Add default mock items if empty or as extra
    if (m_cachedDevices.isEmpty()) {
        QTreeWidgetItem* rtLabs = new QTreeWidgetItem(catalogTree, QStringList() << "RT-Labs");
        QTreeWidgetItem* io = new QTreeWidgetItem(rtLabs, QStringList() << "I/O");
        QTreeWidgetItem* samples = new QTreeWidgetItem(io, QStringList() << "P-Net Samples");
        QTreeWidgetItem *mockItem = new QTreeWidgetItem(samples, QStringList() << "P-Net multi-module sample app");
        mockItem->setData(0, Qt::UserRole, -1);
        mockItem->setIcon(0, qApp->style()->standardIcon(QStyle::SP_DriveHDIcon));
    }
    
    catalogTree->expandAll();
}

void MasterSimulationWidget::onCatalogContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = catalogTree->itemAt(pos);
    if (!item || item->childCount() > 0) return; // Only for leaf nodes

    QMenu menu(this);
    QAction *addAction = menu.addAction("添加到配置");
    connect(addAction, &QAction::triggered, this, &MasterSimulationWidget::onAddToConfiguration);
    menu.exec(catalogTree->mapToGlobal(pos));
}

void MasterSimulationWidget::onProjectTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = projectTree->itemAt(pos);
    if (!item || !stationsItem || item->parent() != stationsItem) return;

    QMenu menu(this);
    QAction *removeAction = menu.addAction("删除设备");
    connect(removeAction, &QAction::triggered, this, &MasterSimulationWidget::onRemoveFromConfiguration);
    menu.exec(projectTree->mapToGlobal(pos));
}

void MasterSimulationWidget::onRemoveFromConfiguration()
{
    QTreeWidgetItem *item = projectTree->currentItem();
    if (!item || !stationsItem || item->parent() != stationsItem) return;

    QString deviceName = item->text(0);
    delete item;
    
    // Clear slot view if the device was viewing
    // (In a more robust version, we'd check if center panel currently belongs to this device)
    while (QLayoutItem* layoutItem = slotLayout->takeAt(0)) {
        if (layoutItem->widget()) delete layoutItem->widget();
        delete layoutItem;
    }
    
    statusLabel->setText(QString(" 已删除设备: %1").arg(deviceName));
}

void MasterSimulationWidget::onProjectTreeSelectionChanged()
{
    QTreeWidgetItem *item = projectTree->currentItem();
    if (!item || !stationsItem || item->parent() != stationsItem) {
        if (!m_isArRunning) btnStart->setEnabled(false);
        return;
    }

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) return;

    int index = data.toInt();
    if (index >= 0 && index < m_cachedDevices.size()) {
        m_currentStationInfo = m_cachedDevices[index];
    } else {
        // Mock slots for samples
        m_currentStationInfo = PNConfigLib::GsdmlInfo();
        m_currentStationInfo.deviceName = "P-Net multi-module sample app";
        m_currentStationInfo.physicalSlots = 4;
        
        PNConfigLib::ModuleInfo m1;
        m1.name = "DO 8xLogicLevel";
        m_currentStationInfo.modules.append(m1);
    }
    
    // Reset selection and display
    m_assignedModules.clear();
    m_selectedSlotIndex = 0; // Default Select Slot 0
    displayDeviceSlots(m_currentStationInfo);
    showBasicConfig(m_currentStationInfo);

    // Enable Start button if connected
    if (m_isConnected && !m_isArRunning) {
        btnStart->setEnabled(true);
    }
}

void MasterSimulationWidget::onProjectTreeDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    // Double click now just ensures selection which triggers onProjectTreeSelectionChanged
    onProjectTreeSelectionChanged();
}

void MasterSimulationWidget::onSlotClicked(int slotIndex)
{
    m_selectedSlotIndex = slotIndex;
    
    // Refresh slot list to update selection styling
    displayDeviceSlots(m_currentStationInfo);
    
    if (slotIndex == 0) {
        rightTabWidget->setCurrentIndex(0); // Show catalog or keep static?
        showBasicConfig(m_currentStationInfo);
    } else {
        showModuleConfig(slotIndex);
        
        // Show available modules in right tab
        rightTabWidget->setCurrentIndex(1); // "子模块列表"
        updateModuleList(m_currentStationInfo);
    }
}

void MasterSimulationWidget::updateModuleList(const PNConfigLib::GsdmlInfo &info)
{
    moduleTree->clear();
    QTreeWidgetItem *category = new QTreeWidgetItem(moduleTree, QStringList() << "Category");
    
    for (int i = 0; i < info.modules.size(); ++i) {
        QTreeWidgetItem *item = new QTreeWidgetItem(category, QStringList() << info.modules[i].name);
        item->setData(0, Qt::UserRole, i);
    }
    
    category->setExpanded(true);
}

void MasterSimulationWidget::onModuleContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = moduleTree->itemAt(pos);
    if (!item || item->parent() == nullptr) return; // Only for modules, not category

    QMenu menu(this);
    QAction *insertAction = menu.addAction("插入到插槽");
    connect(insertAction, &QAction::triggered, this, &MasterSimulationWidget::onInsertModule);
    menu.exec(moduleTree->mapToGlobal(pos));
}

void MasterSimulationWidget::onInsertModule()
{
    QTreeWidgetItem *item = moduleTree->currentItem();
    if (!item || m_selectedSlotIndex <= 0) return;

    int modIndex = item->data(0, Qt::UserRole).toInt();
    if (modIndex >= 0 && modIndex < m_currentStationInfo.modules.size()) {
        m_assignedModules[m_selectedSlotIndex] = m_currentStationInfo.modules[modIndex];
        displayDeviceSlots(m_currentStationInfo);
        statusLabel->setText(QString(" 已将模块 %1 插入到 Slot(0x%2)")
            .arg(item->text(0))
            .arg(m_selectedSlotIndex, 4, 16, QChar('0')));
    }
}

void MasterSimulationWidget::showModuleConfig(int slotIndex)
{
    // Clear config layout
    while (QLayoutItem* item = configLayout->takeAt(0)) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    if (!m_assignedModules.contains(slotIndex)) {
        configLayout->addWidget(new QLabel("Slot(" + QString::number(slotIndex) + ") - 未插入模块"));
        return;
    }

    const auto& mod = m_assignedModules[slotIndex];

    // 1. Header (TIA Style - Light Blue)
    QLabel* header = new QLabel(QString("(0x%1) - %2")
        .arg(slotIndex, 4, 16, QChar('0')).toUpper()
        .arg(mod.name), this);
    header->setStyleSheet("background-color: #87CEEB; padding: 8px; font-weight: bold; font-size: 14px;");
    header->setMinimumHeight(40);
    configLayout->addWidget(header);

    // 2. Parameters Section (Demo based on screenshot)
    auto addParamGroup = [&](const QString& title, const QString& labelText, const QString& defaultValue) {
        QGroupBox* group = new QGroupBox(title, this);
        QFormLayout* form = new QFormLayout(group);
        form->setLabelAlignment(Qt::AlignLeft);
        QLineEdit* edit = new QLineEdit(defaultValue, this);
        edit->setFixedWidth(120);
        form->addRow(labelText, edit);
        configLayout->addWidget(group);
    };

    addParamGroup("Parameter 1", "Demo 1", "1");
    addParamGroup("Parameter 2", "Demo 2", "2");

    // 3. IO Tables (Conditional)
    auto addIOTable = [&](const QString& title, bool isInput) {
        // Check if there is any data for this direction
        bool hasData = false;
        for (const auto& sub : mod.submodules) {
            if ((isInput && sub.inputDataLength > 0) || (!isInput && sub.outputDataLength > 0)) {
                hasData = true;
                break;
            }
        }

        if (!hasData) return;

        configLayout->addSpacing(10);
        QLabel* titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet("font-weight: bold;");
        configLayout->addWidget(titleLabel);

        QTableWidget* table = new QTableWidget(0, 2, this);
        table->setHorizontalHeaderLabels({"名称", "类型"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setMaximumHeight(100);
        table->setStyleSheet("background: white; border: 1px solid #ccc;");
        
        for (const auto& sub : mod.submodules) {
            if ((isInput && sub.inputDataLength > 0) || (!isInput && sub.outputDataLength > 0)) {
                int row = table->rowCount();
                table->insertRow(row);
                table->setItem(row, 0, new QTableWidgetItem(sub.name));
                table->setItem(row, 1, new QTableWidgetItem("Unsigned8")); // Static for demo
            }
        }

        configLayout->addWidget(table);
    };

    addIOTable("输入", true);
    addIOTable("输出", false);

    configLayout->addStretch();
}

void MasterSimulationWidget::displayDeviceSlots(const PNConfigLib::GsdmlInfo &info)
{
    // Clear current slots
    while (QLayoutItem* layoutItem = slotLayout->takeAt(0)) {
        if (layoutItem->widget()) delete layoutItem->widget();
        delete layoutItem;
    }

    // Add DAP Slot (Slot 0)
    QStringList dapSubslots = {"Subslot(0x0001) " + info.deviceName, "Subslot(0x8000) X1", "Subslot(0x8001) X1 P1"};
    addSlot(slotLayout, 0, "Slot(0x0000)", info.deviceName, dapSubslots);

    // Other slots
    for (int i = 1; i <= info.physicalSlots; ++i) {
        QString slotName = QString("Slot(0x%1)").arg(i, 4, 16, QChar('0'));
        if (m_assignedModules.contains(i)) {
            const auto& mod = m_assignedModules[i];
            QStringList subslots;
            
            // Add all submodules from GSDML
            for (const auto& sub : mod.submodules) {
                subslots << QString("Subslot(0x%1) %2").arg(sub.submoduleIdentNumber, 4, 16, QChar('0')).arg(sub.name);
            }
            
            addSlot(slotLayout, i, slotName, mod.name, subslots);
        } else {
            addSlot(slotLayout, i, slotName, "");
        }
    }
    
    slotLayout->addStretch();
}

void MasterSimulationWidget::showBasicConfig(const PNConfigLib::GsdmlInfo &info)
{
    // Clear config layout
    while (QLayoutItem* item = configLayout->takeAt(0)) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    // 1. Station Name Group
    QGroupBox *nameGroup = new QGroupBox("站名称", this);
    QFormLayout *nameForm = new QFormLayout(nameGroup);
    
    // Default station name from GSDML, sanitized for PROFINET (lowercase, DNS labels)
    QString rawName = info.deviceName.toLower();
    QString sanitized;
    for (const QChar &c : rawName) {
        if (c.isLetterOrNumber()) {
            sanitized += c;
        } else if (c.isSpace() || c == '_' || c == '-' || c == '.') {
            if (!sanitized.isEmpty() && sanitized.back() != '-') {
                sanitized += '-';
            }
        }
    }
    if (sanitized.endsWith("-")) sanitized.chop(1);
    if (sanitized.isEmpty()) sanitized = "pn-device";

    editProjectName = new QLineEdit(sanitized, this);
    nameForm->addRow("名称", editProjectName);
    configLayout->addWidget(nameGroup);

    // 2. IP Config Group
    QGroupBox *ipGroup = new QGroupBox("IP配置", this);
    QVBoxLayout *ipVbox = new QVBoxLayout(ipGroup);
    QCheckBox *startupCheck = new QCheckBox("启动期间配置设备", this);
    startupCheck->setChecked(true);
    ipVbox->addWidget(startupCheck);
    
    QFormLayout *ipForm = new QFormLayout();
    editProjectIp = new QLineEdit("192.168.0.253", this);
    editProjectMask = new QLineEdit("255.255.255.0", this);
    editProjectGw = new QLineEdit("192.168.0.1", this);
    ipForm->addRow("IP", editProjectIp);
    ipForm->addRow("子网掩码", editProjectMask);
    ipForm->addRow("网关", editProjectGw);
    ipVbox->addLayout(ipForm);
    configLayout->addWidget(ipGroup);

    // 3. IO Cycle Time
    QGroupBox *ioGroup = new QGroupBox("IO周期时间(ms)", this);
    QVBoxLayout *ioVbox = new QVBoxLayout(ioGroup);
    comboProjectIoCycle = new QComboBox(this);
    comboProjectIoCycle->addItem("256");
    ioVbox->addWidget(comboProjectIoCycle);
    configLayout->addWidget(ioGroup);

    // 4. Watchdog
    QGroupBox *wdGroup = new QGroupBox("看门狗因子", this);
    QHBoxLayout *wdHbox = new QHBoxLayout(wdGroup);
    editProjectWatchdog = new QLineEdit("7", this);
    wdHbox->addWidget(editProjectWatchdog);
    wdHbox->addWidget(new QLabel("允许值: [3..7]", this));
    configLayout->addWidget(wdGroup);
    
    // 5. Subslot List (as seen in image)
    QStringList subSlots = {"(0x0001) - " + info.deviceName, "(0x8000) - X1", "(0x8001) - X1 P1"};
    QListWidget *subList = new QListWidget(this);
    subList->addItems(subSlots);
    subList->setStyleSheet("background-color: #87CEEB; border: none; font-weight: bold;"); // SkyBlue background
    configLayout->addWidget(subList);

    configLayout->addStretch();
}

void MasterSimulationWidget::onAddToConfiguration()
{
    QTreeWidgetItem *item = catalogTree->currentItem();
    if (!item || !stationsItem) return;

    QString deviceName = item->text(0);
    
    // Check for duplicates and append a suffix if needed
    int count = 1;
    QString finalName = deviceName;
    bool exists = true;
    while (exists) {
        exists = false;
        for (int i = 0; i < stationsItem->childCount(); ++i) {
            if (stationsItem->child(i)->text(0) == finalName) {
                exists = true;
                finalName = QString("%1-%2").arg(deviceName).arg(count++);
                break;
            }
        }
    }

    QTreeWidgetItem *newStation = new QTreeWidgetItem(stationsItem, QStringList() << finalName);
    newStation->setIcon(0, qApp->style()->standardIcon(QStyle::SP_ComputerIcon));
    
    // Associate the info index from m_cachedDevices (if found) or use the data from the item itself
    QVariant catalogData = item->data(0, Qt::UserRole);
    newStation->setData(0, Qt::UserRole, catalogData);
    
    stationsItem->setExpanded(true);
    statusLabel->setText(QString(" 已将设备 %1 添加到配置").arg(finalName));
}

void MasterSimulationWidget::onCatalogSelectionChanged()
{
    QTreeWidgetItem *item = catalogTree->currentItem();
    if (!item || item->childCount() > 0) return; // Vendor or I/O node

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) return;

    int index = data.toInt();
    if (index >= 0 && index < m_cachedDevices.size()) {
        updateDeviceDetail(m_cachedDevices[index]);
    } else {
        // Mock detail for samples
        PNConfigLib::GsdmlInfo mockInfo;
        mockInfo.deviceName = "P-Net multi-module sample app";
        mockInfo.deviceVendor = "RT-Labs";
        mockInfo.vendorId = 0x0493;
        mockInfo.deviceId = 0x0002;
        mockInfo.deviceID = "2";
        mockInfo.filePath = "GSDML\\GSDML-V2.4-RT-Labs-P-Net-Sample-App-20220324.xml";
        
        PNConfigLib::ModuleInfo m1;
        m1.name = "P-Net multi-module sample app";
        m1.id = "16#00000001";
        mockInfo.modules.append(m1);
        mockInfo.modules.append(m1); // Duplicate for design
        
        PNConfigLib::ModuleInfo m2;
        m2.name = "X1";
        m2.id = "16#00008000";
        mockInfo.modules.append(m2);

        updateDeviceDetail(mockInfo);
    }
}

QString MasterSimulationWidget::formatIdent(uint32_t val)
{
    return QString::number(val);
}

void MasterSimulationWidget::updateDeviceDetail(const PNConfigLib::GsdmlInfo &info)
{
    // Clear layout
    QLayoutItem *child;
    while ((child = catalogDetailLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    auto addProp = [&](const QString &label, const QString &value) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(label, this);
        lbl->setFixedWidth(100);
        lbl->setStyleSheet("color: #666;");
        QLabel *val = new QLabel(value, this);
        val->setStyleSheet("color: #0000cc;");
        row->addWidget(lbl);
        row->addWidget(val);
        row->addStretch();
        catalogDetailLayout->addLayout(row);
    };

    QLabel *nameTitle = new QLabel(info.deviceName, this);
    QFont titleFont = nameTitle->font();
    titleFont.setPointSize(16);
    nameTitle->setFont(titleFont);
    catalogDetailLayout->addWidget(nameTitle);
    catalogDetailLayout->addSpacing(10);

    addProp("厂商ID", QString::number(info.vendorId));
    addProp("设备ID", QString::number(info.deviceId));
    addProp("厂商名称", info.deviceVendor);
    addProp("软件发布", "V0.1.0"); // Hardcoded for now
    addProp("主系列", "I/O");
    addProp("产品系列", "P-Net Samples");
    addProp("文件", info.filePath);
    catalogDetailLayout->addSpacing(10);
    addProp("产品符号", "");

    catalogDetailLayout->addSpacing(20);
    
    QLabel *moduleTitle = new QLabel("▼ 模块列表", this);
    moduleTitle->setStyleSheet("font-weight: bold; padding: 5px; background: #eee;");
    catalogDetailLayout->addWidget(moduleTitle);

    for (const auto& module : info.modules) {
        QWidget *modWidget = new QWidget(this);
        QHBoxLayout *modLayout = new QHBoxLayout(modWidget);
        modLayout->setContentsMargins(5, 5, 5, 5);
        
        QLabel *idLbl = new QLabel(module.id.isEmpty() ? "16#00000000" : module.id, this);
        idLbl->setFixedWidth(100);
        
        QVBoxLayout *descLayout = new QVBoxLayout();
        QLabel *nameLbl = new QLabel(module.name, this);
        nameLbl->setStyleSheet("font-weight: bold;");
        QLabel *descLbl = new QLabel("Profinet device sample app", this);
        descLbl->setStyleSheet("color: #666; font-size: 10px;");
        descLayout->addWidget(nameLbl);
        descLayout->addWidget(descLbl);
        
        modLayout->addWidget(idLbl);
        modLayout->addLayout(descLayout);
        modLayout->addStretch();
        
        modWidget->setStyleSheet("border-bottom: 1px solid #eee;");
        catalogDetailLayout->addWidget(modWidget);
    }
    
    catalogDetailLayout->addStretch();
}

void MasterSimulationWidget::onImportGsdml()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "导入 GSDML 文件", "", "GSDML 文件 (*.xml);;所有文件 (*)");
    
    if (fileName.isEmpty()) return;

    QString importedPath = PNConfigLib::DeviceCacheManager::instance().importGSDML(fileName);
    if (!importedPath.isEmpty()) {
        statusLabel->setText(QString(" 已导入 GSDML: %1").arg(QFileInfo(fileName).fileName()));
        refreshCatalog();
    } else {
        QMessageBox::warning(this, "导入错误", "无法导入 GSDML 文件，请检查文件格式。");
    }
}
void MasterSimulationWidget::onScanClicked()
{
    if (!m_scanner->isConnected()) return;

    statusLabel->setText(" 正在扫描网络中的 PROFINET 设备...");
    onlineTree->clear();
    
    m_onlineDevices = m_scanner->scan();
    for (int i = 0; i < m_onlineDevices.size(); ++i) {
        const auto &device = m_onlineDevices[i];
        QTreeWidgetItem *item = new QTreeWidgetItem(onlineTree);
        item->setText(0, device.deviceName);
        item->setText(1, device.ipAddress);
        item->setData(0, Qt::UserRole, i); // Store index
    }
    
    if (m_onlineDevices.isEmpty()) {
        statusLabel->setText(" 未发现 PROFINET 设备");
    } else {
        statusLabel->setText(QString(" 发现 %1 个 PROFINET 设备").arg(m_onlineDevices.size()));
        rightTabWidget->setCurrentIndex(2); // Switch to Online tab
    }
    
    if (flashState) {
        statusLed->setStyleSheet("background-color: #00FF00; border: 2px solid #ccc; border-radius: 10px;"); // Bright green
    } else {
        statusLed->setStyleSheet("background-color: #004D00; border: 2px solid #ccc; border-radius: 10px;"); // Dark green
    }
    flashState = !flashState;
    flashRemaining--;
}

void MasterSimulationWidget::onConnectClicked()
{
    if (!m_isConnected) {
        QString interfaceName = nicComboBox->currentData().toString();
        if (m_scanner->connectToInterface(interfaceName)) {
            m_isConnected = true;
            btnConnect->setChecked(true);
            btnConnect->setIcon(createDisconnectIcon());
            btnScan->setEnabled(true);
            nicComboBox->setEnabled(false);
            statusLabel->setText(QString(" 已连接到: %1").arg(nicComboBox->currentText()));
        } else {
            btnConnect->setChecked(false);
            btnConnect->setIcon(createConnectIcon());
            QMessageBox::critical(this, "连接错误", "无法连接到选定的网卡。");
        }
    } else {
        m_scanner->disconnectFromInterface();
        m_arManager->stop();
        m_isConnected = false;
        m_isArRunning = false;
        btnConnect->setChecked(false);
        btnConnect->setIcon(createConnectIcon());
        btnScan->setEnabled(false);
        btnStart->setEnabled(false);
        btnStart->setIcon(createPlayIcon());
        nicComboBox->setEnabled(true);
        statusLabel->setText(" 已断开连接");

        // Clear online list and details
        onlineTree->clear();
        m_onlineDevices.clear();
        onlinePropGroup->setVisible(false);
        onOnlineTreeSelectionChanged(); // Reset property views
    }
}

void MasterSimulationWidget::onOnlineContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = onlineTree->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *setIpAction = menu.addAction("修改 IP 地址");
    connect(setIpAction, &QAction::triggered, this, &MasterSimulationWidget::onSetIp);
    
    QAction *flashAction = menu.addAction("闪烁 LED");
    connect(flashAction, &QAction::triggered, this, &MasterSimulationWidget::onFlashLed);
    
    menu.exec(onlineTree->mapToGlobal(pos));
}

void MasterSimulationWidget::onSetIp()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;

    int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_onlineDevices.size()) return;

    const auto &device = m_onlineDevices[index];
    QString mac = device.macAddress;
    QString currentIp = device.ipAddress;
    QString currentMask = device.subnetMask;
    QString currentGw = device.gateway;

    bool ok;
    QString newIp = QInputDialog::getText(this, "修改 IP 地址", 
        QString("请输入设备 %1 的新 IP 地址:").arg(mac), QLineEdit::Normal, currentIp, &ok);
    
    if (ok && !newIp.isEmpty()) {
        if (m_scanner->setDeviceIp(mac, newIp, currentMask, currentGw)) {
            statusLabel->setText(QString(" 已发送 IP 修改请求: %1 -> %2").arg(mac, newIp));
            // Re-scan after a delay to allow the device to apply the change
            QTimer::singleShot(3000, this, &MasterSimulationWidget::onScanClicked);
        } else {
            QMessageBox::warning(this, "设置错误", "无法发送 IP 修改请求。");
        }
    }
}

void MasterSimulationWidget::onOnlineTreeSelectionChanged()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (item) {
        int index = item->data(0, Qt::UserRole).toInt();
        if (index >= 0 && index < m_onlineDevices.size()) {
            const auto &d = m_onlineDevices[index];
            onlinePropName->setText(d.deviceName);
            onlinePropDeviceId->setText(QString("16#%1").arg(d.deviceId, 4, 16, QChar('0')).toUpper());
            onlinePropVendorId->setText(QString("16#%1").arg(d.vendorId, 4, 16, QChar('0')).toUpper());
            onlinePropType->setText(d.deviceType);
            onlinePropIp->setText(d.ipAddress);
            onlinePropMask->setText(d.subnetMask);
            onlinePropGw->setText(d.gateway);
            onlinePropMac->setText(d.macAddress.toUpper().remove(':').remove('-'));
            onlinePropRole->setText("设备");
            
            // Auto-populate Setup Tab fields
            editOnlineName->setText(d.deviceName);
            editOnlineIp->setText(d.ipAddress);
            editOnlineMask->setText(d.subnetMask);
            editOnlineGw->setText(d.gateway);

            onlinePropGsdml->setText("P-Net multi-module sample app (GSDML-V2.4)");
            onlinePropGroup->setVisible(true);

            if (m_isConnected) {
                btnStart->setEnabled(true);
            }

            // Reset Flash LED status
            statusLed->setStyleSheet("background-color: #f0f0f0; border: 2px solid #ccc; border-radius: 10px;");
            flashTimer->stop();
        }
    } else {
        onlinePropGroup->setVisible(false);
        btnStart->setEnabled(false);
        onlinePropName->setText("");
        onlinePropDeviceId->setText("");
        onlinePropVendorId->setText("");
        onlinePropType->setText("");
        onlinePropIp->setText("");
        onlinePropMask->setText("");
        onlinePropGw->setText("");
        onlinePropMac->setText("");
        onlinePropRole->setText("");
        onlinePropGsdml->setText("");
    }
}

void MasterSimulationWidget::onGetStationName()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < m_onlineDevices.size()) {
        const auto &device = m_onlineDevices[index];
        editOnlineName->setText(device.deviceName);
        statusLabel->setText(QString(" 已获取站名称: %1").arg(device.deviceName));
    }
}

void MasterSimulationWidget::onSetStationName()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_onlineDevices.size()) return;

    QString mac = m_onlineDevices[index].macAddress;
    QString newName = editOnlineName->text().trimmed();
    bool permanent = chkNamePermanent->isChecked();

    if (newName.isEmpty()) {
        QMessageBox::warning(this, "参数错误", "站名称不能为空。");
        return;
    }

    if (m_scanner->setDeviceName(mac, newName, permanent)) {
        statusLabel->setText(QString(" 修改站名称成功: %1 (%2)")
            .arg(newName, permanent ? "永久" : "临时"));
        QTimer::singleShot(2000, this, &MasterSimulationWidget::onScanClicked);
    } else {
        QMessageBox::warning(this, "设置错误", "无法修改站名称。设备可能不支持或请求超时。");
    }
}

void MasterSimulationWidget::onGetIpConfig()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < m_onlineDevices.size()) {
        const auto &d = m_onlineDevices[index];
        editOnlineIp->setText(d.ipAddress);
        editOnlineMask->setText(d.subnetMask);
        editOnlineGw->setText(d.gateway);
        statusLabel->setText(QString(" 已获取 IP 配置: %1").arg(d.ipAddress));
    }
}

void MasterSimulationWidget::onSetIpConfig()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_onlineDevices.size()) return;

    QString mac = m_onlineDevices[index].macAddress;
    QString ip = editOnlineIp->text();
    QString mask = editOnlineMask->text();
    QString gw = editOnlineGw->text();
    bool permanent = chkIpPermanent->isChecked();

    if (m_scanner->setDeviceIp(mac, ip, mask, gw, permanent)) {
        statusLabel->setText(QString(" 修改 IP 配置成功: %1 (%2)")
            .arg(ip, permanent ? "永久" : "临时"));
        QTimer::singleShot(2000, this, &MasterSimulationWidget::onScanClicked);
    } else {
        QMessageBox::warning(this, "设置错误", "无法修改 IP 配置。设备可能不支持或请求超时。");
    }
}

void MasterSimulationWidget::onResetToFactory()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_onlineDevices.size()) return;

    QString mac = m_onlineDevices[index].macAddress;
    
    if (QMessageBox::question(this, "工厂重置", 
        QString("确定要对设备 %1 进行工厂重置吗？").arg(mac)) != QMessageBox::Yes) 
    {
        return;
    }

    if (m_scanner->resetFactory(mac)) {
        statusLabel->setText(QString(" 已发送工厂重置请求: %1").arg(mac));
        QTimer::singleShot(3000, this, &MasterSimulationWidget::onScanClicked);
    } else {
        QMessageBox::warning(this, "重置错误", "无法发送工厂重置请求。");
    }
}

void MasterSimulationWidget::onFlashLed()
{
    QTreeWidgetItem *item = onlineTree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= m_onlineDevices.size()) return;

    QString mac = m_onlineDevices[index].macAddress;
    
    // Set to yellow (waiting)
    statusLed->setStyleSheet("background-color: yellow; border: 2px solid #ccc; border-radius: 10px;");
    statusLabel->setText(QString(" 正在发送闪烁 LED 请求... (%1)").arg(mac));
    QCoreApplication::processEvents(); // Update UI immediately

    if (m_scanner->flashLed(mac)) {
        statusLabel->setText(QString(" 闪烁 LED 请求成功: %1").arg(mac));
        // Start green flashing animation
        flashRemaining = 10; // 10 toggles (5 seconds at 500ms)
        flashState = true;
        flashTimer->start(500);
        onFlashTimerTick();
    } else {
        statusLabel->setText(QString(" 闪烁 LED 请求失败: %1").arg(mac));
        // Solid red
        statusLed->setStyleSheet("background-color: red; border: 2px solid #ccc; border-radius: 10px;");
        flashTimer->stop();
    }
}

void MasterSimulationWidget::onFlashTimerTick()
{
    if (flashRemaining <= 0) {
        flashTimer->stop();
        statusLed->setStyleSheet("background-color: #008000; border: 2px solid #ccc; border-radius: 10px;"); // Solid green at the end
        return;
    }
    
    if (flashState) {
        statusLed->setStyleSheet("background-color: #00FF00; border: 2px solid #ccc; border-radius: 10px;"); // Bright green
    } else {
        statusLed->setStyleSheet("background-color: #004D00; border: 2px solid #ccc; border-radius: 10px;"); // Dark green
    }
    flashState = !flashState;
    flashRemaining--;
}

void MasterSimulationWidget::onStartCommunication()
{
    if (m_isArRunning) {
        m_arManager->stop();
        m_isArRunning = false;
        btnStart->setIcon(createPlayIcon());
        btnStart->setToolTip("开始 cyclic 数据交换 (AR)");
        return;
    }

    QString mac;
    QString ip;
    QString stationName;
    QString nic = nicComboBox->currentData().toString();

    // Priority 1: Use project configuration if a station is selected
    if (editProjectName && !editProjectName->text().isEmpty()) {
        stationName = editProjectName->text().trimmed();
        ip = editProjectIp->text().trimmed();
        
        qDebug() << "Attempting to start AR based on project config. Station Name:" << stationName << "Target IP:" << ip;
        
        // Resolve MAC from online list by matching Station Name
        bool found = false;
        for (const auto &device : m_onlineDevices) {
            if (device.deviceName == stationName) {
                mac = device.macAddress;
                found = true;
                qDebug() << "Matching online device found! MAC:" << mac;
                break;
            }
        }
        
        if (!found) {
            // Fallback: match by IP
            for (const auto &device : m_onlineDevices) {
                if (device.ipAddress == ip) {
                    mac = device.macAddress;
                    found = true;
                    qDebug() << "Matching online device found by IP! MAC:" << mac;
                    break;
                }
            }
        }
        
        if (!found) {
            QMessageBox::warning(this, "启动错误", 
                QString("无法在网络上找到站名称为 '%1' 或 IP 为 '%2' 的在线设备。请先确保设备已连接并已扫描。")
                .arg(stationName, ip));
            return;
        }
    } else {
        // Priority 2: Fallback to selected online device
        QTreeWidgetItem *item = onlineTree->currentItem();
        if (!item) {
            QMessageBox::warning(this, "启动错误", "请先在 '在线' 选项卡中选择一个设备，或切换到 '从站列表' 进行配置。");
            return;
        }
        mac = onlinePropMac->text();
        ip = onlinePropIp->text();
        qDebug() << "Starting AR based on selected online device. MAC:" << mac << "IP:" << ip;
    }

    if (m_arManager->start(nic, mac, ip)) {
        m_isArRunning = true;
        btnStart->setIcon(createStopIcon());
        btnStart->setToolTip("停止数据交换");
    } else {
        QMessageBox::critical(this, "错误", "无法启动数据交换: " + m_arManager->lastError());
    }
}

void MasterSimulationWidget::onArStateChanged(PNConfigLib::ArState state)
{
    switch (state) {
        case PNConfigLib::ArState::Offline:
            statusLabel->setText(" AR 已停止");
            break;
        case PNConfigLib::ArState::Connecting:
            statusLabel->setText(" AR 正在建立连接 (Phase 1)...");
            break;
        case PNConfigLib::ArState::Parameterizing:
            statusLabel->setText(" AR 正在参数化 (Phase 2)...");
            break;
        case PNConfigLib::ArState::Running:
            statusLabel->setText(" AR 正在运行 (Cyclic Data Exchange)");
            break;
        case PNConfigLib::ArState::Error:
            statusLabel->setText(" AR 错误: " + m_arManager->lastError());
            break;
    }
}

void MasterSimulationWidget::onArLogMessage(const QString &msg)
{
    qDebug() << "[AR Simulation]" << msg;
}
