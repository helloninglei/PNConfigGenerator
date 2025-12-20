#ifndef XMLENTITIES_H
#define XMLENTITIES_H

#include <QString>
#include <QVariant>
#include <QList>
#include <QHash>
namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

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
    int scalarBlobLength = -1; // Optional length for Scalar BLOB type
    
    XmlVariable() : aid(0), valueType(XmlValueType::Scalar), dataType(XmlDataType::STRING) {}
};

struct XmlObject {
    QString name;
    uint32_t classRid;
    uint32_t rid = 0; // Optional usually
    QString gsdmlFile; // Optional GSDML file reference for devices
    QList<XmlVariable> variables;
    QList<XmlObject> children;
    
    // Helper to add variables
    void addScalar(const QString& name, uint32_t aid, XmlDataType type, const QVariant& val);
    void addBlobVariable(const QString& name, uint32_t aid, const QList<XmlField>& fields);
};

class XmlEntitySerializer {
public:
    static tinyxml2::XMLElement* serializeObject(tinyxml2::XMLDocument* doc, const XmlObject& obj);
private:
    static tinyxml2::XMLElement* serializeVariable(tinyxml2::XMLDocument* doc, const XmlVariable& var);
    static QString dataTypeToString(XmlDataType type);
    static QString valueTypeToString(XmlValueType type);
};

} // namespace PNConfigLib

#endif // XMLENTITIES_H
