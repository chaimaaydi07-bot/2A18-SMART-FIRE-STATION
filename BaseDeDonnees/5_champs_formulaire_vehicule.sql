-- A executer une fois avec SMARTFIRE, apres la migration 4.
-- Conserve les donnees existantes et ajoute les champs du formulaire Qt.
WHENEVER SQLERROR EXIT SQL.SQLCODE ROLLBACK
ALTER TABLE VEHICULE ADD (
    marque VARCHAR2(50),
    carburant NUMBER(3,0) DEFAULT 0 NOT NULL
        CONSTRAINT ck_vehicule_carburant CHECK (carburant BETWEEN 0 AND 100),
    seuil_carburant NUMBER(3,0) DEFAULT 25 NOT NULL
        CONSTRAINT ck_vehicule_seuil CHECK (seuil_carburant BETWEEN 0 AND 100),
    localisation VARCHAR2(100),
    date_achat DATE,
    date_prochaine_entretien DATE,
    remarques VARCHAR2(1000)
);
