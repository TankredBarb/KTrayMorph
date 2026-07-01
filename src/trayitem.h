#pragma once

#include <QString>

struct TrayItem {
    QString stableId;
    QString kind;
    QString title;
    QString iconName;
    QString status;
    QString category;
    QString service;
    QString path;
    QString iconHash;
    QString replacementIcon;
    QString replacementType;
    QString replacementStatus;
    QString pixmapDataUrl;
    bool hasPixmapIcon = false;
    bool hasReplacement = false;
    bool replacementAvailable = true;
};
