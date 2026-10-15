#include "MainWindow.h"
#include "UiText.h"
#include "ThemeManager.h"
#include "../../core/ContentEngine.h"
#include "../../core/Scheduler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QUuid>
#include <QInputDialog>

namespace dhikr::qt {
void MainWindow::PopulateLibraryList() {
    if (!m_listContent || !m_txtSearch || !m_comboCategory) return;

    QSignalBlocker blocker(m_listContent);
    m_listContent->clear();

    QString filter = m_txtSearch->text().trimmed();
    QString cat = m_comboCategory->currentText();
    int catIdx = m_comboCategory->currentIndex();

    const auto& items = ContentEngine::Instance().GetItems();
    for (const auto& item : items) {
        QString txt = QString::fromStdWString(item.text);
        QString c = QString::fromStdString(item.category);
        if (!filter.isEmpty() && !txt.contains(filter, Qt::CaseInsensitive)) continue;
        if (catIdx > 0 && c != cat) continue;

        auto* listItem = new QListWidgetItem(QString("[%1]  %2").arg(c, txt), m_listContent);
        listItem->setData(Qt::UserRole, QString::fromStdString(item.id));
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
        listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        listItem->setSizeHint(QSize(200, 40));
    }
}

void MainWindow::OnAddCustomDhikr() {
    bool ok = false;
    QString text = QInputDialog::getMultiLineText(
        this,
        QString::fromStdWString(Localization::Instance().Get(L"library.dialog_title", L"إضافة ذكر مخصص")),
        QString::fromStdWString(Localization::Instance().Get(L"library.enter_dhikr", L"اكتب نص الذكر:")),
        QString(),
        &ok
    ).trimmed();

    if (!ok || text.isEmpty()) return;

    ContentItem newItem;
    newItem.id = "custom_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    newItem.text = text.toStdWString();
    newItem.category = "custom";
    newItem.priority = 1;
    newItem.repetitions = 1;
    newItem.enabled = true;

    ContentEngine::Instance().AddItem(newItem);
    if (!ContentEngine::Instance().LastError().empty()) QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));

    // Ensure "custom" category is in the filter combo if not already present
    if (m_comboCategory && m_comboCategory->findText("custom") == -1) {
        m_comboCategory->addItem("custom");
    }

    PopulateLibraryList();
}

void MainWindow::OnDeleteSelectedDhikr() {
    if (!m_listContent) return;
    QListWidgetItem* current = m_listContent->currentItem();
    if (!current) return;

    // Copy ID string safely before any dialog or list mutation to prevent dangling pointer crashes
    QString targetId = current->data(Qt::UserRole).toString();
    if (targetId.isEmpty()) return;

    auto reply = QMessageBox::question(
        this,
        QString::fromStdWString(Localization::Instance().Get(L"library.delete_title", L"حذف الذكر")),
        QString::fromStdWString(Localization::Instance().Get(L"library.delete_item_confirm", L"هل أنت متأكد من حذف هذا الذكر؟")),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        ContentEngine::Instance().DeleteItemById(targetId.toStdString());
        if (!ContentEngine::Instance().LastError().empty()) QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));
        PopulateLibraryList();
    }
}

QWidget* MainWindow::CreateLibraryPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    // Search, Filter & Custom Dhikr Actions Row
    auto* filterRow = new QHBoxLayout();
    m_txtSearch = new QLineEdit();
    m_txtSearch->setPlaceholderText(QString::fromStdWString(Localization::Instance().Get(L"library.search", L"بحث في الأذكار...")));

    m_comboCategory = new QComboBox();
    m_comboCategory->addItem(QString::fromStdWString(Localization::Instance().Get(L"library.all_categories", L"جميع الأقسام")));
    auto cats = ContentEngine::Instance().GetCategories();
    for (const auto& c : cats) {
        m_comboCategory->addItem(QString::fromStdString(c));
    }

    m_btnAddDhikr = new QPushButton(QString::fromStdWString(Localization::Instance().Get(L"library.add_dhikr", L"+ إضافة ذكر")));
    m_btnAddDhikr->setObjectName("PrimaryButton");
    connect(m_btnAddDhikr, &QPushButton::clicked, this, &MainWindow::OnAddCustomDhikr);

    m_btnDeleteDhikr = new QPushButton(Text(L"qt.delete"));
    connect(m_btnDeleteDhikr, &QPushButton::clicked, this, &MainWindow::OnDeleteSelectedDhikr);

    filterRow->addWidget(m_txtSearch, 1);
    filterRow->addWidget(m_comboCategory);
    filterRow->addWidget(m_btnAddDhikr);
    filterRow->addWidget(m_btnDeleteDhikr);
    layout->addLayout(filterRow);

    auto* fileRow = new QHBoxLayout();
    auto* import = new QPushButton(Text(L"qt.import"));
    auto* exportFile = new QPushButton(Text(L"qt.export"));
    fileRow->addWidget(import); fileRow->addWidget(exportFile); fileRow->addStretch();
    layout->addLayout(fileRow);
    connect(import, &QPushButton::clicked, this, [this]() {
        const auto name = QFileDialog::getOpenFileName(this, Text(L"qt.import"), {}, "Dhikr (*.json *.md)");
        if (name.isEmpty()) return;
        if (QMessageBox::question(this, "Azkar Reminder", Text(L"qt.import_replace")) != QMessageBox::Yes) return;
        const auto path = std::filesystem::u8path(name.toUtf8().constData());
        if (ContentEngine::Instance().Load(path) != ContentLoadResult::Success) {
            QMessageBox::warning(this, "Azkar Reminder", Text(L"library.import_error"));
            return;
        }
        if (!ContentEngine::Instance().Save()) QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));
        QSignalBlocker blocker(m_comboCategory);
        m_comboCategory->clear(); m_comboCategory->addItem(Text(L"library.all_categories"));
        for (const auto& category : ContentEngine::Instance().GetCategories()) m_comboCategory->addItem(QString::fromStdString(category));
        PopulateLibraryList();
    });
    connect(exportFile, &QPushButton::clicked, this, [this]() {
        auto name = QFileDialog::getSaveFileName(this, Text(L"qt.export"), "content.json", "JSON (*.json)");
        if (name.isEmpty()) return;
        if (!name.endsWith(".json", Qt::CaseInsensitive)) name += ".json";
        if (!ContentEngine::Instance().Export(std::filesystem::u8path(name.toUtf8().constData())))
            QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));
    });

    // Content List
    m_listContent = new QListWidget();
    m_listContent->setStyleSheet("QListWidget { background-color: #101310; border-radius: 8px; padding: 6px; }");

    connect(m_txtSearch, &QLineEdit::textChanged, this, &MainWindow::PopulateLibraryList);
    connect(m_comboCategory, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::PopulateLibraryList);

    connect(m_listContent, &QListWidget::itemChanged, this, [this](QListWidgetItem* entry) {
        const auto id = entry->data(Qt::UserRole).toString().toStdString();
        const auto items = ContentEngine::Instance().GetItems();
        for (size_t index = 0; index < items.size(); ++index) {
            if (items[index].id != id || items[index].enabled == (entry->checkState() == Qt::Checked)) continue;
            ContentEngine::Instance().ToggleItemEnabled(index);
            if (!ContentEngine::Instance().LastError().empty()) QMessageBox::warning(this, "Azkar Reminder", Text(L"qt.save_failed"));
            break;
        }
    });
    layout->addWidget(m_listContent, 1);
    PopulateLibraryList();

    return page;
}

} // namespace dhikr::qt
