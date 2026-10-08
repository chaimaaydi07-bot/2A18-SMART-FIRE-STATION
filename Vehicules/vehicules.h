#ifndef VEHICULES_H
#define VEHICULES_H
#include <QMainWindow>
#include <QString>
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDateEdit;
class QLabel;
class QTableWidget;
class QVBoxLayout;
class QWidget;
class vehicules : public QMainWindow {
    Q_OBJECT
public:
    vehicules(QWidget *parent = nullptr);
    void setUtilisateur(const QString &nomUtilisateur, const QString &role);
private slots:
    void ajouter();
    void modifier();
    void rechercher();
    void trier();
private:
    QString ajouterLigne(const QString &n, const QString &t, const QString &m,
                         const QString &mod, int q, int s, const QString &e,
                         const QString &loc, const QString &da, const QString &dm,
                         const QString &pm, const QString &rem);
    void chargerFormulaire(int r);
    void colorierLignes();
    void afficherAlertes(bool afficherSiVide);
    void majCartes();
    void afficherMaintenancePredictive();
    void ouvrirFormulaireVehicule(bool edition, int ligne = -1);
    void afficherDetailsVehicule(int ligne);
    void afficherMaintenances();
    void majTableauxDashboard();
    QLineEdit *nom;
    QComboBox *type;
    QLineEdit *marque;
    QLineEdit *modele;
    QSpinBox *quantite;
    QSpinBox *seuil;
    QComboBox *etat;
    QLineEdit *localisation;
    QDateEdit *dateAchat;
    QDateEdit *dateMaintenance;
    QDateEdit *prochaineMaintenance;
    QLineEdit *remarques;
    QLineEdit *recherche;
    QComboBox *triCombo;
    QComboBox *filtreType;
    QTableWidget *table;
    QVBoxLayout *alertesRecentesLayout = nullptr;
    QWidget *donutEtat = nullptr;
    int prochainId = 1;
};
#endif 
