#pragma once
// Универсальная форма ввода данных для справочных таблиц (ОТДЕЛЫ, СОТРУДНИКИ):
// таблица на экране + кнопки «Добавить», «Удалить», «Сохранить», «Отменить».

#include <QDialog>
#include <QSqlRelation>
#include <QSqlRelationalDelegate>

class QPushButton;
class QSqlRelationalTableModel;
class QTableView;
class QSqlRelationalDelegate;

class TableForm : public QDialog
{
    Q_OBJECT
public:
    // title – заголовок окна, table – имя таблицы в БД,
    // headers – подписи столбцов (по порядку столбцов таблицы, включая код)
    TableForm(const QString &title, const QString &table, const QStringList &headers,
              QWidget *parent = nullptr);

    // Столбец column является внешним ключом: вместо кода в ячейке показывается
    // значение из связанной таблицы, а при редактировании открывается поле со списком.
    void setRelation(int column, const QSqlRelation &relation);

    // Загрузка данных из БД (вызывается после настройки связей).
    void load();

signals:
    void dataChanged(); // данные в БД изменились

private slots:
    void addRow();
    void deleteRow();
    void save();
    void revert();

private:
    QSqlRelationalTableModel *model_;
    QTableView *view_;
    QStringList headers_;
};
