#include "tableform.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlRelationalDelegate>
#include <QSqlRelationalTableModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QSqlRelation>
#include <QKeySequence>

TableForm::TableForm(const QString &title, const QString &table, const QStringList &headers,
                     QWidget *parent)
    : QDialog(parent), headers_(headers)
{
    setWindowTitle(title);
    setWindowFlag(Qt::WindowMaximizeButtonHint);
    resize(820, 420);

    // Модель данных: изменения накапливаются и записываются в БД по кнопке «Сохранить».
    model_ = new QSqlRelationalTableModel(this);
    model_->setTable(table);
    model_->setEditStrategy(QSqlTableModel::OnManualSubmit);

    // Представление (таблица на экране) + делегат, создающий поля со списком для внешних ключей.
    view_ = new QTableView(this);
    view_->setModel(model_);
    view_->setItemDelegate(new QSqlRelationalDelegate(view_));
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setAlternatingRowColors(true);
    view_->horizontalHeader()->setStretchLastSection(true);

    auto *addButton    = new QPushButton("Добавить (Ctrl+I)", this);
    auto *deleteButton = new QPushButton("Удалить (Ctrl+D)", this);
    auto *saveButton   = new QPushButton("Сохранить (Ctrl+S)", this);
    auto *revertButton = new QPushButton("Отменить изменения", this);
    auto *closeButton  = new QPushButton("Закрыть", this);

    addButton->setShortcut(QKeySequence("Ctrl+I"));
    deleteButton->setShortcut(QKeySequence("Ctrl+D"));
    saveButton->setShortcut(QKeySequence("Ctrl+S"));

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(deleteButton);
    buttons->addWidget(saveButton);
    buttons->addWidget(revertButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(view_);
    layout->addLayout(buttons);

    // Связь сигналов кнопок со слотами-обработчиками.
    connect(addButton,    &QPushButton::clicked, this, &TableForm::addRow);
    connect(deleteButton, &QPushButton::clicked, this, &TableForm::deleteRow);
    connect(saveButton,   &QPushButton::clicked, this, &TableForm::save);
    connect(revertButton, &QPushButton::clicked, this, &TableForm::revert);
    connect(closeButton,  &QPushButton::clicked, this, &QDialog::close);
}

void TableForm::setRelation(int column, const QSqlRelation &relation)
{
    model_->setRelation(column, relation);
}

void TableForm::load()
{
    model_->select();
    for (int i = 0; i < headers_.size() && i < model_->columnCount(); ++i)
        model_->setHeaderData(i, Qt::Horizontal, headers_.at(i));
    view_->hideColumn(0); // код (автоинкремент) пользователю не показываем
    view_->resizeColumnsToContents();
}

void TableForm::addRow()
{
    const int row = model_->rowCount();
    model_->insertRow(row);
    view_->setCurrentIndex(model_->index(row, 1));
    view_->edit(model_->index(row, 1));
}

void TableForm::deleteRow()
{
    const QModelIndex index = view_->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, windowTitle(), "Выберите строку для удаления.");
        return;
    }
    if (QMessageBox::question(this, windowTitle(), "Удалить выбранную запись?")
        != QMessageBox::Yes)
        return;

    model_->removeRow(index.row());
    if (!model_->submitAll()) {
        // Например, нельзя удалить отдел, в котором числятся сотрудники.
        QMessageBox::warning(this, windowTitle(),
            "Запись не удалена: на неё ссылаются другие таблицы.\n\n" + model_->lastError().text());
        model_->revertAll();
    }
    load();
    emit dataChanged();
}

void TableForm::save()
{
    if (!model_->submitAll()) {
        QMessageBox::warning(this, windowTitle(),
            "Не удалось сохранить данные. Проверьте, что все обязательные поля заполнены.\n\n"
            + model_->lastError().text());
        return;
    }
    load();
    emit dataChanged();
}

void TableForm::revert()
{
    model_->revertAll();
    load();
}