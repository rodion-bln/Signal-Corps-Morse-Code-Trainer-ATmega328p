//  Signal Corps - Mod Trainer + Mod Decoder

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>

#define PIN_CHEIE   2
#define PIN_BUZZER  3
#define PIN_BTN_MOD 4
#define PIN_LED_G   5
#define PIN_LED_R   6
#define PIN_MIC     A0

#define LCD_ADDR    0x27
#define OLED_ADDR   0x3C

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

bool oledOK = false;
bool lcdOK  = false;

// ---- Parametri Morse ----
#define DOT_MAX_MS       250
#define DASH_MIN_MS      250
#define DASH_MAX_MS      1500
#define PAUZA_CARACTER   800
#define PAUZA_CUVANT     2000
#define FRECVENTA_TON    2500

// ---- Detectie microfon ----
int baselineMic = 50;
int pragVariatie = 15;
#define NR_ESANTIOANE  16
#define DEBOUNCE_MIC   25

unsigned long ultimaAdaptareBaseline = 0;
#define INTERVAL_ADAPTARE 100

// ---- Stare semnal Morse ----
#define MAX_MORSE_LEN  8
char secventaMorse[MAX_MORSE_LEN + 1] = "";
uint8_t lungimeMorse = 0;

unsigned long timpInceputSunet = 0;
unsigned long timpUltimSfarsitSunet = 0;
bool sunetDetectat = false;
unsigned long ultimaSchimbareMic = 0;

// ---- Mod Trainer ----
char literaCeruta = 'A';
char morseAsteptat[8] = ".-";
int scorCorect = 0;
int scorTotal = 0;
unsigned long timpInceputLectie = 0;
#define TIMEOUT_LECTIE_MS 30000

#define MAX_TEXT_DECODAT 24
char textDecodat[MAX_TEXT_DECODAT + 1] = "";
uint8_t lungimeText = 0;

enum Mod { MOD_TRAINER, MOD_DECODER };
Mod modCurent = MOD_TRAINER;

enum StareTrainer {
  STARE_LECTIE,
  STARE_FEEDBACK_CORECT,
  STARE_FEEDBACK_GRESIT,
  STARE_TIMEOUT
};
StareTrainer stareTrainer = STARE_LECTIE;
unsigned long timpStare = 0;

// ---- Debounce buton ----
bool stareBtnMod = HIGH;
bool stareBtnModPrecedent = HIGH;
unsigned long ultimaApasareBtn = 0;
#define DEBOUNCE_BTN 250

unsigned long ultimaAnimareLCD = 0;
uint8_t frameAnimLCD = 0;

const char letterA[] PROGMEM = ".-";
const char letterB[] PROGMEM = "-...";
const char letterC[] PROGMEM = "-.-.";
const char letterD[] PROGMEM = "-..";
const char letterE[] PROGMEM = ".";
const char letterF[] PROGMEM = "..-.";
const char letterG[] PROGMEM = "--.";
const char letterH[] PROGMEM = "....";
const char letterI[] PROGMEM = "..";
const char letterJ[] PROGMEM = ".---";
const char letterK[] PROGMEM = "-.-";
const char letterL[] PROGMEM = ".-..";
const char letterM[] PROGMEM = "--";
const char letterN[] PROGMEM = "-.";
const char letterO[] PROGMEM = "---";
const char letterP[] PROGMEM = ".--.";
const char letterQ[] PROGMEM = "--.-";
const char letterR[] PROGMEM = ".-.";
const char letterS[] PROGMEM = "...";
const char letterT[] PROGMEM = "-";
const char letterU[] PROGMEM = "..-";
const char letterV[] PROGMEM = "...-";
const char letterW[] PROGMEM = ".--";
const char letterX[] PROGMEM = "-..-";
const char letterY[] PROGMEM = "-.--";
const char letterZ[] PROGMEM = "--..";

const char* const tabelMorse[] PROGMEM = {
  letterA, letterB, letterC, letterD, letterE, letterF,
  letterG, letterH, letterI, letterJ, letterK, letterL,
  letterM, letterN, letterO, letterP, letterQ, letterR,
  letterS, letterT, letterU, letterV, letterW, letterX,
  letterY, letterZ
};
#define NR_LITERE 26

void getCodMorse(char litera, char* buffer) {
  int idx = litera - 'A';
  if (idx < 0 || idx >= NR_LITERE) {
    buffer[0] = '\0';
    return;
  }
  strcpy_P(buffer, (PGM_P)pgm_read_word(&(tabelMorse[idx])));
}

char decodificaMorse(const char* secventa) {
  char buffer[8];
  for (int i = 0; i < NR_LITERE; i++) {
    strcpy_P(buffer, (PGM_P)pgm_read_word(&(tabelMorse[i])));
    if (strcmp(secventa, buffer) == 0) {
      return 'A' + i;
    }
  }
  return '?';
}

#define LATIME_OSC 128
uint8_t istoricNivel[LATIME_OSC];
int idxOsc = 0;

// CITIRE MICROFON
int citesteVarfMicrofon() {
  int maxim = 0;
  for (int i = 0; i < NR_ESANTIOANE; i++) {
    int val = analogRead(PIN_MIC);
    if (val > maxim) maxim = val;
    delayMicroseconds(250);
  }
  return maxim;
}

// ADAUGA CARACTER MORSE
void adaugaMorse(char c) {
  if (lungimeMorse < MAX_MORSE_LEN) {
    secventaMorse[lungimeMorse++] = c;
    secventaMorse[lungimeMorse] = '\0';
  }
}

void reseteazaMorse() {
  secventaMorse[0] = '\0';
  lungimeMorse = 0;
}

// ADAUGA LA TEXT DECODAT
void adaugaTextDecodat(char c) {
  if (lungimeText >= MAX_TEXT_DECODAT) {
    // Scroll: muta tot stanga cu 1
    for (uint8_t i = 0; i < MAX_TEXT_DECODAT - 1; i++) {
      textDecodat[i] = textDecodat[i + 1];
    }
    textDecodat[MAX_TEXT_DECODAT - 1] = c;
  } else {
    textDecodat[lungimeText++] = c;
    textDecodat[lungimeText] = '\0';
  }
}

// SETUP
void setup() {
  Serial.begin(9600);
  delay(500);
  
  pinMode(PIN_CHEIE, INPUT_PULLUP);
  pinMode(PIN_BTN_MOD, INPUT_PULLUP);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_MIC, INPUT);
  
  for (int i = 0; i < LATIME_OSC; i++) istoricNivel[i] = 0;
  
  Serial.println();
  Serial.println(F("== Signal Corps Boot =="));
  
  Wire.begin();
  Wire.setClock(100000);
  delay(200);
  
  Serial.print(F("OLED... "));
  for (int i = 0; i < 5; i++) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
      oledOK = true;
      break;
    }
    delay(300);
  }
  Serial.println(oledOK ? F("OK") : F("ESEC"));
  
  Serial.print(F("LCD... "));
  lcd.init();
  lcd.backlight();
  lcdOK = true;
  Serial.println(F("OK"));
  
  randomSeed(analogRead(A1) + analogRead(A2) + millis());
  
  afiseazaIntro();
  delay(2000);
  
  calibreazaMicrofon();
  
  porneste_lectie_noua();
}

// LOOP
void loop() {
  bool cheieApasata = (digitalRead(PIN_CHEIE) == LOW);
  if (cheieApasata) {
    tone(PIN_BUZZER, FRECVENTA_TON);
  } else {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
  }
  
  verifica_buton_mod();
  
  if (modCurent == MOD_TRAINER) {
    ruleazaModTrainer();
  } else {
    ruleazaModDecoder();
  }
  
  if (millis() - ultimaAnimareLCD > 800) {
    ultimaAnimareLCD = millis();
    frameAnimLCD = (frameAnimLCD + 1) % 4;
    actualizeaza_design_LCD();
  }
}

// BUTON SCHIMBARE MOD
void verifica_buton_mod() {
  bool stareNoua = digitalRead(PIN_BTN_MOD);
  
  if (stareNoua != stareBtnModPrecedent) {
    ultimaApasareBtn = millis();
  }
  
  if ((millis() - ultimaApasareBtn) > DEBOUNCE_BTN) {
    if (stareNoua != stareBtnMod) {
      stareBtnMod = stareNoua;
      
      if (stareBtnMod == LOW) {
        schimba_mod();
      }
    }
  }
  stareBtnModPrecedent = stareNoua;
}

void schimba_mod() {
  if (modCurent == MOD_TRAINER) {
    modCurent = MOD_DECODER;
    textDecodat[0] = '\0';
    lungimeText = 0;
    reseteazaMorse();
    sunetDetectat = false;
    timpUltimSfarsitSunet = 0;
    Serial.println(F(">>> DECODER <<<"));
  } else {
    modCurent = MOD_TRAINER;
    reseteazaMorse();
    sunetDetectat = false;
    timpUltimSfarsitSunet = 0;
    porneste_lectie_noua();
    Serial.println(F(">>> TRAINER <<<"));
  }
  
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_R, HIGH);
  tone(PIN_BUZZER, 1500, 80);
  delay(120);
  tone(PIN_BUZZER, 2500, 80);
  delay(120);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

// LCD - mod si scor
void actualizeaza_design_LCD() {
  if (!lcdOK) return;
  
  lcd.setCursor(0, 0);
  
  char anim[] = {'.', '-', '.', '-'};
  
  if (modCurent == MOD_TRAINER) {
    lcd.print(anim[frameAnimLCD]);
    lcd.print(F(" MOD TRAINER "));
    lcd.print(anim[(frameAnimLCD + 2) % 4]);
    
    lcd.setCursor(0, 1);
    lcd.print(F("   Scor: "));
    if (scorCorect < 10) lcd.print(F(" "));
    lcd.print(scorCorect);
    lcd.print(F("/"));
    if (scorTotal < 10) lcd.print(F(" "));
    lcd.print(scorTotal);
    lcd.print(F("   "));
  } else {
    lcd.print(anim[frameAnimLCD]);
    lcd.print(F(" MOD DECODER "));
    lcd.print(anim[(frameAnimLCD + 2) % 4]);
    
    lcd.setCursor(0, 1);
    if (sunetDetectat) {
      lcd.print(F("  >> SEMNAL <<  "));
    } else {
      lcd.print(F("  asculta...    "));
    }
  }
}

// MOD TRAINER
void ruleazaModTrainer() {
  switch (stareTrainer) {
    case STARE_LECTIE:
      proceseazaSemnalMicrofon();
      ruleaza_lectie();
      if (stareTrainer == STARE_LECTIE) {
        actualizeazaOLED_Trainer();
      }
      break;
    case STARE_FEEDBACK_CORECT:
      if (millis() - timpStare > 3000) porneste_lectie_noua();
      break;
    case STARE_FEEDBACK_GRESIT:
      if (millis() - timpStare > 3000) porneste_lectie_noua();
      break;
    case STARE_TIMEOUT:
      if (millis() - timpStare > 4000) porneste_lectie_noua();
      break;
  }
}

void porneste_lectie_noua() {
  int idx = random(NR_LITERE);
  literaCeruta = 'A' + idx;
  getCodMorse(literaCeruta, morseAsteptat);
  
  reseteazaMorse();
  sunetDetectat = false;
  timpInceputLectie = millis();
  timpUltimSfarsitSunet = 0;
  stareTrainer = STARE_LECTIE;
  
  Serial.print(F("LECTIE: "));
  Serial.print(literaCeruta);
  Serial.print(F(" ("));
  Serial.print(morseAsteptat);
  Serial.println(F(")"));
  
  actualizeaza_design_LCD();
  actualizeazaOLED_Trainer();
  
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

void ruleaza_lectie() {
  if (millis() - timpInceputLectie > TIMEOUT_LECTIE_MS) {
    semnaleazaTimeout();
    return;
  }
  
  if (!sunetDetectat && timpUltimSfarsitSunet > 0 && lungimeMorse > 0) {
    if (millis() - timpUltimSfarsitSunet > PAUZA_CARACTER) {
      verifica_raspuns();
    }
  }
}

void verifica_raspuns() {
  Serial.print(F("Verif: "));
  Serial.print(secventaMorse);
  Serial.print(F(" vs "));
  Serial.println(morseAsteptat);
  
  if (strcmp(secventaMorse, morseAsteptat) == 0) {
    semnaleazaCorect();
  } else {
    semnaleazaGresit();
  }
}

void semnaleazaCorect() {
  scorCorect++;
  scorTotal++;
  stareTrainer = STARE_FEEDBACK_CORECT;
  
  Serial.println(F("CORECT"));
  
  digitalWrite(PIN_LED_G, HIGH);
  tone(PIN_BUZZER, 2000); delay(100);
  tone(PIN_BUZZER, 2500); delay(100);
  tone(PIN_BUZZER, 3000); delay(150);
  noTone(PIN_BUZZER);
  
  if (oledOK) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    for (int t = 0; t < 3; t++) {
      oled.drawLine(35 + t, 25, 50 + t, 40, SSD1306_WHITE);
      oled.drawLine(50 + t, 40, 90 + t, 10, SSD1306_WHITE);
    }
    oled.setTextSize(2);
    oled.setCursor(15, 48);
    oled.print(F("CORECT"));
    oled.display();
  }
  
  timpStare = millis();
}

void semnaleazaGresit() {
  scorTotal++;
  stareTrainer = STARE_FEEDBACK_GRESIT;
  
  Serial.println(F("GRESIT"));
  
  digitalWrite(PIN_LED_R, HIGH);
  tone(PIN_BUZZER, 1500); delay(200);
  tone(PIN_BUZZER, 800);  delay(300);
  noTone(PIN_BUZZER);
  
  if (oledOK) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    for (int i = 0; i < 3; i++) {
      oled.drawLine(40 + i, 8, 80 + i, 38, SSD1306_WHITE);
      oled.drawLine(80 + i, 8, 40 + i, 38, SSD1306_WHITE);
    }
    oled.setTextSize(1);
    oled.setCursor(0, 48);
    oled.print(F("Era: "));
    oled.print(literaCeruta);
    oled.print(F(" = "));
    oled.print(morseAsteptat);
    oled.setCursor(0, 56);
    oled.print(F("Tu: "));
    oled.print(secventaMorse);
    oled.display();
  }
  
  timpStare = millis();
}

void semnaleazaTimeout() {
  scorTotal++;
  stareTrainer = STARE_TIMEOUT;
  
  Serial.println(F("TIMEOUT"));
  
  noTone(PIN_BUZZER);
  digitalWrite(PIN_LED_R, HIGH);
  
  if (oledOK) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.drawTriangle(50, 8, 78, 8, 64, 30, SSD1306_WHITE);
    oled.drawTriangle(50, 44, 78, 44, 64, 22, SSD1306_WHITE);
    oled.drawLine(45, 6, 83, 6, SSD1306_WHITE);
    oled.drawLine(45, 46, 83, 46, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 55);
    oled.print(F("Era: "));
    oled.print(literaCeruta);
    oled.print(F(" = "));
    oled.print(morseAsteptat);
    oled.display();
  }
  
  timpStare = millis();
}

// OLED MOD TRAINER
void actualizeazaOLED_Trainer() {
  if (!oledOK) return;
  
  int varf = citesteVarfMicrofon();
  int variatie = max(0, varf - baselineMic);
  uint8_t nivel = constrain(map(variatie, 0, 60, 0, 18), 0, 18);
  istoricNivel[idxOsc] = nivel;
  idxOsc = (idxOsc + 1) % LATIME_OSC;
  
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("Transmite:"));
  
  oled.setTextSize(3);
  oled.setCursor(5, 12);
  oled.print(literaCeruta);
  
  int x = 40;
  int y = 18;
  for (uint8_t i = 0; i < strlen(morseAsteptat); i++) {
    if (morseAsteptat[i] == '.') {
      oled.fillCircle(x, y, 2, SSD1306_WHITE);
      x += 8;
    } else {
      oled.fillRect(x - 3, y - 2, 10, 4, SSD1306_WHITE);
      x += 14;
    }
  }
  
  oled.drawFastHLine(0, 35, 128, SSD1306_WHITE);
  
  oled.setTextSize(1);
  oled.setCursor(0, 39);
  oled.print(F("Tu:"));
  
  x = 22;
  y = 42;
  for (uint8_t i = 0; i < lungimeMorse; i++) {
    if (secventaMorse[i] == '.') {
      oled.fillCircle(x, y, 3, SSD1306_WHITE);
      x += 10;
    } else {
      oled.fillRect(x - 4, y - 2, 14, 5, SSD1306_WHITE);
      x += 18;
    }
    if (x > 124) break;
  }
  
  // Osciloscop
  for (int i = 0; i < LATIME_OSC; i++) {
    int h = istoricNivel[(idxOsc + i) % LATIME_OSC];
    if (h > 0) {
      oled.drawFastVLine(i, 63 - h, h, SSD1306_WHITE);
    }
  }
  
  oled.display();
}

// MOD DECODER
void ruleazaModDecoder() {
  proceseazaSemnalMicrofon();
  
  if (!sunetDetectat && timpUltimSfarsitSunet > 0 && lungimeMorse > 0) {
    if (millis() - timpUltimSfarsitSunet > PAUZA_CARACTER) {
      char litera = decodificaMorse(secventaMorse);
      
      Serial.print(F("DECODAT: "));
      Serial.print(secventaMorse);
      Serial.print(F(" = "));
      Serial.println(litera);
      
      adaugaTextDecodat(litera);
      reseteazaMorse();
      timpUltimSfarsitSunet = 0;
      
      if (litera != '?') {
        digitalWrite(PIN_LED_G, HIGH);
        delay(50);
        digitalWrite(PIN_LED_G, LOW);
      } else {
        digitalWrite(PIN_LED_R, HIGH);
        delay(50);
        digitalWrite(PIN_LED_R, LOW);
      }
    }
  }
  
  if (!sunetDetectat && timpUltimSfarsitSunet > 0 && lungimeMorse == 0) {
    if (millis() - timpUltimSfarsitSunet > PAUZA_CUVANT) {
      if (lungimeText > 0 && textDecodat[lungimeText - 1] != ' ') {
        adaugaTextDecodat(' ');
        timpUltimSfarsitSunet = 0;
      }
    }
  }
  
  actualizeazaOLED_Decoder();
}

// OLED MOD DECODER
void actualizeazaOLED_Decoder() {
  if (!oledOK) return;
  
  static unsigned long ultimaActualizare = 0;
  if (millis() - ultimaActualizare < 80) return;
  ultimaActualizare = millis();
  
  int varf = citesteVarfMicrofon();
  int variatie = max(0, varf - baselineMic);
  uint8_t nivel = constrain(map(variatie, 0, 60, 0, 18), 0, 18);
  istoricNivel[idxOsc] = nivel;
  idxOsc = (idxOsc + 1) % LATIME_OSC;
  
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("MOD: DECODER"));
  
  if (sunetDetectat) {
    oled.fillCircle(120, 4, 4, SSD1306_WHITE);
  } else {
    oled.drawCircle(120, 4, 4, SSD1306_WHITE);
  }
  
  oled.setCursor(0, 12);
  oled.print(F("Curent: "));
  oled.print(secventaMorse);
  
  int x = 5;
  int y = 26;
  for (uint8_t i = 0; i < lungimeMorse; i++) {
    if (secventaMorse[i] == '.') {
      oled.fillCircle(x, y, 3, SSD1306_WHITE);
      x += 10;
    } else {
      oled.fillRect(x - 4, y - 2, 14, 5, SSD1306_WHITE);
      x += 18;
    }
    if (x > 120) break;
  }
  
  oled.drawFastHLine(0, 35, 128, SSD1306_WHITE);
  
  oled.setCursor(0, 39);
  oled.print(F("Text:"));
  oled.setCursor(0, 47);
  
  int start = (lungimeText > 16) ? lungimeText - 16 : 0;
  for (int i = start; i < lungimeText; i++) {
    oled.print(textDecodat[i]);
  }
  
  // Osciloscop
  for (int i = 0; i < LATIME_OSC; i++) {
    int h = istoricNivel[(idxOsc + i) % LATIME_OSC];
    if (h > 0) {
      oled.drawFastVLine(i, 63 - h, h, SSD1306_WHITE);
    }
  }
  
  oled.display();
}

// PROCESARE SEMNAL MICROFON
void proceseazaSemnalMicrofon() {
  int varf = citesteVarfMicrofon();
  int variatie = varf - baselineMic;
  
  if (!sunetDetectat && millis() - ultimaAdaptareBaseline > INTERVAL_ADAPTARE) {
    ultimaAdaptareBaseline = millis();
    baselineMic = (baselineMic * 19 + varf) / 20;
  }
  
  int pragOn  = pragVariatie;
  int pragOff = pragVariatie / 2;
  
  if (!sunetDetectat && variatie > pragOn) {
    if (millis() - ultimaSchimbareMic > DEBOUNCE_MIC) {
      sunetDetectat = true;
      timpInceputSunet = millis();
      ultimaSchimbareMic = millis();
      digitalWrite(PIN_LED_G, HIGH);
    }
  }
  
  if (sunetDetectat && variatie < pragOff) {
    if (millis() - ultimaSchimbareMic > DEBOUNCE_MIC) {
      sunetDetectat = false;
      unsigned long durata = millis() - timpInceputSunet;
      timpUltimSfarsitSunet = millis();
      ultimaSchimbareMic = millis();
      
      digitalWrite(PIN_LED_G, LOW);
      
      if (durata < 60) {
      } else if (durata < DOT_MAX_MS) {
        adaugaMorse('.');
        Serial.print(F("DOT -> "));
        Serial.println(secventaMorse);
      } else if (durata < DASH_MAX_MS) {
        adaugaMorse('-');
        Serial.print(F("DASH -> "));
        Serial.println(secventaMorse);
      }
    }
  }
}

// INTRO
void afiseazaIntro() {
  if (oledOK) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(5, 5);
    oled.println(F("SIGNAL"));
    oled.setCursor(15, 25);
    oled.println(F("CORPS"));
    oled.setTextSize(1);
    oled.setCursor(0, 50);
    oled.println(F("Trainer+Decoder"));
    oled.display();
  }
  
  if (lcdOK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("  Signal Corps  "));
    lcd.setCursor(0, 1);
    lcd.print(F(" Morse Trainer  "));
  }
  
  for (int i = 0; i < 3; i++) {
    tone(PIN_BUZZER, FRECVENTA_TON, 100);
    delay(200);
  }
}

// CALIBRARE MICROFON
void calibreazaMicrofon() {
  if (oledOK) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 5);
    oled.println(F("Calibrare microfon"));
    oled.println();
    oled.println(F("Stai in liniste"));
    oled.println(F("3 secunde..."));
    oled.println();
    oled.println(F("NU apasa cheia!"));
    oled.display();
  }
  
  if (lcdOK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Calibrare mic.."));
    lcd.setCursor(0, 1);
    lcd.print(F("LINISTE 3 sec"));
  }
  
  Serial.println(F("Calibrare..."));
  
  long suma = 0;
  int varfMax = 0;
  const int NR_BURST = 100;
  
  for (int i = 0; i < NR_BURST; i++) {
    int varf = citesteVarfMicrofon();
    suma += varf;
    if (varf > varfMax) varfMax = varf;
    delay(30);
  }
  
  baselineMic = suma / NR_BURST;
  pragVariatie = (varfMax - baselineMic) + 15;
  if (pragVariatie < 15) pragVariatie = 15;
  
  Serial.print(F("Baseline: "));
  Serial.print(baselineMic);
  Serial.print(F(" Prag: "));
  Serial.println(pragVariatie);
  
  if (oledOK) {
    oled.clearDisplay();
    oled.setCursor(0, 5);
    oled.println(F("Calibrare gata!"));
    oled.println();
    oled.print(F("Baseline: "));
    oled.println(baselineMic);
    oled.print(F("Prag: +"));
    oled.println(pragVariatie);
    oled.println();
    oled.println(F("Buton = schimba mod"));
    oled.display();
  }
  
  delay(2000);
}
