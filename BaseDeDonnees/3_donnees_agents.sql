-- ============================================================================
--  Smart Fire Station — ÉTAPE 3 : données de départ de la table AGENT
--  À exécuter UNE FOIS, connecté avec smartfire, après 2_creer_tables.sql
--  id_agent n'est pas saisi : il est donné par le trigger (séquence seq_agent_id).
--  Connexion à l'application avec le CIN :
--    10000018 / rh2026 (Responsable RH)     10000015 / chef2026 (Chef de caserne)
--    10000023 / disp2026 (Dispatcher)       10000016 / meca2026 (Mécanicien)
--    10000024 / equip2026 (Responsable des équipements)
--  (mots de passe chiffrés en SHA-256)
--  Dates des certifications calculées à partir d'aujourd'hui (SYSDATE)
-- ============================================================================

INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000014', 'Ben Ali', 'Karim', 'Pompier', 'Sergent', 'Sauvetage-déblaiement', '+216 20 123 456', 'Disponible', 'Sauvetage-déblaiement (SD1)', TRUNC(SYSDATE) - 300, TRUNC(SYSDATE) + 65, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000015', 'Trabelsi', 'Sami', 'Chef de caserne', 'Capitaine', 'Sauvetage-déblaiement', '+216 22 555 010', 'En intervention', 'PSE1 — Premiers secours', TRUNC(SYSDATE) - 200, TRUNC(SYSDATE) + 160, '893782fae95eb925f113b8df39d9ac16ba3723d49d779dee5353d8099353f405');
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000016', 'Gharbi', 'Amine', 'Mécanicien', 'Caporal', NULL, '+216 98 765 432', 'Disponible', NULL, NULL, NULL, '3572fa556c6d8865615b0660b004e87bd7961419c6790b7a85e58a25da10bd23');
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000017', 'Jlassi', 'Nour', 'Secouriste', 'Sergent', 'Secourisme', '+216 50 321 987', 'En repos', 'PSE1 — Premiers secours', TRUNC(SYSDATE) - 353, TRUNC(SYSDATE) + 12, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000018', 'Mansouri', 'Leila', 'Responsable RH', 'Lieutenant', NULL, '+216 71 000 111', 'Disponible', NULL, NULL, NULL, 'edbfb489d6dbeb1ad30049f9e6f32a3eeb93891021cbba23f5b176bc00ab02ff');
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000019', 'Sassi', 'Mohamed', 'Maître-chien', 'Caporal', 'Recherche cynophile', '+216 55 111 222', 'Disponible', 'Recherche cynophile', TRUNC(SYSDATE) - 320, TRUNC(SYSDATE) + 45, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000020', 'Amri', 'Hedi', 'Pompier', 'Lieutenant', 'Risques chimiques (NRBC)', '+216 52 333 444', 'En formation', 'Risques chimiques (NRBC)', TRUNC(SYSDATE) - 370, TRUNC(SYSDATE) - 5, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000021', 'Ben Salah', 'Walid', 'Chauffeur', 'Agent', 'Lutte contre l''incendie', '+216 53 555 666', 'En congé', NULL, NULL, NULL, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000022', 'Hammami', 'Rim', 'Administratif', NULL, NULL, '+216 71 000 222', 'Disponible', NULL, NULL, NULL, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000023', 'Mejri', 'Khaled', 'Dispatcher', 'Sergent', NULL, '+216 58 444 777', 'Disponible', NULL, NULL, NULL, '6f11f66627023b402230fab1772ac4bd04994e0550765c5097718a86cdfdf192');
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000024', 'Ferchichi', 'Sonia', 'Responsable des équipements', 'Adjudant', NULL, '+216 97 222 888', 'Disponible', NULL, NULL, NULL, '909b2a8fe8db029707d6e78d72ed6e511f310553766e8a6bfa5adbea76ec06c9');
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000025', 'Dridi', 'Youssef', 'Pompier', 'Adjudant', 'Lutte contre l''incendie', '+216 21 600 111', 'Disponible', 'Lutte contre l''incendie (FDF)', TRUNC(SYSDATE) - 120, TRUNC(SYSDATE) + 245, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000026', 'Khelifi', 'Anis', 'Chauffeur', 'Caporal', 'Sauvetage-déblaiement', '+216 24 700 222', 'Disponible', 'Conduite d''engins d''intervention', TRUNC(SYSDATE) - 200, TRUNC(SYSDATE) + 165, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000027', 'Hamdi', 'Ines', 'Secouriste', 'Agent', 'Secourisme', '+216 26 800 333', 'Disponible', 'PSE1 — Premiers secours', TRUNC(SYSDATE) - 30, TRUNC(SYSDATE) + 335, NULL);
INSERT INTO AGENT (cin, nom, prenom, poste, grade, specialite, telephone, disponibilite, certification, date_obtention, date_echeance, mot_de_passe)
VALUES ('10000028', 'Saidi', 'Bilel', 'Pompier', 'Caporal', 'Sauvetage-déblaiement', '+216 29 900 444', 'Disponible', 'PSE1 — Premiers secours', TRUNC(SYSDATE) - 45, TRUNC(SYSDATE) + 320, NULL);

COMMIT;
