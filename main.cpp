#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include "database.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv); 
    const QString dbPath = QDir(QCoreApplication::applicationDirPath()).filePath("trips.db");
    QString error;
    if (!Db::open(dbPath, &error) || !Db::createSchema(&error) || !Db::seedIfEmpty(&error)) {
        QMessageBox::critical(nullptr, "Командировки",
                              "Не удалось открыть базу данных:\n" + error);
        return 1;
    }
    MainWindow window;
    window.show();
    return app.exec();
}
