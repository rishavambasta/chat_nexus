#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDataStream>

namespace Protocol {

constexpr quint32 MAGIC = 0x50325043; // "P2PC" in ASCII hex

enum PacketType : quint32 {
    TypeHandshakeRequest     = 1,
    TypeHandshakeResponse    = 2,
    TypePing                 = 3,
    TypePong                 = 4,
    TypeChatMessage          = 5,
    TypeMessageAckDelivered  = 6,
    TypeMessageAckRead       = 7,
    TypeFileOffer            = 8,
    TypeFileAccept           = 9,
    TypeFileChunk            = 10,
    TypeFileComplete         = 11
};

inline QByteArray packJson(PacketType type, const QJsonObject &obj) {
    QByteArray jsonData = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << MAGIC << static_cast<quint32>(type) << static_cast<quint32>(jsonData.size());
    packet.append(jsonData);
    return packet;
}

inline QByteArray packFileChunk(const QString &transferUid, quint64 offset, const QByteArray &data) {
    QByteArray uidBytes = transferUid.toUtf8();
    if (uidBytes.size() > 36) {
        uidBytes = uidBytes.left(36);
    } else {
        uidBytes = uidBytes.leftJustified(36, ' ');
    }

    quint32 payloadSize = 36 + sizeof(quint64) + sizeof(quint32) + data.size();

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << MAGIC << static_cast<quint32>(TypeFileChunk) << payloadSize;

    packet.append(uidBytes);

    QByteArray meta;
    QDataStream metaStream(&meta, QIODevice::WriteOnly);
    metaStream.setByteOrder(QDataStream::BigEndian);
    metaStream << offset << static_cast<quint32>(data.size());
    packet.append(meta);

    packet.append(data);
    return packet;
}

} // namespace Protocol

#endif // PROTOCOL_H
