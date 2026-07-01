#pragma once

#include "trayitemmodel.h"

#include <QMetaObject>
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

class LiveReplacementController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TrayItemModel *trayModel READ trayModel WRITE setTrayModel NOTIFY trayModelChanged)

public:
    explicit LiveReplacementController(QObject *parent = nullptr);

    TrayItemModel *trayModel() const;
    void setTrayModel(TrayItemModel *model);

    Q_INVOKABLE bool registerReplacement(const QString &stableId, QObject *target,
                                         const QString &originalSource,
                                         const QString &replacementSource,
                                         const QString &replacementType,
                                         const QString &replacementValue,
                                         const QString &restoreFallback,
                                         bool isStatusNotifier,
                                         const QString &service,
                                         const QString &path);
    Q_INVOKABLE int reassertAll();
    Q_INVOKABLE bool updateReplacement(const QString &stableId,
                                       const QString &replacementType,
                                       const QString &replacementValue,
                                       const QString &replacementSource);
    Q_INVOKABLE bool retarget(const QString &stableId, QObject *newTarget);
    Q_INVOKABLE bool restore(const QString &stableId);
    Q_INVOKABLE void restoreAll();
    Q_INVOKABLE bool hasRecord(const QString &stableId) const;
    Q_INVOKABLE bool dropRecord(const QString &stableId);
    Q_INVOKABLE QStringList recordIds() const;
    Q_INVOKABLE int recordCount() const;

Q_SIGNALS:
    void debugLog(const QString &message);
    void needsRetarget(const QString &stableId);
    void recordsChanged();
    void trayModelChanged();

private Q_SLOTS:
    void onSourceChangedByTarget();

private:
    struct Record
    {
        QString stableId;
        QPointer<QObject> target;
        QString originalSource;
        QString replacementSource;
        QString replacementType;
        QString replacementValue;
        QString restoreFallback;
        bool isStatusNotifier = false;
        QString service;
        QString path;
        QMetaObject::Connection notifyConnection;
        QMetaObject::Connection destroyedConnection;
        bool hasNotifyGuard = false;
    };

    QVariant resolveRestoreValue(const Record &record) const;
    void installNotifyGuard(Record &record);
    void disconnectGuard(Record &record);
    void reassertOne(const QString &stableId, bool fromNotify);
    static QVariant targetSourceValue(QObject *target);
    static bool setTargetSource(QObject *target, const QVariant &value);
    static QString sourceText(const QVariant &value);
    static QString clippedSource(const QString &text);

    QHash<QString, Record> m_records;
    QHash<QObject *, QString> m_targetIndex;
    TrayItemModel *m_trayModel = nullptr;
};
