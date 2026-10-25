#include "mainwindow.h"
#include "database.h"
#include "tableform.h"
#include "tripform.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), model_(new QSqlQueryModel(this))
{
    setWindowTitle("Кнопочная форма: Командировки");
    resize(900, 560);

    // --- Левая панель с кнопками ----------------------------------
    auto *macroButton   = new QPushButton("Запуск макроса (Пересчет)", this);
    auto *crossButton   = new QPushButton("Перекрестный запрос", this);
    auto *paramButton   = new QPushButton("Запрос с параметром", this);
    auto *tripsButton   = new QPushButton("Форма «КОМАНДИРОВКИ»", this);
    auto *exitButton    = new QPushButton("Выход", this);

    auto *buttons = new QVBoxLayout;
    buttons->addWidget(macroButton);
    buttons->addWidget(crossButton);
    buttons->addWidget(paramButton);
    buttons->addWidget(tripsButton);
    buttons->addStretch();
    buttons->addWidget(exitButton);

    auto *buttonPanel = new QWidget(this);
    buttonPanel->setLayout(buttons);
    buttonPanel->setFixedWidth(240);

    // --- Правая часть: заголовок и таблица результата ---------------------------------
    title_ = new QLabel(this);
    QFont bold = title_->font();
    bold.setBold(true);
    title_->setFont(bold);

    view_ = new QTableView(this);
    view_->setModel(model_);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers); // результат запроса только читается
    view_->setAlternatingRowColors(true);
    view_->horizontalHeader()->setStretchLastSection(true);

    auto *right = new QVBoxLayout;
    right->addWidget(title_);
    right->addWidget(view_, 1);

    auto *central = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->addWidget(buttonPanel);
    mainLayout->addLayout(right, 1);
    setCentralWidget(central);

    // --- Меню (доступ к остальным формам и запросам) ----------------------------------
    QMenu *dataMenu = menuBar()->addMenu("Данные");
    dataMenu->addAction("Отделы…", this, &MainWindow::openDepartmentsForm);
    dataMenu->addAction("Сотрудники…", this, &MainWindow::openWorkersForm);
    dataMenu->addAction("Командировки…", this, &MainWindow::openTripForm);
    dataMenu->addSeparator();
    dataMenu->addAction("Восстановить тестовые данные", this, &MainWindow::resetData);
    dataMenu->addSeparator();
    dataMenu->addAction("Выход", this, &QWidget::close, QKeySequence("Ctrl+Q"));

    QMenu *queryMenu = menuBar()->addMenu("Запросы");
    queryMenu->addAction("Сотрудники, отправленные в г. Москва", this, &MainWindow::runMoscowQuery);
    queryMenu->addAction("Перекрестный запрос", this, &MainWindow::runCrossTab);
    queryMenu->addAction("Запрос с параметром (по ФИО)", this, &MainWindow::runParamQuery);

    // --- Связь сигналов со слотами ------------------------------------------------------
    connect(macroButton, &QPushButton::clicked, this, &MainWindow::runMacro);
    connect(crossButton, &QPushButton::clicked, this, &MainWindow::runCrossTab);
    connect(paramButton, &QPushButton::clicked, this, &MainWindow::runParamQuery);
    connect(tripsButton, &QPushButton::clicked, this, &MainWindow::openTripForm);
    connect(exitButton,  &QPushButton::clicked, this, &QWidget::close);

    // Сочетания клавиш для быстрой работы.
    new QShortcut(QKeySequence("Ctrl+W"), this, SLOT(close()));
    new QShortcut(QKeySequence("1"), this, [this] { runMacro(); });
    new QShortcut(QKeySequence("2"), this, [this] { runCrossTab(); });
    new QShortcut(QKeySequence("3"), this, [this] { runParamQuery(); });
    new QShortcut(QKeySequence("4"), this, [this] { openTripForm(); });

    // При запуске показываем запрос 3 – сотрудников, отправленных в Москву.
    runMoscowQuery();
}

void MainWindow::showResult(const QString &title, const std::function<QSqlQuery()> &makeQuery)
{
    lastTitle_ = title;
    lastQuery_ = makeQuery;
    title_->setText(title);
    model_->setQuery(makeQuery());
    if (model_->lastError().isValid())
        QMessageBox::warning(this, windowTitle(), model_->lastError().text());
    view_->resizeColumnsToContents();
    statusBar()->showMessage(QString("Записей: %1").arg(model_->rowCount()));
}

void MainWindow::refreshResult()
{
    if (lastQuery_)
        showResult(lastTitle_, lastQuery_);
}

// Пункт 1 задания: макрос – пересчёт общей стоимости всех командировок.
void MainWindow::runMacro()
{
    QString error;
    const int count = Db::recalcTotals(&error);
    if (count < 0) {
        QMessageBox::warning(this, "Макрос", "Ошибка выполнения макроса:\n" + error);
        return;
    }
    QSqlQuery q("SELECT COALESCE(SUM(total_cost), 0) FROM trips");
    q.next();
    QMessageBox::information(this, "Макрос",
        QString("Расчёт выполнен по формуле:\nколичество дней * суточные + 2 * стоимость дороги.\n\n"
                "Пересчитано командировок: %1\nОбщая стоимость всех командировок: %2 руб.")
            .arg(count).arg(q.value(0).toLongLong()));

    // Показываем результат пересчёта: таблица командировок с итоговой стоимостью.
    showResult("Результат макроса: общая стоимость командировок", [] {
        QSqlQuery q;
        q.exec("SELECT t.id AS \"Код\", strftime('%d.%m.%Y', t.trip_date) AS \"Дата\","
               " w.surname || ' ' || w.name AS \"Сотрудник\", t.city AS \"Город\","
               " t.days AS \"Дней\", t.allowance AS \"Суточные\", t.road_cost AS \"Дорога\","
               " t.total_cost AS \"Общая стоимость\""
               " FROM trips t JOIN workers w ON w.id = t.worker_id ORDER BY t.id");
        return q;
    });
}

// Пункт 3 задания: выборка сотрудников, отправленных в командировку в г. Москва.
void MainWindow::runMoscowQuery()
{
    showResult("Сотрудники, отправленные в командировку в г. Москва:",
               [] { return Db::tripsByCityQuery("Москва"); });
}

// Пункт 4 задания: перекрёстный запрос.
void MainWindow::runCrossTab()
{
    showResult("Перекрестный запрос: общая стоимость командировок по сотрудникам и городам, руб.",
               [] { return Db::crossTabQuery(); });
}

// Пункт 5 задания: запрос с параметром по ФИО командированного.
void MainWindow::runParamQuery()
{
    bool ok = false;
    const QString fio = QInputDialog::getText(this, "Запрос с параметром",
        "Введите ФИО командированного\n(например: Иванов или Иванов И.И.):",
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || fio.isEmpty())
        return;
    showResult(QString("Командировки сотрудника: %1").arg(fio),
               [fio] { return Db::tripsByFioQuery(fio); });
}

void MainWindow::openTripForm()
{
    if (!tripForm_) {
        tripForm_ = new TripForm(this);
        connect(tripForm_, &TripForm::dataChanged, this, &MainWindow::refreshResult);
    }
    tripForm_->reload();
    tripForm_->show();
    tripForm_->raise();
    tripForm_->activateWindow();
}

void MainWindow::openDepartmentsForm()
{
    if (!departmentsForm_) {
        departmentsForm_ = new TableForm("ОТДЕЛЫ", "departments",
                                         {"Код отдела", "Наименование отдела", "Телефон"}, this);
        connect(departmentsForm_, &TableForm::dataChanged, this, &MainWindow::refreshResult);
    }
    departmentsForm_->load();
    departmentsForm_->show();
    departmentsForm_->raise();
}

void MainWindow::openWorkersForm()
{
    if (!workersForm_) {
        workersForm_ = new TableForm("СОТРУДНИКИ", "workers",
            {"Код сотрудника", "Фамилия", "Имя", "Отчество", "Отдел", "Адрес", "Телефон"}, this);
        // Столбец 4 (код отдела) – внешний ключ: в ячейке показываем название отдела.
        workersForm_->setRelation(4, QSqlRelation("departments", "id", "name"));
        connect(workersForm_, &TableForm::dataChanged, this, &MainWindow::refreshResult);
    }
    workersForm_->load();
    workersForm_->show();
    workersForm_->raise();
}

void MainWindow::resetData()
{
    if (QMessageBox::question(this, windowTitle(),
            "Все текущие данные будут удалены и заменены тестовыми. Продолжить?")
        != QMessageBox::Yes)
        return;
    QString error;
    if (!Db::resetDemoData(&error)) {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    if (tripForm_)
        tripForm_->reload();
    if (departmentsForm_)
        departmentsForm_->load();
    if (workersForm_)
        workersForm_->load();
    runMoscowQuery();
}
