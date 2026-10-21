#ifndef FORMATIONDIALOG_H
#define FORMATIONDIALOG_H
#include <QDialog>

class QLineEdit; class QComboBox; class QDateEdit; class QSpinBox; class QCheckBox;

// Fenêtre ouverte par les boutons Ajouter (idFormation = 0) et Modifier (idFormation > 0, champs préremplis).
// Valide la saisie puis enregistre en base ; reste ouverte en cas d'erreur.
class FormationDialog : public QDialog {
public:
    explicit FormationDialog(int idFormation = 0, QWidget *parent = nullptr);
protected:
    void accept() override;
private:
    QLineEdit *m_intitule, *m_formateur, *m_lieu;
    QComboBox *m_type;
    QDateEdit *m_debut, *m_fin;
    QSpinBox *m_duree, *m_capacite;
    QCheckBox *m_certif, *m_annulee;
    int m_id;
};
#endif
