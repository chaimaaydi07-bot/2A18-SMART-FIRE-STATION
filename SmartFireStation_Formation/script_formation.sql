-- Script Oracle : a executer dans SQL Developer avec le meme utilisateur que celui de connection.cpp.

CREATE TABLE FORMATION (
    ID          NUMBER PRIMARY KEY,
    TITRE       VARCHAR2(100) NOT NULL,
    TYPE        VARCHAR2(50),
    DATE_DEBUT  DATE,
    DATE_FIN    DATE,
    DUREE       NUMBER,
    FORMATEUR   VARCHAR2(60),
    LIEU        VARCHAR2(60),
    NB_PLACES   NUMBER,
    NIVEAU      VARCHAR2(30),
    STATUT      VARCHAR2(30),
    CONSTRAINT ck_dates CHECK (DATE_FIN >= DATE_DEBUT),
    CONSTRAINT ck_places CHECK (NB_PLACES > 0)
);

CREATE TABLE INSCRIPTION (
    ID_FORMATION  NUMBER REFERENCES FORMATION(ID) ON DELETE CASCADE,
    ID_POMPIER    NUMBER,
    DATE_PRESENCE DATE,
    PRIMARY KEY (ID_FORMATION, ID_POMPIER)
);

-- Jeu de donnees de test
INSERT INTO FORMATION VALUES (1, 'Premiers secours PSC1', 'Secourisme', DATE '2026-11-03', DATE '2026-11-04', 14, 'Karim Ben Ali', 'Salle A', 20, 'Débutant', 'Planifiée');
INSERT INTO FORMATION VALUES (2, 'Manipulation des extincteurs', 'Extincteurs', DATE '2026-11-10', DATE '2026-11-10', 6, 'Sami Trabelsi', 'Cour', 15, 'Débutant', 'Planifiée');
INSERT INTO FORMATION VALUES (3, 'Lutte contre incendie urbain', 'Incendie', DATE '2026-11-17', DATE '2026-11-21', 30, 'Nour Haddad', 'Terrain', 12, 'Avancé', 'Planifiée');

COMMIT;
