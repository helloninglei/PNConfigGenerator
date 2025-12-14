/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Device Configuration Tool                  */
/*****************************************************************************/

#ifndef QUICKSETUPWIZARD_H
#define QUICKSETUPWIZARD_H

#include <QWizard>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>

/**
 * @brief Quick setup wizard for generating configuration from a single GSDML file
 * 
 * This wizard provides a simplified interface similar to PNconfigFileGenerator CLI,
 * allowing users to quickly create a basic configuration with one master and one slave device.
 */
class QuickSetupWizard : public QWizard
{
    Q_OBJECT

public:
    explicit QuickSetupWizard(QWidget *parent = nullptr);

    QString gsdmlPath() const { return field("gsdmlPath").toString(); }
    QString masterName() const { return field("masterName").toString(); }
    QString masterIP() const { return field("masterIP").toString(); }
    QString masterRouterIP() const { return field("masterRouterIP").toString(); }
    QString slaveName() const { return field("slaveName").toString(); }
    QString slaveIP() const { return field("slaveIP").toString(); }
    QString slaveRouterIP() const { return field("slaveRouterIP").toString(); }
    QString inputStartAddress() const { return field("inputStartAddress").toString(); }
    QString outputStartAddress() const { return field("outputStartAddress").toString(); }
    QString outputPath() const { return field("outputPath").toString(); }
    
    /**
     * @brief Generate configuration files from wizard parameters
     * @return true if successful
     */
    bool generateConfiguration();

private:
    // No member variables needed - using wizard fields
};

// Wizard page classes
class GSDMLSelectionPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit GSDMLSelectionPage(QWidget *parent = nullptr);
    bool isComplete() const override;

private slots:
    void onBrowse();

private:
    QLineEdit *m_pathEdit;
    QLabel *m_infoLabel;
};

class MasterConfigPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit MasterConfigPage(QWidget *parent = nullptr);
    bool isComplete() const override;

private:
    QLineEdit *m_nameEdit;
    QLineEdit *m_ipEdit;
    QLineEdit *m_routerEdit;
};

class SlaveConfigPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SlaveConfigPage(QWidget *parent = nullptr);
    bool isComplete() const override;

private:
    QLineEdit *m_nameEdit;
    QLineEdit *m_ipEdit;
    QLineEdit *m_routerEdit;
};

class IOAddressConfigPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit IOAddressConfigPage(QWidget *parent = nullptr);

private:
    QLineEdit *m_inputStartEdit;
    QLineEdit *m_outputStartEdit;
    QLabel *m_previewLabel;
};

class SummaryPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SummaryPage(QWidget *parent = nullptr);
    void initializePage() override;

private slots:
    void onBrowseOutput();

private:
    QLabel *m_summaryLabel;
    QLineEdit *m_outputPathEdit;
};

#endif // QUICKSETUPWIZARD_H
