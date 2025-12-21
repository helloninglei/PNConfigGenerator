#include "MasterSimulationWidget.h"
#include <QHeaderView>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>

MasterSimulationWidget::MasterSimulationWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
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
    QTreeWidgetItem *logicItem = new QTreeWidgetItem(projectItem, QStringList() << "Logic");
    QTreeWidgetItem *stationsItem = new QTreeWidgetItem(projectItem, QStringList() << "从站列表");
    
    QTreeWidgetItem *devItem = new QTreeWidgetItem(stationsItem, QStringList() << "rt-labs-dev [Discovery]");
    devItem->setSelected(true);
    
    projectTree->expandAll();
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
    
    QTreeWidget *catalogTree = new QTreeWidget(this);
    catalogTree->setHeaderLabel("设备列表");
    QTreeWidgetItem *ioItem = new QTreeWidgetItem(catalogTree, QStringList() << "I/O");
    new QTreeWidgetItem(ioItem, QStringList() << "P-Net Samples");
    catalogTree->expandAll();
    
    rightTabWidget->addTab(catalogTree, "设备列表");
    rightTabWidget->addTab(new QWidget(), "子模块列表");
    rightTabWidget->addTab(new QWidget(), "在线");
    
    splitter->addWidget(rightTabWidget);
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
    // Find the Catalog Tree in the right tab widget
    QTreeWidget *catalogTree = nullptr;
    for (int i = 0; i < rightTabWidget->count(); ++i) {
        if (rightTabWidget->tabText(i) == "设备列表") {
            catalogTree = qobject_cast<QTreeWidget*>(rightTabWidget->widget(i));
            break;
        }
    }

    if (!catalogTree) return;

    catalogTree->clear();
    QTreeWidgetItem *ioItem = new QTreeWidgetItem(catalogTree, QStringList() << "I/O");
    
    // Load from DeviceCacheManager
    auto cachedDevices = PNConfigLib::DeviceCacheManager::instance().getCachedDevices();
    for (const auto& device : cachedDevices) {
        QString vendorName = device.deviceVendor;
        QTreeWidgetItem *vendorItem = nullptr;
        
        // Find existing vendor item
        for (int i = 0; i < ioItem->childCount(); ++i) {
            if (ioItem->child(i)->text(0) == vendorName) {
                vendorItem = ioItem->child(i);
                break;
            }
        }
        
        if (!vendorItem) {
            vendorItem = new QTreeWidgetItem(ioItem, QStringList() << vendorName);
        }
        
        new QTreeWidgetItem(vendorItem, QStringList() << device.deviceName);
    }
    
    // Add default mock items if empty or as extra
    if (cachedDevices.isEmpty()) {
        QTreeWidgetItem *mockVendor = new QTreeWidgetItem(ioItem, QStringList() << "P-Net Samples");
        new QTreeWidgetItem(mockVendor, QStringList() << "P-Net multi-module sample app");
    }
    
    catalogTree->expandAll();
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
