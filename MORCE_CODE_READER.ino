#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin definitions
#define BUTTON_PIN 14
#define BUZZER_PIN 25
#define LED_PIN    26

// Timing variables
unsigned long pressStart = 0;
bool buttonPressed = false;
String morseBuffer = "";
String decodedText = "";
unsigned long lastInputTime = 0;
bool inScreensaver = false;

// Thresholds (ms)
const int dotThreshold = 300;
const int letterPause = 500;
const int wordPause = 3000;
const int idleTimeout = 15000;  // 15 seconds of inactivity → go to screensaver

// 8x8 star bitmap
const unsigned char starBitmap[] PROGMEM = {
  0b00011000,
  0b00111100,
  0b01111110,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

// Morse code mapping
struct MorseMap {
  const char* code;
  char letter;
};

MorseMap morseTable[] = {
  {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
  {".", 'E'}, {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'},
  {"..", 'I'}, {".---", 'J'}, {"-.-", 'K'}, {".-..", 'L'},
  {"--", 'M'}, {"-.", 'N'}, {"---", 'O'}, {".--.", 'P'},
  {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
  {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'},
  {"-.--", 'Y'}, {"--..", 'Z'},
  {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
  {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
  {"---..", '8'}, {"----.", '9'}
};

// Decode Morse
char decodeMorse(String code) {
  for (MorseMap m : morseTable) {
    if (code == m.code) return m.letter;
  }
  return '?';
}

// Centered text helper
void drawCenteredText(int y, String text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// Show welcome screen
void showWelcomeScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  for (int y = -8; y < SCREEN_HEIGHT + 8; y += 2) {
    display.clearDisplay();
    for (int i = 0; i < 6; i++) {
      int x = 10 + i * 20;
      int yOffset = (y + i * 7) % SCREEN_HEIGHT;
      display.drawBitmap(x, yOffset, starBitmap, 8, 8, WHITE);
    }
    display.display();
    delay(50);
  }

  display.clearDisplay();
  drawCenteredText(20, "Welcome to");
  drawCenteredText(35, "Morse Code Reader");
  display.display();
  delay(3000);
  display.clearDisplay();
  display.display();
}

// Show Morse & Message
void showOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("Morse: ");
  display.println(morseBuffer);

  display.setCursor(0, 20);
  display.print("Message:");
  display.setCursor(0, 32);
  display.println(decodedText);

  display.display();
}

// Screensaver animation
void showScreensaver() {
  static int yOffset = -8;
  display.clearDisplay();
  for (int i = 0; i < 6; i++) {
    int x = 10 + i * 20;
    int y = (yOffset + i * 7) % SCREEN_HEIGHT;
    display.drawBitmap(x, y, starBitmap, 8, 8, WHITE);
  }
  display.display();
  yOffset += 2;
  if (yOffset > SCREEN_HEIGHT) yOffset = -8;
  delay(50);
}

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (1);
  }

  randomSeed(analogRead(0));
  showWelcomeScreen();
  showOLED();
  lastInputTime = millis();
}

void loop() {
  bool state = digitalRead(BUTTON_PIN);

  // Wake up from screensaver
  if (inScreensaver && state == HIGH) {
    inScreensaver = false;
    showOLED();
    delay(300);  // debounce
  }

  // If in screensaver, animate stars
  if (inScreensaver) {
    showScreensaver();
    return;
  }

  // Button pressed
  if (state == HIGH && !buttonPressed) {
    pressStart = millis();
    buttonPressed = true;
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  }

  // Button released
  if (state == LOW && buttonPressed) {
    unsigned long pressDuration = millis() - pressStart;
    buttonPressed = false;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    morseBuffer += (pressDuration < dotThreshold) ? "." : "-";
    lastInputTime = millis();
    showOLED();
  }

  // Decode letter
  if (!buttonPressed && morseBuffer.length() > 0 && millis() - lastInputTime > letterPause) {
    char decodedChar = decodeMorse(morseBuffer);
    decodedText += decodedChar;
    morseBuffer = "";
    lastInputTime = millis();  // reset last activity
    showOLED();
  }

  // Detect word pause
  if (!buttonPressed && morseBuffer.length() == 0 && decodedText.length() > 0 &&
      millis() - lastInputTime > wordPause) {
    if (decodedText[decodedText.length() - 1] != ' ') {
      decodedText += " ";
      showOLED();
    }
  }

  // Enter screensaver after long idle time
  if (!buttonPressed && millis() - lastInputTime > idleTimeout) {
    inScreensaver = true;
  }
}
