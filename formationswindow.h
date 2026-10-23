#ifndef FORMATIONSWINDOW_H
#define FORMATIONSWINDOW_H
#include <QMainWindow>

class QLineEdit; class QComboBox; class QDateEdit; class QSpinBox; class QCheckBox;
class QTableView; class QPushButton; class QLabel; class FormationModel; class QSoundEffect;

class FormationsWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit FormationsWindow(QWidget *parent = nullptr);
private:
    void rafraichir();
    void remplirFormulaire();
    void viderFormulaire();
    bool lireFormulaire(class Formation &f);
    int idSelectionne() const;
    void ajouter();
    void modifier();
    void supprimer();
    void participants();
    void exporterPdf();

    QLineEdit *m_intitule, *m_formateur, *m_lieu, *m_recherche;
    QComboBox *m_type, *m_tri;
    QDateEdit *m_debut, *m_fin;
    QSpinBox *m_duree, *m_capacite;
    QCheckBox *m_certif, *m_annulee;
    QPushButton *m_ordre;
    QTableView *m_table;
    QLabel *m_resume;
    FormationModel *m_model;
    bool m_croissant = true;
    QLabel *m_badgeNb;
    QSoundEffect *m_clic;
};
#endif
