#ifndef BLOBBUILDER_H
#define BLOBBUILDER_H

#include <QByteArray>
#include <QString>
#include <QDataStream>

namespace PNConfigLib {

/**
 * @brief Helper class to build binary blobs for PROFINET data records.
 * 
 * Handles Big Endian serialization of standard data types.
 */
class BlobBuilder {
public:
    BlobBuilder();

    /**
     * @brief Append a single byte.
     */
    void appendByte(uint8_t val);

    /**
     * @brief Append a 16-bit unsigned integer (Big Endian).
     */
    void appendUint16(uint16_t val);

    /**
     * @brief Append a 32-bit unsigned integer (Big Endian).
     */
    void appendUint32(uint32_t val);

    /**
     * @brief Append a byte array.
     */
    void appendBytes(const QByteArray& bytes);

    /**
     * @brief Append a string prefixed with its length.
     * @param str The string to append (UTF-8/ASCII).
     * @param lengthByteCount Number of bytes used to store the length prefix (e.g. 2 or 4).
     */
    void appendString(const QString& str, int lengthByteCount = 2);

    /**
     * @brief Append an IPv4 address (4 bytes).
     * @param ip The IP address string (e.g., "192.168.0.1").
     * @return true if valid IP and appended, false otherwise.
     */
    bool appendIpAddress(const QString& ip);

    /**
     * @brief Append a MAC address (6 bytes).
     * @param mac The MAC address string (e.g., "00:11:22:33:44:55").
     * @return true if valid MAC and appended, false otherwise.
     */
    bool appendMacAddress(const QString& mac);

    /**
     * @brief Get the constructed byte array.
     */
    QByteArray toByteArray() const;

    /**
     * @brief Convert the byte array to a hexadecimal string.
     */
    QString toHexString() const;

private:
    QByteArray m_data;
};

} // namespace PNConfigLib

#endif // BLOBBUILDER_H
