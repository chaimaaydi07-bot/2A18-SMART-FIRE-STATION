#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QButtonGroup>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSize>
#include <QTableWidgetItem>

static int cleanNumber(const QString &text)
{
    QString value = text;
    value.remove(' ');
    return value.toInt();
}

static int percent(int value, int total)
{
    if (total == 0)
        return 0;

    return qRound(value * 100.0 / total);
}

static void setBar(QProgressBar *bar, const QString &name, int value, int total)
{
    const int ratio = percent(value, total);
    bar->setRange(0, 100);
    bar->setValue(ratio);
    bar->setFormat(QString("%1   %2 (%3%)").arg(name).arg(value).arg(ratio));
}

static void setMetric(QLabel *label, const QString &icon, int value, const QString &caption)
{
    label->setTextFormat(Qt::RichText);
    label->setText(QString(
                       "<table><tr>"
                       "<td><img src='%1' width='28' height='28'></td>"
                       "<td style='padding-left:8px;'><b>%2</b><br>%3</td>"
                       "</tr></table>")
                       .arg(icon)
                       .arg(value)
                       .arg(caption));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupNavigation();
    setupIcons();
    setupVehicleTable();
    updateVehicleStats();

    connect(ui->statisticsButton, &QPushButton::clicked, this, [this]() {
        updateVehicleStats();
        ui->vehiclesStack->setCurrentWidget(ui->vehicleEmptyPage);
    });

    connect(ui->backToVehiclesButton, &QPushButton::clicked, this, [this]() {
        ui->vehiclesStack->setCurrentWidget(ui->vehiclesListPage);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupNavigation()
{
    auto *menu = new QButtonGroup(this);
    menu->setExclusive(true);

    const QList<QPair<QPushButton *, QWidget *>> pages = {
        {ui->dashboardButton, ui->dashboardPage},
        {ui->incidentsButton, ui->incidentsPage},
        {ui->buildingsButton, ui->buildingsPage},
        {ui->firefightersButton, ui->firefightersPage},
        {ui->vehiclesButton, ui->vehiclesPage},
        {ui->equipmentButton, ui->equipmentPage},
        {ui->reportsButton, ui->reportsPage},
        {ui->settingsButton, ui->settingsPage}
    };

    for (const auto &page : pages) {
        menu->addButton(page.first);

        connect(page.first, &QPushButton::clicked, this, [this, page]() {
            ui->contentStack->setCurrentWidget(page.second);
        });
    }

    ui->vehiclesButton->setChecked(true);
    ui->contentStack->setCurrentWidget(ui->vehiclesPage);
}

void MainWindow::setupIcons()
{
    const QSize navIconSize(18, 18);

    ui->dashboardButton->setIcon(QIcon(":/icons/success.png"));
    ui->incidentsButton->setIcon(QIcon(":/icons/close.png"));
    ui->buildingsButton->setIcon(QIcon(":/icons/maintenance.png"));
    ui->firefightersButton->setIcon(QIcon(":/icons/success.png"));
    ui->vehiclesButton->setIcon(QIcon(":/icons/vehicle.png"));
    ui->equipmentButton->setIcon(QIcon(":/icons/maintenance.png"));
    ui->reportsButton->setIcon(QIcon(":/icons/success.png"));
    ui->settingsButton->setIcon(QIcon(":/icons/settings.png"));

    const QList<QPushButton *> navButtons = {
        ui->dashboardButton,
        ui->incidentsButton,
        ui->buildingsButton,
        ui->firefightersButton,
        ui->vehiclesButton,
        ui->equipmentButton,
        ui->reportsButton,
        ui->settingsButton
    };

    for (QPushButton *button : navButtons)
        button->setIconSize(navIconSize);

    ui->backToVehiclesButton->setIcon(QIcon(":/icons/back.png"));
    ui->backToVehiclesButton->setIconSize(QSize(16, 16));
}

void MainWindow::setupVehicleTable()
{
    const QStringList headers = {
        "ID", "IMMATRICULATION", "TYPE", "KM", "ÉTAT", "DISPONIBILITÉ", "ACTIONS"
    };

    const QList<QStringList> rows = {
        {"VEH-001", "245 TU 1789", "Camion", "28 450", "Bon", "Disponible", "Voir"},
        {"VEH-002", "198 TU 2234", "Ambulance", "74 800", "Moyen", "En service", "Voir"},
        {"VEH-003", "221 TU 5412", "Camion", "49 700", "Moyen", "Maintenance", "Voir"},
        {"VEH-004", "187 TU 8865", "Citerne", "112 300", "Critique", "Hors service", "Voir"},
        {"VEH-005", "336 TU 7711", "Secours", "35 600", "Bon", "Disponible", "Voir"},
        {"VEH-006", "205 TU 9987", "Commandement", "62 100", "Moyen", "En service", "Voir"},
        {"VEH-007", "412 TU 6621", "Ambulance", "15 200", "Bon", "Disponible", "Voir"},
        {"VEH-008", "178 TU 4490", "Citerne", "95 300", "Critique", "Maintenance", "Voir"}
    };

    ui->vehicleTable->setColumnCount(headers.size());
    ui->vehicleTable->setRowCount(rows.size());
    ui->vehicleTable->setHorizontalHeaderLabels(headers);
    ui->vehicleTable->verticalHeader()->hide();
    ui->vehicleTable->horizontalHeader()->setStretchLastSection(true);
    ui->vehicleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int row = 0; row < rows.size(); ++row) {
        for (int column = 0; column < rows[row].size(); ++column) {
            auto *item = new QTableWidgetItem(rows[row][column]);
            ui->vehicleTable->setItem(row, column, item);
        }
    }

    connect(ui->vehicleTable, &QTableWidget::itemChanged, this, [this]() {
        updateVehicleStats();
    });
}

void MainWindow::updateVehicleStats()
{
    QHash<QString, int> availability;
    QHash<QString, int> states;
    QHash<QString, int> types;
    QHash<QString, int> kmTotals;
    QHash<QString, int> kmCounts;

    const int rows = ui->vehicleTable->rowCount();

    for (int row = 0; row < rows; ++row) {
        const QString type = ui->vehicleTable->item(row, 2)->text();
        const int km = cleanNumber(ui->vehicleTable->item(row, 3)->text());
        const QString state = ui->vehicleTable->item(row, 4)->text();
        const QString status = ui->vehicleTable->item(row, 5)->text();

        availability[status]++;
        states[state]++;
        types[type]++;
        kmTotals[type] += km;
        kmCounts[type]++;
    }

    setMetric(ui->totalVehiclesMetric, ":/icons/vehicle.png", rows, "Véhicules total");
    setMetric(ui->availableMetric, ":/icons/success.png", availability["Disponible"], "Disponibles");
    setMetric(ui->serviceMetric, ":/icons/settings.png", availability["En service"], "En service");
    setMetric(ui->maintenanceMetric, ":/icons/maintenance.png", availability["Maintenance"], "En maintenance");
    setMetric(ui->offlineMetric, ":/icons/close.png", availability["Hors service"], "Hors service");

    setBar(ui->availableBar, "Disponible", availability["Disponible"], rows);
    setBar(ui->serviceBar, "En service", availability["En service"], rows);
    setBar(ui->maintenanceBar, "Maintenance", availability["Maintenance"], rows);
    setBar(ui->offlineBar, "Hors service", availability["Hors service"], rows);

    setBar(ui->camionBar, "Camions", types["Camion"], rows);
    setBar(ui->ambulanceBar, "Ambulances", types["Ambulance"], rows);
    setBar(ui->citerneBar, "Citernes", types["Citerne"], rows);
    setBar(ui->otherTypeBar, "Autres", rows - types["Camion"] - types["Ambulance"] - types["Citerne"], rows);

    setBar(ui->goodStateBar, "Bon", states["Bon"], rows);
    setBar(ui->mediumStateBar, "Moyen", states["Moyen"], rows);
    setBar(ui->criticalStateBar, "Critique", states["Critique"], rows);

    int maxAverage = 1;
    QHash<QString, int> averages;
    const QStringList kmTypes = {"Camion", "Ambulance", "Citerne", "Secours", "Commandement"};

    for (const QString &type : kmTypes) {
        const int average = kmCounts[type] == 0 ? 0 : kmTotals[type] / kmCounts[type];
        averages[type] = average;
        maxAverage = qMax(maxAverage, average);
    }

    setBar(ui->camionKmBar, QString("Camions %1 km").arg(averages["Camion"]), averages["Camion"], maxAverage);
    setBar(ui->ambulanceKmBar, QString("Ambulances %1 km").arg(averages["Ambulance"]), averages["Ambulance"], maxAverage);
    setBar(ui->citerneKmBar, QString("Citernes %1 km").arg(averages["Citerne"]), averages["Citerne"], maxAverage);
    setBar(ui->otherKmBar, QString("Autres %1 km").arg(qMax(averages["Secours"], averages["Commandement"])),
           qMax(averages["Secours"], averages["Commandement"]), maxAverage);
}
