# Modele de donnees commun - Smart Fire Station

## Modele entite-association

- `FORMATION` : une session de formation (intitule, type, dates, formateur, lieu, duree, capacite, certification et statut).
- `AGENT` : entite Personnel commune, identifiee par `ID_AGENT`.
- `INSCRIPTION` : association entre un agent et une formation, avec etat, presence et resultat.
- Une formation possede zero a plusieurs inscriptions; un agent possede zero a plusieurs inscriptions. Chaque inscription concerne exactement une formation et un agent.

```text
FORMATION (1) ---- (0..N) INSCRIPTION
AGENT     (1) ---- (0..N) INSCRIPTION
PK INSCRIPTION: (ID_FORMATION, ID_AGENT)
```

## Modele relationnel physique Oracle

```text
FORMATION(
  ID PK, TITRE, TYPE, DATE_DEBUT, DATE_FIN, DUREE,
  FORMATEUR, LIEU, NB_PLACES, NIVEAU, CERTIFICATION, STATUT
)
AGENT(ID_AGENT PK, NOM, PRENOM, ...)
INSCRIPTION(
  ID_FORMATION PK/FK -> FORMATION.ID,
  ID_AGENT PK/FK -> AGENT.ID_AGENT,
  DATE_INSCRIPTION, ETAT, PRESENCE, RESULTAT
)
```

L'interface Qt nomme la capacite `capacite` et l'intitule `intitule`; le modele Oracle commun les stocke sous `NB_PLACES` et `TITRE`. `INSCRIPTION` utilise sa cle naturelle composite. Les contraintes Oracle verifient les identifiants positifs, les dates, la duree, la capacite, les valeurs de certification et les statuts. Les contraintes et la sequence `SEQ_FORMATION` sont ajoutees par `schema_commun_oracle.sql` sans supprimer les donnees. Les anciennes valeurs de statut et le nom `STATUT_INSCRIPTION` sont migres vers les codes utilises par l'application.

La base commune contient egalement `CERTIFICATION`, `INCIDENT`, `BATIMENT_RISQUE`, `VEHICULE`, `EQUIPEMENT` et `INTERVENTION`. Le script ne recree ni ne supprime ces tables partagees. Si `INSCRIPTION` contient encore des lignes de l'ancienne structure `ID_POMPIER`, le script s'arrete et demande une migration manuelle pour eviter d'associer des personnes aux mauvais agents.

## Connexion Oracle Qt

L'application utilise une seule connexion Qt `QODBC`. Le DSN Windows 64 bits est `SmartFireStation` par defaut et doit pointer vers `localhost:1521/XEPDB1`. Le DSN, le nom de schema et le mot de passe sont configurables avec `SMARTFIRE_ODBC_DSN`, `SMARTFIRE_DB_USER` et `SMARTFIRE_DB_PASSWORD`. Le mot de passe n'est pas stocke dans les sources. Exemple PowerShell pour la session de lancement de Qt Creator :

```powershell
$env:SMARTFIRE_DB_PASSWORD = "<mot-de-passe Oracle>"
```

Executer `schema_commun_oracle.sql` une fois dans le schema commun, puis ouvrir l'application depuis le meme environnement que Qt Creator. Les requetes du modele utilisent `prepare()` et les parametres lies; les ecritures rechargent ensuite les modeles d'affichage.
