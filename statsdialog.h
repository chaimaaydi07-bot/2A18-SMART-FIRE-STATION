#ifndef STATSDIALOG_H
#define STATSDIALOG_H
#include <QDialog>
#include <QVector>

struct StatType { QString type; int formations, inscrits, presents, reussis; };

// Histogramme dessiné avec QPainter : taux de participation et taux de réussite par type.
class BarChart : public QWidget {
public:
    explicit BarChart(const QVector<StatType> &data, QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<StatType> m_data;
};

class StatsDialog : public QDialog {
public:
    explicit StatsDialog(QWidget *parent = nullptr);
};
#endif
