#pragma once

#include <QString>

namespace KTrayMorph
{
void configureLogging(bool enabled, const QString &path);
void debugLog(const QString &message);
}
