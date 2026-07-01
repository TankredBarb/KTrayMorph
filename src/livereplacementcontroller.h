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
    Q_PROPERTY(QObject *rootQmlObject READ rootQmlObject WRITE setRootQmlObject NOTIFY rootQmlObjectChanged)

public:
    explicit LiveReplacementController(QObject *parent = nullptr);

    TrayItemModel *trayModel() const;
    void setTrayModel(TrayItemModel *model);

    QObject *rootQmlObject() const;
    void setRootQmlObject(QObject *object);

    Q_INVOKABLE bool registerReplacement(const QString &stableId, QObject *target,
                                         const QString &replacementSource,
                                         const QString &replacementType,
                                         const QString &replacementValue,
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
    void rootQmlObjectChanged();

private:
    struct Record
    {
        QString stableId;
        QPointer<QObject> target;
        QPointer<QObject> overlay;
        QString originalSource;
        QString replacementSource;
        QString replacementType;
        QString replacementValue;
        bool isStatusNotifier = false;
        QString service;
        QString path;
        QMetaObject::Connection targetDestroyedConnection;
    };

    QObject *createOverlay(QObject *target, const QString &source);
    bool configureOverlay(Record &record);
    QString overlaySourceForRecord(const Record &record) const;
    bool destroyOverlay(QObject *overlay);
    void disconnectTargetGuard(Record &record);
    static bool isColorTint(const Record &record);
    static QVariant targetSourceValue(QObject *target);
    static QString sourceText(const QVariant &value);
    static QString clippedSource(const QString &text);
    static QString objectSummary(QObject *object);

    QHash<QString, Record> m_records;
    TrayItemModel *m_trayModel = nullptr;
    QPointer<QObject> m_rootQmlObject;
};
