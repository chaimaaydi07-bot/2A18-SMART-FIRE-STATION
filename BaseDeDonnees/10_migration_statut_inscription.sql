-- Exécuter avec le compte Oracle de l'application après la création des tables.
-- Autorise les inscriptions en attente dans une base existante.
WHENEVER SQLERROR EXIT SQL.SQLCODE

DECLARE
    definition VARCHAR2(4000);
    remplacement NUMBER;
BEGIN
    SELECT MAX(search_condition_vc) INTO definition
    FROM user_constraints
    WHERE table_name = 'INSCRIRE' AND constraint_name = 'CK_INSCRIRE_STATUT';

    IF definition IS NULL OR INSTR(definition, 'En attente') = 0 THEN
        SELECT COUNT(*) INTO remplacement
        FROM user_constraints
        WHERE table_name = 'INSCRIRE' AND constraint_name = 'CK_INSCRIRE_STATUT_FORMATION';

        IF remplacement = 0 THEN
            EXECUTE IMMEDIATE q'~ALTER TABLE INSCRIRE ADD CONSTRAINT ck_inscrire_statut_formation
                CHECK (statut_inscription IN ('Inscrit', 'Confirmé', 'Annulé', 'En attente'))~';
        END IF;

        IF definition IS NOT NULL THEN
            EXECUTE IMMEDIATE 'ALTER TABLE INSCRIRE DROP CONSTRAINT ck_inscrire_statut';
        END IF;

        EXECUTE IMMEDIATE 'ALTER TABLE INSCRIRE RENAME CONSTRAINT ck_inscrire_statut_formation TO ck_inscrire_statut';
    END IF;
END;
/
