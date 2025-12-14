/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#include "QuickSetupWizard.h"
#include "../../PNConfigLib/ConfigGenerator/ConfigurationBuilder.h"
#include "../../PNConfigLib/ConfigGenerator/ListOfNodesBuilder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDir>

// QuickSetupWizard implementation
QuickSetupWizard::QuickSetupWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Quick Setup Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, false);
    
    addPage(new GSDMLSelectionPage(this));
    addPage(new MasterConfigPage(this));
    addPage(new SlaveConfigPage(this));
    addPage(new IOAddressConfigPage(this));
    addPage(new SummaryPage(this));
    
    resize(600, 500);
}

bool QuickSetupWizard::generateConfiguration()
{
    // Get all parameters from wizard fields
    QString gsdmlPath = field("gsdmlPath").toString();
    QString masterName = field("masterName").toString();
    QString masterIP = field("masterIP").toString();
    QString masterRouterIP = field("masterRouterIP").toString();
    QString slaveName = field("slaveName").toString();
    QString slaveIP = field("slaveIP").toString();
    QString slaveRouterIP = field("slaveRouterIP").toString();
    QString inputStart = field("inputStartAddress").toString();
    QString outputStart = field("outputStartAddress").toString();
    QString outputPath = field("outputPath").toString();
    
    // Parse addresses
    bool ok;
    int inputStartAddr = inputStart.toInt(&ok);
    if (!ok) inputStartAddr = 0;
    
    int outputStartAddr = outputStart.toInt(&ok);
    if (!ok) outputStartAddr = 0;
    
    // Create output directory if it doesn't exist
    QDir dir;
    if (!dir.exists(outputPath)) {
        if (!dir.mkpath(outputPath)) {
            QMessageBox::critical(this, tr("Error"), 
                tr("Failed to create output directory: %1").arg(outputPath));
            return false;
        }
    }
    
    // Build configuration paths
    QString configPath = QDir(outputPath).filePath("Configuration.xml");
    QString nodesPath = QDir(outputPath).filePath("ListOfNodes.xml");
    
    try {
        // Create master config
        PNConfigLib::ConfigurationBuilder::DeviceConfig masterConfig;
        masterConfig.name = masterName;
        masterConfig.ipAddress = masterIP;
        masterConfig.routerIpAddress = masterRouterIP;
        masterConfig.inputStartAddress = inputStartAddr;
        masterConfig.outputStartAddress = outputStartAddr;
        
        // Create slave config
        PNConfigLib::ConfigurationBuilder::DeviceConfig slaveConfig;
        slaveConfig.name = slaveName;
        slaveConfig.ipAddress = slaveIP;
        slaveConfig.routerIpAddress = slaveRouterIP;
        slaveConfig.inputStartAddress = inputStartAddr;
        slaveConfig.outputStartAddress = outputStartAddr;
        
        // Generate Configuration.xml
        bool success = PNConfigLib::ConfigurationBuilder::saveConfigurationXml(
            gsdmlPath, masterConfig, slaveConfig, configPath);
        
        if (!success) {
            QMessageBox::critical(this, tr("Error"), 
                tr("Failed to generate Configuration.xml"));
            return false;
        }
        
        // Generate ListOfNodes.xml
        PNConfigLib::ListOfNodesBuilder::DeviceNode masterNode;
        masterNode.name = masterName;
        masterNode.ipAddress = masterIP;
        masterNode.routerIpAddress = masterRouterIP;
        
        PNConfigLib::ListOfNodesBuilder::DeviceNode slaveNode;
        slaveNode.name = slaveName;
        slaveNode.ipAddress = slaveIP;
        slaveNode.routerIpAddress = slaveRouterIP;
        
        success = PNConfigLib::ListOfNodesBuilder::saveListOfNodesXml(
            gsdmlPath, masterNode, slaveNode, nodesPath);
        
        if (!success) {
            QMessageBox::critical(this, tr("Error"), 
                tr("Failed to generate ListOfNodes.xml"));
            return false;
        }
        
        // Success message
        QMessageBox::information(this, tr("Success"),
            tr("Configuration files generated successfully!\n\n"
               "Output directory: %1\n"
               "- Configuration.xml\n"
               "- ListOfNodes.xml").arg(outputPath));
        
        return true;
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to generate configuration:\n%1").arg(e.what()));
        return false;
    } catch (...) {
        QMessageBox::critical(this, tr("Error"),
            tr("Unknown error occurred during configuration generation"));
        return false;
    }
}

// GSDMLSelectionPage implementation
GSDMLSelectionPage::GSDMLSelectionPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select GSDML File"));
    setSubTitle(tr("Choose the GSDML file for the device you want to configure."));
    
    QVBoxLayout *layout = new QVBoxLayout;
    
    QHBoxLayout *pathLayout = new QHBoxLayout;
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText(tr("Path to GSDML file..."));
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    
    m_infoLabel = new QLabel(tr("No file selected"));
    m_infoLabel->setWordWrap(true);
    
    layout->addLayout(pathLayout);
    layout->addWidget(m_infoLabel);
    layout->addStretch();
    
    setLayout(layout);
    
    registerField("gsdmlPath*", m_pathEdit);
    
    connect(browseBtn, &QPushButton::clicked, this, &GSDMLSelectionPage::onBrowse);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &GSDMLSelectionPage::completeChanged);
}

bool GSDMLSelectionPage::isComplete() const
{
    return !m_pathEdit->text().isEmpty() && QFile::exists(m_pathEdit->text());
}

void GSDMLSelectionPage::onBrowse()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Select GSDML File"),
        "",
        tr("GSDML Files (*.xml);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        m_pathEdit->setText(fileName);
        m_infoLabel->setText(tr("Selected: %1").arg(QFileInfo(fileName).fileName()));
        
        // Try to parse GSDML and show device info
        try {
            auto gsdmlInfo = PNConfigLib::GsdmlParser::parseGSDML(fileName);
            QString info = tr("<b>Device Information:</b><br>"
                            "Name: %1<br>"
                            "Vendor: %2<br>"
                            "Modules: %3")
                          .arg(gsdmlInfo.deviceName)
                          .arg(gsdmlInfo.deviceVendor)
                          .arg(gsdmlInfo.modules.size());
            m_infoLabel->setText(info);
        } catch (...) {
            m_infoLabel->setText(tr("Selected: %1<br><i>Could not parse device info</i>")
                                .arg(QFileInfo(fileName).fileName()));
        }
    }
}

// Master ConfigPage implementation
MasterConfigPage::MasterConfigPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Master Device Configuration"));
    setSubTitle(tr("Configure the PROFINET controller (master device)."));
    
    QFormLayout *layout = new QFormLayout;
    
    m_nameEdit = new QLineEdit;
    m_nameEdit->setText("PN_Driver");
    m_nameEdit->setPlaceholderText(tr("Master device name"));
    
    m_ipEdit = new QLineEdit;
    m_ipEdit->setText("192.168.0.1");
    m_ipEdit->setPlaceholderText(tr("IP address"));
    
    m_routerEdit = new QLineEdit;
    m_routerEdit->setText("0.0.0.0");
    m_routerEdit->setPlaceholderText(tr("Router IP (optional)"));
    
    layout->addRow(tr("Device Name:"), m_nameEdit);
    layout->addRow(tr("IP Address:"), m_ipEdit);
    layout->addRow(tr("Router IP:"), m_routerEdit);
    
    setLayout(layout);
    
    registerField("masterName*", m_nameEdit);
    registerField("masterIP*", m_ipEdit);
    registerField("masterRouterIP", m_routerEdit);
    
    // Connect text changes to emit completeChanged signal
    connect(m_nameEdit, &QLineEdit::textChanged, this, &MasterConfigPage::completeChanged);
    connect(m_ipEdit, &QLineEdit::textChanged, this, &MasterConfigPage::completeChanged);
}

bool MasterConfigPage::isComplete() const
{
    return !m_nameEdit->text().trimmed().isEmpty() && 
           !m_ipEdit->text().trimmed().isEmpty();
}

// SlaveConfigPage implementation
SlaveConfigPage::SlaveConfigPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Slave Device Configuration"));
    setSubTitle(tr("Configure the PROFINET IO device (slave)."));
    
    QFormLayout *layout = new QFormLayout;
    
    m_nameEdit = new QLineEdit;
    m_nameEdit->setText("IO_Device");
    m_nameEdit->setPlaceholderText(tr("Slave device name"));
    
    m_ipEdit = new QLineEdit;
    m_ipEdit->setText("192.168.0.2");
    m_ipEdit->setPlaceholderText(tr("IP address"));
    
    m_routerEdit = new QLineEdit;
    m_routerEdit->setPlaceholderText(tr("Router IP (optional)"));
    
    layout->addRow(tr("Device Name:"), m_nameEdit);
    layout->addRow(tr("IP Address:"), m_ipEdit);
    layout->addRow(tr("Router IP:"), m_routerEdit);
    
    setLayout(layout);
    
    registerField("slaveName*", m_nameEdit);
    registerField("slaveIP*", m_ipEdit);
    registerField("slaveRouterIP", m_routerEdit);
    
    // Connect text changes to emit completeChanged signal
    connect(m_nameEdit, &QLineEdit::textChanged, this, &SlaveConfigPage::completeChanged);
    connect(m_ipEdit, &QLineEdit::textChanged, this, &SlaveConfigPage::completeChanged);
}

bool SlaveConfigPage::isComplete() const
{
    return !m_nameEdit->text().trimmed().isEmpty() && 
           !m_ipEdit->text().trimmed().isEmpty();
}

// IOAddressConfigPage implementation
IOAddressConfigPage::IOAddressConfigPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("IO Address Configuration"));
    setSubTitle(tr("Configure the starting addresses for input and output data."));
    
    QFormLayout *layout = new QFormLayout;
    
    m_inputStartEdit = new QLineEdit;
    m_inputStartEdit->setText("0");
    m_inputStartEdit->setPlaceholderText(tr("Input start address (decimal or 0x hex)"));
    
    m_outputStartEdit = new QLineEdit;
    m_outputStartEdit->setText("0");
    m_outputStartEdit->setPlaceholderText(tr("Output start address (decimal or 0x hex)"));
    
    m_previewLabel = new QLabel(tr("Address allocation will be calculated automatically\nbased on module IO data lengths."));
    m_previewLabel->setWordWrap(true);
    
    layout->addRow(tr("Input Start:"), m_inputStartEdit);
    layout->addRow(tr("Output Start:"), m_outputStartEdit);
    layout->addRow(tr("Preview:"), m_previewLabel);
    
    setLayout(layout);
    
    registerField("inputStartAddress", m_inputStartEdit);
    registerField("outputStartAddress", m_outputStartEdit);
}

// SummaryPage implementation
SummaryPage::SummaryPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Summary"));
    setSubTitle(tr("Review your configuration and select output location."));
    
    QVBoxLayout *layout = new QVBoxLayout;
    
    m_summaryLabel = new QLabel;
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextFormat(Qt::RichText);
    
    QHBoxLayout *outputLayout = new QHBoxLayout;
    QLabel *outputLabel = new QLabel(tr("Output Directory:"));
    m_outputPathEdit = new QLineEdit;
    m_outputPathEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(m_outputPathEdit);
    outputLayout->addWidget(browseBtn);
    
    layout->addWidget(m_summaryLabel);
    layout->addLayout(outputLayout);
    layout->addStretch();
    
    setLayout(layout);
    
    registerField("outputPath", m_outputPathEdit);
    
    connect(browseBtn, &QPushButton::clicked, this, &SummaryPage::onBrowseOutput);
}

void SummaryPage::initializePage()
{
    QString summary = tr("<h3>Configuration Summary</h3>");
    summary += tr("<p><b>GSDML File:</b><br>%1</p>").arg(field("gsdmlPath").toString());
    summary += tr("<p><b>Master Device:</b><br>");
    summary += tr("Name: %1<br>").arg(field("masterName").toString());
    summary += tr("IP: %1</p>").arg(field("masterIP").toString());
    summary += tr("<p><b>Slave Device:</b><br>");
    summary += tr("Name: %1<br>").arg(field("slaveName").toString());
    summary += tr("IP: %1</p>").arg(field("slaveIP").toString());
    summary += tr("<p><b>IO Addresses:</b><br>");
    summary += tr("Input Start: %1<br>").arg(field("inputStartAddress").toString());
    summary += tr("Output Start: %1</p>").arg(field("outputStartAddress").toString());
    
    m_summaryLabel->setText(summary);
}

void SummaryPage::onBrowseOutput()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        m_outputPathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        m_outputPathEdit->setText(dir);
    }
}
