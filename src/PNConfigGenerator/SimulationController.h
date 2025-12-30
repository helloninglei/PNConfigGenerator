#ifndef SIMULATIONCONTROLLER_H
#define SIMULATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include "../PNConfigLib/Network/DcpScanner.h"
#include "../PNConfigLib/Network/ArExchangeManager.h"

class SimulationController : public QObject
{
    Q_OBJECT
public:
    explicit SimulationController(QObject *parent = nullptr);
    void start();

private slots:
    void onArStateChanged(PNConfigLib::ArState state);

private:
    PNConfigLib::DcpScanner *m_scanner;
    PNConfigLib::ArExchangeManager *m_arManager;
    QString m_nicName;
};

#endif // SIMULATIONCONTROLLER_H
