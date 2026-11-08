#include "inscriptiondialog.h"
#include "formation.h"
#include "inscription.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableView>
#include <QHeaderView>
#include <QSqlQueryModel>
#include <QMessageBox>
#include <QSqlError>
#include <utility>

InscriptionDialog::InscriptionDialog(int idFormation, QWidget *parent)
    : QDialog(parent), m_idFormation(idFormation)
{
    Formation f; Formation::charger(idFormation, f);
    setWindowTitle("Participants — " + f.intitule);
    resize(820, 480);

    m_info = new QLabel; m_info->setStyleSheet("font-weight:bold;");
    m_model = new QSqlQueryModel(this);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_agents = new QComboBox;
    auto *inscrire = new QPushButton("Inscrire");
    auto *desinscrire = new QPushButton("Désinscrire");
    auto *reco = new QPushButton("Agents recommandés…");
    auto *present = new QPushButton("Présent");
    auto *absent = new QPushButton("Absent");
    auto *reussi = new QPushButton("Réussi");
    auto *echoue = new QPushButton("Échoué");

    auto *haut = new QHBoxLayout;
    haut->addWidget(new QLabel("Agent :")); haut->addWidget(m_agents, 1);
    haut->addWidget(inscrire); haut->addWidget(reco);
    auto *bas = new QHBoxLayout;
    bas->addWidget(desinscrire); bas->addStretch();
    bas->addWidget(present); bas->addWidget(absent); bas->addWidget(reussi); bas->addWidget(echoue);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_info); lay->addLayout(haut); lay->addWidget(m_table); lay->addLayout(bas);

    connect(inscrire, &QPushButton::clicked, this, [this] {
        if (m_agents->currentIndex() < 0) return;
        afficher(Inscription::inscrire(m_idFormation, m_agents->currentData().toInt()));
    });
    connect(desinscrire, &QPushButton::clicked, this, [this] {
        const auto cle = inscriptionSelectionnee();
        if (cle.first < 0) { QMessageBox::information(this, "Participants", "Sélectionnez un participant."); return; }
        afficher(Inscription::desinscrire(cle.first, cle.second));
    });
    auto marquer = [this](int presence, const QString &res) {
        const auto cle = inscriptionSelectionnee();
        if (cle.first < 0) { QMessageBox::information(this, "Participants", "Sélectionnez un participant."); return; }
        afficher(Inscription::marquer(cle.first, cle.second, presence, res));
    };
    connect(present, &QPushButton::clicked, this, [=] { marquer(1, ""); });
    connect(absent,  &QPushButton::clicked, this, [=] { marquer(0, ""); });
    connect(reussi,  &QPushButton::clicked, this, [=] { marquer(-1, "reussi"); });
    connect(echoue,  &QPushButton::clicked, this, [=] { marquer(-1, "echoue"); });

    // Fonction innovante n°1 : recommandation à partir des certifications proches de l'échéance
    connect(reco, &QPushButton::clicked, this, [this] {
        const auto liste = Inscription::recommandations(m_idFormation);
        if (liste.isEmpty()) {
            QMessageBox::information(this, "Recommandations",
                "Aucun agent n'a de certification de ce type arrivant à échéance dans les 90 jours.");
            return;
        }
        QString txt = "Agents dont la certification expire dans les 90 jours (ou est expirée) :\n\n";
        for (const auto &r : liste)
            txt += QString("• %1 — échéance %2\n").arg(r.nom, r.echeance.toString("dd/MM/yyyy"));
        txt += "\nLes inscrire tous à cette formation ?";
        if (QMessageBox::question(this, "Recommandations", txt) != QMessageBox::Yes) return;
        QStringList rapport;
        for (const auto &r : liste) rapport << Inscription::inscrire(m_idFormation, r.idAgent).message;
        QMessageBox::information(this, "Résultat", rapport.join("\n"));
        rafraichir();
    });
    rafraichir();
}

QPair<int, int> InscriptionDialog::inscriptionSelectionnee() const
{
    const QModelIndex i = m_table->currentIndex();
    if (!i.isValid()) return {-1, -1};
    return {m_model->data(m_model->index(i.row(), 0)).toInt(),
            m_model->data(m_model->index(i.row(), 1)).toInt()};
}

void InscriptionDialog::afficher(const Retour &r)
{
    rafraichir();
    if (r.ok) QMessageBox::information(this, "Participants", r.message);
    else      QMessageBox::warning(this, "Participants", r.message);
}

void InscriptionDialog::rafraichir()
{
    Formation::majStatuts();
    Formation f; Formation::charger(m_idFormation, f);
    m_info->setText(QString("%1 — %2 | Du %3 au %4 | Places : %5 / %6 | Statut : %7")
        .arg(f.intitule, f.type, f.dateDebut.toString("dd/MM/yyyy"), f.dateFin.toString("dd/MM/yyyy"))
        .arg(Formation::inscrits(m_idFormation)).arg(f.capacite).arg(Formation::libelleStatut(f.statut)));

    QSqlQuery participants = Inscription::participants(m_idFormation);
    const QString erreurParticipants = participants.lastError().text();
    m_model->setQuery(std::move(participants));
    if (!erreurParticipants.isEmpty())
        m_info->setText("Erreur de chargement des participants : " + erreurParticipants);
    // Libellés lisibles
    m_model->setHeaderData(0, Qt::Horizontal, "Formation");
    m_model->setHeaderData(1, Qt::Horizontal, "ID agent");
    m_model->setHeaderData(2, Qt::Horizontal, "Agent");
    m_model->setHeaderData(3, Qt::Horizontal, "Grade");
    m_model->setHeaderData(4, Qt::Horizontal, "État");
    m_model->setHeaderData(5, Qt::Horizontal, "Présence");
    m_model->setHeaderData(6, Qt::Horizontal, "Résultat");
    m_table->hideColumn(0); m_table->hideColumn(1);

    const QVariant courant = m_agents->currentData();
    m_agents->clear();
    QSqlQuery agents = Inscription::agentsDisponibles(m_idFormation);
    if (!agents.exec()) {
        m_info->setText("Erreur de chargement des agents : " + agents.lastError().text());
        return;
    }
    while (agents.next()) m_agents->addItem(agents.value(1).toString(), agents.value(0));
    const int idx = m_agents->findData(courant);
    if (idx >= 0) m_agents->setCurrentIndex(idx);
}
