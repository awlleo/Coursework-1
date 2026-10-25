#pragma once
// Главное окно – «кнопочная форма»: слева кнопки запуска макроса, запросов и форм,
// справа – таблица с результатом последнего запроса.

#include <QMainWindow>
#include <functional>

class QLabel;
class QSqlQueryModel;
class QSqlQuery;
class QTableView;
class TableForm;
class TripForm;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Выполнить запрос и показать результат в таблице справа.
    // Функция reload запоминается, чтобы обновить результат после правки данных.
    void showResult(const QString &title, const std::function<QSqlQuery()> &makeQuery);

private slots:
    void runMacro();
    void runCrossTab();
    void runParamQuery();
    void runMoscowQuery();
    void openTripForm();
    void openDepartmentsForm();
    void openWorkersForm();
    void resetData();
    void refreshResult();

private:
    QLabel *title_;
    QTableView *view_;
    QSqlQueryModel *model_;
    std::function<QSqlQuery()> lastQuery_;
    QString lastTitle_;

    TripForm *tripForm_ = nullptr;
    TableForm *departmentsForm_ = nullptr;
    TableForm *workersForm_ = nullptr;
};
