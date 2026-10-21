#ifndef INSCRIPTIONDIALOG_H
#define INSCRIPTIONDIALOG_H
#include <QDialog>
#include "inscription.h"

class QLabel; class QTableView; class QComboBox; class QSqlQueryModel;

// Gestion des participants d'une formation : inscription, présence, résultat, recommandations.
class InscriptionDialog : public QDialog {
public:
    InscriptionDialog(int idFormation, QWidget *parent = nullptr);
private:
    void rafraichir();
    int inscriptionSelectionnee() const;
    void afficher(const Retour &r);

    int m_idFormation;
    QLabel *m_info;
    QTableView *m_table;
    QComboBox *m_agents;
    QSqlQueryModel *m_model;
};
#endif
