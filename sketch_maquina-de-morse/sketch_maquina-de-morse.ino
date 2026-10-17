#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Maquina de Morse: botao -> ponto/traco -> letra -> texto no OLED.
// O bip acompanha o botao enquanto ele esta pressionado (sem delay).
constexpr byte BUTTON_PIN = 2, BUZZER_PIN = 9;
constexpr unsigned long UNIT_MS = 180, DEBOUNCE_MS = 25;
constexpr unsigned long LETTER_GAP_MS = UNIT_MS * 3, WORD_GAP_MS = UNIT_MS * 7;
constexpr unsigned long CLEAR_MS = 2000;  // segurar o botao por este tempo limpa o texto
constexpr unsigned int TONE_HZ = 800;
constexpr byte MAX_LEN = 5, TREE_SIZE = 64, MAX_MSG = 32;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

struct Entry { const char *code; char value; };
const Entry table[] = {
  {".-",'A'},{"-...",'B'},{"-.-.",'C'},{"-..",'D'},{".",'E'},
  {"..-.",'F'},{"--.",'G'},{"....",'H'},{"..",'I'},{".---",'J'},
  {"-.-",'K'},{".-..",'L'},{"--",'M'},{"-.",'N'},{"---",'O'},
  {".--.",'P'},{"--.-",'Q'},{".-.",'R'},{"...",'S'},{"-",'T'},
  {"..-",'U'},{"...-",'V'},{".--",'W'},{"-..-",'X'},{"-.--",'Y'},
  {"--..",'Z'},{"-----",'0'},{".----",'1'},{"..---",'2'},
  {"...--",'3'},{"....-",'4'},{".....",'5'},{"-....",'6'},
  {"--...",'7'},{"---..",'8'},{"----.",'9'}
};
char tree[TREE_SIZE] = {}, code[MAX_LEN + 1] = "";
byte codeLen = 0;
String message = "";
bool displayOk = false;
bool stable = HIGH, reading = HIGH, pressing = false, letterWait = false, wordWait = false;
unsigned long debounceAt = 0, pressAt = 0, releaseAt = 0;

// Arvore binaria em vetor: ponto -> 2n+1, traco -> 2n+2 (indices max 62 < 64).
void buildTree() {
  for (const Entry &e : table) {
    byte n = 0;
    for (const char *p = e.code; *p; ++p) {
      n = 2 * n + (*p == '.' ? 1 : 2);
      if (n >= TREE_SIZE) break;
    }
    if (n < TREE_SIZE) tree[n] = e.value;
  }
}
char decode() {
  byte n = 0;
  for (byte i = 0; i < codeLen; ++i) {
    n = 2 * n + (code[i] == '.' ? 1 : 2);
    if (n >= TREE_SIZE) return '?';
  }
  return tree[n] ? tree[n] : '?';
}
void show(const __FlashStringHelper *status) {
  if (!displayOk) return;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println(F("MAQUINA DE MORSE"));
  display.drawLine(0,10,127,10,SSD1306_WHITE);
  display.setCursor(0,16); display.print(F("Codigo: ")); display.println(code[0] ? code : "-");
  display.setCursor(0,29); display.print(F("Letra: "));
  if (codeLen) display.println(decode()); else display.println(letterWait ? F("...") : F("-"));
  display.setCursor(0,43); display.print(F("Texto: "));
  display.println(message.length()>16 ? message.substring(message.length()-16) : message);
  display.setCursor(0,56); display.print(status); display.display();
}
void addPulse(unsigned long ms) {
  if (codeLen >= MAX_LEN) { show(F("LIMITE 5")); return; }
  char symbol = ms <= UNIT_MS*2 ? '.' : '-';
  code[codeLen++] = symbol; code[codeLen] = 0;
  letterWait = true; wordWait = false;
  show(symbol=='.' ? F("PONTO") : F("TRACO"));
}
void appendChar(char c) {
  if (message.length() >= MAX_MSG) message.remove(0, 1);  // mantem as ultimas letras
  message += c;
}
void finishLetter() {
  if (!codeLen) return;
  appendChar(decode());
  code[0]=0; codeLen=0; letterWait=false; wordWait=true; show(F("LETRA"));
}
void clearAll() {
  message = ""; code[0]=0; codeLen=0; letterWait=false; wordWait=false;
  show(F("LIMPO"));
}
void setup() {
  pinMode(BUTTON_PIN,INPUT_PULLUP); pinMode(BUZZER_PIN,OUTPUT);
  message.reserve(MAX_MSG + 1);
  buildTree();
  displayOk = display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  show(F("PRONTO"));
}
void loop() {
  unsigned long now=millis(); bool sample=digitalRead(BUTTON_PIN);
  if (sample!=reading) { reading=sample; debounceAt=now; }
  if (now-debounceAt>=DEBOUNCE_MS && stable!=reading) {
    stable=reading;
    if (stable==LOW) {            // botao pressionado: bip enquanto segura
      pressAt=now; pressing=true;
      tone(BUZZER_PIN,TONE_HZ);
    } else if (pressing) {        // botao solto
      noTone(BUZZER_PIN);
      unsigned long held=now-pressAt;
      pressing=false; releaseAt=now;
      if (held >= CLEAR_MS) clearAll(); else addPulse(held);
    }
  }
  if (stable==HIGH && letterWait && now-releaseAt>=LETTER_GAP_MS) {
    finishLetter();
  } else if (stable==HIGH && wordWait && now-releaseAt>=WORD_GAP_MS) {
    if (message.length() && message[message.length()-1]!=' ') appendChar(' ');
    wordWait=false; show(F("PAUSA"));
  }
}
