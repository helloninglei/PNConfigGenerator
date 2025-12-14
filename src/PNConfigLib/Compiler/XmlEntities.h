#ifndef XMLENTITIES_H
#define XMLENTITIES_H

#include <QString>
#include <QVariant>
#include <QList>
#include <QHash>
#include <QDomDocument>
#include <QDomElement>

namespace PNConfigLib {

enum class XmlValueType {
    Scalar,
    SparseArray
};

enum class XmlDataType {
    UINT16,
    UINT32,
    BLOB,
    BOOL,
    STRING
};

struct XmlField {
    uint32_t key;
    uint32_t length;
    QByteArray value;
};

struct XmlVariable {
    QString name;
    uint32_t aid;
    XmlValueType valueType;
    XmlDataType dataType;
    QVariant value; // Used if Scalar
    QList<XmlField> fields; // Used if SparseArray
    
    XmlVariable() : aid(0), valueType(XmlValueType::Scalar), dataType(XmlDataType::STRING) {}
};

struct XmlObject {
    QString name;
    uint32_t classRid;
    uint32_t rid = 0; // Optional usually
    QList<XmlVariable> variables;
    QList<XmlObject> children;
    
    // Helper to add variables
    void addScalar(const QString& name, uint32_t aid, XmlDataType type, const QVariant& val);
    void addBlobVariable(const QString& name, uint32_t aid, const QList<XmlField>& fields);
};

class XmlEntitySerializer {
public:
    static QDomElement serializeObject(QDomDocument& doc, const XmlObject& obj);
private:
    static QDomElement serializeVariable(QDomDocument& doc, const XmlVariable& var);
    static QString dataTypeToString(XmlDataType type);
    static QString valueTypeToString(XmlValueType type);
};

} // namespace PNConfigLib

#endif // XMLENTITIES_H
