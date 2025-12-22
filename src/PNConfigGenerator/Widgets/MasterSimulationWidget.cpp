#include "MasterSimulationWidget.h"
#include <QHeaderView>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QMenu>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>

MasterSimulationWidget::MasterSimulationWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshCatalog();
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
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 1);
    
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

    // Mock Icons using Standard Icons
    QAction *scanAction = toolbar->addAction(qApp->style()->standardIcon(QStyle::SP_BrowserReload), "搜索设备");
    connect(scanAction, &QAction::triggered, this, &MasterSimulationWidget::onScanClicked);
    
    toolbar->addSeparator();
    
    toolbar->addWidget(new QLabel(" 网卡: "));
    nicComboBox = new QComboBox(this);
    nicComboBox->setMinimumWidth(250);
    
    // Dynamically populate available network interfaces
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        // Skip loopback and inactive interfaces for a cleaner list
        if (interface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
            
        QString displayName = interface.humanReadableName();
        if (displayName.isEmpty()) displayName = interface.name();
        
        nicComboBox->addItem(displayName, interface.name());
    }
    
    // If no real interfaces found, add mock ones as seen in the design
    if (nicComboBox->count() == 0) {
        nicComboBox->addItem("WLAN");
        nicComboBox->addItem("本地连接* 10");
        nicComboBox->addItem("本地连接* 9");
        nicComboBox->addItem("VirtualBox Host-Only Network");
        nicComboBox->addItem("Meta");
    }
    
    toolbar->addWidget(nicComboBox);
    
    toolbar->addSeparator();

    QAction *connectAction = toolbar->addAction(qApp->style()->standardIcon(QStyle::SP_DialogOkButton), "建立连接");
    connect(connectAction, &QAction::triggered, this, &MasterSimulationWidget::onConnectClicked);
    
    QAction *stopAction = toolbar->addAction(qApp->style()->standardIcon(QStyle::SP_DialogCancelButton), "停止");
    
    toolbar->addSeparator();

    QAction *importAction = toolbar->addAction(qApp->style()->standardIcon(QStyle::SP_FileIcon), "导入GSDML");
    connect(importAction, &QAction::triggered, this, &MasterSimulationWidget::onImportGsdml);
    
    toolbar->addSeparator();
    
    toolbar->addAction(qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation), "属性");
}

void MasterSimulationWidget::createLeftPanel(QSplitter *splitter)
{
    projectTree = new QTreeWidget(this);
    projectTree->setHeaderLabel("工程");
    
    QTreeWidgetItem *projectItem = new QTreeWidgetItem(projectTree, QStringList() << "Project");
    new QTreeWidgetItem(projectItem, QStringList() << "Logic");
    stationsItem = new QTreeWidgetItem(projectItem, QStringList() << "从站列表");
    
    QTreeWidgetItem *devItem = new QTreeWidgetItem(stationsItem, QStringList() << "rt-labs-dev [Discovery]");
    devItem->setSelected(true);
    
    projectTree->expandAll();
    
    projectTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(projectTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onProjectTreeContextMenu);
    connect(projectTree, &QTreeWidget::itemDoubleClicked, this, &MasterSimulationWidget::onProjectTreeDoubleClicked);

    splitter->addWidget(projectTree);
}

void MasterSimulationWidget::createCenterPanel(QSplitter *splitter)
{
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    centerWidget = new QWidget();
    slotLayout = new QVBoxLayout(centerWidget);
    slotLayout->setAlignment(Qt::AlignTop);
    
    addSlot(slotLayout, "Slot(0x0000)", "P-Net multi-module sample app", {"Subslot(0x0001) P-Net multi-module sample app", "Subslot(0x8000) X1", "Subslot(0x8001) X1 P1"});
    addSlot(slotLayout, "Slot(0x0001)", "DO 8xLogicLevel", {"Subslot(0x0001) DO 8xLogicLevel"});
    addSlot(slotLayout, "Slot(0x0002)", "Empty Slot");
    addSlot(slotLayout, "Slot(0x0003)", "Empty Slot");
    addSlot(slotLayout, "Slot(0x0004)", "Empty Slot");
    
    scrollArea->setWidget(centerWidget);
    splitter->addWidget(scrollArea);
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
    
    rightTabWidget->addTab(catalogTab, "设备列表");
    rightTabWidget->addTab(new QWidget(), "子模块列表");
    rightTabWidget->addTab(new QWidget(), "在线");
    
    splitter->addWidget(rightTabWidget);

    connect(catalogTree, &QTreeWidget::itemSelectionChanged, this, &MasterSimulationWidget::onCatalogSelectionChanged);
    
    catalogTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(catalogTree, &QTreeWidget::customContextMenuRequested, this, &MasterSimulationWidget::onCatalogContextMenu);
}

void MasterSimulationWidget::addSlot(QVBoxLayout *layout, const QString &slotName, const QString &description, const QStringList &subslots)
{
    QFrame *slotFrame = new QFrame(this);
    slotFrame->setFrameShape(QFrame::StyledPanel);
    slotFrame->setStyleSheet("background-color: #f7f7f7; margin-bottom: 5px;");
    
    QVBoxLayout *vbox = new QVBoxLayout(slotFrame);
    vbox->setContentsMargins(5, 5, 5, 5);
    
    QLabel *header = new QLabel(QString("<b>%1</b> - %2").arg(slotName, description), this);
    header->setStyleSheet("background-color: #e0e0e0; padding: 3px;");
    vbox->addWidget(header);
    
    for (const QString &subslot : subslots) {
        QLabel *subLabel = new QLabel(subslot, this);
        subLabel->setStyleSheet("padding-left: 15px; background-color: white; border: 1px solid #eee;");
        vbox->addWidget(subLabel);
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

void MasterSimulationWidget::onProjectTreeDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item || !stationsItem || item->parent() != stationsItem) return;

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) return;

    int index = data.toInt();
    if (index >= 0 && index < m_cachedDevices.size()) {
        displayDeviceSlots(m_cachedDevices[index]);
    } else {
        // Mock slots for samples
        PNConfigLib::GsdmlInfo mockInfo;
        mockInfo.deviceName = "P-Net multi-module sample app";
        
        PNConfigLib::ModuleInfo m1;
        m1.name = "DO 8xLogicLevel";
        mockInfo.modules.append(m1);
        
        displayDeviceSlots(mockInfo);
    }
}

void MasterSimulationWidget::displayDeviceSlots(const PNConfigLib::GsdmlInfo &info)
{
    // Clear current slots
    while (QLayoutItem* layoutItem = slotLayout->takeAt(0)) {
        if (layoutItem->widget()) delete layoutItem->widget();
        delete layoutItem;
    }

    // Add DAP Slot (Slot 0)
    QStringList dapSubslots = {"Subslot(0x0001) " + info.deviceName, "Subslot(0x8000) Interface", "Subslot(0x8001) Port - RJ 45"};
    addSlot(slotLayout, "Slot(0x0000)", info.deviceName, dapSubslots);

    // Add Modules (Starting from Slot 1)
    for (int i = 0; i < info.modules.size(); ++i) {
        const auto& mod = info.modules[i];
        QString slotName = QString("Slot(0x%1)").arg(i + 1, 4, 16, QChar('0'));
        QStringList subslots = { QString("Subslot(0x0001) ") + mod.name };
        addSlot(slotLayout, slotName, mod.name, subslots);
    }

    // Fill some empty slots for visuals per design
    for (int i = info.modules.size() + 1; i <= 8; ++i) {
        QString slotName = QString("Slot(0x%1)").arg(i, 4, 16, QChar('0'));
        addSlot(slotLayout, slotName, "Empty Slot");
    }
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
    statusLabel->setText(" 正在扫描网络中的 PROFINET 设备...");
}

void MasterSimulationWidget::onConnectClicked()
{
    statusLabel->setText(" 正在与 rt-labs-dev 建立连接...");
}
