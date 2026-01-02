/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#include "ConsistencyLogger.h"

#include <QDebug>

namespace PNConfigLib {

QList<ConsistencyLog> ConsistencyLogger::s_logs;

void ConsistencyLogger::reset()
{
    s_logs.clear();
}

void ConsistencyLogger::log(ConsistencyType type, LogSeverity severity, const QString& source, const QString& message)
{
    s_logs.append(ConsistencyLog(type, severity, source, message));
    
    // Log to qDebug for file redirection
    QString sev;
    switch(severity) {
        case LogSeverity::Info: sev = "INFO"; break;
        case LogSeverity::Warning: sev = "WARN"; break;
        case LogSeverity::Error: sev = "ERROR"; break;
    }
    qDebug() << QString("[CONSISTENCY %1] %2: %3").arg(sev, source, message);
}


QList<ConsistencyLog>& ConsistencyLogger::logs()
{
    return s_logs;
}

bool ConsistencyLogger::hasErrors()
{
    for (const auto& log : s_logs) {
        if (log.severity == LogSeverity::Error) {
            return true;
        }
    }
    return false;
}

QString ConsistencyLogger::formatLogs()
{
    QString result;
    for (const auto& log : s_logs) {
        QString severityStr;
        switch (log.severity) {
            case LogSeverity::Error: severityStr = "错误"; break;
            case LogSeverity::Warning: severityStr = "警告"; break;
            case LogSeverity::Info: severityStr = "信息"; break;
        }
        
        QString typeStr;
        switch (log.type) {
            case ConsistencyType::XML: typeStr = "XML"; break;
            case ConsistencyType::XSD: typeStr = "XSD"; break;
            case ConsistencyType::PN: typeStr = "PN"; break;
            case ConsistencyType::GSDML: typeStr = "GSDML"; break;
        }
        
        result += QString("[%1][%2] %3: %4\n").arg(typeStr, severityStr, log.source, log.message);
    }
    return result;
}

} // namespace PNConfigLib
