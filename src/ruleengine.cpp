#include "ruleengine.h"
#include "logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>

void RuleEngine::reload()
{
    m_rules.clear();

    QFile file(rulesPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return;
    }

    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        Rule rule;
        rule.enabled = object.value(QStringLiteral("enabled")).toBool();
        rule.matchType = object.value(QStringLiteral("matchType")).toString();
        rule.matchValue = object.value(QStringLiteral("matchValue")).toString();
        rule.replacementType = object.value(QStringLiteral("replacementType")).toString(QStringLiteral("themeIcon"));
        rule.replacementValue = object.value(QStringLiteral("replacementValue")).toString();

        if (rule.enabled
            && !rule.matchType.isEmpty()
            && !rule.matchValue.isEmpty()
            && !rule.replacementType.isEmpty()
            && !rule.replacementValue.isEmpty()) {
            m_rules.push_back(std::move(rule));
        }
    }
}

void RuleEngine::apply(QVector<TrayItem> &items) const
{
    for (TrayItem &item : items) {
        item.replacementIcon.clear();
        item.replacementType.clear();
        item.replacementStatus.clear();
        item.replacementAvailable = true;
        item.hasReplacement = false;

        for (const Rule &rule : m_rules) {
            if (!matches(rule, item)) {
                continue;
            }

            item.replacementIcon = rule.replacementValue;
            item.replacementType = rule.replacementType;
            item.replacementAvailable = true;
            item.replacementStatus.clear();
            if (rule.replacementType == QStringLiteral("localFile")) {
                const QFileInfo fileInfo(rule.replacementValue);
                if (!fileInfo.exists()) {
                    item.replacementAvailable = false;
                    item.replacementStatus = QStringLiteral("Missing file");
                } else if (!fileInfo.isFile() || !fileInfo.isReadable()) {
                    item.replacementAvailable = false;
                    item.replacementStatus = QStringLiteral("File is not readable");
                }
            } else if (rule.replacementType == QStringLiteral("colorTint")) {
                if (item.hasPixmapIcon) {
                    item.replacementAvailable = false;
                    item.replacementStatus = QStringLiteral("Color tint is not supported for pixmap icons");
                } else if (!QColor::isValidColorName(rule.replacementValue)) {
                    item.replacementAvailable = false;
                    item.replacementStatus = QStringLiteral("Invalid color");
                }
            }
            item.hasReplacement = true;
            break;
        }
    }
}

bool RuleEngine::addRuleForItem(const TrayItem &item, const QString &replacementType, const QString &replacementValue)
{
    const QString normalizedType = replacementType.trimmed().isEmpty()
        ? QStringLiteral("themeIcon")
        : replacementType.trimmed();
    const QString normalizedReplacement = replacementValue.trimmed();
    if (normalizedReplacement.isEmpty()) {
        return false;
    }

    if (normalizedType != QStringLiteral("themeIcon")
        && normalizedType != QStringLiteral("localFile")
        && normalizedType != QStringLiteral("colorTint")) {
        return false;
    }

    if (normalizedType == QStringLiteral("localFile")) {
        const QFileInfo fileInfo(normalizedReplacement);
        if (!fileInfo.isFile() || !fileInfo.isReadable()) {
            return false;
        }
    } else if (normalizedType == QStringLiteral("colorTint")) {
        if (item.hasPixmapIcon || !QColor::isValidColorName(normalizedReplacement)) {
            return false;
        }
    }

    Rule rule;
    rule.enabled = true;
    rule.replacementType = normalizedType;
    rule.replacementValue = normalizedReplacement;

    if (!item.stableId.isEmpty()) {
        rule.matchType = QStringLiteral("stableId");
        rule.matchValue = item.stableId;
    } else if (item.hasPixmapIcon && !item.iconHash.isEmpty()) {
        rule.matchType = QStringLiteral("iconHash");
        rule.matchValue = item.iconHash;
    } else if (!item.title.isEmpty()) {
        rule.matchType = QStringLiteral("title");
        rule.matchValue = item.title;
    }

    if (rule.matchType.isEmpty() || rule.matchValue.isEmpty()) {
        return false;
    }

    m_rules.erase(std::remove_if(m_rules.begin(),
                                 m_rules.end(),
                                 [this, &item](const Rule &existingRule) {
                                     return matches(existingRule, item);
                                 }),
                  m_rules.end());

    KTrayMorph::debugLog(QStringLiteral("RuleEngine::addRuleForItem pushing rule matchType=%1 matchValue=%2 replacementType=%3 replacementValue=%4")
                             .arg(rule.matchType, rule.matchValue, rule.replacementType, rule.replacementValue));
    m_rules.push_back(std::move(rule));
    KTrayMorph::debugLog(QStringLiteral("RuleEngine::addRuleForItem m_rules size after push=%1").arg(m_rules.size()));
    return save();
}

bool RuleEngine::removeRulesForItem(const TrayItem &item)
{
    const auto oldSize = m_rules.size();
    m_rules.erase(std::remove_if(m_rules.begin(),
                                 m_rules.end(),
                                 [this, &item](const Rule &rule) {
                                     return matches(rule, item);
                                 }),
                  m_rules.end());

    if (m_rules.size() == oldSize) {
        return true;
    }

    return save();
}

bool RuleEngine::clear()
{
    m_rules.clear();
    return save();
}

QString RuleEngine::rulesPath() const
{
    const QString overridePath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("KTRAYMORPH_RULES_FILE"));
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    const QString baseConfigDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString configDir = QDir(baseConfigDir).filePath(QStringLiteral("ktraymorph"));
    if (configDir.isEmpty()) {
        return {};
    }

    return QDir(configDir).filePath(QStringLiteral("rules.json"));
}

bool RuleEngine::save() const
{
    const QString path = rulesPath();
    KTrayMorph::debugLog(QStringLiteral("RuleEngine::save saving to path=%1 rules count=%2").arg(path).arg(m_rules.size()));
    if (path.isEmpty()) {
        return false;
    }

    QDir dir(QFileInfo(path).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonArray rules;
    for (const Rule &rule : m_rules) {
        QJsonObject object;
        object.insert(QStringLiteral("enabled"), rule.enabled);
        object.insert(QStringLiteral("matchType"), rule.matchType);
        object.insert(QStringLiteral("matchValue"), rule.matchValue);
        object.insert(QStringLiteral("replacementType"), rule.replacementType);
        object.insert(QStringLiteral("replacementValue"), rule.replacementValue);
        rules.append(object);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(rules).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

bool RuleEngine::matches(const Rule &rule, const TrayItem &item) const
{
    if (rule.matchType == QStringLiteral("stableId")) {
        return item.stableId == rule.matchValue;
    }

    if (rule.matchType == QStringLiteral("sniId")) {
        return item.kind == QStringLiteral("StatusNotifier") && item.stableId == rule.matchValue;
    }

    if (rule.matchType == QStringLiteral("title")) {
        return item.title == rule.matchValue;
    }

    if (rule.matchType == QStringLiteral("iconName")) {
        return item.iconName == rule.matchValue;
    }

    if (rule.matchType == QStringLiteral("iconHash")) {
        return item.iconHash == rule.matchValue;
    }

    if (rule.matchType == QStringLiteral("servicePath")) {
        return QString(item.service + item.path) == rule.matchValue;
    }

    return false;
}
