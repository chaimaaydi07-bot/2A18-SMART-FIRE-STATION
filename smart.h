
#ifndef SMART_H
#define SMART_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QTextEdit>
#include <QTableWidget>

class smart : public QMainWindow
{
    Q_OBJECT
public:
    explicit smart(QWidget *parent = nullptr);

private slots:
    void ajouter();
    void modifier();
    void archiver();
    void rechercher();
    void trier();

private:
    void ajouterLigne(const QString &n, const QString &t, const QString &m,
                      const QString &mod, int q, const QString &e,
                      const QString &da, const QString &dm, const QString &pm);
    QLineEdit *nom;
    QComboBox *type;
    QLineEdit *marque;
    QLineEdit *modele;
    QSpinBox *quantite;
    QComboBox *etat;
    QDateEdit *dateAchat;
    QDateEdit *dateMaintenance;
    QDateEdit *prochaineMaintenance;
    QLineEdit *remarques;
    QLineEdit *recherche;
    QTableWidget *table;
    QComboBox *triCombo;
};

#endif // SMART_H
