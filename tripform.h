#pragma once
// Форма «КОМАНДИРОВКИ»: поля ввода (в том числе поле со списком сотрудников)
// и таблица со всеми командировками. Общая стоимость считается по формуле
// «количество дней * суточные + 2 * стоимость дороги».

#include <QDialog>
#include <QDate>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QSpinBox;
class QSqlQueryModel;
class QTableView;

class TripForm : public QDialog
{
    Q_OBJECT
public:
    explicit TripForm(QWidget *parent = nullptr);

    // Перечитать список сотрудников и таблицу (после изменений в других формах).
    void reload();

signals:
    void dataChanged(); // данные в БД изменились

private slots:
    void newRecord();
    void saveRecord();
    void deleteRecord();
    void updateTotal();
    void showCurrentRecord(const QModelIndex &current);

private:
    void fillWorkers();
    void refreshTable(int selectId = -1, const QDate &selectDate = QDate());

    QDateEdit *tripDate_;
    QComboBox *worker_;       // поле со списком: показывает ФИО, хранит код сотрудника
    QLineEdit *city_;
    QDateEdit *departure_;
    QSpinBox *days_;
    QSpinBox *allowance_;
    QSpinBox *roadCost_;
    QLineEdit *total_;        // только для чтения, вычисляется автоматически

    QSqlQueryModel *model_;
    QTableView *view_;

    bool isNew_ = true;       // true – вводится новая запись, false – правится существующая
    int currentId_ = 0;       // ключ редактируемой записи: код командировки
    QDate currentDate_;       // и дата (вместе – составной первичный ключ)
};
