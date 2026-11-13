# Ouvrir Smart Fire Station sur le PC d’un collègue

1. Récupérer la branche `Integration-CRUD` du dépôt GitHub. Dans Qt Creator, ouvrir `Personnel/FireStation.pro` et choisir un kit Qt 6 64 bits avec Qt Charts et le pilote Qt SQL ODBC. Compiler puis lancer le projet.
2. Dans l’administrateur ODBC Windows **64 bits**, créer la source `SmartFireStation` avec le pilote Oracle. Pour voir les mêmes données que l’équipe, configurer cette source sur le même serveur, port et service Oracle que les autres postes. Pour une base de travail locale, utiliser l’adresse de l’instance locale.
3. Le programme utilise le compte Oracle `smartfire` par défaut. Si la configuration diffère, renseigner `SMARTFIRE_ODBC_DSN`, `SMARTFIRE_DB_USER` et `SMARTFIRE_DB_PASSWORD` dans l’environnement d’exécution de Qt Creator.
4. Se connecter dans l’application avec un compte **Chef de caserne** ou **Responsable RH**, puis choisir **Formations** dans le menu de gauche.

## Oracle SQL Developer

Pour une base commune neuve, créer le compte avec `BaseDeDonnees/1_creer_utilisateur.sql`, puis ouvrir une connexion avec ce compte. Exécuter `2_creer_tables.sql` et `3_donnees_agents.sql` en mode script (F5).

Pour une base existante, utiliser les migrations ci-dessous. Le script `2_creer_tables.sql` recrée les tables communes et efface leurs données.

1. `11_compatibilite_formations.sql` prépare la séquence des formations et reprend les anciennes inscriptions lorsque la base contient la table historique `INSCRIPTION`.
2. `10_migration_statut_inscription.sql` ajoute le statut « En attente » au schéma commun si nécessaire.
3. `12_tables_module_formations.sql` crée les référentiels de types, de niveaux et de statuts.
4. `13_resultats_inscription.sql` ajoute le référentiel des résultats et affiche les huit tables du module.

| Table | Contenu |
| --- | --- |
| `FORMATION` | Sessions et identifiant automatique |
| `AGENT` | Personnel partagé avec le module Personnel |
| `INSCRIRE` | Inscriptions, présence et résultat. Clé composée de la formation et de l’agent |
| `TYPE_FORMATION` | Types de formation |
| `NIVEAU_FORMATION` | Niveaux de formation |
| `STATUT_FORMATION` | Statuts des sessions |
| `STATUT_INSCRIPTION` | Statuts des inscriptions |
| `RESULTAT_INSCRIPTION` | Résultats possibles des inscriptions |
