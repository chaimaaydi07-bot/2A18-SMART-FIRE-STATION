#pragma once
#include <QMainWindow>
#include <QDate>
#include <QList>
#include <QString>

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

struct Agent {
    QString id, nom, prenom, fonction, grade, specialite, tel, dispo;
    QString rh, chef;               // affectés automatiquement (non saisis à l'ajout)
    QList<Certification> certs;
    int interv48 = 0;               // interventions sur les dernières 48 h
    int reposH   = 24;              // heures de repos depuis la dernière garde
    int mois     = 0;               // interventions sur les 30 derniers jours
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

private slots:
    void ajouter();
    void modifier();
    void reinitialiser();
    void onFonctionChanged();
    void rafraichir();
    void exporterPdf();
    void afficherStats();
    void exporterStatsPdf();

private:
    QWidget *creerSidebar();
    QWidget *creerFormulaire();
    QWidget *creerListe();
    QWidget *creerStatsWidget();

    void chargerDonnees();
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
    void ajouterAlerte(const QString &titre, const QString &detail,
                       bool info, const QString &idAgent);

    QList<Agent> m_agents;
    QString      m_editingId;

    // navigation
    QStackedWidget *pages = nullptr;
    QScrollArea    *statsScroll = nullptr;
    QWidget        *m_statsContent = nullptr;
    QPushButton    *m_btnStatsRetour = nullptr, *m_btnStatsExport = nullptr;
    QPushButton    *navPersonnel = nullptr;

    // formulaire
    QLabel       *lblFormTitle = nullptr;
    QComboBox    *cbFonction = nullptr, *cbGrade = nullptr, *cbSpec = nullptr, *cbDispo = nullptr;
    QLineEdit    *edNom = nullptr, *edPrenom = nullptr, *edTel = nullptr;
    QVBoxLayout  *certsLayout = nullptr;
    QList<CertRow> m_certRows;
    QLabel       *lblErreur = nullptr;
    QPushButton  *btnAjouter = nullptr, *btnModifier = nullptr;

    // liste
    QComboBox    *cbTri = nullptr, *cbFiltreFonction = nullptr;
    QLineEdit    *edRecherche = nullptr;
    QTableWidget *tblAgents = nullptr;
    QVBoxLayout  *alertesLayout = nullptr;
};