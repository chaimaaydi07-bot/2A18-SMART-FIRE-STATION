#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int bouton = 2;
int derniereLecture = HIGH;
int etatStable = HIGH;
unsigned long dernierChangement = 0;
bool enAttente = false;

void afficher(const char *ligne1, const char *ligne2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ligne1);
  lcd.setCursor(0, 1);
  lcd.print(ligne2);
}

void setup() {
  pinMode(bouton, INPUT_PULLUP);
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  afficher("Pret", "Appuyez bouton");
}

void loop() {
  int lecture = digitalRead(bouton);

  if (lecture != derniereLecture) {
    derniereLecture = lecture;
    dernierChangement = millis();
  }

  if (millis() - dernierChangement >= 30 && lecture != etatStable) {
    etatStable = lecture;
    if (etatStable == LOW && !enAttente) {
      enAttente = true;
      afficher("Alerte envoyee", "En attente");
      Serial.println("ALERTE_INCIDENT");
    }
  }

  if (Serial.available() > 0) {
    String message = Serial.readStringUntil('\n');
    message.trim();

    if (enAttente && message == "INCIDENT_OK") {
      afficher("Incident", "enregistre");
      enAttente = false;
    } else if (enAttente && message == "INCIDENT_ERREUR") {
      afficher("Echec", "Reessayez");
      enAttente = false;
    }
  }
}