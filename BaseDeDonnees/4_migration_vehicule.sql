-- Migration de l'ancien schema VEHICULE vers le schema numerique Oracle 11g.
-- Executer UNE FOIS, connecte avec SMARTFIRE, sur le schema original vide.
-- Pour une nouvelle installation, utiliser directement 2_creer_tables.sql.
-- Oracle valide chaque instruction DDL : sauvegarder le schema avant execution.
WHENEVER OSERROR EXIT FAILURE
WHENEVER SQLERROR EXIT SQL.SQLCODE ROLLBACK
SET SERVEROUTPUT ON

DECLARE
    n NUMBER;
BEGIN
    SELECT COUNT(*) INTO n FROM VEHICULE;
    IF n <> 0 THEN
        RAISE_APPLICATION_ERROR(-20001, 'VEHICULE contient des donnees : migration arretee.');
    END IF;
    SELECT COUNT(*) INTO n FROM MOBILISATION;
    IF n <> 0 THEN
        RAISE_APPLICATION_ERROR(-20002, 'MOBILISATION contient des donnees : migration arretee.');
    END IF;
    SELECT COUNT(*) INTO n FROM EQUIPEMENT WHERE id_vehicule IS NOT NULL;
    IF n <> 0 THEN
        RAISE_APPLICATION_ERROR(-20003, 'EQUIPEMENT reference des vehicules : migration arretee.');
    END IF;
    SELECT COUNT(*) INTO n FROM user_tab_columns
    WHERE table_name = 'VEHICULE'
      AND column_name IN ('ID_VEHICULE', 'IMMATRICULATION', 'DATE_DERNIER_ENTRETIEN');
    IF n <> 3 THEN
        RAISE_APPLICATION_ERROR(-20004, 'Schema deja migre ou different du schema original.');
    END IF;
    SELECT COUNT(*) INTO n FROM user_constraints
    WHERE constraint_type = 'R' AND r_constraint_name = 'PK_VEHICULE';
    IF n <> 2 THEN
        RAISE_APPLICATION_ERROR(-20005, 'Relations VEHICULE inattendues : verifier avant migration.');
    END IF;
    SELECT COUNT(*) INTO n FROM user_objects
    WHERE object_name IN ('SEQ_VEHICULE_ID', 'TRG_VEHICULE_ID');
    IF n <> 0 THEN
        RAISE_APPLICATION_ERROR(-20006, 'Sequence ou trigger VEHICULE existe deja.');
    END IF;
END;
/

ALTER TABLE MOBILISATION DROP CONSTRAINT fk_mobil_vehicule;
ALTER TABLE EQUIPEMENT DROP CONSTRAINT fk_equip_vehicule;

ALTER TABLE VEHICULE MODIFY (id_vehicule NUMBER(10,0));
ALTER TABLE VEHICULE RENAME COLUMN id_vehicule TO id;
ALTER TABLE VEHICULE RENAME COLUMN immatriculation TO matricule;
ALTER TABLE VEHICULE RENAME COLUMN date_dernier_entretien TO date_derniere_entretien;
ALTER TABLE VEHICULE ADD (modele VARCHAR2(50));
ALTER TABLE VEHICULE MODIFY (
    etat DEFAULT 'Disponible',
    disponibilite DEFAULT 'Disponible' NOT NULL,
    kilometrage NUMBER(10,0) DEFAULT 0 NOT NULL
);

ALTER TABLE MOBILISATION MODIFY (id_vehicule NUMBER(10,0));
ALTER TABLE EQUIPEMENT MODIFY (id_vehicule NUMBER(10,0));
ALTER TABLE MOBILISATION ADD CONSTRAINT fk_mobil_vehicule
    FOREIGN KEY (id_vehicule) REFERENCES VEHICULE(id) ON DELETE CASCADE;
ALTER TABLE EQUIPEMENT ADD CONSTRAINT fk_equip_vehicule
    FOREIGN KEY (id_vehicule) REFERENCES VEHICULE(id) ON DELETE SET NULL;

CREATE SEQUENCE seq_vehicule_id START WITH 1 INCREMENT BY 1 NOCYCLE;
CREATE OR REPLACE TRIGGER trg_vehicule_id
BEFORE INSERT ON VEHICULE
FOR EACH ROW
WHEN (NEW.id IS NULL)
BEGIN
    :NEW.id := seq_vehicule_id.NEXTVAL;
END;
/

PROMPT Migration VEHICULE terminee.
