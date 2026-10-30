-- Donnees de demonstration de la table INCIDENT.
-- Executer avec SMARTFIRE apres 2_creer_tables.sql.

INSERT INTO INCIDENT
    (id_incident, type, date_heure, adresse, niveau_gravite,
     nb_victimes, description, statut, code)
SELECT
    'INC-DEMO-001', 'Incendie', SYSDATE - 2,
    'Tunis', 'Critique', 2,
    'Incendie dans un immeuble - demonstration',
    'En cours', NULL
FROM DUAL
WHERE NOT EXISTS (
    SELECT 1 FROM INCIDENT WHERE id_incident = 'INC-DEMO-001'
);

INSERT INTO INCIDENT
    (id_incident, type, date_heure, adresse, niveau_gravite,
     nb_victimes, description, statut, code)
SELECT
    'INC-DEMO-002', 'Accident', SYSDATE - 1,
    'Ariana', 'Moyenne', 1,
    'Accident de circulation - demonstration',
    'Signalé', NULL
FROM DUAL
WHERE NOT EXISTS (
    SELECT 1 FROM INCIDENT WHERE id_incident = 'INC-DEMO-002'
);

INSERT INTO INCIDENT
    (id_incident, type, date_heure, adresse, niveau_gravite,
     nb_victimes, description, statut, code)
SELECT
    'INC-DEMO-003', 'Fuite de gaz', SYSDATE,
    'Beja', 'Faible', 0,
    'Fuite de gaz dans une habitation - demonstration',
    'Terminé', NULL
FROM DUAL
WHERE NOT EXISTS (
    SELECT 1 FROM INCIDENT WHERE id_incident = 'INC-DEMO-003'
);

COMMIT;

SELECT id_incident, type, adresse, statut
FROM INCIDENT
WHERE id_incident LIKE 'INC-DEMO-%';