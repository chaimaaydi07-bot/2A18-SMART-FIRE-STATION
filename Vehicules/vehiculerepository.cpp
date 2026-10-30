// ACCES A ORACLE :
// charger()       -> SELECT (lire les vehicules)
// enregistrer()   -> INSERT ou UPDATE (ajouter ou modifier)
// supprimer()     -> DELETE (supprimer)
// planifier()     -> UPDATE des dates de maintenance
// Les ? sont des parametres : ne pas construire du SQL avec les saisies utilisateur.
#include "vehiculerepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace {
// Limites correspondant aux colonnes de la table Oracle.
bool verifierVehicule(const VehiculeRecord &vehicule, QString &erreur)
{
    bool textesValides = !vehicule.matricule.trimmed().isEmpty()
    && vehicule.matricule.size() <= 20
        && !vehicule.type.isEmpty() && vehicule.type.size() <= 40
        && vehicule.marque.size() <= 50 && vehicule.modele.size() <= 50
        && vehicule.localisation.size() <= 100 && vehicule.remarques.size() <= 1000
        && !vehicule.etat.isEmpty() && vehicule.etat.size() <= 20
        && !vehicule.disponibilite.isEmpty() && vehicule.disponibilite.size() <= 20;
    if (!textesValides) {
        erreur = "Vérifiez les champs obligatoires et la longueur des textes.";
        return false;
    }

    bool nombresValides = vehicule.carburant >= 0 && vehicule.carburant <= 100
                          && vehicule.seuil >= 0 && vehicule.seuil <= 100
                          && vehicule.kilometrage >= 0 && vehicule.kilometrage <= 9999999999LL;
    if (!nombresValides) {
        erreur = "Le carburant doit être entre 0 et 100 %, et le kilométrage positif ou nul.";
        return false;
    }

    if (vehicule.entretien.isValid() && vehicule.prochainEntretien.isValid()
        && vehicule.prochainEntretien <= vehicule.entretien) {
        erreur = "La prochaine maintenance doit être après la dernière.";
        return false;
    }
    return true;
}

QString messageErreur(const QSqlError &error) {
    // Le pilote Oracle 11g peut ajouter des octets apres le premier NUL.
    const QString texte = error.text().section(QChar(0), 0, 0);
    if (texte.contains("ORA-00001") && (texte.contains("UQ_VEHICULE_IMMAT") || texte.contains("UQ_VEHICULE_MATRICULE"))) {
        return "Ce matricule existe déjà. Choisissez un matricule unique.";
    }
    return texte;
}
QSqlDatabase connexion(QString &erreur) {
    QSqlDatabase base = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    if (!base.isValid() || !base.isOpen()) {
        erreur = "La connexion Oracle est fermée.";
    }
    return base;
}
QVariant dateSql(const QDate &date) {
    if (date.isValid()) {
        return date.toString("yyyy-MM-dd");
    }
    return QVariant(QString()); // Date absente : envoyer NULL a Oracle.
}
bool executer(QSqlQuery &requete, const QString &sql, const QVariantList &valeurs, QString &erreur) {
    if (!requete.prepare(sql)) {
        erreur = messageErreur(requete.lastError());
        return false;
    }
    // Les valeurs sont liees aux ? dans le meme ordre que la requete SQL.
    for (const QVariant &valeur : valeurs) {
        requete.addBindValue(valeur);
    }
    if (!requete.exec()) {
        erreur = messageErreur(requete.lastError());
        return false;
    }
    return true;
}
bool verrouiller(QSqlQuery &requete, const QString &id, QString &erreur) {
    // Verrouiller la ligne pendant sa modification ou sa suppression.
    if (!executer(requete, "SELECT id_vehicule FROM VEHICULE WHERE id_vehicule = ? FOR UPDATE", {id}, erreur)) {
        return false;
    }
    const bool existe = requete.next();
    if (!existe) {
        erreur = "Ce véhicule n'existe plus. Actualisez la liste.";
    }
    requete.finish();
    return existe;
}
bool terminerTransaction(QSqlDatabase &base, QSqlQuery &requete, bool succes, QString &erreur) {
    requete.finish();
    if (succes && base.commit()) {
        return true; // COMMIT : garder les changements dans Oracle.
    }
    if (succes) {
        erreur = messageErreur(base.lastError());
    }
    base.rollback(); // ROLLBACK : annuler une operation qui a echoue.
    return false;
}
}

bool VehiculeRepository::charger(QVector<VehiculeRecord> &records, QString &erreur) {
    erreur.clear();
    QSqlDatabase base = connexion(erreur);
    if (!erreur.isEmpty()) {
        return false;
    }
    QSqlQuery requete(base);
    if (!requete.exec("SELECT id_vehicule AS id, matricule, type, marque, modele, carburant, seuil_carburant, etat, "
                      "localisation, date_achat, date_derniere_entretien, date_prochaine_entretien, "
                      "remarques, kilometrage, disponibilite FROM VEHICULE ORDER BY id_vehicule")) {
        erreur = messageErreur(requete.lastError());
        return false;
    }
    QVector<VehiculeRecord> resultat;
    while (requete.next()) {
        VehiculeRecord vehicule;
        vehicule.id = requete.value("ID").toString();
        vehicule.matricule = requete.value("MATRICULE").toString();
        vehicule.type = requete.value("TYPE").toString();
        vehicule.marque = requete.value("MARQUE").toString();
        vehicule.modele = requete.value("MODELE").toString();
        vehicule.carburant = requete.value("CARBURANT").toInt();
        vehicule.seuil = requete.value("SEUIL_CARBURANT").toInt();
        vehicule.etat = requete.value("ETAT").toString();
        vehicule.localisation = requete.value("LOCALISATION").toString();
        vehicule.achat = requete.value("DATE_ACHAT").toDate();
        vehicule.entretien = requete.value("DATE_DERNIERE_ENTRETIEN").toDate();
        vehicule.prochainEntretien = requete.value("DATE_PROCHAINE_ENTRETIEN").toDate();
        vehicule.remarques = requete.value("REMARQUES").toString();
        vehicule.kilometrage = requete.value("KILOMETRAGE").toLongLong();
        vehicule.disponibilite = requete.value("DISPONIBILITE").toString();
        resultat.append(vehicule);
    }
    if (requete.lastError().isValid()) {
        erreur = messageErreur(requete.lastError());
        return false;
    }
    records = resultat;
    return true;
}

bool VehiculeRepository::enregistrer(VehiculeRecord &vehicule, bool modification, QString &erreur) {
    erreur.clear();
    // Verifier les champs avant de commencer une transaction.
    if (!verifierVehicule(vehicule, erreur)) {
        return false;
    }
    QSqlDatabase base = connexion(erreur);
    if (!erreur.isEmpty()) {
        return false;
    }
    // Debut d'une transaction : tout reussit, ou tout est annule.
    if (!base.transaction()) {
        erreur = messageErreur(base.lastError());
        return false;
    }
    QSqlQuery requete(base);
    QString id = vehicule.id;
    if (modification) {
        if (!verrouiller(requete, id, erreur)) {
            return terminerTransaction(base, requete, false, erreur);
        }
    } else {
        if (!requete.exec("SELECT seq_vehicule_id.NEXTVAL FROM DUAL") || !requete.next()) {
            erreur = messageErreur(requete.lastError());
            return terminerTransaction(base, requete, false, erreur);
        }
        id = requete.value(0).toString();
        requete.finish();
    }
    // Oracle ODBC 11g ne gere pas la liaison SQL_BIGINT : envoyer les grands
    // entiers sous forme decimale, qu'Oracle convertit vers NUMBER sans perte.
    QVariantList valeurs = {
        vehicule.matricule.trimmed(),
        vehicule.type,
        vehicule.marque,
        vehicule.modele,
        vehicule.carburant,
        vehicule.seuil,
        vehicule.etat,
        vehicule.localisation,
        dateSql(vehicule.achat),
        dateSql(vehicule.entretien),
        dateSql(vehicule.prochainEntretien),
        vehicule.remarques,
        QString::number(vehicule.kilometrage),
        vehicule.disponibilite,
        id
    };
    QString sql;
    if (modification) {
        sql = "UPDATE VEHICULE SET matricule=?, type=?, marque=?, modele=?, carburant=?, seuil_carburant=?, "
              "etat=?, localisation=?, date_achat=TO_DATE(?,'YYYY-MM-DD'), "
              "date_derniere_entretien=TO_DATE(?,'YYYY-MM-DD'), date_prochaine_entretien=TO_DATE(?,'YYYY-MM-DD'), "
              "remarques=?, kilometrage=?, disponibilite=? WHERE id_vehicule=?";
    } else {
        sql = "INSERT INTO VEHICULE (matricule,type,marque,modele,carburant,seuil_carburant,etat,localisation,"
              "date_achat,date_derniere_entretien,date_prochaine_entretien,remarques,kilometrage,disponibilite,id_vehicule) "
              "VALUES (?,?,?,?,?,?,?,?,TO_DATE(?,'YYYY-MM-DD'),TO_DATE(?,'YYYY-MM-DD'),TO_DATE(?,'YYYY-MM-DD'),?,?,?,?)";
    }

    bool requeteReussie = executer(requete, sql, valeurs, erreur);
    if (!terminerTransaction(base, requete, requeteReussie, erreur)) {
        return false;
    }
    vehicule.id = id;
    return true;
}

bool VehiculeRepository::supprimer(const QString &id, QString &erreur) {
    erreur.clear();
    QSqlDatabase base = connexion(erreur);
    if (!erreur.isEmpty()) {
        return false;
    }
    // Debut d'une transaction : tout reussit, ou tout est annule.
    if (!base.transaction()) {
        erreur = messageErreur(base.lastError());
        return false;
    }
    QSqlQuery requete(base);
    if (!verrouiller(requete, id, erreur)) {
        return terminerTransaction(base, requete, false, erreur);
    }
    bool requeteReussie = executer(requete, "DELETE FROM VEHICULE WHERE id_vehicule=?", {id}, erreur);
    return terminerTransaction(base, requete, requeteReussie, erreur);
}

bool VehiculeRepository::planifier(const QVector<QPair<QString, QDate>> &dates, QString &erreur) {
    erreur.clear();
    QSqlDatabase base = connexion(erreur);
    if (!erreur.isEmpty()) {
        return false;
    }
    // Debut d'une transaction : tout reussit, ou tout est annule.
    if (!base.transaction()) {
        erreur = messageErreur(base.lastError());
        return false;
    }
    QSqlQuery requete(base);
    for (const auto &date : dates) {
        if (!verrouiller(requete, date.first, erreur)) {
            return terminerTransaction(base, requete, false, erreur);
        }
        if (!executer(requete, "UPDATE VEHICULE SET date_prochaine_entretien=TO_DATE(?,'YYYY-MM-DD') "
                               "WHERE id_vehicule=? AND (date_derniere_entretien IS NULL OR date_derniere_entretien<TO_DATE(?,'YYYY-MM-DD'))",
                      {dateSql(date.second), date.first, dateSql(date.second)}, erreur)) {
            return terminerTransaction(base, requete, false, erreur);
        }
        if (requete.numRowsAffected() == 0) {
            erreur = "La date d'entretien a changé. Actualisez la liste.";
            return terminerTransaction(base, requete, false, erreur);
        }
        requete.finish();
    }
    return terminerTransaction(base, requete, true, erreur);
}