-- Migration des identifiants AGENT vers NUMBER.
-- Sauvegarder la base et fermer l'application avant execution.
-- Ce script ne supprime aucune table.
-- Les instructions DDL ne peuvent pas etre annulees par ROLLBACK.
-- En cas d'erreur : arreter et examiner la sortie avant de relancer.

SET SERVEROUTPUT ON
WHENEVER SQLERROR EXIT SQL.SQLCODE

DECLARE
    v_type       VARCHAR2(30);
    v_nombre     NUMBER;
    v_depart     NUMBER;
BEGIN
    SELECT data_type INTO v_type
    FROM user_tab_columns
    WHERE table_name = 'AGENT'
      AND column_name = 'ID_AGENT';

    IF v_type = 'NUMBER' THEN
        DBMS_OUTPUT.PUT_LINE(
            'ID_AGENT est deja numerique : aucune modification.'
        );
    ELSIF v_type = 'VARCHAR2' THEN

        -- Verifier les conditions avant toute modification.
        SELECT COUNT(*) INTO v_nombre FROM AGENT;
        IF v_nombre <> 15 THEN
            RAISE_APPLICATION_ERROR(-20001,
                'Migration prevue pour les 15 agents de demonstration.');
        END IF;

        SELECT COUNT(*) INTO v_nombre
        FROM AGENT
        WHERE id_agent IN (
            'AGT-014', 'AGT-015', 'AGT-016', 'AGT-017', 'AGT-018',
            'AGT-019', 'AGT-020', 'AGT-021', 'AGT-022', 'AGT-023',
            'AGT-024', 'AGT-025', 'AGT-026', 'AGT-027', 'AGT-028'
        );

        IF v_nombre <> 15 THEN
            RAISE_APPLICATION_ERROR(-20002,
                'Identifiants inattendus : migration interrompue.');
        END IF;

        SELECT
            (SELECT COUNT(*) FROM INTERVENTION)
          + (SELECT COUNT(*) FROM INSCRIRE)
        INTO v_nombre FROM DUAL;

        IF v_nombre <> 0 THEN
            RAISE_APPLICATION_ERROR(-20003,
                'Tables de liaison non vides : migration adaptee requise.');
        END IF;

        SELECT COUNT(*) INTO v_nombre
        FROM user_tab_columns
        WHERE table_name = 'AGENT'
          AND column_name = 'ID_AGENT_NUM';

        IF v_nombre <> 0 THEN
            RAISE_APPLICATION_ERROR(-20004,
                'Migration partielle detectee : verifier la base.');
        END IF;

        SELECT COUNT(*) INTO v_nombre
        FROM user_sequences
        WHERE sequence_name = 'SEQ_AGENT_ID';

        IF v_nombre <> 0 THEN
            RAISE_APPLICATION_ERROR(-20005,
                'SEQ_AGENT_ID existe deja : verifier la base.');
        END IF;

        -- Completer les CIN sans remplacer ceux deja renseignes.
        UPDATE AGENT
        SET cin = '10000' || SUBSTR(id_agent, 5)
        WHERE cin IS NULL;

        COMMIT;

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT ADD (id_agent_num NUMBER(10))';

        EXECUTE IMMEDIATE
            'UPDATE AGENT SET id_agent_num =
             TO_NUMBER(SUBSTR(id_agent, 5))';

        COMMIT;

        EXECUTE IMMEDIATE
            'ALTER TABLE INTERVENTION
             DROP CONSTRAINT fk_intervention_agent';

        EXECUTE IMMEDIATE
            'ALTER TABLE INSCRIRE
             DROP CONSTRAINT fk_inscrire_agent';

        EXECUTE IMMEDIATE
            'ALTER TABLE INTERVENTION MODIFY (id_agent NUMBER(10))';

        EXECUTE IMMEDIATE
            'ALTER TABLE INSCRIRE MODIFY (id_agent NUMBER(10))';

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT DROP CONSTRAINT pk_agent';

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT DROP COLUMN id_agent';

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT
             RENAME COLUMN id_agent_num TO id_agent';

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT
             ADD CONSTRAINT pk_agent PRIMARY KEY (id_agent)';

        EXECUTE IMMEDIATE
            'ALTER TABLE AGENT MODIFY (cin NOT NULL)';

        EXECUTE IMMEDIATE
            'ALTER TABLE INTERVENTION
             ADD CONSTRAINT fk_intervention_agent
             FOREIGN KEY (id_agent) REFERENCES AGENT(id_agent)
             ON DELETE CASCADE';

        EXECUTE IMMEDIATE
            'ALTER TABLE INSCRIRE
             ADD CONSTRAINT fk_inscrire_agent
             FOREIGN KEY (id_agent) REFERENCES AGENT(id_agent)
             ON DELETE CASCADE';

        SELECT MAX(id_agent) + 1 INTO v_depart FROM AGENT;

        EXECUTE IMMEDIATE
            'CREATE SEQUENCE seq_agent_id START WITH '
            || TO_CHAR(v_depart, 'FM9999999990')
            || ' INCREMENT BY 1 NOCYCLE';

        EXECUTE IMMEDIATE
            'CREATE OR REPLACE TRIGGER trg_agent_id
             BEFORE INSERT ON AGENT
             FOR EACH ROW
             WHEN (NEW.id_agent IS NULL)
             BEGIN
                 SELECT seq_agent_id.NEXTVAL
                 INTO :NEW.id_agent FROM DUAL;
             END;';

        DBMS_OUTPUT.PUT_LINE('Migration terminee.');
    ELSE
        RAISE_APPLICATION_ERROR(-20006,
            'Type ID_AGENT inattendu : aucune migration.');
    END IF;
END;
/