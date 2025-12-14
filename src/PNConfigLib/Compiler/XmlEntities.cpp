#include "XmlEntities.h"

namespace PNConfigLib {

void XmlObject::addScalar(const QString& name, uint32_t aid, XmlDataType type, const QVariant& val)
{
    XmlVariable var;
    var.name = name;
    var.aid = aid;
    var.valueType = XmlValueType::Scalar;
    var.dataType = type;
    var.value = val;
    variables.append(var);
}

void XmlObject::addBlobVariable(const QString& name, uint32_t aid, const QList<XmlField>& fields)
{
    XmlVariable var;
    var.name = name;
    var.aid = aid;
    var.valueType = XmlValueType::SparseArray;
    var.dataType = XmlDataType::BLOB;
    var.fields = fields;
    variables.append(var);
}

QDomElement XmlEntitySerializer::serializeVariable(QDomDocument& doc, const XmlVariable& var)
{
    // Special handling for Key
    if (var.name == "Key") {
        QDomElement keyElem = doc.createElement("Key");
        keyElem.setAttribute("AID", var.aid);
        keyElem.appendChild(doc.createTextNode(var.value.toString()));
        return keyElem;
    }
    
    // Standard Variable
    QDomElement elem = doc.createElement("Variable");
    elem.setAttribute("Name", var.name);
    
    // AID
    QDomElement aidElem = doc.createElement("AID");
    aidElem.appendChild(doc.createTextNode(QString::number(var.aid)));
    elem.appendChild(aidElem);
    
    // Value
    QDomElement valElem = doc.createElement("Value");
    valElem.setAttribute("Datatype", valueTypeToString(var.valueType));
    valElem.setAttribute("Valuetype", dataTypeToString(var.dataType));
    
    if (var.valueType == XmlValueType::Scalar) {
        QString strVal = var.value.toString();
        if (var.dataType == XmlDataType::BOOL) {
            strVal = var.value.toBool() ? "true" : "false";
        }
        valElem.appendChild(doc.createTextNode(strVal));
    } else if (var.valueType == XmlValueType::SparseArray) {
        for (const auto& field : var.fields) {
            QDomElement fieldElem = doc.createElement("Field");
            fieldElem.setAttribute("Key", field.key);
            fieldElem.setAttribute("Length", field.length);
            fieldElem.appendChild(doc.createTextNode(field.value.toHex().toUpper()));
            valElem.appendChild(fieldElem);
        }
    }
    
    elem.appendChild(valElem);
    return elem;
}

QDomElement XmlEntitySerializer::serializeObject(QDomDocument& doc, const XmlObject& obj)
{
    // Special handling for Link Object (if we treat it as object)
    if (obj.name == "Link") {
        QDomElement linkElem = doc.createElement("Link");
        
        // Find AID
        for(const auto& var : obj.variables) {
            if(var.name == "AID") {
                QDomElement aidElem = doc.createElement("AID");
                aidElem.appendChild(doc.createTextNode(var.value.toString()));
                linkElem.appendChild(aidElem);
            }
            if(var.name == "TargetRID") {
                QDomElement tridElem = doc.createElement("TargetRID");
                tridElem.appendChild(doc.createTextNode(var.value.toString()));
                linkElem.appendChild(tridElem);
            }
        }
        return linkElem;
    }

    QDomElement elem = doc.createElement("Object");
    elem.setAttribute("Name", obj.name);
    
    // ClassRID
    QDomElement classRidElem = doc.createElement("ClassRID");
    classRidElem.appendChild(doc.createTextNode(QString::number(obj.classRid)));
    elem.appendChild(classRidElem);
    
    // RID (optional)
    if (obj.rid != 0) {
        QDomElement ridElem = doc.createElement("RID");
        ridElem.appendChild(doc.createTextNode(QString::number(obj.rid)));
        elem.appendChild(ridElem);
    }
    
    // Variables
    for (const auto& var : obj.variables) {
        elem.appendChild(serializeVariable(doc, var));
    }
    
    // Children
    for (const auto& child : obj.children) {
        elem.appendChild(serializeObject(doc, child));
    }
    
    return elem;
}

QString XmlEntitySerializer::dataTypeToString(XmlDataType type)
{
    switch (type) {
        case XmlDataType::UINT16: return "UINT16";
        case XmlDataType::UINT32: return "UINT32";
        case XmlDataType::BLOB: return "BLOB";
        case XmlDataType::BOOL: return "BOOL";
        case XmlDataType::STRING: return "STRING";
        default: return "STRING";
    }
}

QString XmlEntitySerializer::valueTypeToString(XmlValueType type)
{
    switch (type) {
        case XmlValueType::Scalar: return "Scalar";
        case XmlValueType::SparseArray: return "SparseArray";
        default: return "Scalar";
    }
}

} // namespace PNConfigLib
