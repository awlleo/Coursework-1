#include "database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>

namespace {

// Выражение «Фамилия И.О.» для подстановки в SQL (alias – псевдоним таблицы workers).
QString fioExpr(const QString &alias)
{
    return alias + ".surname || ' ' || substr(" + alias + ".name, 1, 1) || '.' || "
           "CASE WHEN " + alias + ".patronymic IS NULL OR " + alias + ".patronymic = '' "
           "THEN '' ELSE substr(" + alias + ".patronymic, 1, 1) || '.' END";
}

// Выполнение одного запроса без параметров с записью текста ошибки.
bool run(QSqlQuery &q, const QString &sql, QString *error)
{
    if (q.exec(sql))
        return true;
    if (error)
        *error = q.lastError().text();
    return false;
}

// Экранирование идентификатора (имени столбца) для SQL: "..." с удвоением кавычек.
QString quoteIdent(const QString &name)
{
    QString s = name;
    s.replace('"', "\"\"");
    return '"' + s + '"';
}

} // namespace

namespace Db {

bool open(const QString &path, QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE"); // соединение по умолчанию
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }
    // В SQLite внешние ключи по умолчанию не проверяются – включаем явно.
    QSqlQuery q(db);
    return run(q, "PRAGMA foreign_keys = ON", error);
}

bool createSchema(QString *error)
{
    QSqlQuery q;
    // ОТДЕЛЫ: код отдела (ПК), наименование, телефон
    if (!run(q,
            "CREATE TABLE IF NOT EXISTS departments ("
            " id    INTEGER PRIMARY KEY AUTOINCREMENT,"
            " name  TEXT NOT NULL UNIQUE,"
            " phone TEXT)", error))
        return false;

    // СОТРУДНИКИ: код (ПК), фамилия, имя, отчество, код отдела (ВК), адрес, телефон
    if (!run(q,
            "CREATE TABLE IF NOT EXISTS workers ("
            " id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            " surname       TEXT NOT NULL,"
            " name          TEXT NOT NULL,"
            " patronymic    TEXT,"
            " department_id INTEGER NOT NULL REFERENCES departments(id),"
            " address       TEXT,"
            " phone         TEXT)", error))
        return false;

    // КОМАНДИРОВКИ: составной первичный ключ (код командировки, дата),
    // код сотрудника (ВК), город, дата выезда, дни, суточные, дорога, общая стоимость
    if (!run(q,
            "CREATE TABLE IF NOT EXISTS trips ("
            " id             INTEGER NOT NULL,"
            " trip_date      TEXT    NOT NULL,"
            " worker_id      INTEGER NOT NULL REFERENCES workers(id),"
            " city           TEXT    NOT NULL,"
            " departure_date TEXT    NOT NULL,"
            " days           INTEGER NOT NULL CHECK (days > 0),"
            " allowance      INTEGER NOT NULL CHECK (allowance >= 0),"
            " road_cost      INTEGER NOT NULL CHECK (road_cost >= 0),"
            " total_cost     INTEGER NOT NULL DEFAULT 0,"
            " PRIMARY KEY (id, trip_date))", error))
        return false;
    return true;
}

bool seedIfEmpty(QString *error)
{
    QSqlQuery q;
    if (!run(q, "SELECT (SELECT COUNT(*) FROM departments) + (SELECT COUNT(*) FROM workers)"
                " + (SELECT COUNT(*) FROM trips)", error))
        return false;
    q.next();
    if (q.value(0).toInt() > 0)
        return true; // данные уже есть

    const char *statements[] = {
        "INSERT INTO departments (id, name, phone) VALUES"
        " (1, 'Отдел кадров',               '+7-391-200-10-01'),"
        " (2, 'Бухгалтерия',                '+7-391-200-10-02'),"
        " (3, 'Отдел информационных технологий', '+7-391-200-10-03'),"
        " (4, 'Отдел закупок',              '+7-391-200-10-04'),"
        " (5, 'Планово-экономический отдел', '+7-391-200-10-05')",

        "INSERT INTO workers (id, surname, name, patronymic, department_id, address, phone) VALUES"
        " (1, 'Иванов',   'Иван',    'Иванович',     1, 'ул. Ленина, д. 12, кв. 5',       '+7-923-100-00-01'),"
        " (2, 'Петров',   'Пётр',    'Петрович',     3, 'ул. Мира, д. 7, кв. 41',         '+7-923-100-00-02'),"
        " (3, 'Сидоров',  'Сидор',   'Сидорович',    2, 'пр. Свободный, д. 30, кв. 118',  '+7-923-100-00-03'),"
        " (4, 'Козлова',  'Анна',    'Сергеевна',    4, 'ул. Взлётная, д. 3, кв. 9',      '+7-923-100-00-04'),"
        " (5, 'Морозов',  'Олег',    'Андреевич',    3, 'ул. Партизана Железняка, д. 21', '+7-923-100-00-05'),"
        " (6, 'Смирнова', 'Елена',   'Викторовна',   5, 'ул. Копылова, д. 66, кв. 27',    '+7-923-100-00-06'),"
        " (7, 'Волков',   'Дмитрий', 'Николаевич',   4, 'ул. Алексеева, д. 14, кв. 83',   '+7-923-100-00-07')",

        // total_cost сразу заполняется «макросом» (см. recalcTotals), поэтому здесь 0
        "INSERT INTO trips (id, trip_date, worker_id, city, departure_date, days, allowance, road_cost, total_cost) VALUES"
        " (1, '2026-05-10', 1, 'Москва',      '2026-05-12',  5, 1000, 5000, 0),"
        " (2, '2026-05-11', 2, 'Новосибирск', '2026-05-15',  3,  700, 2000, 0),"
        " (3, '2026-05-12', 3, 'Москва',      '2026-05-18', 10, 1000, 4500, 0),"
        " (4, '2026-05-12', 4, 'Томск',       '2026-05-20',  4,  800, 3000, 0),"
        " (5, '2026-05-14', 5, 'Москва',      '2026-05-22',  2, 1200, 6000, 0),"
        " (6, '2026-05-16', 2, 'Москва',      '2026-05-26',  6, 1000, 5500, 0),"
        " (7, '2026-05-18', 6, 'Иркутск',     '2026-05-28',  3,  900, 2500, 0)",
    };
    for (const char *sql : statements)
        if (!run(q, QString::fromUtf8(sql), error))
            return false;

    return recalcTotals(error) >= 0;
}

bool resetDemoData(QString *error)
{
    QSqlQuery q;
    // Порядок важен: сначала дочерние таблицы, затем справочники (внешние ключи).
    if (!run(q, "DELETE FROM trips", error) || !run(q, "DELETE FROM workers", error)
        || !run(q, "DELETE FROM departments", error)
        || !run(q, "DELETE FROM sqlite_sequence", error))
        return false;
    return seedIfEmpty(error);
}

int recalcTotals(QString *error)
{
    QSqlQuery q;
    if (!run(q, "UPDATE trips SET total_cost = days * allowance + 2 * road_cost", error))
        return -1;
    return q.numRowsAffected();
}

int tripTotal(int days, int allowance, int roadCost)
{
    return days * allowance + 2 * roadCost;
}

QSqlQuery tripsByCityQuery(const QString &city)
{
    QSqlQuery q;
    q.prepare("SELECT " + fioExpr("w") + " AS \"ФИО\","
              " t.city AS \"Город\","
              " strftime('%d.%m.%Y', t.departure_date) AS \"Дата выезда\","
              " t.total_cost AS \"Общая стоимость\""
              " FROM trips t JOIN workers w ON w.id = t.worker_id"
              " WHERE t.city = ?"
              " ORDER BY t.departure_date");
    q.addBindValue(city);
    q.exec();
    return q;
}

QSqlQuery crossTabQuery()
{
    // Список городов определяет набор столбцов результата.
    QStringList cities;
    QSqlQuery citiesQuery("SELECT DISTINCT city FROM trips ORDER BY city");
    while (citiesQuery.next())
        cities << citiesQuery.value(0).toString();

    QString sql = "SELECT " + fioExpr("w") + " AS \"Сотрудник\"";
    for (const QString &city : cities)
        sql += ", SUM(CASE WHEN t.city = ? THEN t.total_cost END) AS " + quoteIdent(city);
    sql += ", SUM(t.total_cost) AS \"Итого\""
           " FROM trips t JOIN workers w ON w.id = t.worker_id"
           " GROUP BY w.id ORDER BY w.surname";

    QSqlQuery q;
    q.prepare(sql);
    for (const QString &city : cities)
        q.addBindValue(city); // названия городов передаются параметрами, а не в тексте запроса
    q.exec();
    return q;
}

QStringList normalizeFioTokens(const QString &text)
{
    QStringList tokens;
    const QStringList parts = text.split(QRegularExpression("[\\s.,]+"), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part.remove('%');
        part.remove('_');
        if (part.isEmpty())
            continue;
        part = part.left(1).toUpper() + part.mid(1).toLower();
        tokens << part;
    }
    return tokens;
}

QSqlQuery tripsByFioQuery(const QString &fio)
{
    const QStringList tokens = normalizeFioTokens(fio);

    // Каждое введённое слово должно совпасть с началом фамилии, имени или отчества.
    QString sql = "SELECT " + fioExpr("w") + " AS \"ФИО\","
                  " t.city AS \"Город\","
                  " strftime('%d.%m.%Y', t.departure_date) AS \"Дата выезда\","
                  " t.days AS \"Дней\","
                  " t.total_cost AS \"Общая стоимость\""
                  " FROM trips t JOIN workers w ON w.id = t.worker_id WHERE 1 = 1";
    for (int i = 0; i < tokens.size(); ++i)
        sql += " AND (w.surname LIKE ? OR w.name LIKE ? OR w.patronymic LIKE ?)";
    sql += " ORDER BY w.surname, t.departure_date";

    QSqlQuery q;
    q.prepare(sql);
    for (const QString &token : tokens)
        for (int i = 0; i < 3; ++i)
            q.addBindValue(token + '%');
    q.exec();
    return q;
}

} // namespace Db
