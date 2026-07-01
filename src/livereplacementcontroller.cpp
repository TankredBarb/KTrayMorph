#include "livereplacementcontroller.h"
#include "logging.h"

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

QObject *LiveReplacementController::rootQmlObject() const
{
    return m_rootQmlObject;
}

void LiveReplacementController::setRootQmlObject(QObject *object)
{
    if (m_rootQmlObject == object) {
        return;
    }
    m_rootQmlObject = object;
    Q_EMIT rootQmlObjectChanged();
}

QVariant LiveReplacementController::targetSourceValue(QObject *target)
{
    if (!target) {
        return {};
    }
    return target->property("source");
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

QString LiveReplacementController::objectSummary(QObject *object)
{
    if (!object) {
        return QStringLiteral("null");
    }

    const QStringList interestingProperties = {
        QStringLiteral("objectName"),
        QStringLiteral("source"),
        QStringLiteral("replacementType"),
        QStringLiteral("replacementValue"),
        QStringLiteral("tintLayerEnabled"),
        QStringLiteral("tintColor"),
        QStringLiteral("tintRenderer"),
        QStringLiteral("visible"),
        QStringLiteral("opacity"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("z"),
    };

    QStringList details;
    details << QString::fromLatin1(object->metaObject()->className());
    for (const QString &propertyName : interestingProperties) {
        const QByteArray propertyNameBytes = propertyName.toLatin1();
        const QVariant value = object->property(propertyNameBytes.constData());
        if (value.isValid()) {
            details << QStringLiteral("%1=%2").arg(propertyName, clippedSource(sourceText(value)));
        }
    }

    return details.join(QLatin1Char(' '));
}

bool LiveReplacementController::isColorTint(const Record &record)
{
    return record.replacementType == QStringLiteral("colorTint");
}

QString LiveReplacementController::overlaySourceForRecord(const Record &record) const
{
    return record.replacementSource;
}

bool LiveReplacementController::configureOverlay(Record &record)
{
    if (record.overlay.isNull()) {
        return false;
    }

    const QString source = overlaySourceForRecord(record);
    if (source.isEmpty()) {
        return false;
    }

    bool changed = false;
    if (record.overlay->property("source").toString() != source) {
        record.overlay->setProperty("source", source);
        changed = true;
    }
    if (record.overlay->property("replacementType").toString() != record.replacementType) {
        record.overlay->setProperty("replacementType", record.replacementType);
        changed = true;
    }
    if (record.overlay->property("replacementValue").toString() != record.replacementValue) {
        record.overlay->setProperty("replacementValue", record.replacementValue);
        changed = true;
    }
    if (changed && isColorTint(record)) {
        Q_EMIT debugLog(QStringLiteral("configureOverlay colorTint stableId=%1 changed=%2 overlay=[%3] target=[%4]")
                            .arg(record.stableId,
                                 changed ? QStringLiteral("true") : QStringLiteral("false"),
                                 objectSummary(record.overlay),
                                 objectSummary(record.target)));
    }
    return changed;
}

QObject *LiveReplacementController::createOverlay(QObject *target, const QString &source)
{
    if (!target) {
        Q_EMIT debugLog(QStringLiteral("createOverlay: null target"));
        return nullptr;
    }
    if (!m_rootQmlObject) {
        Q_EMIT debugLog(QStringLiteral("createOverlay: no rootQmlObject wired"));
        return nullptr;
    }
    if (source.isEmpty()) {
        Q_EMIT debugLog(QStringLiteral("createOverlay: empty source"));
        return nullptr;
    }

    QVariant result;
    const bool ok = QMetaObject::invokeMethod(m_rootQmlObject.data(),
                                              "createIconOverlay",
                                              Qt::DirectConnection,
                                              Q_RETURN_ARG(QVariant, result),
                                              Q_ARG(QVariant, QVariant::fromValue(target)),
                                              Q_ARG(QVariant, QVariant::fromValue(source)));
    if (!ok) {
        Q_EMIT debugLog(QStringLiteral("createOverlay: invokeMethod failed"));
        return nullptr;
    }

    QObject *overlay = result.value<QObject *>();
    if (!overlay) {
        Q_EMIT debugLog(QStringLiteral("createOverlay: QML returned null overlay"));
        return nullptr;
    }

    Q_EMIT debugLog(QStringLiteral("createOverlay OK target=[%1] overlay=[%2] source=[%3]")
                        .arg(objectSummary(target),
                             objectSummary(overlay),
                             clippedSource(source)));
    return overlay;
}

bool LiveReplacementController::destroyOverlay(QObject *overlay)
{
    if (!overlay) {
        return false;
    }
    if (!m_rootQmlObject) {
        overlay->deleteLater();
        return true;
    }

    QVariant result;
    const bool ok = QMetaObject::invokeMethod(m_rootQmlObject.data(),
                                              "destroyIconOverlay",
                                              Qt::DirectConnection,
                                              Q_RETURN_ARG(QVariant, result),
                                              Q_ARG(QVariant, QVariant::fromValue(overlay)));
    if (!ok) {
        overlay->deleteLater();
        return false;
    }
    return result.toBool();
}

void LiveReplacementController::disconnectTargetGuard(Record &record)
{
    if (record.targetDestroyedConnection) {
        QObject::disconnect(record.targetDestroyedConnection);
        record.targetDestroyedConnection = {};
    }
}

bool LiveReplacementController::registerReplacement(const QString &stableId, QObject *target,
                                                    const QString &replacementSource,
                                                    const QString &replacementType,
                                                    const QString &replacementValue,
                                                    bool isStatusNotifier,
                                                    const QString &service,
                                                    const QString &path)
{
    if (stableId.isEmpty() || !target || replacementSource.isEmpty()) {
        Q_EMIT debugLog(QStringLiteral("registerReplacement refused: empty id/source/null target"));
        return false;
    }

    if (m_records.contains(stableId)) {
        return updateReplacement(stableId, replacementType, replacementValue, replacementSource);
    }

    Record record;
    record.stableId = stableId;
    record.target = target;
    record.originalSource = sourceText(targetSourceValue(target));
    record.replacementSource = replacementSource;
    record.replacementType = replacementType;
    record.replacementValue = replacementValue;
    record.isStatusNotifier = isStatusNotifier;
    record.service = service;
    record.path = path;

    QObject *overlay = createOverlay(target, overlaySourceForRecord(record));
    if (!overlay) {
        Q_EMIT debugLog(QStringLiteral("registerReplacement: overlay creation failed stableId=%1").arg(stableId));
        return false;
    }
    record.overlay = overlay;
    configureOverlay(record);

    const QString capturedStableId = stableId;
    record.targetDestroyedConnection = QObject::connect(
        target, &QObject::destroyed, this, [this, capturedStableId]() {
            auto it = m_records.find(capturedStableId);
            if (it == m_records.end()) {
                return;
            }
            if (it.value().overlay) {
                destroyOverlay(it.value().overlay);
            }
            it.value().target.clear();
            it.value().overlay.clear();
            it.value().targetDestroyedConnection = {};
            Q_EMIT debugLog(QStringLiteral("target destroyed stableId=%1, requesting retarget").arg(capturedStableId));
            Q_EMIT needsRetarget(capturedStableId);
            Q_EMIT recordsChanged();
        });

    m_records.insert(stableId, record);

    Q_EMIT debugLog(QStringLiteral("registerReplacement OK stableId=%1 original=[%2] overlay source=[%3] type=%4 isSNI=%5")
                        .arg(stableId,
                             clippedSource(record.originalSource),
                             clippedSource(overlaySourceForRecord(record)),
                             record.replacementType,
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

    Record &record = it.value();
    if (record.target.isNull()) {
        Q_EMIT debugLog(QStringLiteral("updateReplacement: target gone, dropping record %1").arg(stableId));
        disconnectTargetGuard(record);
        if (record.overlay) {
            destroyOverlay(record.overlay);
            record.overlay = nullptr;
        }
        m_records.erase(it);
        Q_EMIT recordsChanged();
        return false;
    }

    record.replacementType = replacementType;
    record.replacementValue = replacementValue;
    record.replacementSource = replacementSource;

    if (record.overlay.isNull()) {
        QObject *newOverlay = createOverlay(record.target, overlaySourceForRecord(record));
        if (!newOverlay) {
            Q_EMIT debugLog(QStringLiteral("updateReplacement: overlay recreation failed stableId=%1").arg(stableId));
            return false;
        }
        record.overlay = newOverlay;
        configureOverlay(record);
        Q_EMIT debugLog(QStringLiteral("updateReplacement: overlay recreated stableId=%1 source=[%2]")
                            .arg(stableId, clippedSource(overlaySourceForRecord(record))));
        return true;
    }

    if (configureOverlay(record)) {
        Q_EMIT debugLog(QStringLiteral("updateReplacement OK stableId=%1 overlay source=[%2]")
                            .arg(stableId, clippedSource(overlaySourceForRecord(record))));
    }
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
            disconnectTargetGuard(record);
            if (record.overlay) {
                destroyOverlay(record.overlay);
            }
            record.overlay = nullptr;
            retargets << record.stableId;
            it = m_records.erase(it);
            continue;
        }

        if (record.overlay.isNull()) {
            QObject *newOverlay = createOverlay(record.target, overlaySourceForRecord(record));
            if (!newOverlay) {
                Q_EMIT debugLog(QStringLiteral("reassert stableId=%1 overlay recreation failed").arg(record.stableId));
                ++it;
                continue;
            }
            record.overlay = newOverlay;
            configureOverlay(record);
            ++reasserted;
            ++it;
            continue;
        }

        if (record.target->property("opacity").toDouble() != 0.0) {
            record.target->setProperty("opacity", 0.0);
        }

        if (configureOverlay(record)) {
            ++reasserted;
        }
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

    Record &record = it.value();
    if (record.target == newTarget) {
        return true;
    }

    disconnectTargetGuard(record);

    if (record.overlay) {
        destroyOverlay(record.overlay);
        record.overlay = nullptr;
    }

    record.target = newTarget;

    QObject *overlay = createOverlay(newTarget, overlaySourceForRecord(record));
    if (!overlay) {
        Q_EMIT debugLog(QStringLiteral("retarget stableId=%1 overlay creation failed").arg(stableId));
        return false;
    }
    record.overlay = overlay;
    configureOverlay(record);

    const QString capturedStableId = stableId;
    record.targetDestroyedConnection = QObject::connect(
        newTarget, &QObject::destroyed, this, [this, capturedStableId]() {
            auto it = m_records.find(capturedStableId);
            if (it == m_records.end()) {
                return;
            }
            if (it.value().overlay) {
                destroyOverlay(it.value().overlay);
            }
            it.value().target.clear();
            it.value().overlay.clear();
            it.value().targetDestroyedConnection = {};
            Q_EMIT debugLog(QStringLiteral("retarget target destroyed stableId=%1").arg(capturedStableId));
            Q_EMIT needsRetarget(capturedStableId);
            Q_EMIT recordsChanged();
        });

    Q_EMIT debugLog(QStringLiteral("retarget OK stableId=%1").arg(stableId));
    return true;
}

bool LiveReplacementController::restore(const QString &stableId)
{
    auto it = m_records.find(stableId);
    if (it == m_records.end()) {
        Q_EMIT debugLog(QStringLiteral("restore: no record for %1").arg(stableId));
        return false;
    }

    Record record = it.value();
    disconnectTargetGuard(it.value());

    bool ok = false;
    if (record.overlay) {
        ok = destroyOverlay(record.overlay);
        Q_EMIT debugLog(QStringLiteral("restore stableId=%1 overlay destroyed ok=%2")
                            .arg(stableId, ok ? QStringLiteral("true") : QStringLiteral("false")));
    } else {
        Q_EMIT debugLog(QStringLiteral("restore stableId=%1 no overlay; nothing to remove").arg(stableId));
    }

    m_records.erase(it);
    Q_EMIT recordsChanged();
    return ok;
}

void LiveReplacementController::restoreAll()
{
    for (auto it = m_records.begin(); it != m_records.end(); ++it) {
        Record &record = it.value();
        disconnectTargetGuard(record);
        if (record.overlay) {
            destroyOverlay(record.overlay);
            record.overlay = nullptr;
        }
    }
    m_records.clear();
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
    disconnectTargetGuard(it.value());
    if (it.value().overlay) {
        destroyOverlay(it.value().overlay);
        it.value().overlay = nullptr;
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
