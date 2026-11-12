-- ============================================================================
--  Smart Fire Station — Données de démonstration : INTERVENTION et MOBILISATION
--  À exécuter avec smartfire (F5), APRÈS les scripts 3, 6 et les deux scripts 7.
--  L'agent est retrouvé par son CIN (id_agent est auto-incrémenté),
--  le véhicule par son matricule (id_vehicule est auto-incrémenté).
--  Les lignes déjà présentes ne sont pas ajoutées deux fois.
-- ============================================================================

-- INTERVENIR : quels agents sont intervenus sur quels incidents
INSERT INTO INTERVENTION (id_agent, id_incident)
SELECT a.id_agent, x.id_incident
FROM (
    SELECT '10000014' cin, 'INC-DEMO-001'    id_incident FROM DUAL UNION ALL
    SELECT '10000020',     'INC-DEMO-001'                FROM DUAL UNION ALL
    SELECT '10000015',     'INC-DEMO-001'                FROM DUAL UNION ALL
    SELECT '10000017',     'INC-DEMO-002'                FROM DUAL UNION ALL
    SELECT '10000021',     'INC-DEMO-002'                FROM DUAL UNION ALL
    SELECT '10000025',     'INC-DEMO-003'                FROM DUAL UNION ALL
    SELECT '10000014',     'INC-DEMO-003'                FROM DUAL UNION ALL
    SELECT '10000028',     'INC-DEMO0000001'             FROM DUAL UNION ALL
    SELECT '10000014',     'INC-DEMO0000001'             FROM DUAL UNION ALL
    SELECT '10000026',     'INC-DEMO0000002'             FROM DUAL UNION ALL
    SELECT '10000027',     'INC-DEMO0000002'             FROM DUAL UNION ALL
    SELECT '10000019',     'INC-DEMO0000003'             FROM DUAL UNION ALL
    SELECT '10000020',     'INC-DEMO0000004'             FROM DUAL UNION ALL
    SELECT '10000025',     'INC-DEMO0000004'             FROM DUAL UNION ALL
    SELECT '10000015',     'INC-DEMO0000005'             FROM DUAL UNION ALL
    SELECT '10000017',     'INC-DEMO0000006'             FROM DUAL
) x
JOIN AGENT a    ON a.cin = x.cin
JOIN INCIDENT i ON i.id_incident = x.id_incident
WHERE NOT EXISTS (SELECT 1 FROM INTERVENTION t
                  WHERE t.id_agent = a.id_agent AND t.id_incident = x.id_incident);

-- MOBILISER : quels véhicules ont été mobilisés sur quels incidents
INSERT INTO MOBILISATION (id_vehicule, id_incident)
SELECT v.id_vehicule, x.id_incident
FROM (
    SELECT '104TU1004' matricule, 'INC-DEMO-001' id_incident FROM DUAL UNION ALL
    SELECT '101TU1001',           'INC-DEMO-002'             FROM DUAL UNION ALL
    SELECT '108TU1008',           'INC-DEMO-003'             FROM DUAL UNION ALL
    SELECT '104TU1004',           'INC-DEMO0000001'          FROM DUAL UNION ALL
    SELECT '102TU1002',           'INC-DEMO0000002'          FROM DUAL UNION ALL
    SELECT '105TU1005',           'INC-DEMO0000004'          FROM DUAL
) x
JOIN VEHICULE v ON v.matricule = x.matricule
JOIN INCIDENT i ON i.id_incident = x.id_incident
WHERE NOT EXISTS (SELECT 1 FROM MOBILISATION m
                  WHERE m.id_vehicule = v.id_vehicule AND m.id_incident = x.id_incident);

COMMIT;

-- Vérification
SELECT COUNT(*) AS nb_interventions FROM INTERVENTION;
SELECT COUNT(*) AS nb_mobilisations FROM MOBILISATION;
