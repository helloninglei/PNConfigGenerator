/*****************************************************************************/
/*  PNConfigGenerator - PROFINET Configuration Validation                   */
/*****************************************************************************/

#ifndef CONSISTENCYLOGGER_H
#define CONSISTENCYLOGGER_H

#include <QString>
#include <QList>

namespace PNConfigLib {

enum class LogSeverity {
    Info,
    Warning,
    Error
};

enum class ConsistencyType {
    XML,
    XSD,
    PN,
    GSDML
};

struct ConsistencyLog {
    ConsistencyType type;
    LogSeverity severity;
    QString source;
    QString message;
    
    ConsistencyLog() = default;
    ConsistencyLog(ConsistencyType t, LogSeverity s, const QString& src, const QString& msg)
        : type(t), severity(s), source(src), message(msg) {}
};

class ConsistencyLogger {
public:
    static void reset();
    static void log(ConsistencyType type, LogSeverity severity, const QString& source, const QString& message);
    static QList<ConsistencyLog>& logs();
    static bool hasErrors();
    static QString formatLogs();
    
private:
    static QList<ConsistencyLog> s_logs;
};

} // namespace PNConfigLib

#endif // CONSISTENCYLOGGER_H
