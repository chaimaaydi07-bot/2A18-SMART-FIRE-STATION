-- ============================================================================
--  Smart Fire Station (USPC) — Base de données commune (Oracle)
--  Modèle physique : 9 tables (Personnel, Incidents, Véhicules, Équipements), clés primaires, clés étrangères et contraintes
-- ============================================================================

-- --------------------------- Module Gestion du personnel --------------------
CREATE TABLE AGENT (
    id_agent        VARCHAR2(15)  CONSTRAINT pk_agent PRIMARY KEY,
    nom             VARCHAR2(50)  NOT NULL,
    prenom          VARCHAR2(50)  NOT NULL,
    poste           VARCHAR2(40)  NOT NULL
        CONSTRAINT ck_agent_poste CHECK (poste IN ('Pompier', 'Chauffeur', 'Maître-chien',
            'Secouriste', 'Mécanicien', 'Administratif', 'Dispatcher',
            'Responsable des équipements', 'Responsable RH', 'Chef de caserne')),
    grade           VARCHAR2(20)
        CONSTRAINT ck_agent_grade CHECK (grade IN ('Agent', 'Caporal', 'Sergent',
            'Adjudant', 'Lieutenant', 'Capitaine')),
    specialite      VARCHAR2(50),
    telephone       VARCHAR2(20)  CONSTRAINT uq_agent_tel UNIQUE,
    disponibilite   VARCHAR2(20)  DEFAULT 'Disponible' NOT NULL
        CONSTRAINT ck_agent_dispo CHECK (disponibilite IN ('Disponible', 'En intervention',
            'En formation', 'En repos', 'En congé')),
    mot_de_passe    VARCHAR2(128)            -- chiffré ; vide = n'utilise pas l'application
);

CREATE TABLE CERTIFICATION (                -- catalogue des certifications (PSE1, NRBC…)
    id_certification NUMBER        CONSTRAINT pk_certification PRIMARY KEY,
    nom              VARCHAR2(80)  NOT NULL CONSTRAINT uq_certification_nom UNIQUE
);

-- Association OBTENIR (AGENT n — n CERTIFICATION) : dates propres à chaque obtention
CREATE TABLE OBTENTION (
    id_agent          VARCHAR2(15)  CONSTRAINT fk_obt_agent REFERENCES AGENT(id_agent) ON DELETE CASCADE,
    id_certification  NUMBER        CONSTRAINT fk_obt_certif REFERENCES CERTIFICATION(id_certification) ON DELETE CASCADE,
    date_obtention    DATE          NOT NULL,
    date_echeance     DATE          NOT NULL,
    CONSTRAINT pk_obtention PRIMARY KEY (id_agent, id_certification),
    CONSTRAINT ck_obt_dates CHECK (date_echeance > date_obtention)
);

CREATE TABLE SMS (
    id_sms          NUMBER         CONSTRAINT pk_sms PRIMARY KEY,
    date_envoi      DATE           DEFAULT SYSDATE NOT NULL,
    message         VARCHAR2(400)  NOT NULL,
    id_agent        VARCHAR2(15)   NOT NULL
        CONSTRAINT fk_sms_agent REFERENCES AGENT(id_agent) ON DELETE CASCADE
);

-- --------------------------- Module Gestion des incidents -------------------
CREATE TABLE INCIDENT (
    id_incident     VARCHAR2(15)   CONSTRAINT pk_incident PRIMARY KEY,
    type            VARCHAR2(40)   NOT NULL,
    date_heure      DATE           NOT NULL,
    adresse         VARCHAR2(150)  NOT NULL,
    niveau_gravite  VARCHAR2(20)   NOT NULL
        CONSTRAINT ck_incident_gravite CHECK (niveau_gravite IN ('Faible', 'Moyenne', 'Élevée', 'Critique')),
    nb_victimes     NUMBER(4)      DEFAULT 0 CONSTRAINT ck_incident_victimes CHECK (nb_victimes >= 0),
    description     VARCHAR2(500),
    statut          VARCHAR2(20)   DEFAULT 'Signalé'
        CONSTRAINT ck_incident_statut CHECK (statut IN ('Signalé', 'En cours', 'Terminé', 'Archivé')),
    id_agent        VARCHAR2(15)   -- dispatcher qui a enregistré l'incident (ENREGISTRER)
        CONSTRAINT fk_incident_agent REFERENCES AGENT(id_agent) ON DELETE SET NULL
);

-- Association INTERVENIR (AGENT n — n INCIDENT) : sert aussi à la charge de travail
CREATE TABLE INTERVENTION (
    id_agent      VARCHAR2(15)  CONSTRAINT fk_interv_agent REFERENCES AGENT(id_agent) ON DELETE CASCADE,
    id_incident   VARCHAR2(15)  CONSTRAINT fk_interv_incident REFERENCES INCIDENT(id_incident) ON DELETE CASCADE,
    role          VARCHAR2(40),
    CONSTRAINT pk_intervention PRIMARY KEY (id_agent, id_incident)
);

-- --------------------------- Module Gestion des véhicules -------------------
CREATE TABLE VEHICULE (
    id_vehicule             VARCHAR2(15)  CONSTRAINT pk_vehicule PRIMARY KEY,
    type                    VARCHAR2(40)  NOT NULL,
    immatriculation         VARCHAR2(20)  NOT NULL CONSTRAINT uq_vehicule_immat UNIQUE,
    etat                    VARCHAR2(20)  NOT NULL,
    kilometrage             NUMBER(8)     DEFAULT 0 CONSTRAINT ck_vehicule_km CHECK (kilometrage >= 0),
    date_dernier_entretien  DATE,
    disponibilite           VARCHAR2(20)  DEFAULT 'Disponible'
);

-- Association MOBILISER (VEHICULE n — n INCIDENT)
CREATE TABLE MOBILISATION (
    id_vehicule   VARCHAR2(15)  CONSTRAINT fk_mobil_vehicule REFERENCES VEHICULE(id_vehicule) ON DELETE CASCADE,
    id_incident   VARCHAR2(15)  CONSTRAINT fk_mobil_incident REFERENCES INCIDENT(id_incident) ON DELETE CASCADE,
    CONSTRAINT pk_mobilisation PRIMARY KEY (id_vehicule, id_incident)
);

-- --------------------------- Module Gestion des équipements -----------------
CREATE TABLE EQUIPEMENT (
    id_equipement   VARCHAR2(15)  CONSTRAINT pk_equipement PRIMARY KEY,
    nom             VARCHAR2(80)  NOT NULL,
    type            VARCHAR2(40)  NOT NULL,
    marque          VARCHAR2(50),
    modele          VARCHAR2(50),
    quantite        NUMBER(6)     DEFAULT 0 NOT NULL CONSTRAINT ck_equip_qte CHECK (quantite >= 0),
    etat            VARCHAR2(20)  NOT NULL,
    date_achat      DATE,
    date_entretien  DATE,
    id_vehicule     VARCHAR2(15)  -- véhicule dans lequel l'équipement est embarqué (EQUIPER)
        CONSTRAINT fk_equip_vehicule REFERENCES VEHICULE(id_vehicule) ON DELETE SET NULL
);

-- Numérotation automatique des certifications et des SMS
CREATE SEQUENCE seq_certification START WITH 1 INCREMENT BY 1;
CREATE SEQUENCE seq_sms           START WITH 1 INCREMENT BY 1;
