#pragma once
#include <QMainWindow>
#include <QDate>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
#include "pageconnexion.h"

class QLineEdit;
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QStackedWidget;
class QScrollArea;
class QFrame;
class QDateEdit;
class QWidget;

struct Certification {
    QString nom;
    QDate   obtention;
    QDate   echeance;
};

// Agent de l'USPC
// Les utilisateurs de l'application sont des agents qui ont un mot de passe ;
// leur rôle (droits d'accès) correspond à leur poste.
struct Agent {
    QString id, nom, prenom, poste, grade, specialite, tel, dispo;
    QList<Certification> certs;
    int mois = 0;                   // interventions sur les 30 derniers jours (statistiques)
    QString mdpHash;                // mot de passe chiffré (vide = n'utilise pas l'application)
};

// Session de formation proposée par un centre (catalogue des formations)
struct SessionFormation {
    QString     id;
    QString     certification;      // nom de la certification délivrée
    QString     lieu;
    QDate       date;
    int         places = 0;         // places restantes
    QStringList inscrits;           // id des agents inscrits
};

// Une recommandation calculée pour un agent
struct Recommandation {
    QString agentId;
    QString certification;
    QString motif;                  // pourquoi elle est proposée (poste, spécialité, grade)
    QString statut;                 // « Manquante », « Expire dans 12 j », « Expirée »
    bool    urgente = false;
    bool    inscrit = false;        // l'agent est déjà inscrit à la session
    int     sessionIndex = -1;      // index dans m_sessions (-1 = aucune session programmée)
};

// Une ligne de saisie de certification dans le formulaire
struct CertRow {
    QFrame    *frame = nullptr;
    QLineEdit *nom   = nullptr;
    QDateEdit *obt   = nullptr;
    QDateEdit *ech   = nullptr;
};

class FireStation : public QMainWindow
{
    Q_OBJECT
public:
    explicit FireStation(QWidget *parent = nullptr);

    // Authentification
    QList<Compte> comptes() const;                                // agents ayant un mot de passe
    void appliquerRole(const QString &identifiant, const QString &nom,
                       const QString &role);                      // adapte le menu et les droits

    // SMS (simulation) et mot de passe, utilisés aussi par la page de connexion
    void envoyerSms(const QString &tel, const QString &message, const QString &destinataire);
    void changerMotDePasse(const QString &identifiant, const QString &mdpHash);

signals:
    void deconnexion();

private slots:
    void ajouter();
    void modifier();
    void reinitialiser();
    void onPosteChanged();
    void rafraichir();
    void exporterPdf();
    void afficherStats();
    void exporterStatsPdf();
    void afficherFormations();
    void afficherJournalSms();

private:
    QWidget *creerSidebar();
    QWidget *creerFormulaire();
    QWidget *creerListe();
    QWidget *creerStatsWidget();
    QWidget *creerFormationsWidget();

    void chargerDonnees();
    void chargerSessions();
    bool lireFormulaire(Agent &a, QString &erreur);
    QString nouvelId() const;
    int indexParId(const QString &id) const;

    void consulter(const QString &id);
    void chargerDansFormulaire(const QString &id);
    void supprimer(const QString &id);

    void ajouterCertLigne(const Certification &c = Certification());
    void retirerCert(QFrame *frame);
    void viderCerts();
    void mettreAJourAlertes();
    void ajouterAlerte(const QString &titre, const QString &detail, bool info,
                       const QString &idAgent, const QString &actionTxt = QString(),
                       std::function<void()> action = nullptr);

    // Planning de garde hebdomadaire (export PDF)
    bool genererPlanningPdf(const QString &chemin, QDate debut, int agentsParGarde);

    // Navigation animée entre les pages
    void allerPage(int index);

    // Innovation 1 : SMS de renouvellement des certifications
    void verifierEcheancesSms();           // envoi automatique
    QString cleSms(const Agent &a, const Certification &c) const;

    // Innovation 2 : recommandation de certifications
    QList<Recommandation> recommandationsPour(const Agent &a) const;
    void inscrire(const QString &agentId, int sessionIndex);

    QList<Agent>            m_agents;
    QList<SessionFormation> m_sessions;
    QString                 m_editingId;
    QSet<QString>           m_smsEnvoyes;   // certifications déjà notifiées par SMS
    QStringList             m_journalSms;   // historique des SMS

    // navigation
    QStackedWidget *pages = nullptr;
    QScrollArea    *statsScroll = nullptr, *formScroll = nullptr;
    QWidget        *m_statsContent = nullptr;
    QPushButton    *m_btnStatsRetour = nullptr, *m_btnStatsExport = nullptr;
    QPushButton    *navPersonnel = nullptr;
    QString         m_formFiltre;   // agent sélectionné sur la page Formations

    // formulaire
    QLabel       *lblFormTitle = nullptr;
    QComboBox    *cbPoste = nullptr, *cbGrade = nullptr, *cbSpec = nullptr, *cbDispo = nullptr;
    QLineEdit    *edNom = nullptr, *edPrenom = nullptr, *edTel = nullptr, *edMdp = nullptr;
    QFrame       *m_carteFormulaire = nullptr;
    QVBoxLayout  *certsLayout = nullptr;
    QList<CertRow> m_certRows;
    QLabel       *lblErreur = nullptr;
    QPushButton  *btnAjouter = nullptr, *btnModifier = nullptr;

    // liste
    QComboBox    *cbTri = nullptr, *cbFiltrePoste = nullptr;
    QLineEdit    *edRecherche = nullptr;
    QTableWidget *tblAgents = nullptr;
    QVBoxLayout  *alertesLayout = nullptr;

    // droits d'accès
    QLabel       *lblActeur = nullptr;
    QLabel       *lblAVenir = nullptr;
    QMap<QString, QPushButton *> m_boutonsModules;
    bool          m_lectureSeule = false;   // true = consultation uniquement
    QString       m_idConnecte;             // ID de l'agent connecté
    QString       m_roleConnecte;           // rôle de l'agent connecté
    void          majUtilisateurConnecte(); // met à jour le nom affiché dans le menu
};