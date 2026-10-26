-- ============================================================================
--  Smart Fire Station (USPC) — BASE DE DONNÉES COMMUNE (modèle physique)
--  ÉTAPE 2 : à exécuter dans SQL Developer, connecté avec l'utilisateur smartfire
--            (touche F5 = « Exécuter le script »)
--
--  Même script pour toute l'équipe : chacun l'exécute sur son PC pour avoir
--  exactement les mêmes tables, relations et contraintes.
--
--  8 tables (modèle relationnel validé) :
--    AGENT, INCIDENT, VEHICULE, FORMATION, EQUIPEMENT
--    INTERVENTION (INTERVENIR), MOBILISATION (MOBILISER), INSCRIRE (INSCRIRE)
--    CONTENIR (0,1 – 1,n) → clé étrangère #id_vehicule + date_affectation dans EQUIPEMENT
--
--  Compatible Oracle 11g XE et 21c XE (incrémentation par séquence + trigger).
--  ATTENTION : le bloc 0 supprime les anciennes tables et leurs données.
--  Après ce script : 3_donnees_agents.sql puis 6_donnees_vehicules.sql
--  (NE PLUS exécuter 4_migration_vehicule.sql ni 5_champs_formulaire_vehicule.sql)
-- ============================================================================


-- ----------------------------------------------------------------------------
-- 0. Suppression des anciens objets (pour pouvoir relancer le script)
-- ----------------------------------------------------------------------------
BEGIN
    FOR t IN (SELECT table_name FROM user_tables
              WHERE table_name IN ('INTERVENTION', 'MOBILISATION', 'INSCRIRE', 'EQUIPEMENT',
                                   'AGENT', 'INCIDENT', 'VEHICULE', 'FORMATION')) LOOP
        EXECUTE IMMEDIATE 'DROP TABLE ' || t.table_name || ' CASCADE CONSTRAINTS';
    END LOOP;
    FOR s IN (SELECT sequence_name FROM user_sequences
              WHERE sequence_name IN ('SEQ_VEHICULE_ID', 'SEQ_FORMATION_ID')) LOOP
        EXECUTE IMMEDIATE 'DROP SEQUENCE ' || s.sequence_name;
    END LOOP;
END;
/


-- ============================================================================
-- 1. ENTITÉS
-- ============================================================================

-- --------------------------- Module Gestion du personnel (Chaima) -----------
CREATE TABLE AGENT (
    id_agent        VARCHAR2(15)   CONSTRAINT pk_agent PRIMARY KEY,
    nom             VARCHAR2(50)   NOT NULL,
    prenom          VARCHAR2(50)   NOT NULL,
    poste           VARCHAR2(40)   NOT NULL
        CONSTRAINT ck_agent_poste CHECK (poste IN ('Pompier', 'Chauffeur', 'Maître-chien',
            'Secouriste', 'Mécanicien', 'Administratif', 'Dispatcher',
            'Responsable des équipements', 'Responsable RH', 'Chef de caserne')),
    grade           VARCHAR2(20)
        CONSTRAINT ck_agent_grade CHECK (grade IN ('Agent', 'Caporal', 'Sergent',
            'Adjudant', 'Lieutenant', 'Capitaine')),
    specialite      VARCHAR2(50),
    telephone       VARCHAR2(20)   CONSTRAINT uq_agent_tel UNIQUE,
    cin             VARCHAR2(8)    CONSTRAINT uq_agent_cin UNIQUE
        CONSTRAINT ck_agent_cin CHECK (REGEXP_LIKE(cin, '^[0-9]{8}$')),
    disponibilite   VARCHAR2(20)   DEFAULT 'Disponible' NOT NULL
        CONSTRAINT ck_agent_dispo CHECK (disponibilite IN ('Disponible', 'En intervention',
            'En formation', 'En repos', 'En congé')),
    certification   VARCHAR2(80),
    date_obtention  DATE,
    date_echeance   DATE,
    mot_de_passe    VARCHAR2(128),            -- chiffré (SHA-256) ; vide = pas d'accès
    CONSTRAINT ck_agent_certif_dates CHECK (date_echeance IS NULL OR date_obtention IS NULL
                                            OR date_echeance > date_obtention)
);

-- --------------------------- Module Gestion des incidents (Maram) -----------
CREATE TABLE INCIDENT (
    id_incident     VARCHAR2(15)   CONSTRAINT pk_incident PRIMARY KEY,
    type            VARCHAR2(40)   NOT NULL,
    date_heure      DATE           NOT NULL,
    adresse         VARCHAR2(150)  NOT NULL,
    niveau_gravite  VARCHAR2(20)   NOT NULL
        CONSTRAINT ck_incident_gravite CHECK (niveau_gravite IN ('Faible', 'Moyenne', 'Élevée', 'Critique')),
    nb_victimes     NUMBER(4)      DEFAULT 0 NOT NULL
        CONSTRAINT ck_incident_victimes CHECK (nb_victimes >= 0),
    description     VARCHAR2(500),
    statut          VARCHAR2(20)   DEFAULT 'Signalé' NOT NULL
        CONSTRAINT ck_incident_statut CHECK (statut IN ('Signalé', 'En cours', 'Terminé', 'Archivé')),
    code            VARCHAR2(20)   CONSTRAINT uq_incident_code UNIQUE
);

-- --------------------------- Module Gestion des véhicules (Youssef) ---------
CREATE TABLE VEHICULE (
    id_vehicule              NUMBER(10)     CONSTRAINT pk_vehicule PRIMARY KEY,
    matricule                VARCHAR2(20)   NOT NULL CONSTRAINT uq_vehicule_matricule UNIQUE,
    type                     VARCHAR2(40)   NOT NULL,
    marque                   VARCHAR2(50),
    modele                   VARCHAR2(50),
    etat                     VARCHAR2(20)   DEFAULT 'Disponible' NOT NULL
        CONSTRAINT ck_vehicule_etat CHECK (etat IN ('Disponible', 'En mission', 'En maintenance', 'Hors service')),
    disponibilite            VARCHAR2(20)   DEFAULT 'Disponible' NOT NULL
        CONSTRAINT ck_vehicule_dispo CHECK (disponibilite IN ('Disponible', 'Indisponible')),
    kilometrage              NUMBER(10)     DEFAULT 0 NOT NULL
        CONSTRAINT ck_vehicule_km CHECK (kilometrage >= 0),
    carburant                NUMBER(3)      DEFAULT 0 NOT NULL
        CONSTRAINT ck_vehicule_carburant CHECK (carburant BETWEEN 0 AND 100),
    seuil_carburant          NUMBER(3)      DEFAULT 25 NOT NULL
        CONSTRAINT ck_vehicule_seuil CHECK (seuil_carburant BETWEEN 0 AND 100),
    localisation             VARCHAR2(100),
    date_achat               DATE,
    date_derniere_entretien  DATE,
    date_prochaine_entretien DATE,
    remarques                VARCHAR2(1000),
    CONSTRAINT ck_vehicule_entretien CHECK (date_prochaine_entretien IS NULL OR date_derniere_entretien IS NULL
                                            OR date_prochaine_entretien >= date_derniere_entretien)
);

-- --------------------------- Module Gestion des formations (Linda, Mahmoud) -
CREATE TABLE FORMATION (
    id              NUMBER(10)     CONSTRAINT pk_formation PRIMARY KEY,
    titre           VARCHAR2(100)  NOT NULL CONSTRAINT uq_formation_titre UNIQUE,
    type            VARCHAR2(50)   NOT NULL,
    date_debut      DATE           NOT NULL,
    date_fin        DATE           NOT NULL,
    duree           NUMBER(4)
        CONSTRAINT ck_formation_duree CHECK (duree > 0),
    formateur       VARCHAR2(60),
    lieu            VARCHAR2(60),
    nb_places       NUMBER(4)      NOT NULL
        CONSTRAINT ck_formation_places CHECK (nb_places > 0),
    niveau          VARCHAR2(30)
        CONSTRAINT ck_formation_niveau CHECK (niveau IN ('Débutant', 'Intermédiaire', 'Avancé')),
    statut          VARCHAR2(30)   DEFAULT 'Planifiée' NOT NULL
        CONSTRAINT ck_formation_statut CHECK (statut IN ('Planifiée', 'En cours', 'Terminée', 'Annulée')),
    CONSTRAINT ck_formation_dates CHECK (date_fin >= date_debut)
);

-- --------------------------- Module Gestion des équipements (Amen) ----------
-- CONTENIR (EQUIPEMENT 0,1 — 1,n VEHICULE) : simple–complexe
--   → clé étrangère id_vehicule + attribut de l'association date_affectation
CREATE TABLE EQUIPEMENT (
    id_equipement    VARCHAR2(15)  CONSTRAINT pk_equipement PRIMARY KEY,
    nom              VARCHAR2(80)  NOT NULL,
    type             VARCHAR2(40)  NOT NULL,
    marque           VARCHAR2(50),
    modele           VARCHAR2(50)  CONSTRAINT uq_equipement_modele UNIQUE,
    quantite         NUMBER(6)     DEFAULT 0 NOT NULL
        CONSTRAINT ck_equipement_qte CHECK (quantite >= 0),
    etat             VARCHAR2(20)  DEFAULT 'Disponible' NOT NULL
        CONSTRAINT ck_equipement_etat CHECK (etat IN ('Disponible', 'En maintenance', 'Hors service')),
    date_achat       DATE,
    date_entretien   DATE,
    id_vehicule      NUMBER(10)
        CONSTRAINT fk_equipement_vehicule REFERENCES VEHICULE(id_vehicule) ON DELETE SET NULL,
    date_affectation DATE,
    CONSTRAINT ck_equipement_dates CHECK (date_entretien IS NULL OR date_achat IS NULL
                                          OR date_entretien >= date_achat),
    CONSTRAINT ck_equipement_affect CHECK (date_affectation IS NULL OR id_vehicule IS NOT NULL)
);


-- ============================================================================
-- 2. TABLES DE LIAISON (associations complexe – complexe)
--    Clé primaire composée des deux clés étrangères
-- ============================================================================

-- INTERVENIR (AGENT 0,n — 1,n INCIDENT)
CREATE TABLE INTERVENTION (
    id_agent      VARCHAR2(15)
        CONSTRAINT fk_intervention_agent REFERENCES AGENT(id_agent) ON DELETE CASCADE,
    id_incident   VARCHAR2(15)
        CONSTRAINT fk_intervention_incident REFERENCES INCIDENT(id_incident) ON DELETE CASCADE,
    CONSTRAINT pk_intervention PRIMARY KEY (id_agent, id_incident)
);

-- MOBILISER (INCIDENT 1,n — 0,n VEHICULE)
CREATE TABLE MOBILISATION (
    id_vehicule   NUMBER(10)
        CONSTRAINT fk_mobilisation_vehicule REFERENCES VEHICULE(id_vehicule) ON DELETE CASCADE,
    id_incident   VARCHAR2(15)
        CONSTRAINT fk_mobilisation_incident REFERENCES INCIDENT(id_incident) ON DELETE CASCADE,
    CONSTRAINT pk_mobilisation PRIMARY KEY (id_vehicule, id_incident)
);

-- INSCRIRE (AGENT 0,n — 1,n FORMATION) avec les attributs de l'association
CREATE TABLE INSCRIRE (
    id_agent            VARCHAR2(15)
        CONSTRAINT fk_inscrire_agent REFERENCES AGENT(id_agent) ON DELETE CASCADE,
    id                  NUMBER(10)
        CONSTRAINT fk_inscrire_formation REFERENCES FORMATION(id) ON DELETE CASCADE,
    date_inscription    DATE          DEFAULT SYSDATE NOT NULL,
    statut_inscription  VARCHAR2(20)  DEFAULT 'Inscrit' NOT NULL
        CONSTRAINT ck_inscrire_statut CHECK (statut_inscription IN ('Inscrit', 'Confirmé', 'Annulé')),
    presence            VARCHAR2(10)
        CONSTRAINT ck_inscrire_presence CHECK (presence IN ('Présent', 'Absent')),
    resultat            VARCHAR2(20)
        CONSTRAINT ck_inscrire_resultat CHECK (resultat IN ('En cours', 'Réussi', 'Échoué')),
    CONSTRAINT pk_inscrire PRIMARY KEY (id_agent, id)
);


-- ============================================================================
-- 3. INCRÉMENTATION AUTOMATIQUE DES IDENTIFIANTS NUMÉRIQUES
--    (Oracle 11g n'a pas de colonne IDENTITY : séquence + trigger)
--    AGENT, INCIDENT et EQUIPEMENT : identifiant texte (AGT-001, INC-…)
--    généré par l'application.
-- ============================================================================
CREATE SEQUENCE seq_vehicule_id START WITH 1 INCREMENT BY 1 NOCYCLE;

CREATE OR REPLACE TRIGGER trg_vehicule_id
BEFORE INSERT ON VEHICULE
FOR EACH ROW
WHEN (NEW.id_vehicule IS NULL)
BEGIN
    SELECT seq_vehicule_id.NEXTVAL INTO :NEW.id_vehicule FROM DUAL;
END;
/

CREATE SEQUENCE seq_formation_id START WITH 1 INCREMENT BY 1 NOCYCLE;

CREATE OR REPLACE TRIGGER trg_formation_id
BEFORE INSERT ON FORMATION
FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
    SELECT seq_formation_id.NEXTVAL INTO :NEW.id FROM DUAL;
END;
/


-- ============================================================================
-- 4. VÉRIFICATION : doit afficher les 8 tables
-- ============================================================================
SELECT table_name FROM user_tables ORDER BY table_name;
