#include "BlobBuilder.h"
#include <QtEndian>
#include <QStringList>

namespace PNConfigLib {

BlobBuilder::BlobBuilder()
{
}

void BlobBuilder::appendByte(uint8_t val)
{
    m_data.append(static_cast<char>(val));
}

void BlobBuilder::appendUint16(uint16_t val)
{
    uint16_t beVal = qToBigEndian(val);
    m_data.append(reinterpret_cast<const char*>(&beVal), sizeof(beVal));
}

void BlobBuilder::appendUint32(uint32_t val)
{
    uint32_t beVal = qToBigEndian(val);
    m_data.append(reinterpret_cast<const char*>(&beVal), sizeof(beVal));
}

void BlobBuilder::appendBytes(const QByteArray& bytes)
{
    m_data.append(bytes);
}

void BlobBuilder::appendString(const QString& str, int lengthByteCount)
{
    QByteArray utf8 = str.toUtf8();
    int length = utf8.size();
    
    if (lengthByteCount == 1) {
        appendByte(static_cast<uint8_t>(length));
    } else if (lengthByteCount == 2) {
        appendUint16(static_cast<uint16_t>(length));
    } else if (lengthByteCount == 4) {
        appendUint32(static_cast<uint32_t>(length));
    }
    
    m_data.append(utf8);
}

bool BlobBuilder::appendIpAddress(const QString& ip)
{
    QStringList parts = ip.split('.');
    if (parts.size() != 4) {
        return false;
    }
    
    for (const QString& part : parts) {
        bool ok;
        int val = part.toInt(&ok);
        if (!ok || val < 0 || val > 255) {
            return false;
        }
        appendByte(static_cast<uint8_t>(val));
    }
    
    return true;
}

bool BlobBuilder::appendMacAddress(const QString& mac)
{
    // Simple parser for hex strings like "00:11:22..." or "00-11-22..."
    QString cleanMac = mac;
    cleanMac.replace(":", "").replace("-", "");
    
    if (cleanMac.length() != 12) {
        return false;
    }
    
    bool ok;
    for (int i = 0; i < 12; i += 2) {
        uint8_t byteVal = static_cast<uint8_t>(cleanMac.mid(i, 2).toUInt(&ok, 16));
        if (!ok) return false;
        appendByte(byteVal);
    }
    
    return true;
}

QByteArray BlobBuilder::toByteArray() const
{
    return m_data;
}

QString BlobBuilder::toHexString() const
{
    return m_data.toHex().toUpper();
}

} // namespace PNConfigLib
