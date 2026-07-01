#include "trayitemmodel.h"

#include <QCoreApplication>
#include <QTextStream>

namespace
{
QString roleValue(const TrayItemModel &model, int row, int role)
{
    return model.data(model.index(row, 0), role).toString();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    TrayItemModel model;
    QTextStream out(stdout);

    const QStringList args = app.arguments();
    if (args.size() == 4 && args.at(1) == QStringLiteral("--add-stable")) {
        const QString stableId = args.at(2);
        const QString replacementIcon = args.at(3);

        for (int row = 0; row < model.rowCount(); ++row) {
            if (roleValue(model, row, TrayItemModel::StableIdRole) != stableId) {
                continue;
            }

            const bool added = model.addReplacementRule(row, QStringLiteral("themeIcon"), replacementIcon);
            out << (added ? "Added" : "Failed") << " rule for " << stableId << '\n';
            return added ? 0 : 1;
        }

        out << "No item with stableId " << stableId << '\n';
        return 1;
    }

    out << "StatusNotifierItems: " << model.rowCount() << '\n';

    for (int row = 0; row < model.rowCount(); ++row) {
        out << "- "
            << roleValue(model, row, TrayItemModel::StableIdRole)
            << " | title: " << roleValue(model, row, TrayItemModel::TitleRole)
            << " | icon: " << roleValue(model, row, TrayItemModel::IconNameRole)
            << " | hasPixmap: " << roleValue(model, row, TrayItemModel::HasPixmapIconRole)
            << " | hash: " << roleValue(model, row, TrayItemModel::IconHashRole)
            << " | pixmapDataUrl: " << roleValue(model, row, TrayItemModel::PixmapDataUrlRole).left(40) << "..."
            << " | replacement: " << roleValue(model, row, TrayItemModel::ReplacementTypeRole)
            << ":" << roleValue(model, row, TrayItemModel::ReplacementIconRole)
            << " | replacementAvailable: " << roleValue(model, row, TrayItemModel::ReplacementAvailableRole)
            << " | replacementStatus: " << roleValue(model, row, TrayItemModel::ReplacementStatusRole)
            << " | status: " << roleValue(model, row, TrayItemModel::StatusRole)
            << " | category: " << roleValue(model, row, TrayItemModel::CategoryRole)
            << " | servicePath: " << roleValue(model, row, TrayItemModel::ServicePathRole)
            << '\n';
    }

    return 0;
}
