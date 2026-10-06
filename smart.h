#ifndef SMART_H
#define SMART_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDateEdit;
class QTableWidget;

class smart : public QMainWindow
{
    Q_OBJECT

public:
    smart(QWidget *parent = nullptr);
    void setUtilisateur(const QString &nomUtilisateur, const QString &role);

private slots:
    void ajouter();
    void modifier();
    void rechercher();
    void trier();

private:
    void ajouterLigne(const QString &n, const QString &t, const QString &m,
                      const QString &mod, int q, int s, const QString &e,
                      const QString &loc, const QString &da, const QString &dm,
                      const QString &pm, const QString &rem);
    void chargerFormulaire(int r);
    void colorierLignes();
    void afficherAlertes(bool afficherSiVide);
    void majCartes();
    void journaliser(const QString &action, const QString &equipement, const QString &details);
    void afficherJournal();
    void afficherMaintenancePredictive();
    void envoyerAlerteEmail(const QString &sujet, const QString &corps, const QString &ref, bool manuel = false);
    void afficherParametresEmail();
    void appliquerTheme(bool sombre);

    // formulaire
    QLineEdit *nom;
    QComboBox *type;
    QLineEdit *marque;
    QLineEdit *modele;
    QSpinBox  *quantite;
    QSpinBox  *seuil;
    QComboBox *etat;
    QLineEdit *localisation;
    QDateEdit *dateAchat;
    QDateEdit *dateMaintenance;
    QDateEdit *prochaineMaintenance;
    QLineEdit *remarques;

    // liste
    QLineEdit    *recherche;
    QComboBox    *triCombo;
    QComboBox    *filtreType;
    QTableWidget *table;

    int prochainId = 1;

    // journal d'activité : [date/heure, action, équipement, détails, utilisateur]
    QVector<QStringList> journal;
    QString utilisateurCourant = "Admin";
};

#endif // SMART_H
