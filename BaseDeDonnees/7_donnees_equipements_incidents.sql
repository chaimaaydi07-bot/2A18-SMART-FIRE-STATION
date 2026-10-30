-- ============================================================================
--  Smart Fire Station — Données de démonstration : EQUIPEMENT et INCIDENT
--  À exécuter UNE FOIS, connecté avec smartfire (SQL Developer : F5).
--  Les valeurs respectent les contraintes des tables et les listes des
--  formulaires Qt (types, états, gravités, statuts).
-- ============================================================================

-- ------------------------------------------------------------- EQUIPEMENT
-- type  : Protection, Secours, Communication, Médical, Outillage (liste du formulaire)
-- etat  : Disponible, En maintenance, Hors service (contrainte ck_equipement_etat)
-- modele: unique (contrainte uq_equipement_modele)
-- date_entretien >= date_achat (contrainte ck_equipement_dates)
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-001', 'Extincteur CO2', 'Protection', 'Rosenbauer', 'CO2-5KG', 12, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -24), ADD_MONTHS(TRUNC(SYSDATE), -3));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-002', 'Casque', 'Protection', 'MSA', 'Gallet F1 XF', 25, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -18), ADD_MONTHS(TRUNC(SYSDATE), -6));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-003', 'Appareil respiratoire', 'Secours', 'Dräger', 'PSS 7000', 8, 'En maintenance',
        ADD_MONTHS(TRUNC(SYSDATE), -36), ADD_MONTHS(TRUNC(SYSDATE), -1));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-004', 'Tenue de feu', 'Protection', 'Bullard', 'XT Series', 30, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -12), ADD_MONTHS(TRUNC(SYSDATE), -2));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-005', 'Radio portative', 'Communication', 'Motorola', 'DP4800e', 15, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -20), ADD_MONTHS(TRUNC(SYSDATE), -8));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-006', 'Défibrillateur', 'Médical', 'Philips', 'HeartStart FRx', 4, 'Hors service',
        ADD_MONTHS(TRUNC(SYSDATE), -48), ADD_MONTHS(TRUNC(SYSDATE), -14));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-007', 'Scie à béton', 'Outillage', 'Husqvarna', 'K 970', 2, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -30), ADD_MONTHS(TRUNC(SYSDATE), -5));
INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, quantite, etat, date_achat, date_entretien)
VALUES ('EQ-008', 'Lampe torche', 'Outillage', 'Petzl', 'Duo S', 20, 'Disponible',
        ADD_MONTHS(TRUNC(SYSDATE), -10), ADD_MONTHS(TRUNC(SYSDATE), -1));

-- --------------------------------------------------------------- INCIDENT
-- id_incident : 15 caractères max (format de l'application : INC- + 11 caractères)
-- type        : Incendie, Accident, Fuite de gaz, Sauvetage, Autre (liste du formulaire)
-- gravité     : Faible, Moyenne, Élevée, Critique
-- statut      : Signalé, En cours, Terminé
-- code        : unique, facultatif
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000001', 'Incendie', TRUNC(SYSDATE) - 12 + 14/24, 'Tunis, Avenue Habib Bourguiba',
        'Élevée', 2, 'Feu dans un appartement au 3e étage, fumée importante.', 'Terminé', 'INC-2026-001');
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000002', 'Accident', TRUNC(SYSDATE) - 8 + 9/24, 'Ariana, Route de Raoued',
        'Moyenne', 3, 'Collision entre deux véhicules, passagers bloqués.', 'Terminé', 'INC-2026-002');
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000003', 'Fuite de gaz', TRUNC(SYSDATE) - 5 + 18/24, 'La Marsa, Rue Tahar Sfar',
        'Critique', 0, 'Odeur de gaz signalée dans un immeuble, évacuation en cours.', 'Terminé', 'INC-2026-003');
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000004', 'Sauvetage', TRUNC(SYSDATE) - 2 + 11/24, 'Bizerte, Plage de la Corniche',
        'Moyenne', 1, 'Personne en difficulté dans l''eau.', 'En cours', 'INC-2026-004');
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000005', 'Incendie', TRUNC(SYSDATE) - 1 + 22/24, 'Ben Arous, Zone industrielle',
        'Critique', 0, 'Incendie dans un entrepôt de stockage.', 'En cours', 'INC-2026-005');
INSERT INTO INCIDENT (id_incident, type, date_heure, adresse, niveau_gravite, nb_victimes, description, statut, code)
VALUES ('INC-DEMO0000006', 'Autre', TRUNC(SYSDATE) + 8/24, 'Tunis, Lac 2',
        'Faible', 0, 'Arbre tombé sur la chaussée.', 'Signalé', 'INC-2026-006');

COMMIT;
