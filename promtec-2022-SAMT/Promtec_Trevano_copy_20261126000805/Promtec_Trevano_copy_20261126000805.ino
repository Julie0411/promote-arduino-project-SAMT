/*
 * Promtec Trevano - ricostruzione dal firmware di Julie
 *
 * Questo e' nuovo codice leggibile, NON il sorgente originale del 2022.
 * Derivato da arduino_firmware.hex, recuperato il 25 novembre 2026.
 * SHA-256: 5bca9aba4318fffd752e7854cfb1bbda6f6713f6194398606e36dc0e26c51494
 *
 * Hardware rilevato dal programma: LCD parallelo, RS=8, E=9,
 * D4=4, D5=5, D6=6, D7=7; tastiera resistiva sull'ingresso A0.
 * Display ricostruito come 16 colonne x 2 righe, coerente con il layout.
 * Dipendenza: LiquidCrystal, di Arduino/Adafruit, da Gestione librerie.
 *
 * Le schermate, le frecce, le soglie della tastiera, il codice di tre
 * valori 1..4 e i tre tentativi provengono dal firmware.
 * La gestione del rilascio dei tasti e il blocco finale sono riscritti:
 * per una nuova partita si usa il pulsante fisico RST.
 */

#include <Arduino.h>
#include <LiquidCrystal.h>

const char NOME[] = "Julie";
const uint8_t LUNGHEZZA_CODICE = 3;
const uint8_t MAX_TENTATIVI = 3;
const unsigned long ANTIRIMBALZO_MS = 30;

// Nel firmware una posizione corretta rimane confermata anche dopo un errore.
// Metti false per richiedere tutte e tre le risposte corrette nello stesso turno.
const bool CONSERVA_CONFERME_ORIGINALI = true;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Il numero coincide con lo slot del carattere LCD e con il valore del codice.
enum class Tasto : uint8_t {
  Nessuno = 0,
  Su = 1,
  Destra = 2,
  Giu = 3,
  Sinistra = 4,
  Select = 5
};

enum class Stato : uint8_t { AttesaEvento, Inserimento, Terminato };

// Prototipi espliciti: mantengono i tipi enum visibili al preprocessore Arduino.
Tasto leggiTasto();
void azzeraLetturaTasti();
Tasto nuovaPressione();
void mostraRiga(uint8_t colonna, uint8_t riga, const char *testo);
void mostraIntroduzione();
void generaCodice();
void mostraTentativo();
void avviaEvento();
void mostraVittoria();
void mostraErrore();
void mostraSconfitta();
void registraRisposta(Tasto tasto);
void setup();
void loop();

Stato stato = Stato::AttesaEvento;
uint8_t codice[LUNGHEZZA_CODICE] = {1, 1, 1};
bool conferme[LUNGHEZZA_CODICE] = {false, false, false};
uint8_t posizione = 0;
uint8_t tentativo = 1;

Tasto ultimoLetto = Tasto::Nessuno;
Tasto stabile = Tasto::Nessuno;
unsigned long cambiatoA = 0;
bool prontoPerPressione = false;

// Bitmap estratte esattamente dai dati inizializzati del firmware.
uint8_t frecciaSu[8]       = {0, 4, 10, 17, 4, 4, 4, 0};
uint8_t frecciaDestra[8]   = {0, 4, 2, 31, 2, 4, 0, 0};
uint8_t frecciaGiu[8]      = {0, 4, 4, 4, 21, 14, 4, 0};
uint8_t frecciaSinistra[8] = {0, 4, 8, 31, 8, 4, 0, 0};

Tasto leggiTasto() {
  const int valore = analogRead(A0);
  if (valore < 60)  return Tasto::Destra;
  if (valore < 200) return Tasto::Su;
  if (valore < 350) return Tasto::Giu;
  if (valore < 600) return Tasto::Sinistra;
  // Il firmware avvia l'evento solo nell'intervallo 651..799.
  if (valore >= 651 && valore < 800) return Tasto::Select;
  return Tasto::Nessuno;
}

void azzeraLetturaTasti() {
  ultimoLetto = leggiTasto();
  stabile = ultimoLetto;
  cambiatoA = millis();
  prontoPerPressione = false;
}

// Un solo evento per pressione: per ripetere una freccia occorre rilasciarla.
Tasto nuovaPressione() {
  const Tasto letto = leggiTasto();
  const unsigned long adesso = millis();
  if (letto != ultimoLetto) {
    ultimoLetto = letto;
    cambiatoA = adesso;
  }
  if (adesso - cambiatoA < ANTIRIMBALZO_MS) return Tasto::Nessuno;
  stabile = letto;
  if (stabile == Tasto::Nessuno) {
    prontoPerPressione = true;
    return Tasto::Nessuno;
  }
  if (!prontoPerPressione) return Tasto::Nessuno;
  prontoPerPressione = false;
  return stabile;
}

void mostraRiga(uint8_t colonna, uint8_t riga, const char *testo) {
  lcd.setCursor(colonna, riga);
  lcd.print(testo);
}

void mostraIntroduzione() {
  lcd.clear();
  mostraRiga(2, 0, "Promtec 2022");
  delay(1000);
  mostraRiga(0, 1, "EMM & InfEdifici");
  delay(2000);

  lcd.clear();
  mostraRiga(0, 0, "Ciao ");
  mostraRiga(5, 0, NOME);
  delay(1000);
  mostraRiga(0, 1, " incominciamo...");
  delay(2000);

  lcd.clear();
  mostraRiga(0, 0, "Codici sicurezza");
  for (uint8_t valore = 1; valore <= 4; ++valore) {
    lcd.setCursor((valore - 1) * 4, 1);
    lcd.print(valore);
    lcd.print(':');
    lcd.write(valore);
  }
  delay(5000);
  lcd.clear();
  mostraRiga(0, 0, "Premi select per");
  mostraRiga(0, 1, "generare evento");
}

void generaCodice() {
  // Come nel firmware, nessuna inizializzazione esplicita del seme.
  // La sequenza viene fatta avanzare mentre si aspetta Select.
  for (uint8_t i = 0; i < LUNGHEZZA_CODICE; ++i) {
    codice[i] = static_cast<uint8_t>(random(1, 5));
  }
}

void mostraTentativo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(tentativo);
  lcd.print("o tentativo: ");
}

void avviaEvento() {
  for (uint8_t i = 0; i < LUNGHEZZA_CODICE; ++i) conferme[i] = false;
  tentativo = 1;
  posizione = 0;

  for (uint8_t i = 0; i < 3; ++i) {
    lcd.clear();
    delay(500);
    mostraRiga(0, 0, "creazione evento");
    mostraRiga(0, 1, "attendere...");
    delay(500);
  }

  lcd.clear();
  mostraRiga(0, 0, "ALLARME INTRUSO");
  mostraRiga(0, 1, "Codice:");
  for (uint8_t i = 0; i < LUNGHEZZA_CODICE; ++i) {
    lcd.setCursor(8 + i, 1);
    lcd.print(codice[i]);
  }
  delay(1000);
  for (int8_t conto = 5; conto >= 0; --conto) {
    lcd.setCursor(15, 1);
    lcd.print(conto);
    delay(500);
  }

  lcd.clear();
  mostraRiga(0, 0, "Inserire codice:");
  stato = Stato::Inserimento;
  azzeraLetturaTasti();
}

void mostraVittoria() {
  for (uint8_t i = 0; i < 3; ++i) {
    lcd.clear();
    delay(300);
    mostraRiga(2, 0, "ALLARME");
    mostraRiga(5, 1, "ANNULLATO");
    delay(300);
  }
  delay(700);
  lcd.clear();
  mostraRiga(0, 0, "!!COMPLIMENTI!!");
  mostraRiga(1, 1, NOME);
  delay(2000);
  lcd.clear();
  mostraRiga(0, 0, "per ricominciare");
  delay(500);
  mostraRiga(1, 1, "premi tasto RST");
  stato = Stato::Terminato;
}

void mostraErrore() {
  for (uint8_t i = 0; i < 3; ++i) {
    lcd.clear();
    delay(300);
    mostraRiga(0, 0, "CODICE ERRATO");
    delay(300);
  }
  mostraRiga(6, 1, "...riprova");
}

void mostraSconfitta() {
  lcd.clear();
  delay(300);
  mostraRiga(0, 0, "SEI UNA SCHIAPPA");
  mostraRiga(0, 1, "premi tasto RST");
  stato = Stato::Terminato;
}

void registraRisposta(Tasto tasto) {
  const uint8_t valore = static_cast<uint8_t>(tasto);
  if (valore < 1 || valore > 4 || posizione >= LUNGHEZZA_CODICE) return;
  if (posizione == 0) mostraTentativo();

  const bool corretta = valore == codice[posizione];
  if (CONSERVA_CONFERME_ORIGINALI) {
    conferme[posizione] = conferme[posizione] || corretta;
  } else {
    conferme[posizione] = corretta;
  }
  lcd.setCursor(6 + posizione, 1);
  lcd.write(valore);
  Serial.print(valore);
  ++posizione;
  delay(300);

  if (posizione < LUNGHEZZA_CODICE) return;
  Serial.println();
  bool tutteCorrette = true;
  for (uint8_t i = 0; i < LUNGHEZZA_CODICE; ++i) {
    tutteCorrette = tutteCorrette && conferme[i];
  }
  if (tutteCorrette) {
    mostraVittoria();
    return;
  }

  mostraErrore();
  ++tentativo;
  posizione = 0;
  if (tentativo > MAX_TENTATIVI) mostraSconfitta();
  azzeraLetturaTasti();
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.createChar(1, frecciaSu);
  lcd.createChar(2, frecciaDestra);
  lcd.createChar(3, frecciaGiu);
  lcd.createChar(4, frecciaSinistra);
  mostraIntroduzione();
  generaCodice();
  stato = Stato::AttesaEvento;
  azzeraLetturaTasti();
}

void loop() {
  if (stato == Stato::Terminato) return;
  const Tasto tasto = nuovaPressione();
  if (stato == Stato::AttesaEvento) {
    if (tasto == Tasto::Select) avviaEvento();
    else generaCodice();
    return;
  }
  if (tasto != Tasto::Nessuno) registraRisposta(tasto);
}
