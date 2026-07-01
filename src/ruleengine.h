#pragma once

#include "trayitem.h"

#include <QString>
#include <QVector>

class RuleEngine
{
public:
    void reload();
    void apply(QVector<TrayItem> &items) const;
    bool addRuleForItem(const TrayItem &item, const QString &replacementType, const QString &replacementValue);
    bool removeRulesForItem(const TrayItem &item);
    bool clear();

private:
    struct Rule {
        bool enabled = false;
        QString matchType;
        QString matchValue;
        QString replacementType;
        QString replacementValue;
    };

    QString rulesPath() const;
    bool save() const;
    bool matches(const Rule &rule, const TrayItem &item) const;

    QVector<Rule> m_rules;
};
