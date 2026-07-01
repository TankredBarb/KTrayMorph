#pragma once

#include "ruleengine.h"
#include "statusnotifierscanner.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>
#include <QVector>

class TrayItemModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Role {
        StableIdRole = Qt::UserRole + 1,
        KindRole,
        TitleRole,
        IconNameRole,
        StatusRole,
        CategoryRole,
        ServiceRole,
        PathRole,
        ServicePathRole,
        IconHashRole,
        HasPixmapIconRole,
        PixmapDataUrlRole,
        ReplacementIconRole,
        ReplacementTypeRole,
        ReplacementAvailableRole,
        ReplacementStatusRole,
        HasReplacementRole,
    };
    Q_ENUM(Role)

    explicit TrayItemModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addReplacementRule(int row, const QString &replacementType, const QString &replacementIcon);
    Q_INVOKABLE bool removeReplacementRules(int row);
    Q_INVOKABLE bool clearRules();
    Q_INVOKABLE void setPlasmaItems(const QVariantList &items);
    Q_INVOKABLE QString iconDataUrlForStableId(const QString &stableId) const;
    Q_INVOKABLE QVariant iconForStableId(const QString &stableId) const;
    Q_INVOKABLE bool hasReplacementItems() const;
    Q_INVOKABLE QVariantList replacementItems() const;
    Q_INVOKABLE QStringList matchingIconNames(const QString &filter, int limit) const;
    Q_INVOKABLE int searchIconNames(const QString &filter, int limit);
    Q_INVOKABLE void configureLogging(bool enabled, const QString &path);
    Q_INVOKABLE void debugLog(const QString &message) const;

Q_SIGNALS:
    void itemsAboutToReload();
    void iconSearchFinished(int requestId, const QString &filter, const QStringList &names);
    void itemsReloaded();
    void statusNotifierItemsReloaded();

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void reloadItems();

private:
    void rebuildItems();

    StatusNotifierScanner m_statusNotifierScanner;
    RuleEngine m_ruleEngine;
    QVector<TrayItem> m_statusNotifierItems;
    QVector<TrayItem> m_plasmaItems;
    QVector<TrayItem> m_items;
    bool m_hasReplacementItems = false;
    int m_iconSearchRequestId = 0;
};
