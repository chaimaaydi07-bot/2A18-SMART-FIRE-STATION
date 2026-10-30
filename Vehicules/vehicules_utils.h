#ifndef VEHICULES_UTILS_H
#define VEHICULES_UTILS_H

// Declarations internes partagees entre le controleur et la vue Qt.
#include <QDate>
#include <QString>
#include <QStringList>
class QWidget;
class QTableWidget;

namespace VehiculesDetail {
inline const QString FMT("dd/MM/yyyy");

// Une seule definition des colonnes pour eviter les nombres comme item(ligne, 5).
// L'ordre correspond aux titres dans creerListeVehicules().
enum ColonneVehicule {
    ColId,
    ColMatricule,
    ColType,
    ColCarburant,
    ColSeuilCarburant,
    ColEtat,
    ColLocalisation,
    ColDateAchat,
    ColDernierEntretien,
    ColProchainEntretien,
    ColActions,
    ColMarque,
    ColModele,
    ColRemarques,
    ColSante,
    ColIcone,
    ColKilometrage,
    ColDisponibilite,
    NombreColonnes
};

// Calculs metier et identification des lignes : vehicules.cpp.
int scoreSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine);
QStringList facteursSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine);
QString recommandation(int score, const QDate &prochaine, QDate &suggeree);
bool ligneVehiculeComplete(QTableWidget *tableauVehicules, int row);
int trouverLigneParId(QTableWidget *tableauVehicules, const QString &id);

// Presentation : vehicules_view.cpp.
void toast(QWidget *parent, const QString &message, const QString &couleur = "#2e7d32");
void majEtatVide(QTableWidget *tableauVehicules);
}
#endif
