-- Donnees de demonstration : 12 vehicules, sans modifier les vehicules existants.
-- Executer avec SMARTFIRE apres le script 5. Peut etre relance sans doublons.
WHENEVER SQLERROR EXIT SQL.SQLCODE ROLLBACK
SET SERVEROUTPUT ON
BEGIN
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '101TU1001', 'Renault', 'Master', 'Ambulance', 'Disponible', 'Disponible', 85, 25, 32000, 'Remise principale', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-210), TRUNC(SYSDATE) + (45), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '101TU1001');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '102TU1002', 'Ford', 'Transit', 'Secours médical', 'Disponible', 'Disponible', 75, 25, 45000, 'Remise principale', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-150), TRUNC(SYSDATE) + (90), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '102TU1002');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '103TU1003', 'Fiat', 'Ducato', 'Ambulance', 'En mission', 'Indisponible', 60, 25, 58000, 'Centre-ville', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-100), TRUNC(SYSDATE) + (80), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '103TU1003');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '104TU1004', 'Iveco', 'Daily', 'Incendie', 'Disponible', 'Disponible', 90, 25, 27000, 'Remise principale', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-180), TRUNC(SYSDATE) + (60), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '104TU1004');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '105TU1005', 'Mercedes', 'Sprinter', 'Secours médical', 'En maintenance', 'Indisponible', 40, 25, 72000, 'Atelier mécanique', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-190), TRUNC(SYSDATE) + (10), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '105TU1005');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '106TU1006', 'Peugeot', 'Boxer', 'Intervention technique', 'Disponible', 'Disponible', 80, 25, 36000, 'Remise principale', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-120), TRUNC(SYSDATE) + (120), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '106TU1006');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '107TU1007', 'Citroen', 'Jumper', 'Intervention technique', 'Hors service', 'Indisponible', 20, 25, 115000, 'Atelier mécanique', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-240), TRUNC(SYSDATE) + (-5), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '107TU1007');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '108TU1008', 'Toyota', 'Hilux', 'Incendie', 'En mission', 'Indisponible', 65, 25, 49000, 'Zone industrielle', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-160), TRUNC(SYSDATE) + (70), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '108TU1008');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '109TU1009', 'Nissan', 'Navara', 'Incendie', 'Disponible', 'Disponible', 18, 25, 63000, 'Remise principale', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-175), TRUNC(SYSDATE) + (15), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '109TU1009');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '110TU1010', 'Mitsubishi', 'L200', 'Intervention technique', 'En maintenance', 'Indisponible', 55, 25, 81000, 'Atelier mécanique', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-200), TRUNC(SYSDATE) + (-3), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '110TU1010');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '111TU1011', 'Dacia', 'Duster', 'Commandement', 'Disponible', 'Disponible', 95, 25, 19000, 'Aire de stationnement', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-90), TRUNC(SYSDATE) + (150), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '111TU1011');
    INSERT INTO VEHICULE (matricule, marque, modele, type, etat, disponibilite,
        carburant, seuil_carburant, kilometrage, localisation, date_achat,
        date_derniere_entretien, date_prochaine_entretien, remarques)
    SELECT '112TU1012', 'Volkswagen', 'Caddy', 'Commandement', 'Disponible', 'Disponible', 70, 25, 24000, 'Aire de stationnement', ADD_MONTHS(TRUNC(SYSDATE), -36), TRUNC(SYSDATE) + (-130), TRUNC(SYSDATE) + (110), 'Véhicule de démonstration' FROM DUAL
    WHERE NOT EXISTS (SELECT 1 FROM VEHICULE WHERE matricule = '112TU1012');
    DBMS_OUTPUT.PUT_LINE('Vehicules de demonstration ajoutes (matricules existants conserves).');
END;
/
COMMIT;
