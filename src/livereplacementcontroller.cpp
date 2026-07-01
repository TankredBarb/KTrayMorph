#include "livereplacementcontroller.h"
#include "logging.h"

#include <QMetaMethod>
#include <QMetaProperty>

LiveReplacementController::LiveReplacementController(QObject *parent)
    : QObject(parent)
{
}

TrayItemModel *LiveReplacementController::trayModel() const
{
    return m_trayModel;
}

void LiveReplacementController::setTrayModel(TrayItemModel *model)
{
    if (m_trayModel == model) {
        return;
    }
    m_trayModel = model;
    Q_EMIT trayModelChanged();
}

QVariant LiveReplacementController::targetSourceValue(QObject *target)
{
    if (!target) {
        return {};
    }
    return target->property("source");
}

bool LiveReplacementController::setTargetSource(QObject *target, const QVariant &value)
{
    if (!target) {
        return false;
    }
    return target->setProperty("source", value);
}

QString LiveReplacementController::sourceText(const QVariant &value)
{
    if (value.userType() == QMetaType::QString) {
        return value.toString();
    }
    if (value.canConvert<QString>()) {
        return value.toString();
    }
    return QString::fromLatin1(value.typeName());
}

QString LiveReplacementController::clippedSource(const QString &text)
{
    if (text.length() <= 120) {
        return text;
    }
    return text.left(120) + QStringLiteral("...(") + QString::number(text.length()) + QStringLiteral(")");
}

void LiveReplacementController::installNotifyGuard(Record &record)
{
    if (!record.target) {
        return;
    }

    const QMetaObject *meta = record.target->metaObject();
    const int idx = meta ? meta->indexOfProperty("source") : -1;
    if (idx < 0) {
        Q_EMIT debugLog(QStringLiteral("notifyGuard: no 'source' property on target stableId=%1").arg(record.stableId));
        return;
    }

    const QMetaProperty prop = meta->property(idx);
    if (!prop.hasNotifySignal()) {
        Q_EMIT debugLog(QStringLiteral("notifyGuard: 'source' has no notify signal stableId=%1").arg(record.stableId));
        return;
    }

    const QMetaMethod notify = prop.notifySignal();
    const int slotIndex = metaObject()->indexOfSlot("onSourceChangedByTarget()");
    if (slotIndex < 0) {
        Q_EMIT debugLog(QStringLiteral("notifyGuard: controller slot not found stableId=%1").arg(record.stableId));
        return;
    }

    const QMetaMethod slot = metaObject()->method(slotIndex);
    const QString stableId = record.stableId;
    record.notifyConnection = QObject::connect(record.target, notify, this, slot, Qt::DirectConnection);

    record.destroyedConnection = QObject::connect(
        record.target, &QObject::destroyed, this, [this, stableId]() {
            auto it = m_records.find(stableId);
            if (it == m_records.end()) {
                return;
            }
            QObject *rawTarget = it.value().target.data();
            it.value().target.clear();
            it.value().notifyConnection = {};
            it.value().destroyedConnection = {};
            it.value().hasNotifyGuard = false;
            const bool isSni = it.value().isStatusNotifier;
            if (rawTarget) {
                m_targetIndex.remove(rawTarget);
            }
            m_records.erase(it);
            Q_EMIT debugLog(QStringLiteral("notifyGuard: target destroyed stableId=%1 isSNI=%2, retargeting")
                                .arg(stableId, isSni ? QStringLiteral("true") : QStringLiteral("false")));
            Q_EMIT needsRetarget(stableId);
            Q_EMIT recordsChanged();
        });

    record.hasNotifyGuard = record.notifyConnection;
    Q_EMIT debugLog(QStringLiteral("notifyGuard installed stableId=%1 hasNotify=%2")
                        .arg(record.stableId, record.hasNotifyGuard ? QStringLiteral("true") : QStringLiteral("false")));
}

void LiveReplacementController::onSourceChangedByTarget()
{
    auto *target = qobject_cast<QObject *>(sender());
    if (!target) {
        return;
    }

    auto idxIt = m_targetIndex.constFind(target);
    if (idxIt == m_targetIndex.constEnd()) {
        return;
    }
    reassertOne(idxIt.value(), true);
}

void LiveReplacementController::disconnectGuard(Record &record)
{
    if (record.notifyConnection) {
        QObject::disconnect(record.notifyConnection);
        record.notifyConnection = {};
    }
    if (record.destroyedConnection) {
        QObject::disconnect(record.destroyedConnection);
        record.destroyedConnection = {};
    }
    record.hasNotifyGuard = false;
}

void LiveReplacementController::reassertOne(const QString &stableId, bool fromNotify)
{
    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        if (fromNotify) {
            Q_EMIT debugLog(QStringLiteral("reassertOne (notify) stableId=%1 no record").arg(stableId));
        }
        return;
    }

    Record &record = it.value();
    if (record.target.isNull()) {
        if (fromNotify) {
            Q_EMIT debugLog(QStringLiteral("reassertOne (notify) stableId=%1 target gone").arg(stableId));
        }
        return;
    }

    const QString currentText = sourceText(targetSourceValue(record.target));
    if (currentText == record.replacementSource) {
        return;
    }

    Q_EMIT debugLog(QStringLiteral("reassertOne (notify=%1) stableId=%2 drifted=[%3] -> [%4]")
                        .arg(fromNotify ? QStringLiteral("true") : QStringLiteral("false"),
                             stableId, clippedSource(currentText), clippedSource(record.replacementSource)));

    if (!setTargetSource(record.target, record.replacementSource)) {
        Q_EMIT debugLog(QStringLiteral("reassertOne: setProperty(source) failed stableId=%1").arg(stableId));
    }
}

QVariant LiveReplacementController::resolveRestoreValue(const Record &record) const
{
    if (record.isStatusNotifier) {
        if (!record.restoreFallback.isEmpty()) {
            return record.restoreFallback;
        }
        if (m_trayModel) {
            return m_trayModel->iconForStableId(record.stableId);
        }
        return {};
    }

    if (!record.originalSource.isEmpty()) {
        return record.originalSource;
    }
    return record.restoreFallback;
}

bool LiveReplacementController::registerReplacement(const QString &stableId, QObject *target,
                                                    const QString &originalSource,
                                                    const QString &replacementSource,
                                                    const QString &replacementType,
                                                    const QString &replacementValue,
                                                    const QString &restoreFallback,
                                                    bool isStatusNotifier,
                                                    const QString &service,
                                                    const QString &path)
{
    if (stableId.isEmpty() || replacementSource.isEmpty() || !target) {
        Q_EMIT debugLog(QStringLiteral("registerReplacement refused: empty id/replacement/null target"));
        return false;
    }

    const QString currentText = sourceText(targetSourceValue(target));
    if (currentText == replacementSource) {
        Q_EMIT debugLog(QStringLiteral("registerReplacement refused: would capture replacement as original stableId=%1").arg(stableId));
        return false;
    }

    Record record;
    record.stableId = stableId;
    record.target = target;
    record.originalSource = originalSource.isEmpty() ? currentText : originalSource;
    record.replacementSource = replacementSource;
    record.replacementType = replacementType;
    record.replacementValue = replacementValue;
    record.restoreFallback = restoreFallback;
    record.isStatusNotifier = isStatusNotifier;
    record.service = service;
    record.path = path;

    m_records.insert(stableId, record);
    m_targetIndex.insert(target, stableId);

    if (!setTargetSource(target, replacementSource)) {
        Q_EMIT debugLog(QStringLiteral("registerReplacement: setProperty(source) failed stableId=%1").arg(stableId));
        return false;
    }

    installNotifyGuard(m_records[stableId]);

    Q_EMIT debugLog(QStringLiteral("registerReplacement OK stableId=%1 original=[%2] -> replacement=[%3] isSNI=%4")
                        .arg(stableId, clippedSource(record.originalSource), clippedSource(replacementSource),
                             isStatusNotifier ? QStringLiteral("true") : QStringLiteral("false")));
    Q_EMIT recordsChanged();
    return true;
}

bool LiveReplacementController::updateReplacement(const QString &stableId,
                                                  const QString &replacementType,
                                                  const QString &replacementValue,
                                                  const QString &replacementSource)
{
    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        Q_EMIT debugLog(QStringLiteral("updateReplacement: no record for %1").arg(stableId));
        return false;
    }

    if (it.value().target.isNull()) {
        Q_EMIT debugLog(QStringLiteral("updateReplacement: target gone, dropping record %1").arg(stableId));
        m_records.erase(it);
        Q_EMIT recordsChanged();
        return false;
    }

    Record &record = it.value();
    record.replacementType = replacementType;
    record.replacementValue = replacementValue;
    record.replacementSource = replacementSource;
    // notify guard keeps watching the same target; only the value changed.
    if (!record.hasNotifyGuard) {
        installNotifyGuard(record);
    }

    if (!setTargetSource(record.target, replacementSource)) {
        Q_EMIT debugLog(QStringLiteral("updateReplacement: setProperty(source) failed stableId=%1").arg(stableId));
        return false;
    }

    Q_EMIT debugLog(QStringLiteral("updateReplacement OK stableId=%1 replacementSource=[%2]")
                        .arg(stableId, clippedSource(replacementSource)));
    return true;
}

int LiveReplacementController::reassertAll()
{
    int reasserted = 0;
    QStringList retargets;

    for (auto it = m_records.begin(); it != m_records.end();) {
        Record &record = it.value();

        if (record.target.isNull()) {
            Q_EMIT debugLog(QStringLiteral("reassert stableId=%1 target gone, scheduling retarget").arg(record.stableId));
            disconnectGuard(record);
            retargets << record.stableId;
            it = m_records.erase(it);
            continue;
        }

        if (!record.hasNotifyGuard) {
            installNotifyGuard(record);
        }

        const QString currentText = sourceText(targetSourceValue(record.target));
        if (currentText == record.replacementSource) {
            ++it;
            continue;
        }

        Q_EMIT debugLog(QStringLiteral("reassert stableId=%1 drifted=[%2] -> [%3]")
                            .arg(record.stableId, clippedSource(currentText), clippedSource(record.replacementSource)));

        if (!setTargetSource(record.target, record.replacementSource)) {
            Q_EMIT debugLog(QStringLiteral("reassert: setProperty(source) failed stableId=%1").arg(record.stableId));
        }
        reasserted += 1;
        ++it;
    }

    for (const QString &id : retargets) {
        Q_EMIT needsRetarget(id);
    }

    return reasserted;
}

bool LiveReplacementController::retarget(const QString &stableId, QObject *newTarget)
{
    if (stableId.isEmpty() || !newTarget) {
        return false;
    }

    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        return false;
    }

    if (it.value().target) {
        m_targetIndex.remove(it.value().target.data());
    }
    disconnectGuard(it.value());
    it.value().target = newTarget;
    m_targetIndex.insert(newTarget, stableId);
    installNotifyGuard(it.value());
    const bool ok = setTargetSource(newTarget, it.value().replacementSource);
    Q_EMIT debugLog(QStringLiteral("retarget stableId=%1 ok=%2")
                        .arg(stableId, ok ? QStringLiteral("true") : QStringLiteral("false")));
    return ok;
}

bool LiveReplacementController::restore(const QString &stableId)
{
    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        Q_EMIT debugLog(QStringLiteral("restore: no record for %1").arg(stableId));
        return false;
    }

    const Record record = it.value();
    const QVariant restoreValue = resolveRestoreValue(record);

    disconnectGuard(it.value());

    bool ok = false;
    if (!record.target.isNull() && restoreValue.isValid()) {
        ok = setTargetSource(record.target, restoreValue);
        Q_EMIT debugLog(QStringLiteral("restore stableId=%1 ok=%2 value=[%3]")
                            .arg(stableId, ok ? QStringLiteral("true") : QStringLiteral("false"),
                                 clippedSource(sourceText(restoreValue))));
    } else {
        Q_EMIT debugLog(QStringLiteral("restore stableId=%1 no target/restore value, cleared").arg(stableId));
    }

    disconnectGuard(it.value());
    if (it.value().target) {
        m_targetIndex.remove(it.value().target.data());
    }
    m_records.erase(it);
    Q_EMIT recordsChanged();
    return ok;
}

void LiveReplacementController::restoreAll()
{
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        Record &record = it.value();
        disconnectGuard(record);
        const QVariant restoreValue = resolveRestoreValue(record);
        if (!record.target.isNull() && restoreValue.isValid()) {
            setTargetSource(record.target, restoreValue);
        }
    }
    m_records.clear();
    m_targetIndex.clear();
    Q_EMIT debugLog(QStringLiteral("restoreAll cleared"));
    Q_EMIT recordsChanged();
}

bool LiveReplacementController::hasRecord(const QString &stableId) const
{
    return m_records.contains(stableId);
}

bool LiveReplacementController::dropRecord(const QString &stableId)
{
    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        return false;
    }
    disconnectGuard(it.value());
    if (it.value().target) {
        m_targetIndex.remove(it.value().target.data());
    }
    m_records.erase(it);
    Q_EMIT recordsChanged();
    return true;
}

QStringList LiveReplacementController::recordIds() const
{
    return m_records.keys();
}

int LiveReplacementController::recordCount() const
{
    return m_records.size();
}
