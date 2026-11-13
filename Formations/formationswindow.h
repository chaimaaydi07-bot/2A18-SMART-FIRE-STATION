#ifndef FORMATIONSWINDOW_H
#define FORMATIONSWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QStringList>
#include <QAbstractItemModel>

#include <QtCharts/QChartView>
#include "formation.h"
#include "inscription.h"

class FormationsWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit FormationsWindow(QWidget *parent = nullptr);
    void setModeApercu(bool actif);   // true = pas de base Oracle : donnees de demonstration

private slots:
    void on_ajouter();
    void on_modifier();
    void on_supprimerForm();          // bouton « Supprimer » du formulaire
    void on_modifierLigne(int id);   // bouton « Modifier » de chaque ligne
    void on_supprimer(int id);
    void on_voir(int id);
    void on_trier();
    void on_pdf();
    void on_stats();
    void on_calendrier();
    void on_vider();
    void on_alerteDetails();
    void on_gestionInscriptions();

private:
    void construireUI();
    QWidget* construireFormulaire();
    QWidget* construireListe();
    void rafraichir();
    void majAlerte();
    bool controleSaisie(QString &erreur);
    Formation lireFormulaire() const;
    static QWidget* champ(const QString &label, QWidget *w);

    bool modeApercu = false;
    QList<QStringList> demo;          // donnees de demonstration (modifiables en memoire)
    QAbstractItemModel* modeleApercu(const QString &critere) const;
    int demoIndex(int id) const;
    static QStringList ligneDemo(const Formation &f);
    void initDemo();
    static void effetLueur(QWidget *w);
    Formation Ftmp;

    // formulaire
    int formationSelectionnee = 0;
    QLineEdit *leTitre, *leFormateur, *leLieu;
    QComboBox *cbType, *cbNiveau, *cbStatut;
    QDateEdit *deDebut, *deFin;
    QSpinBox *sbDuree, *sbPlaces;

    // liste
    QLineEdit *leRecherche;
    QComboBox *cbFiltre, *cbTri;
    QTableWidget *table;
    QLabel *lbCompteur, *lbAlerteTexte, *lbAlerteDetail;

    // etat courant du tri
    QString triColonne = "DATE_DEBUT";
    bool triCroissant = false;
};

#endif // FORMATIONSWINDOW_H
