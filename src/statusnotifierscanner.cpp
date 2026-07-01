#include "statusnotifierscanner.h"
#include "logging.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QVariantMap>

namespace
{
void debugLog(const QString &message)
{
    KTrayMorph::debugLog(message);
}
}

struct StatusNotifierPixmap {
    int width = 0;
    int height = 0;
    QByteArray bytes;
};

using StatusNotifierPixmapList = QList<StatusNotifierPixmap>;

Q_DECLARE_METATYPE(StatusNotifierPixmap)
Q_DECLARE_METATYPE(StatusNotifierPixmapList)

const QDBusArgument &operator>>(const QDBusArgument &argument, StatusNotifierPixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.bytes;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const StatusNotifierPixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.bytes;
    argument.endStructure();
    return argument;
}

namespace
{
constexpr auto watcherService = "org.kde.StatusNotifierWatcher";
constexpr auto watcherPath = "/StatusNotifierWatcher";
constexpr auto watcherInterface = "org.kde.StatusNotifierWatcher";
constexpr auto itemInterface = "org.kde.StatusNotifierItem";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";

QString serviceFromRegisteredItem(const QString &serviceAndPath)
{
    const int pathStart = serviceAndPath.indexOf(QLatin1Char('/'));
    if (pathStart <= 0) {
        return serviceAndPath;
    }

    return serviceAndPath.left(pathStart);
}

QString pathFromRegisteredItem(const QString &serviceAndPath)
{
    const int pathStart = serviceAndPath.indexOf(QLatin1Char('/'));
    if (pathStart < 0) {
        return QStringLiteral("/StatusNotifierItem");
    }

    return serviceAndPath.mid(pathStart);
}

QString stringValue(const QVariantMap &properties, const char *name)
{
    return properties.value(QString::fromLatin1(name)).toString();
}

QString hashIconPixmap(const QVariant &value)
{
    QVariant payload = value;
    if (payload.canConvert<QDBusVariant>()) {
        payload = payload.value<QDBusVariant>().variant();
    }

    if (!payload.canConvert<QDBusArgument>()) {
        return {};
    }

    const StatusNotifierPixmapList pixmaps = qdbus_cast<StatusNotifierPixmapList>(payload.value<QDBusArgument>());
    int selectedWidth = 0;
    int selectedHeight = 0;
    QByteArray selectedBytes;

    for (const StatusNotifierPixmap &pixmap : pixmaps) {
        if (pixmap.width * pixmap.height > selectedWidth * selectedHeight && !pixmap.bytes.isEmpty()) {
            selectedWidth = pixmap.width;
            selectedHeight = pixmap.height;
            selectedBytes = pixmap.bytes;
        }
    }

    if (selectedBytes.isEmpty()) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray::number(selectedWidth));
    hash.addData(":");
    hash.addData(QByteArray::number(selectedHeight));
    hash.addData(":");
    hash.addData(selectedBytes);

    return QString::fromLatin1(hash.result().toHex());
}

QImage pixmapToImage(const QVariant &value, const QString &logPrefix)
{
    QVariant payload = value;
    if (payload.canConvert<QDBusVariant>()) {
        payload = payload.value<QDBusVariant>().variant();
    }

    debugLog(QStringLiteral("%1: payload type=%2 canConvertArg=%3")
                 .arg(logPrefix,
                      QString::fromLatin1(payload.typeName()),
                      payload.canConvert<QDBusArgument>() ? QStringLiteral("true") : QStringLiteral("false")));

    if (!payload.canConvert<QDBusArgument>()) {
        debugLog(QStringLiteral("%1: not QDBusArgument, type=%2").arg(logPrefix, QString::fromLatin1(payload.typeName())));
        return {};
    }

    const StatusNotifierPixmapList pixmaps = qdbus_cast<StatusNotifierPixmapList>(payload.value<QDBusArgument>());
    int selectedWidth = 0;
    int selectedHeight = 0;
    QByteArray selectedBytes;

    for (const StatusNotifierPixmap &pixmap : pixmaps) {
        if (pixmap.width * pixmap.height > selectedWidth * selectedHeight && !pixmap.bytes.isEmpty()) {
            selectedWidth = pixmap.width;
            selectedHeight = pixmap.height;
            selectedBytes = pixmap.bytes;
        }
    }

    if (selectedBytes.isEmpty() || selectedWidth <= 0 || selectedHeight <= 0) {
        debugLog(QStringLiteral("%1: no usable pixmap, count=%2").arg(logPrefix).arg(pixmaps.size()));
        return {};
    }

    if (selectedBytes.size() < selectedWidth * selectedHeight * 4) {
        debugLog(QStringLiteral("%1: byte array too small, expected=%2, got=%3")
                     .arg(logPrefix)
                     .arg(selectedWidth * selectedHeight * 4)
                     .arg(selectedBytes.size()));
        return {};
    }

    QByteArray rgbaBytes;
    rgbaBytes.resize(selectedBytes.size());
    const uchar *src = reinterpret_cast<const uchar *>(selectedBytes.constData());
    uchar *dst = reinterpret_cast<uchar *>(rgbaBytes.data());
    const int pixels = selectedBytes.size() / 4;
    for (int i = 0; i < pixels; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 1]; // R
        dst[i * 4 + 1] = src[i * 4 + 2]; // G
        dst[i * 4 + 2] = src[i * 4 + 3]; // B
        dst[i * 4 + 3] = src[i * 4 + 0]; // A
    }

    QImage image(reinterpret_cast<const uchar *>(rgbaBytes.constData()),
                 selectedWidth,
                 selectedHeight,
                 selectedWidth * 4,
                 QImage::Format_RGBA8888);
    if (image.isNull()) {
        debugLog(QStringLiteral("%1: QImage is null, size=%2x%3 bytes=%4")
                     .arg(logPrefix)
                     .arg(selectedWidth)
                     .arg(selectedHeight)
                     .arg(selectedBytes.size()));
        return {};
    }

    return image.copy();
}

QString pixmapToDataUrl(const QVariant &value)
{
    const QImage image = pixmapToImage(value, QStringLiteral("pixmapToDataUrl"));
    if (image.isNull()) {
        return {};
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        debugLog(QStringLiteral("pixmapToDataUrl: QImage::save failed, size=%1x%2").arg(image.width()).arg(image.height()));
        return {};
    }

    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(pngData.toBase64());
}

QVariant pixmapToIcon(const QVariant &value)
{
    const QImage image = pixmapToImage(value, QStringLiteral("pixmapToIcon"));
    if (image.isNull()) {
        return {};
    }

    return QVariant::fromValue(QIcon(QPixmap::fromImage(image)));
}

QVariantMap statusNotifierProperties(const QString &service, const QString &path, const QString &logPrefix)
{
    if (service.isEmpty() || path.isEmpty()) {
        debugLog(QStringLiteral("%1: empty service/path").arg(logPrefix));
        return {};
    }

    QDBusInterface properties(service,
                              path,
                              QString::fromLatin1(propertiesInterface),
                              QDBusConnection::sessionBus());
    const QDBusReply<QVariantMap> reply = properties.call(QStringLiteral("GetAll"),
                                                          QString::fromLatin1(itemInterface));
    if (!reply.isValid()) {
        debugLog(QStringLiteral("%1: GetAll failed for %2 %3: %4").arg(logPrefix, service, path, reply.error().message()));
        return {};
    }

    const QVariantMap values = reply.value();
    QStringList keys;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        keys << it.key();
    }
    debugLog(QStringLiteral("%1: %2 %3 keys=%4 IconName=%5 IconPixmap type=%6")
                 .arg(logPrefix,
                      service,
                      path,
                      keys.join(QLatin1Char(',')),
                      values.value(QStringLiteral("IconName")).toString(),
                      QString::fromLatin1(values.value(QStringLiteral("IconPixmap")).typeName())));
    return values;
}

}

StatusNotifierScanner::StatusNotifierScanner(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<StatusNotifierPixmap>();
    qDBusRegisterMetaType<StatusNotifierPixmapList>();

    connectWatcherSignals();
}

QVector<TrayItem> StatusNotifierScanner::scan()
{
    debugLog(QStringLiteral("StatusNotifierScanner::scan starting"));
    QDBusInterface watcher(QString::fromLatin1(watcherService),
                           QString::fromLatin1(watcherPath),
                           QString::fromLatin1(watcherInterface),
                           QDBusConnection::sessionBus());

    QStringList registeredItems = watcher.property("RegisteredStatusNotifierItems").toStringList();
    if (registeredItems.isEmpty()) {
        const QDBusReply<QStringList> reply = watcher.call(QStringLiteral("RegisteredStatusNotifierItems"));
        if (reply.isValid()) {
            registeredItems = reply.value();
        }
    }

    QVector<TrayItem> items;
    items.reserve(registeredItems.size());

    for (const QString &serviceAndPath : registeredItems) {
        TrayItem item = readStatusNotifierItem(serviceAndPath);
        if (item.service.isEmpty()) {
            continue;
        }

        connectItemSignals(item);
        items.push_back(std::move(item));
    }

    debugLog(QStringLiteral("StatusNotifierScanner::scan completed registered=%1 usable=%2")
                 .arg(registeredItems.size())
                 .arg(items.size()));
    return items;
}

QString StatusNotifierScanner::iconDataUrlForServicePath(const QString &service, const QString &path) const
{
    const QVariantMap values = statusNotifierProperties(service, path, QStringLiteral("iconDataUrl"));
    const QString result = pixmapToDataUrl(values.value(QStringLiteral("IconPixmap")));
    debugLog(QStringLiteral("iconDataUrl: %1 %2 result length=%3").arg(service, path).arg(result.length()));
    return result;
}

QVariant StatusNotifierScanner::iconForServicePath(const QString &service, const QString &path) const
{
    const QVariantMap values = statusNotifierProperties(service, path, QStringLiteral("iconForServicePath"));
    const QVariant result = pixmapToIcon(values.value(QStringLiteral("IconPixmap")));
    debugLog(QStringLiteral("iconForServicePath: %1 %2 result type=%3")
                 .arg(service, path, QString::fromLatin1(result.typeName())));
    return result;
}

void StatusNotifierScanner::onStatusNotifierChanged(const QString &serviceAndPath)
{
    debugLog(QStringLiteral("StatusNotifierScanner watcher changed item=%1").arg(serviceAndPath));
    Q_EMIT itemsChanged();
}

void StatusNotifierScanner::onItemIconChanged()
{
    debugLog(QStringLiteral("StatusNotifierScanner item icon changed"));
    Q_EMIT itemsChanged();
}

void StatusNotifierScanner::onItemStatusChanged(const QString &status)
{
    debugLog(QStringLiteral("StatusNotifierScanner item status changed status=%1").arg(status));
    Q_EMIT itemsChanged();
}

void StatusNotifierScanner::connectWatcherSignals()
{
    QDBusConnection::sessionBus().connect(QString::fromLatin1(watcherService),
                                          QString::fromLatin1(watcherPath),
                                          QString::fromLatin1(watcherInterface),
                                          QStringLiteral("StatusNotifierItemRegistered"),
                                          this,
                                          SLOT(onStatusNotifierChanged(QString)));

    QDBusConnection::sessionBus().connect(QString::fromLatin1(watcherService),
                                          QString::fromLatin1(watcherPath),
                                          QString::fromLatin1(watcherInterface),
                                          QStringLiteral("StatusNotifierItemUnregistered"),
                                          this,
                                          SLOT(onStatusNotifierChanged(QString)));
}

void StatusNotifierScanner::connectItemSignals(const TrayItem &item)
{
    const QString servicePath = QString(item.service + item.path);
    if (m_connectedItems.contains(servicePath)) {
        return;
    }

    auto bus = QDBusConnection::sessionBus();
    bus.connect(item.service, item.path, QString::fromLatin1(itemInterface), QStringLiteral("NewIcon"), this, SLOT(onItemIconChanged()));
    bus.connect(item.service, item.path, QString::fromLatin1(itemInterface), QStringLiteral("NewAttentionIcon"), this, SLOT(onItemIconChanged()));
    bus.connect(item.service, item.path, QString::fromLatin1(itemInterface), QStringLiteral("NewOverlayIcon"), this, SLOT(onItemIconChanged()));
    bus.connect(item.service, item.path, QString::fromLatin1(itemInterface), QStringLiteral("NewStatus"), this, SLOT(onItemStatusChanged(QString)));

    debugLog(QStringLiteral("StatusNotifierScanner connected item stableId=%1 service=%2 path=%3")
                 .arg(item.stableId, item.service, item.path));
    m_connectedItems.insert(servicePath);
}

TrayItem StatusNotifierScanner::readStatusNotifierItem(const QString &serviceAndPath) const
{
    TrayItem item;
    item.kind = QStringLiteral("StatusNotifier");
    item.service = serviceFromRegisteredItem(serviceAndPath);
    item.path = pathFromRegisteredItem(serviceAndPath);

    if (item.service.isEmpty() || item.path.isEmpty()) {
        return {};
    }

    QDBusInterface properties(item.service, item.path, QString::fromLatin1(propertiesInterface), QDBusConnection::sessionBus());
    const QDBusReply<QVariantMap> reply = properties.call(QStringLiteral("GetAll"), QString::fromLatin1(itemInterface));

    if (!reply.isValid()) {
        qWarning() << "Failed to read StatusNotifierItem properties for" << serviceAndPath << reply.error().message();
        return item;
    }

    const QVariantMap values = reply.value();
    item.stableId = stringValue(values, "Id");
    item.title = stringValue(values, "Title");
    item.iconName = stringValue(values, "IconName");
    item.status = stringValue(values, "Status");
    item.category = stringValue(values, "Category");
    item.iconHash = hashIconPixmap(values.value(QStringLiteral("IconPixmap")));
    item.hasPixmapIcon = !item.iconHash.isEmpty();
    if (item.hasPixmapIcon) {
        item.pixmapDataUrl = pixmapToDataUrl(values.value(QStringLiteral("IconPixmap")));
    }

    if (item.stableId.isEmpty()) {
        item.stableId = QString(item.service + item.path);
    }

    if (item.title.isEmpty()) {
        item.title = item.stableId;
    }

    debugLog(QStringLiteral("StatusNotifierScanner read item stableId=%1 title=%2 iconName=%3 hasPixmap=%4 hash=%5 status=%6 service=%7 path=%8")
                 .arg(item.stableId,
                      item.title,
                      item.iconName,
                      item.hasPixmapIcon ? QStringLiteral("true") : QStringLiteral("false"),
                      item.iconHash.left(12),
                      item.status,
                      item.service,
                      item.path));

    return item;
}
