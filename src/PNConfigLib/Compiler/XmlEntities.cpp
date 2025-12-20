#include "XmlEntities.h"
#include "../tinyxml2/tinyxml2.h"

using namespace tinyxml2;

namespace PNConfigLib {

void XmlObject::addScalar(const QString& name, uint32_t aid, XmlDataType type, const QVariant& val)
{
    XmlVariable var;
    var.name = name;
    var.aid = aid;
    var.valueType = XmlValueType::Scalar;
    var.dataType = type;
    var.value = val;
    if (type == XmlDataType::BLOB) {
        var.scalarBlobLength = val.toByteArray().size();
    }
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

XMLElement* XmlEntitySerializer::serializeVariable(XMLDocument* doc, const XmlVariable& var)
{
    // Special handling for Key
    if (var.name == "Key") {
        XMLElement* keyElem = doc->NewElement("Key");
        keyElem->SetAttribute("AID", var.aid);
        keyElem->SetText(var.value.toString().toStdString().c_str());
        return keyElem;
    }
    
    // Standard Variable
    XMLElement* elem = doc->NewElement("Variable");
    elem->SetAttribute("Name", var.name.toStdString().c_str());
    
    // AID
    XMLElement* aidElem = doc->NewElement("AID");
    aidElem->SetText(QString::number(var.aid).toStdString().c_str());
    elem->InsertEndChild(aidElem);
    
    // Value
    XMLElement* valElem = doc->NewElement("Value");
    valElem->SetAttribute("Datatype", valueTypeToString(var.valueType).toStdString().c_str());
    valElem->SetAttribute("Valuetype", dataTypeToString(var.dataType).toStdString().c_str());
    
    // Add Length attribute for Scalar BLOB if specified
    if (var.valueType == XmlValueType::Scalar && var.dataType == XmlDataType::BLOB && var.scalarBlobLength >= 0) {
        valElem->SetAttribute("Length", var.scalarBlobLength);
    }
    
    if (var.valueType == XmlValueType::Scalar) {
        QString strVal = var.value.toString();
        if (var.dataType == XmlDataType::BOOL) {
            strVal = var.value.toBool() ? "true" : "false";
        }
        // Don't set text for empty scalar BLOBs (self-closing tag)
        if (!(var.dataType == XmlDataType::BLOB && var.scalarBlobLength == 0)) {
            valElem->SetText(strVal.toStdString().c_str());
        }
    } else if (var.valueType == XmlValueType::SparseArray) {
        for (const auto& field : var.fields) {
            XMLElement* fieldElem = doc->NewElement("Field");
            fieldElem->SetAttribute("Key", field.key);
            fieldElem->SetAttribute("Length", field.length);
            fieldElem->SetText(field.value.toHex().toUpper().toStdString().c_str());
            valElem->InsertEndChild(fieldElem);
        }
    }
    
    elem->InsertEndChild(valElem);
    return elem;
}

XMLElement* XmlEntitySerializer::serializeObject(XMLDocument* doc, const XmlObject& obj)
{
    // Special handling for Link Object (if we treat it as object)
    if (obj.name == "Link") {
        XMLElement* linkElem = doc->NewElement("Link");
        
        // Add AID first
        for(const auto& var : obj.variables) {
            if(var.name == "AID") {
                XMLElement* aidElem = doc->NewElement("AID");
                aidElem->SetText(var.value.toString().toStdString().c_str());
                linkElem->InsertEndChild(aidElem);
                break;
            }
        }
        
        // Then add TargetRID
        for(const auto& var : obj.variables) {
            if(var.name == "TargetRID") {
                XMLElement* tridElem = doc->NewElement("TargetRID");
                tridElem->SetText(var.value.toString().toStdString().c_str());
                linkElem->InsertEndChild(tridElem);
                break;
            }
        }
        return linkElem;
    }

    XMLElement* elem = doc->NewElement("Object");
    elem->SetAttribute("Name", obj.name.toStdString().c_str());
    
    // GSDMLFile (optional, before ClassRID for devices)
    if (!obj.gsdmlFile.isEmpty()) {
        XMLElement* gsdmlElem = doc->NewElement("GSDMLFile");
        gsdmlElem->SetText(obj.gsdmlFile.toStdString().c_str());
        elem->InsertEndChild(gsdmlElem);
    }
    
    // ClassRID
    XMLElement* classRidElem = doc->NewElement("ClassRID");
    classRidElem->SetText(QString::number(obj.classRid).toStdString().c_str());
    elem->InsertEndChild(classRidElem);
    
    // RID (optional)
    if (obj.rid != 0) {
        XMLElement* ridElem = doc->NewElement("RID");
        ridElem->SetText(QString::number(obj.rid).toStdString().c_str());
        elem->InsertEndChild(ridElem);
    }
    
    // Variables
    for (const auto& var : obj.variables) {
        elem->InsertEndChild(serializeVariable(doc, var));
    }
    
    // Children
    for (const auto& child : obj.children) {
        elem->InsertEndChild(serializeObject(doc, child));
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
