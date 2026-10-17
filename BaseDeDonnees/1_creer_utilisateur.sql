-- ============================================================================
--  Smart Fire Station (USPC) — ÉTAPE 1 : création de l'utilisateur commun
--  À exécuter UNE SEULE FOIS, connecté avec l'utilisateur SYSTEM
--  (SQL Developer : connexion « system », nom de service XEPDB1)
-- ============================================================================

CREATE USER smartfire IDENTIFIED BY smartfire2026;

GRANT CONNECT, RESOURCE, CREATE VIEW, CREATE SEQUENCE TO smartfire;
ALTER USER smartfire QUOTA UNLIMITED ON USERS;
