// PLAN DU MODULE :
// - vehicules.cpp      : actions (charger, ajouter, modifier, supprimer, rechercher).
// - vehicules_view.cpp : construction de l'interface et des fenetres.
// - vehiculerepository.cpp : requetes SQL vers Oracle.
// - vehicules_utils.h  : noms des colonnes et fonctions partagees.
#ifndef VEHICULES_H
#define VEHICULES_H
#include <QMainWindow>
#include <QString>
#include <QDate>
#include <QPair>
#include <QVector>
struct VehiculeRecord;
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
    bool ajouter();
    bool modifier();
    void rechercher();
    void trier();
private:
    // Vue : ces fonctions sont definies dans vehicules_view.cpp.
    void appliquerStyle();
    void creerChampsFormulaire();
    QWidget *creerEntete();
    QWidget *creerListeVehicules();
    QWidget *creerNotifications();

    // Actions et donnees : vehicules.cpp.
    bool chargerDepuisBase();
    VehiculeRecord lireFormulaire() const;
    void selectionnerVehicule(const QString &id);
    bool sauvegarder(bool modification);
    bool supprimerVehicule(const QString &id);
    bool planifierMaintenances(const QVector<QPair<QString, QDate>> &dates, QString &erreur);

    // Affichage du tableau et des fenetres : vehicules_view.cpp.
    void ajouterLigne(const VehiculeRecord &vehicule);
    void colorierLignes();
    void afficherAlertes(bool afficherSiVide);
    void majCartes();
    void afficherMaintenancePredictive();
    void ouvrirFormulaireVehicule(bool edition, int ligne = -1);
    void afficherDetailsVehicule(int ligne);
    void afficherMaintenances();
    void majTableauxDashboard();

    // Champs du formulaire.
    QLineEdit *champMatricule = nullptr;
    QComboBox *type = nullptr;
    QLineEdit *marque = nullptr;
    QLineEdit *modele = nullptr;
    QSpinBox *champCarburant = nullptr;
    QSpinBox *seuil = nullptr;
    QComboBox *etat = nullptr;
    QLineEdit *localisation = nullptr;
    QDateEdit *dateAchat = nullptr;
    QDateEdit *dateDernierEntretien = nullptr;
    QDateEdit *dateProchainEntretien = nullptr;
    QLineEdit *remarques = nullptr;
    QLineEdit *champKilometrage = nullptr;
    QComboBox *disponibilite = nullptr;

    // Recherche, tri et tableau principal.
    QLineEdit *champRecherche = nullptr;
    QComboBox *choixTri = nullptr;
    QComboBox *filtreType = nullptr;
    QTableWidget *tableauVehicules = nullptr;

    // Zone des notifications.
    QVBoxLayout *alertesRecentesLayout = nullptr;
};
#endif