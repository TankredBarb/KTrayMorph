#include "trayitemmodel.h"
#include "logging.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QVariantMap>

#include <QtConcurrent>

#include <KIconLoader>

#include <algorithm>

namespace
{
QStringList allIconNames()
{
    static QStringList names;
    static bool initialized = false;
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (!initialized) {
        QSet<QString> seen;
        for (const QString &icon : KIconLoader::global()->queryIcons()) {
            QString name = QFileInfo(icon).completeBaseName();
            if (name.isEmpty()) {
                name = icon;
            }
            if (name.isEmpty() || seen.contains(name)) {
                continue;
            }
            seen.insert(name);
            names.push_back(name);
        }
        names.sort(Qt::CaseInsensitive);
        initialized = true;
    }
    return names;
}

QStringList matchingIconNamesForFilter(const QString &filter, int limit)
{
    const int maxResults = limit > 0 ? limit : 120;
    const QString trimmedFilter = filter.trimmed();
    if (trimmedFilter.isEmpty()) {
        return {};
    }

    const QStringList iconNames = allIconNames();
    QStringList matches;
    matches.reserve(maxResults);

    for (const QString &name : iconNames) {
        if (name.compare(trimmedFilter, Qt::CaseInsensitive) == 0) {
            matches.push_back(name);
        }
    }

    for (const QString &name : iconNames) {
        if (matches.size() >= maxResults) {
            return matches;
        }
        if (matches.contains(name) || !name.startsWith(trimmedFilter, Qt::CaseInsensitive)) {
            continue;
        }
        matches.push_back(name);
    }

    for (const QString &name : iconNames) {
        if (matches.size() >= maxResults) {
            return matches;
        }
        if (matches.contains(name) || !name.contains(trimmedFilter, Qt::CaseInsensitive)) {
            continue;
        }

        matches.push_back(name);
    }

    return matches;
}
}

TrayItemModel::TrayItemModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_statusNotifierScanner, &StatusNotifierScanner::itemsChanged, this, &TrayItemModel::reloadItems);
    refresh();
}

int TrayItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_items.size();
}

QVariant TrayItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());

    switch (role) {
    case StableIdRole:
        return item.stableId;
    case KindRole:
        return item.kind;
    case TitleRole:
        return item.title;
    case IconNameRole:
        return item.iconName;
    case StatusRole:
        return item.status;
    case CategoryRole:
        return item.category;
    case ServiceRole:
        return item.service;
    case PathRole:
        return item.path;
    case ServicePathRole:
        return QString(item.service + item.path);
    case IconHashRole:
        return item.iconHash;
    case HasPixmapIconRole:
        return item.hasPixmapIcon;
    case PixmapDataUrlRole:
        return item.pixmapDataUrl;
    case ReplacementIconRole:
        return item.replacementIcon;
    case ReplacementTypeRole:
        return item.replacementType;
    case ReplacementAvailableRole:
        return item.replacementAvailable;
    case ReplacementStatusRole:
        return item.replacementStatus;
    case HasReplacementRole:
        return item.hasReplacement;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrayItemModel::roleNames() const
{
    return {
        {StableIdRole, QByteArrayLiteral("stableId")},
        {KindRole, QByteArrayLiteral("kind")},
        {TitleRole, QByteArrayLiteral("title")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {StatusRole, QByteArrayLiteral("status")},
        {CategoryRole, QByteArrayLiteral("category")},
        {ServiceRole, QByteArrayLiteral("service")},
        {PathRole, QByteArrayLiteral("path")},
        {ServicePathRole, QByteArrayLiteral("servicePath")},
        {IconHashRole, QByteArrayLiteral("iconHash")},
        {HasPixmapIconRole, QByteArrayLiteral("hasPixmapIcon")},
        {PixmapDataUrlRole, QByteArrayLiteral("pixmapDataUrl")},
        {ReplacementIconRole, QByteArrayLiteral("replacementIcon")},
        {ReplacementTypeRole, QByteArrayLiteral("replacementType")},
        {ReplacementAvailableRole, QByteArrayLiteral("replacementAvailable")},
        {ReplacementStatusRole, QByteArrayLiteral("replacementStatus")},
        {HasReplacementRole, QByteArrayLiteral("hasReplacement")},
    };
}

bool TrayItemModel::addReplacementRule(int row, const QString &replacementType, const QString &replacementIcon)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }

    if (!m_ruleEngine.addRuleForItem(m_items.at(row), replacementType, replacementIcon)) {
        return false;
    }

    rebuildItems();
    return true;
}

bool TrayItemModel::removeReplacementRules(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }

    if (!m_ruleEngine.removeRulesForItem(m_items.at(row))) {
        return false;
    }

    rebuildItems();
    return true;
}

bool TrayItemModel::clearRules()
{
    if (!m_ruleEngine.clear()) {
        return false;
    }

    rebuildItems();
    return true;
}

void TrayItemModel::setPlasmaItems(const QVariantList &items)
{
    QVector<TrayItem> plasmaItems;
    plasmaItems.reserve(items.size());

    for (const QVariant &value : items) {
        const QVariantMap map = value.toMap();
        TrayItem item;
        item.kind = QStringLiteral("PlasmaApplet");
        item.stableId = map.value(QStringLiteral("stableId")).toString();
        item.title = map.value(QStringLiteral("title")).toString();
        item.iconName = map.value(QStringLiteral("iconName")).toString();
        item.status = map.value(QStringLiteral("status")).toString();
        item.category = QStringLiteral("PlasmaInternal");

        if (item.stableId.isEmpty()) {
            continue;
        }

        if (item.title.isEmpty()) {
            item.title = item.stableId;
        }

        plasmaItems.push_back(std::move(item));
    }

    m_plasmaItems = std::move(plasmaItems);
    rebuildItems();
}

QString TrayItemModel::iconDataUrlForStableId(const QString &stableId) const
{
    for (const TrayItem &item : m_items) {
        if (item.stableId == stableId && item.kind == QStringLiteral("StatusNotifier")) {
            return m_statusNotifierScanner.iconDataUrlForServicePath(item.service, item.path);
        }
    }
    return {};
}

QVariant TrayItemModel::iconForStableId(const QString &stableId) const
{
    for (const TrayItem &item : m_items) {
        if (item.stableId == stableId && item.kind == QStringLiteral("StatusNotifier")) {
            return m_statusNotifierScanner.iconForServicePath(item.service, item.path);
        }
    }
    return {};
}

bool TrayItemModel::hasReplacementItems() const
{
    return m_hasReplacementItems;
}

QVariantList TrayItemModel::replacementItems() const
{
    QVariantList items;
    for (const TrayItem &item : m_items) {
        if (!item.hasReplacement || item.replacementIcon.isEmpty()) {
            continue;
        }

        QVariantMap map;
        map.insert(QStringLiteral("stableId"), item.stableId);
        map.insert(QStringLiteral("iconName"), item.iconName);
        map.insert(QStringLiteral("service"), item.service);
        map.insert(QStringLiteral("path"), item.path);
        map.insert(QStringLiteral("replacementType"), item.replacementType);
        map.insert(QStringLiteral("replacementIcon"), item.replacementIcon);
        map.insert(QStringLiteral("replacementAvailable"), item.replacementAvailable);
        map.insert(QStringLiteral("replacementStatus"), item.replacementStatus);
        items.push_back(map);
    }
    return items;
}

QStringList TrayItemModel::matchingIconNames(const QString &filter, int limit) const
{
    return matchingIconNamesForFilter(filter, limit);
}

int TrayItemModel::searchIconNames(const QString &filter, int limit)
{
    const int requestId = ++m_iconSearchRequestId;
    const QString trimmedFilter = filter.trimmed();
    auto *watcher = new QFutureWatcher<QStringList>(this);

    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, requestId, trimmedFilter]() {
        const QStringList names = watcher->result();
        watcher->deleteLater();
        Q_EMIT iconSearchFinished(requestId, trimmedFilter, names);
    });

    watcher->setFuture(QtConcurrent::run([trimmedFilter, limit]() {
        return matchingIconNamesForFilter(trimmedFilter, limit);
    }));

    return requestId;
}

void TrayItemModel::configureLogging(bool enabled, const QString &path)
{
    KTrayMorph::configureLogging(enabled, path);
}

void TrayItemModel::debugLog(const QString &message) const
{
    KTrayMorph::debugLog(message);
}

void TrayItemModel::refresh()
{
    m_ruleEngine.reload();
    m_statusNotifierItems = m_statusNotifierScanner.scan();
    rebuildItems();
}

void TrayItemModel::rebuildItems()
{
    QVector<TrayItem> items = m_statusNotifierItems;
    items += m_plasmaItems;
    m_ruleEngine.apply(items);
    m_hasReplacementItems = std::any_of(items.cbegin(), items.cend(), [](const TrayItem &item) {
        return item.hasReplacement && !item.replacementIcon.isEmpty();
    });
    Q_EMIT itemsAboutToReload();
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    Q_EMIT itemsReloaded();
}

void TrayItemModel::reloadItems()
{
    refresh();
    Q_EMIT statusNotifierItemsReloaded();
}
