#ifndef PAGECONNEXION_H
#define PAGECONNEXION_H

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTimer;

// Un compte utilisateur = un employé (agent) qui a un mot de passe.
// Le rôle correspond au poste de l'agent (Responsable RH, Chef de caserne...).
struct Compte {
    QString identifiant;   // ID de l'agent, ex. AGT-018
    QString nom;           // nom affiché, ex. Mansouri Leila
    QString role;          // poste de l'agent
    QString mdpHash;       // mot de passe chiffré (SHA-256), jamais en clair
    QString tel;           // téléphone (code de réinitialisation envoyé par SMS)
};

// Page d'authentification commune à tous les modules.
// Elle ne connaît aucun module : en cas de succès, elle émet connexionReussie()
// avec le nom et le rôle de l'utilisateur.
class PageConnexion : public QWidget
{
    Q_OBJECT

public:
    explicit PageConnexion(QWidget *parent = nullptr);

    void setComptes(const QList<Compte> &comptes);   // liste des utilisateurs autorisés
    void reinitialiser();                            // vide les champs (après déconnexion)

    static QString hacher(const QString &motDePasse); // chiffrement SHA-256

signals:
    void connexionReussie(const QString &identifiant, const QString &nom, const QString &role);
    // Mot de passe oublié : demande d'envoi du code par SMS, puis nouveau mot de passe
    void smsDemande(const QString &tel, const QString &message, const QString &destinataire);
    void motDePasseReinitialise(const QString &identifiant, const QString &mdpHash);

private slots:
    void seConnecter();
    void afficherMasquerMdp();
    void motDePasseOublie();

private:
    void bloquer();

    QList<Compte> m_comptes;
    QLineEdit    *edIdentifiant = nullptr;
    QLineEdit    *edMdp = nullptr;
    QToolButton  *btnOeil = nullptr;
    QPushButton  *btnConnexion = nullptr;
    QLabel       *lblErreur = nullptr;
    QTimer       *m_minuteur = nullptr;
    int           m_echecs = 0;          // tentatives échouées consécutives
    int           m_secondesRestantes = 0;
};

#endif // PAGECONNEXION_H