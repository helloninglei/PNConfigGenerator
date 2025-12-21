/*****************************************************************************/
/*  PNConfigGenerator - PROFINET 配置生成工具                                */
/*****************************************************************************/

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "../PNConfigLib/ConfigGenerator/ConfigurationBuilder.h"
#include "../PNConfigLib/ConfigGenerator/ListOfNodesBuilder.h"
#include "../PNConfigLib/ConfigReader/ConfigReader.h"
#include "../PNConfigLib/Compiler/Compiler.h"
#include "../PNConfigLib/Consistency/ConsistencyManager.h"
#include <QStatusBar>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , masterSimWidget(nullptr)
{
    ui->setupUi(this);
    
    // 初始化主站模拟页面
    masterSimWidget = new MasterSimulationWidget(this);
    ui->simTabLayout->addWidget(masterSimWidget);
    
    setupUi();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUi()
{
    setWindowTitle("PROFINET 配置生成工具");
    resize(1200, 800);

    // 创建菜单栏
    QMenuBar *menubar = menuBar();
    QMenu *operationMenu = menubar->addMenu("操作");
    QAction *importAction = operationMenu->addAction(qApp->style()->standardIcon(QStyle::SP_FileIcon), "导入GSDML");
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportGsdmlGlobal);

    menubar->addMenu("语言");
    menubar->addMenu("帮助");
    menubar->addMenu("关于");

    // 设置默认输出路径到项目根目录/results/时间戳
    QString appDir = QCoreApplication::applicationDirPath();
    QDir projectDir(appDir);
    projectDir.cdUp();
    if (projectDir.dirName() == "bin") {
        projectDir.cdUp();
    }
    if (projectDir.dirName() == "build") {
        projectDir.cdUp();
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString defaultOutput = projectDir.absolutePath() + "/results/" + timestamp;
    ui->outputPathEdit->setText(defaultOutput);
    
    statusBar()->showMessage("就绪 - 配置设备后点击\"生成配置文件\"");
}

void MainWindow::setupConnections()
{
    connect(ui->browseGsdmlButton, &QPushButton::clicked, this, &MainWindow::onBrowseGsdml);
    connect(ui->browseOutputButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(ui->generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateConfiguration);
}

void MainWindow::onBrowseGsdml()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择 GSDML 文件",
        "",
        "GSDML 文件 (*.xml);;所有文件 (*)"
    );
    
    if (!fileName.isEmpty()) {
        ui->gsdmlPathEdit->setText(fileName);
        statusBar()->showMessage(QString("已选择 GSDML 文件: %1").arg(QFileInfo(fileName).fileName()), 3000);
    }
}

void MainWindow::onBrowseOutput()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择输出目录",
        ui->outputPathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        ui->outputPathEdit->setText(dir);
    }
}

void MainWindow::onGenerateConfiguration()
{
    // 验证输入
    QString masterName = ui->masterNameEdit->text().trimmed();
    QString masterIP = ui->masterIPEdit->text().trimmed();
    QString slaveName = ui->slaveNameEdit->text().trimmed();
    QString slaveIP = ui->slaveIPEdit->text().trimmed();
    QString gsdmlPath = ui->gsdmlPathEdit->text().trimmed();
    QString outputPath = ui->outputPathEdit->text().trimmed();
    
    if (masterName.isEmpty() || masterIP.isEmpty()) {
        QMessageBox::warning(this, "验证错误", "请输入主站设备名称和IP地址。");
        return;
    }
    
    if (slaveName.isEmpty() || slaveIP.isEmpty()) {
        QMessageBox::warning(this, "验证错误", "请输入从站设备名称和IP地址。");
        return;
    }
    
    if (gsdmlPath.isEmpty() || !QFile::exists(gsdmlPath)) {
        QMessageBox::warning(this, "验证错误", "请选择有效的 GSDML 文件。");
        return;
    }
    
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "验证错误", "请选择输出目录。");
        return;
    }
    
    // 创建输出目录
    QDir dir;
    if (!dir.exists(outputPath)) {
        if (!dir.mkpath(outputPath)) {
            QMessageBox::critical(this, "错误", QString("无法创建输出目录: %1").arg(outputPath));
            return;
        }
    }
    
    statusBar()->showMessage("正在生成配置文件...");
    
    // 准备配置文件路径
    QString configPath = QDir(outputPath).filePath("Configuration.xml");
    QString nodesPath = QDir(outputPath).filePath("ListOfNodes.xml");
    QString finalOutputPath = QDir(outputPath).filePath("PROFINET_Driver_Output.xml");
    
    try {
        // 步骤1: 生成 Configuration.xml
        PNConfigLib::ConfigurationBuilder::DeviceConfig masterConfig;
        masterConfig.name = masterName;
        masterConfig.ipAddress = masterIP;
        masterConfig.routerIpAddress = ui->masterRouterEdit->text().trimmed();
        masterConfig.inputStartAddress = 0;
        masterConfig.outputStartAddress = 0;
        
        PNConfigLib::ConfigurationBuilder::DeviceConfig slaveConfig;
        slaveConfig.name = slaveName;
        slaveConfig.ipAddress = slaveIP;
        slaveConfig.routerIpAddress = ui->slaveRouterEdit->text().trimmed();
        slaveConfig.inputStartAddress = ui->inputStartEdit->text().trimmed().toInt();
        slaveConfig.outputStartAddress = ui->outputStartEdit->text().trimmed().toInt();
        
        bool success = PNConfigLib::ConfigurationBuilder::saveConfigurationXml(
            gsdmlPath, masterConfig, slaveConfig, configPath);
        
        if (!success) {
            QMessageBox::critical(this, "错误", "生成 Configuration.xml 失败");
            statusBar()->showMessage("生成失败", 5000);
            return;
        }
        
        // 步骤2: 生成 ListOfNodes.xml
        PNConfigLib::ListOfNodesBuilder::DeviceNode masterNode;
        masterNode.name = masterName;
        masterNode.ipAddress = masterIP;
        masterNode.routerIpAddress = ui->masterRouterEdit->text().trimmed();
        
        PNConfigLib::ListOfNodesBuilder::DeviceNode slaveNode;
        slaveNode.name = slaveName;
        slaveNode.ipAddress = slaveIP;
        slaveNode.routerIpAddress = ui->slaveRouterEdit->text().trimmed();
        
        success = PNConfigLib::ListOfNodesBuilder::saveListOfNodesXml(
            gsdmlPath, masterNode, slaveNode, nodesPath);
        
        if (!success) {
            QMessageBox::critical(this, "错误", "生成 ListOfNodes.xml 失败");
            statusBar()->showMessage("生成失败", 5000);
            return;
        }
        
        // 步骤3: 验证生成的配置文件
        auto config = PNConfigLib::ConfigReader::parseConfiguration(configPath);
        auto nodes = PNConfigLib::ConfigReader::parseListOfNodes(nodesPath);
        
        PNConfigLib::ConsistencyManager validator;
        if (!validator.validateInputs(config, nodes)) {
            QString errors = validator.getFormattedMessages();
            QMessageBox::critical(this, "配置验证失败",
                QString("生成的配置文件验证失败:\n\n%1").arg(errors));
            statusBar()->showMessage("验证失败", 5000);
            return;
        }
        
        // 步骤4: 编译生成最终输出
        success = PNConfigLib::Compiler::compile(config, nodes, finalOutputPath);
        
        if (!success) {
            QMessageBox::critical(this, "编译错误", "生成最终输出文件失败。");
            statusBar()->showMessage("编译失败", 5000);
            return;
        }
        
        // 成功!
        statusBar()->showMessage("配置文件生成成功!", 5000);
        
        QMessageBox::information(this, "成功",
            QString("配置文件生成成功!\n\n"
               "输出目录:\n%1\n\n"
               "生成的文件:\n"
               "• Configuration.xml\n"
               "• ListOfNodes.xml\n"
               "• PROFINET_Driver_Output.xml").arg(outputPath));
        
        // 在资源管理器中打开文件夹
        #ifdef Q_OS_WIN
        QString explorer = "explorer.exe";
        QString param = "/select," + QDir::toNativeSeparators(finalOutputPath);
        QProcess::startDetached(explorer, QStringList() << param);
        #endif
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("生成过程中出错:\n%1").arg(e.what()));
        statusBar()->showMessage("生成失败", 5000);
    }
}

void MainWindow::onImportGsdmlGlobal()
{
    if (masterSimWidget) {
        // Since we didn't make onImportGsdml public, let's just implement the logic or trigger refresh
        // For simplicity in this step, let's assume we want to trigger the same logic.
        // We can use QMetaObject::invokeMethod if we want to be safe with private slots.
        QMetaObject::invokeMethod(masterSimWidget, "onImportGsdml");
    }
}
