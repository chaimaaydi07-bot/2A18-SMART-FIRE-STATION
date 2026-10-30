// Controleur du module Vehicules : donnees, actions CRUD et calculs metier.
#include "vehicules.h"
#include "vehicules_utils.h"
#include "vehiculerepository.h"
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>

using namespace VehiculesDetail;

// =====================================================================
//  MÉTIER INNOVANT : maintenance prédictive
//  Score de santé d'un véhicule (0 - 100 %), calculé automatiquement :
//   - part du cycle de maintenance déjà écoulée (dernière -> prochaine)
//   - âge de du véhicule
//   - état (en maintenance / hors service)
// =====================================================================
int VehiculesDetail::scoreSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine)
{
    QDate today = QDate::currentDate();
    double score = 100.0;

    if (derniere.isValid() && prochaine.isValid() && prochaine > derniere) {
        double total  = derniere.daysTo(prochaine);
        double ecoule = derniere.daysTo(today);
        double ratio  = ecoule / total;
        if (ratio < 0) ratio = 0;
        if (ratio <= 1.0) score -= 40.0 * ratio;
        else              score -= 40.0 + qMin(30.0, (ratio - 1.0) * 60.0);
    }
    if (achat.isValid()) {
        double ans = achat.daysTo(today) / 365.0;
        if (ans > 0) score -= qMin(20.0, ans * 3.0);
    }
    if (etat == "En maintenance") score -= 15.0;
    if (etat == "Hors service")   score = qMin(score, 15.0);

    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return int(score + 0.5);
}

QStringList VehiculesDetail::facteursSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine)
{
    QDate today = QDate::currentDate();
    QStringList f;
    if (etat == "Hors service")   f << "véhicule hors service";
    if (etat == "En maintenance") f << "en cours de maintenance";
    if (prochaine.isValid()) {
        int d = int(today.daysTo(prochaine));
        if (d < 0)
            f << QString("maintenance en retard de %1 jour(s)").arg(-d);
        else if (derniere.isValid() && prochaine > derniere) {
            double ratio = double(derniere.daysTo(today)) / double(derniere.daysTo(prochaine));
            if (ratio >= 0.8)
                f << QString("maintenance dans %1 jour(s)").arg(d);
        }
    }
    if (achat.isValid()) {
        int ans = int(achat.daysTo(today) / 365);
        if (ans >= 3) f << QString("âge : %1 an(s)").arg(ans);
    }
    if (f.isEmpty()) f << "aucun facteur de risque";
    return f;
}

QString VehiculesDetail::recommandation(int score, const QDate &pm, QDate &suggeree)
{
    QDate today = QDate::currentDate();
    if (score < 40) { suggeree = today;              return "Intervention immédiate"; }
    if (score < 70) { suggeree = today.addDays(15);  return "Planifier sous 15 jours"; }
    suggeree = pm;
    return "Aucune action requise";
}

bool VehiculesDetail::ligneVehiculeComplete(QTableWidget *tableauVehicules, int row)
{
    if (!tableauVehicules || row < 0 || row >= tableauVehicules->rowCount()) return false;
    const int cols[] = {ColId, ColMatricule, ColType, ColCarburant, ColSeuilCarburant, ColEtat, ColLocalisation, ColDateAchat, ColDernierEntretien, ColProchainEntretien, ColMarque, ColModele, ColRemarques, ColSante, ColIcone};
    for (int col : cols)
        if (!tableauVehicules->item(row, col))
            return false;
    return true;
}

int VehiculesDetail::trouverLigneParId(QTableWidget *tableauVehicules, const QString &id)
{
    if (!tableauVehicules || id.isEmpty()) return -1;
    for (int r = 0; r < tableauVehicules->rowCount(); ++r)
        if (tableauVehicules->item(r, ColId) && tableauVehicules->item(r, ColId)->text() == id)
            return r;
    return -1;
}

bool vehicules::ajouter() {
    return sauvegarder(false);
}

bool vehicules::modifier() {
    return sauvegarder(true);
}

bool vehicules::sauvegarder(bool modification) {

    int ligne = tableauVehicules->currentRow();
    if (modification && ligne < 0) {
        return false;
    }

    bool kilometrageValide = false;
    champKilometrage->text().toLongLong(&kilometrageValide);
    if (!kilometrageValide) {
        QMessageBox::critical(this, "Enregistrement impossible","Saisissez un kilométrage entier positif ou nul.");
        return false;
    }

    VehiculeRecord vehicule = lireFormulaire();
    if (modification) {
        vehicule.id = tableauVehicules->item(ligne, ColId)->text();
    }

    QString erreur;
    if (!VehiculeRepository::enregistrer(vehicule, modification, erreur)) {
        QMessageBox::critical(this, "Enregistrement impossible", erreur);
        return false;
    }

    if (chargerDepuisBase()) {
        selectionnerVehicule(vehicule.id);
    }

    QString message = "Véhicule ajouté dans la base de données";
    if (modification) {
        message = "Véhicule modifié dans la base de données";
    }

    toast(this, message);
    return true;
}

bool vehicules::supprimerVehicule(const QString &id) {
    QString erreur;
    if (!VehiculeRepository::supprimer(id, erreur)) {
        QMessageBox::critical(this, "Suppression impossible", erreur);
        return false;
    }

    chargerDepuisBase();
    toast(this, "Véhicule supprimé de la base de données", "#c62828");
    return true;
}



// 1. Lire Oracle, puis reconstruire les lignes du tableau.
bool vehicules::chargerDepuisBase()
{
    QVector<VehiculeRecord> vehicules;
    QString erreur;

    if (!VehiculeRepository::charger(vehicules, erreur)) {
        QMessageBox::critical(this, "Chargement des véhicules", erreur);
        return false;
    }

    QString idSelectionne;
    int ligneSelectionnee = tableauVehicules->currentRow();
    if (ligneSelectionnee >= 0) {
        idSelectionne = tableauVehicules->item(ligneSelectionnee, ColId)->text();
    }

    // Suspendre le tri pendant l'ajout des lignes pour ne pas melanger leurs cellules.
    bool triActif = tableauVehicules->isSortingEnabled();
    tableauVehicules->setSortingEnabled(false);
    tableauVehicules->setRowCount(0);

    for (const VehiculeRecord &vehicule : vehicules) {
        ajouterLigne(vehicule);
    }

    tableauVehicules->setSortingEnabled(triActif);
    colorierLignes();
    rechercher();
    selectionnerVehicule(idSelectionne);
    return true;
}

// 2. Rassembler les champs dans une seule variable representant le vehicule.
VehiculeRecord vehicules::lireFormulaire() const
{
    VehiculeRecord vehicule;
    vehicule.matricule = champMatricule->text().trimmed();
    if (type->currentIndex() > 0) {
        vehicule.type = type->currentText();
    }
    vehicule.marque = marque->text();
    vehicule.modele = modele->text();
    vehicule.carburant = champCarburant->value();
    vehicule.seuil = seuil->value();
    vehicule.etat = etat->currentText();
    vehicule.localisation = localisation->text();
    vehicule.remarques = remarques->text();
    vehicule.kilometrage = champKilometrage->text().toLongLong();
    vehicule.disponibilite = disponibilite->currentText();

    // Le calendrier affiche cette date speciale lorsqu'une date est absente.
    const QDate dateVide(1900, 1, 1);
    if (dateAchat->date() > dateVide) {
        vehicule.achat = dateAchat->date();
    }
    if (dateDernierEntretien->date() > dateVide) {
        vehicule.entretien = dateDernierEntretien->date();
    }
    if (dateProchainEntretien->date() > dateVide) {
        vehicule.prochainEntretien = dateProchainEntretien->date();
    }
    return vehicule;
}

// 3. Enregistrer dans Oracle. Ne rafraichir la vue qu'apres la reussite.



void vehicules::selectionnerVehicule(const QString &id)
{
    int ligne = trouverLigneParId(tableauVehicules, id);
    if (ligne >= 0) {
        tableauVehicules->setCurrentCell(ligne, ColId);
    }
}

// 4. Rechercher masque les lignes sans modifier les donnees dans Oracle.
void vehicules::rechercher()
{
    QString texte = champRecherche->text();
    QString typeRecherche;
    if (filtreType->currentIndex() > 0) {
        typeRecherche = filtreType->currentText();
    }

    for (int ligne = 0; ligne < tableauVehicules->rowCount(); ligne++) {
        bool texteTrouve = false;
        for (int colonne = 0; colonne < tableauVehicules->columnCount(); colonne++) {
            QTableWidgetItem *cellule = tableauVehicules->item(ligne, colonne);
            if (cellule && cellule->text().contains(texte, Qt::CaseInsensitive)) {
                texteTrouve = true;
                break;
            }
        }

        QString typeVehicule = tableauVehicules->item(ligne, ColType)->text();
        bool typeCorrespond = typeRecherche.isEmpty() || typeVehicule == typeRecherche;
        bool afficherLigne = texteTrouve && typeCorrespond;
        tableauVehicules->setRowHidden(ligne, !afficherLigne);
    }
    majEtatVide(tableauVehicules);
}

void vehicules::trier()
{
    int colonne;
    switch (choixTri->currentIndex()) {
    case 0: colonne = ColMatricule; break;
    case 1: colonne = ColEtat; break;
    case 2: colonne = ColProchainEntretien; break;
    case 3: colonne = ColCarburant; break;
    default: return;
    }

    tableauVehicules->sortItems(colonne, Qt::AscendingOrder);
    colorierLignes();
    rechercher();
}

bool vehicules::planifierMaintenances(const QVector<QPair<QString, QDate>> &dates, QString &erreur)
{
    return VehiculeRepository::planifier(dates, erreur);
}