#include "statsdialog.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QSqlQuery>
#include <QLabel>

BarChart::BarChart(const QVector<StatType> &data, QWidget *parent) : QWidget(parent), m_data(data)
{
    setMinimumSize(560, 260);
}

void BarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect area(50, 20, width() - 70, height() - 80);
    p.setPen(Qt::gray);
    for (int pct = 0; pct <= 100; pct += 25) {
        const int y = area.bottom() - area.height() * pct / 100;
        p.drawLine(area.left(), y, area.right(), y);
        p.drawText(5, y + 4, QString("%1 %").arg(pct));
    }
    if (m_data.isEmpty()) { p.drawText(area, Qt::AlignCenter, "Aucune donnée"); return; }

    const int slot = area.width() / m_data.size();
    const int bw = qMax(10, slot / 4);
    for (int i = 0; i < m_data.size(); ++i) {
        const StatType &s = m_data[i];
        const double part  = s.inscrits ? 100.0 * s.presents / s.inscrits : 0;
        const double reuss = s.presents ? 100.0 * s.reussis / s.presents : 0;
        const int x0 = area.left() + i * slot + slot / 2 - bw;
        const int h1 = int(area.height() * part / 100), h2 = int(area.height() * reuss / 100);
        p.fillRect(x0, area.bottom() - h1, bw, h1, QColor("#C8102E"));
        p.fillRect(x0 + bw, area.bottom() - h2, bw, h2, QColor("#F28C28"));
        p.setPen(Qt::black);
        p.drawText(QRect(area.left() + i * slot, area.bottom() + 4, slot, 34),
                   Qt::AlignHCenter | Qt::TextWordWrap, s.type);
    }
    p.fillRect(area.right() - 190, 2, 10, 10, QColor("#C8102E"));
    p.drawText(area.right() - 175, 11, "Participation");
    p.fillRect(area.right() - 90, 2, 10, 10, QColor("#F28C28"));
    p.drawText(area.right() - 75, 11, "Réussite");
}

StatsDialog::StatsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Statistiques des formations");
    resize(700, 520);

    QVector<StatType> data;
    QSqlQuery q("SELECT f.type, COUNT(DISTINCT f.id),"
                " COALESCE(SUM(i.etat='inscrit'),0),"
                " COALESCE(SUM(i.etat='inscrit' AND i.presence=1),0),"
                " COALESCE(SUM(i.etat='inscrit' AND i.resultat='reussi'),0)"
                " FROM formation f LEFT JOIN inscription i ON i.id_formation=f.id"
                " WHERE f.statut <> 'annulee' GROUP BY f.type ORDER BY f.type");
    while (q.next())
        data.append({q.value(0).toString(), q.value(1).toInt(), q.value(2).toInt(),
                     q.value(3).toInt(), q.value(4).toInt()});

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("<b>Taux de participation</b> (présents / inscrits) et "
                              "<b>taux de réussite</b> (réussis / présents) par type de formation"));
    lay->addWidget(new BarChart(data));

    auto *t = new QTableWidget(data.size(), 6);
    t->setHorizontalHeaderLabels({"Type", "Sessions", "Inscrits", "Présents", "Réussis", "Taux de réussite"});
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int r = 0; r < data.size(); ++r) {
        const StatType &s = data[r];
        const QStringList v = {s.type, QString::number(s.formations), QString::number(s.inscrits),
            QString::number(s.presents), QString::number(s.reussis),
            s.presents ? QString("%1 %").arg(100 * s.reussis / s.presents) : "—"};
        for (int c = 0; c < 6; ++c) t->setItem(r, c, new QTableWidgetItem(v[c]));
    }
    lay->addWidget(t);
}
