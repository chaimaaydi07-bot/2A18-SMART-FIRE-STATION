#include "vehiculerepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace {
QString messageErreur(const QSqlError &error) {
    // Le pilote Oracle 11g peut ajouter des octets apres le premier NUL.
    const QString texte = error.text().section(QChar(0), 0, 0);
    if (texte.contains("ORA-00001") && texte.contains("UQ_VEHICULE_IMMAT"))
        return "Ce matricule existe déjà. Choisissez un matricule unique.";
    return texte;
}
QSqlDatabase connexion(QString &erreur) {
    auto db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    if (!db.isValid() || !db.isOpen()) erreur = "La connexion Oracle est fermée.";
    return db;
}
QVariant dateSql(const QDate &date) {
    return date.isValid() ? QVariant(date.toString("yyyy-MM-dd")) : QVariant(QString());
}
bool executer(QSqlQuery &query, const QString &sql, const QVariantList &valeurs, QString &erreur) {
    if (!query.prepare(sql)) { erreur = messageErreur(query.lastError()); return false; }
    for (const auto &valeur : valeurs) query.addBindValue(valeur);
    if (!query.exec()) { erreur = messageErreur(query.lastError()); return false; }
    return true;
}
bool verrouiller(QSqlQuery &query, const QString &id, QString &erreur) {
    if (!executer(query, "SELECT id FROM VEHICULE WHERE id = ? FOR UPDATE", {id}, erreur)) return false;
    const bool existe = query.next();
    if (!existe) erreur = "Ce véhicule n'existe plus. Actualisez la liste.";
    query.finish();
    return existe;
}
bool terminer(QSqlDatabase &db, QSqlQuery &query, bool succes, QString &erreur) {
    query.finish();
    if (succes && db.commit()) return true;
    if (succes) erreur = messageErreur(db.lastError());
    db.rollback();
    return false;
}
}

bool VehiculeRepository::charger(QVector<VehiculeRecord> &records, QString &erreur) {
    erreur.clear();
    auto db = connexion(erreur);
    if (!erreur.isEmpty()) return false;
    QSqlQuery query(db);
    if (!query.exec("SELECT id, matricule, type, marque, modele, carburant, seuil_carburant, etat, "
                    "localisation, date_achat, date_derniere_entretien, date_prochaine_entretien, "
                    "remarques, kilometrage, disponibilite FROM VEHICULE ORDER BY id")) {
        erreur = messageErreur(query.lastError()); return false;
    }
    QVector<VehiculeRecord> resultat;
    while (query.next()) {
        VehiculeRecord r;
        r.id = query.value(0).toString(); r.matricule = query.value(1).toString();
        r.type = query.value(2).toString(); r.marque = query.value(3).toString();
        r.modele = query.value(4).toString(); r.carburant = query.value(5).toInt();
        r.seuil = query.value(6).toInt(); r.etat = query.value(7).toString();
        r.localisation = query.value(8).toString(); r.achat = query.value(9).toDate();
        r.entretien = query.value(10).toDate(); r.prochainEntretien = query.value(11).toDate();
        r.remarques = query.value(12).toString(); r.kilometrage = query.value(13).toLongLong();
        r.disponibilite = query.value(14).toString();
        resultat.append(r);
    }
    if (query.lastError().isValid()) { erreur = messageErreur(query.lastError()); return false; }
    records = resultat;
    return true;
}

bool VehiculeRepository::enregistrer(VehiculeRecord &r, bool modification, QString &erreur) {
    erreur.clear();
    if (r.matricule.trimmed().isEmpty() || r.matricule.size() > 20 || r.type.isEmpty()
        || r.type.size() > 40 || r.marque.size() > 50 || r.modele.size() > 50
        || r.localisation.size() > 100 || r.remarques.size() > 1000
        || r.etat.isEmpty() || r.etat.size() > 20 || r.disponibilite.isEmpty()
        || r.disponibilite.size() > 20 || r.carburant < 0 || r.carburant > 100
        || r.seuil < 0 || r.seuil > 100 || r.kilometrage < 0 || r.kilometrage > 9999999999LL
        || (r.entretien.isValid() && r.prochainEntretien.isValid() && r.prochainEntretien <= r.entretien)) {
        erreur = "Vérifiez les champs du véhicule et les dates de maintenance."; return false;
    }
    auto db = connexion(erreur);
    if (!erreur.isEmpty()) return false;
    if (!db.transaction()) { erreur = messageErreur(db.lastError()); return false; }
    QSqlQuery query(db);
    QString id = r.id;
    if (modification) {
        if (!verrouiller(query, id, erreur)) return terminer(db, query, false, erreur);
    } else {
        if (!query.exec("SELECT seq_vehicule_id.NEXTVAL FROM DUAL") || !query.next()) {
            erreur = messageErreur(query.lastError()); return terminer(db, query, false, erreur);
        }
        id = query.value(0).toString(); query.finish();
    }
    // Oracle ODBC 11g ne gere pas la liaison SQL_BIGINT : envoyer les grands
    // entiers sous forme decimale, qu'Oracle convertit vers NUMBER sans perte.
    QVariantList valeurs = {r.matricule.trimmed(), r.type, r.marque, r.modele, r.carburant, r.seuil,
        r.etat, r.localisation, dateSql(r.achat), dateSql(r.entretien), dateSql(r.prochainEntretien),
        r.remarques, QString::number(r.kilometrage), r.disponibilite, id};
    const QString sql = modification
        ? "UPDATE VEHICULE SET matricule=?, type=?, marque=?, modele=?, carburant=?, seuil_carburant=?, "
          "etat=?, localisation=?, date_achat=TO_DATE(?,'YYYY-MM-DD'), "
          "date_derniere_entretien=TO_DATE(?,'YYYY-MM-DD'), date_prochaine_entretien=TO_DATE(?,'YYYY-MM-DD'), "
          "remarques=?, kilometrage=?, disponibilite=? WHERE id=?"
        : "INSERT INTO VEHICULE (matricule,type,marque,modele,carburant,seuil_carburant,etat,localisation,"
          "date_achat,date_derniere_entretien,date_prochaine_entretien,remarques,kilometrage,disponibilite,id) "
          "VALUES (?,?,?,?,?,?,?,?,TO_DATE(?,'YYYY-MM-DD'),TO_DATE(?,'YYYY-MM-DD'),TO_DATE(?,'YYYY-MM-DD'),?,?,?,?)";
    if (!terminer(db, query, executer(query, sql, valeurs, erreur), erreur)) return false;
    r.id = id;
    return true;
}

bool VehiculeRepository::supprimer(const QString &id, QString &erreur) {
    erreur.clear();
    auto db = connexion(erreur);
    if (!erreur.isEmpty()) return false;
    if (!db.transaction()) { erreur = messageErreur(db.lastError()); return false; }
    QSqlQuery query(db);
    if (!verrouiller(query, id, erreur)) return terminer(db, query, false, erreur);
    return terminer(db, query, executer(query, "DELETE FROM VEHICULE WHERE id=?", {id}, erreur), erreur);
}

bool VehiculeRepository::planifier(const QVector<QPair<QString, QDate>> &dates, QString &erreur) {
    erreur.clear();
    auto db = connexion(erreur);
    if (!erreur.isEmpty()) return false;
    if (!db.transaction()) { erreur = messageErreur(db.lastError()); return false; }
    QSqlQuery query(db);
    for (const auto &date : dates) {
        if (!verrouiller(query, date.first, erreur)) return terminer(db, query, false, erreur);
        if (!executer(query, "UPDATE VEHICULE SET date_prochaine_entretien=TO_DATE(?,'YYYY-MM-DD') "
                     "WHERE id=? AND (date_derniere_entretien IS NULL OR date_derniere_entretien<TO_DATE(?,'YYYY-MM-DD'))",
                     {dateSql(date.second), date.first, dateSql(date.second)}, erreur))
            return terminer(db, query, false, erreur);
        if (query.numRowsAffected() == 0) {
            erreur = "La date d'entretien a changé. Actualisez la liste.";
            return terminer(db, query, false, erreur);
        }
        query.finish();
    }
    return terminer(db, query, true, erreur);
}
