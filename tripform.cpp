#include "tripform.h"
#include "database.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

namespace {
const char *kIso = "yyyy-MM-dd"; // формат хранения дат в БД
const char *kRu  = "dd.MM.yyyy"; // формат отображения дат пользователю

// Номера столбцов результата запроса, который показывается в таблице формы.
enum Column { ColId, ColTripDateRaw, ColTripDate, ColWorkerId, ColFio, ColCity,
              ColDepartureRaw, ColDeparture, ColDays, ColAllowance, ColRoad, ColTotal };
}

TripForm::TripForm(QWidget *parent)
    : QDialog(parent), model_(new QSqlQueryModel(this))
{
    setWindowTitle("КОМАНДИРОВКИ");
    setWindowFlag(Qt::WindowMaximizeButtonHint);
    resize(980, 640);

    // --- Поля ввода -------------------------------------------------------------------
    tripDate_  = new QDateEdit(QDate::currentDate(), this);
    departure_ = new QDateEdit(QDate::currentDate(), this);
    for (QDateEdit *edit : {tripDate_, departure_}) {
        edit->setDisplayFormat(kRu);
        edit->setCalendarPopup(true);
    }

    worker_ = new QComboBox(this);            // поле со списком
    city_ = new QLineEdit(this);
    city_->setPlaceholderText("Например: Москва");

    days_ = new QSpinBox(this);
    days_->setRange(1, 365);
    days_->setSuffix(" дн.");
    allowance_ = new QSpinBox(this);
    allowance_->setRange(0, 1000000);
    allowance_->setSuffix(" руб.");
    roadCost_ = new QSpinBox(this);
    roadCost_->setRange(0, 10000000);
    roadCost_->setSuffix(" руб.");

    total_ = new QLineEdit(this);
    total_->setReadOnly(true);

    auto *form = new QFormLayout;
    form->addRow("Дата оформления:", tripDate_);
    form->addRow("Сотрудник:", worker_);
    form->addRow("Город:", city_);
    form->addRow("Дата выезда:", departure_);
    form->addRow("Количество дней:", days_);
    form->addRow("Суточные (в день):", allowance_);
    form->addRow("Стоимость дороги (в одну сторону):", roadCost_);
    form->addRow("Общая стоимость:", total_);

    // --- Кнопки -------------------------------------------------------------------------
    auto *newButton    = new QPushButton("Новая запись (Ctrl+N)", this);
    auto *saveButton   = new QPushButton("Сохранить (Ctrl+S)", this);
    auto *deleteButton = new QPushButton("Удалить (Ctrl+D)", this);
    auto *closeButton  = new QPushButton("Закрыть", this);
    newButton->setShortcut(QKeySequence("Ctrl+N"));
    saveButton->setShortcut(QKeySequence("Ctrl+S"));
    deleteButton->setShortcut(QKeySequence("Ctrl+D"));

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(newButton);
    buttons->addWidget(saveButton);
    buttons->addWidget(deleteButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);

    // --- Таблица ------------------------------------------------------------------------
    view_ = new QTableView(this);
    view_->setModel(model_);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers); // правка – через поля формы
    view_->setAlternatingRowColors(true);
    view_->horizontalHeader()->setStretchLastSection(true);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
    layout->addWidget(view_, 1);

    // --- Связь сигналов со слотами ------------------------------------------------------
    connect(newButton,    &QPushButton::clicked, this, &TripForm::newRecord);
    connect(saveButton,   &QPushButton::clicked, this, &TripForm::saveRecord);
    connect(deleteButton, &QPushButton::clicked, this, &TripForm::deleteRecord);
    connect(closeButton,  &QPushButton::clicked, this, &QDialog::close);

    // Любое изменение числовых полей сразу пересчитывает общую стоимость.
    connect(days_,      &QSpinBox::valueChanged, this, &TripForm::updateTotal);
    connect(allowance_, &QSpinBox::valueChanged, this, &TripForm::updateTotal);
    connect(roadCost_,  &QSpinBox::valueChanged, this, &TripForm::updateTotal);

    reload();
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
                showCurrentRecord(current);
            });
    newRecord();
}

void TripForm::reload()
{
    fillWorkers();
    refreshTable();
}

// Заполнение поля со списком: текст – ФИО сотрудника, данные элемента – его код.
void TripForm::fillWorkers()
{
    const QVariant selected = worker_->currentData();
    worker_->clear();
    QSqlQuery q("SELECT id, surname || ' ' || name || ' ' || COALESCE(patronymic, '')"
                " FROM workers ORDER BY surname, name");
    while (q.next())
        worker_->addItem(q.value(1).toString().trimmed(), q.value(0));
    const int index = worker_->findData(selected);
    if (index >= 0)
        worker_->setCurrentIndex(index);
}

// Загрузка таблицы командировок; при необходимости выделяется запись с заданным ключом.
void TripForm::refreshTable(int selectId, const QDate &selectDate)
{
    // Столбцы с «сырыми» значениями (код, даты в формате БД) скрываются – они нужны программе.
    model_->setQuery(
        "SELECT t.id, t.trip_date, strftime('%d.%m.%Y', t.trip_date),"
        " t.worker_id,"
        " w.surname || ' ' || w.name || ' ' || COALESCE(w.patronymic, ''),"
        " t.city, t.departure_date, strftime('%d.%m.%Y', t.departure_date),"
        " t.days, t.allowance, t.road_cost, t.total_cost"
        " FROM trips t JOIN workers w ON w.id = t.worker_id"
        " ORDER BY t.id, t.trip_date");
    if (model_->lastError().isValid())
        QMessageBox::warning(this, windowTitle(), model_->lastError().text());

    const QStringList headers = {"Код", "", "Дата", "", "Сотрудник", "Город", "",
                                 "Дата выезда", "Дней", "Суточные", "Дорога", "Общая стоимость"};
    for (int i = 0; i < headers.size(); ++i)
        model_->setHeaderData(i, Qt::Horizontal, headers.at(i));
    for (int hidden : {ColTripDateRaw, ColWorkerId, ColDepartureRaw})
        view_->hideColumn(hidden);
    view_->resizeColumnsToContents();

    if (selectId >= 0) {
        for (int row = 0; row < model_->rowCount(); ++row) {
            if (model_->data(model_->index(row, ColId)).toInt() == selectId
                && model_->data(model_->index(row, ColTripDateRaw)).toString()
                   == selectDate.toString(kIso)) {
                view_->selectRow(row);
                return;
            }
        }
    }
}

void TripForm::updateTotal()
{
    total_->setText(QString::number(
        Db::tripTotal(days_->value(), allowance_->value(), roadCost_->value())));
}

// Очистка полей для ввода новой записи.
void TripForm::newRecord()
{
    isNew_ = true;
    view_->clearSelection();
    tripDate_->setDate(QDate::currentDate());
    departure_->setDate(QDate::currentDate());
    if (worker_->count() > 0)
        worker_->setCurrentIndex(0);
    city_->clear();
    days_->setValue(1);
    allowance_->setValue(0);
    roadCost_->setValue(0);
    updateTotal();
    city_->setFocus();
}

// Показ выбранной в таблице записи в полях формы.
void TripForm::showCurrentRecord(const QModelIndex &current)
{
    if (!current.isValid())
        return;
    const int row = current.row();
    auto value = [&](int column) { return model_->data(model_->index(row, column)); };

    isNew_ = false;
    currentId_ = value(ColId).toInt();
    currentDate_ = QDate::fromString(value(ColTripDateRaw).toString(), kIso);

    tripDate_->setDate(currentDate_);
    worker_->setCurrentIndex(worker_->findData(value(ColWorkerId)));
    city_->setText(value(ColCity).toString());
    departure_->setDate(QDate::fromString(value(ColDepartureRaw).toString(), kIso));
    days_->setValue(value(ColDays).toInt());
    allowance_->setValue(value(ColAllowance).toInt());
    roadCost_->setValue(value(ColRoad).toInt());
    updateTotal();
}

void TripForm::saveRecord()
{
    if (worker_->currentIndex() < 0) {
        QMessageBox::warning(this, windowTitle(), "Выберите сотрудника из списка.");
        return;
    }
    if (city_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), "Укажите город командировки.");
        city_->setFocus();
        return;
    }

    const int total = Db::tripTotal(days_->value(), allowance_->value(), roadCost_->value());
    QSqlQuery q;
    int id = currentId_;

    if (isNew_) {
        // Код новой командировки – на единицу больше максимального существующего.
        q.exec("SELECT COALESCE(MAX(id), 0) + 1 FROM trips");
        q.next();
        id = q.value(0).toInt();
        q.prepare("INSERT INTO trips (id, trip_date, worker_id, city, departure_date,"
                  " days, allowance, road_cost, total_cost) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(id);
    } else {
        q.prepare("UPDATE trips SET trip_date = ?, worker_id = ?, city = ?, departure_date = ?,"
                  " days = ?, allowance = ?, road_cost = ?, total_cost = ?"
                  " WHERE id = ? AND trip_date = ?");
    }
    q.addBindValue(tripDate_->date().toString(kIso));
    q.addBindValue(worker_->currentData());
    q.addBindValue(city_->text().trimmed());
    q.addBindValue(departure_->date().toString(kIso));
    q.addBindValue(days_->value());
    q.addBindValue(allowance_->value());
    q.addBindValue(roadCost_->value());
    q.addBindValue(total);
    if (!isNew_) {
        q.addBindValue(currentId_);
        q.addBindValue(currentDate_.toString(kIso));
    }

    if (!q.exec()) {
        QMessageBox::warning(this, windowTitle(),
            "Не удалось сохранить запись.\n\n" + q.lastError().text());
        return;
    }
    refreshTable(id, tripDate_->date());
    emit dataChanged();
}

void TripForm::deleteRecord()
{
    if (isNew_) {
        QMessageBox::information(this, windowTitle(), "Выберите в таблице запись для удаления.");
        return;
    }
    if (QMessageBox::question(this, windowTitle(), "Удалить выбранную командировку?")
        != QMessageBox::Yes)
        return;

    QSqlQuery q;
    q.prepare("DELETE FROM trips WHERE id = ? AND trip_date = ?");
    q.addBindValue(currentId_);
    q.addBindValue(currentDate_.toString(kIso));
    if (!q.exec()) {
        QMessageBox::warning(this, windowTitle(), q.lastError().text());
        return;
    }
    refreshTable();
    newRecord();
    emit dataChanged();
}
