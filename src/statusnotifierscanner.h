#pragma once

#include "trayitem.h"

#include <QObject>
#include <QSet>
#include <QVariant>
#include <QVector>

class StatusNotifierScanner : public QObject
{
    Q_OBJECT

public:
    explicit StatusNotifierScanner(QObject *parent = nullptr);

    QVector<TrayItem> scan();
    Q_INVOKABLE QString iconDataUrlForServicePath(const QString &service, const QString &path) const;
    Q_INVOKABLE QVariant iconForServicePath(const QString &service, const QString &path) const;

Q_SIGNALS:
    void itemsChanged();

private Q_SLOTS:
    void onStatusNotifierChanged(const QString &serviceAndPath);
    void onItemIconChanged();
    void onItemStatusChanged(const QString &status);

private:
    void connectWatcherSignals();
    void connectItemSignals(const TrayItem &item);
    TrayItem readStatusNotifierItem(const QString &serviceAndPath) const;

    QSet<QString> m_connectedItems;
};
