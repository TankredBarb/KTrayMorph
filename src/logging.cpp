#include "logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace
{
QMutex &logMutex()
{
    static QMutex mutex;
    return mutex;
}

bool &loggingEnabled()
{
    static bool enabled = false;
    return enabled;
}

QString &loggingPath()
{
    static QString path;
    return path;
}
}

namespace KTrayMorph
{
void configureLogging(bool enabled, const QString &path)
{
    QMutexLocker locker(&logMutex());
    loggingEnabled() = enabled;
    loggingPath() = path.trimmed();
}

void debugLog(const QString &message)
{
    QMutexLocker locker(&logMutex());
    if (!loggingEnabled() || loggingPath().isEmpty()) {
        return;
    }

    QDir dir(QFileInfo(loggingPath()).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return;
    }

    QFile file(loggingPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream(&file) << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                        << " " << message << '\n';
}
}
