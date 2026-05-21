#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  10
#define TFT_DC   8
#define TFT_RST -1
#define LED_PIN 13

#define NUM_BUTTONS 8
const int BUTTON_PINS[NUM_BUTTONS] = {7, 6, 5, 4, 3, 2, A0, A1};

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---- Settings ----
int maxTime = 480;
int playerTime[6];
int activePlayer = -1;
bool running = false;

// ---- Button state ----
bool buttonState[NUM_BUTTONS];
bool lastButtonState[NUM_BUTTONS];
unsigned long lastTick = 0;

// ---- Hourglass ----
int hourglassFrame = 0;
unsigned long lastHourglassUpdate = 0;

// ---- Helpers ----
bool justPressed(int i) {
  return buttonState[i] && !lastButtonState[i];
}

void readButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    lastButtonState[i] = buttonState[i];
    buttonState[i] = digitalRead(BUTTON_PINS[i]) == HIGH;
  }
}

void formatTime(int seconds, char* buf) {
  sprintf(buf, "%d:%02d", seconds / 60, seconds % 60);
}

// ---- Timeout Flash ----
void flashTimeout() {
  // Stop clock and flash screen red/black 5 times
  running = false;
  for (int f = 0; f < 5; f++) {
    tft.fillScreen(ST77XX_RED);
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    tft.fillScreen(ST77XX_BLACK);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  // Advance to next player but don't start their clock
  int next = (activePlayer + 1) % 6;
  activePlayer = next;
  running = false;
  drawScreen();
}

// ---- Hourglass ----
// Draws a large hourglass in the right half of the screen
// x,y = top left corner, w/h = bounding box
void drawHourglass(int x, int y, int w, int h, float fraction, uint16_t color) {
  // fraction = 1.0 full, 0.0 empty
  int cx = x + w / 2;
  int midY = y + h / 2;

  // Clear area
  tft.fillRect(x, y, w, h, ST77XX_BLACK);

  // Top triangle outline
  tft.drawTriangle(x, y, x + w, y, cx, midY, color);
  // Bottom triangle outline
  tft.drawTriangle(x, y + h, x + w, y + h, cx, midY, color);

  // Fill top triangle (sand draining) - fill from bottom of top triangle up
  if (fraction > 0) {
    int sandH = (int)((midY - y) * fraction);
    // Fill top sand - shrinking triangle from top
    for (int row = 0; row < sandH; row++) {
      float t = (float)row / (midY - y);
      int rowW = (int)(w * t);
      int rowX = cx - rowW / 2;
      tft.drawFastHLine(rowX, y + row, rowW, color);
    }
  }

  // Fill bottom triangle (sand collecting) - fill from bottom up
  if (fraction < 1.0) {
    int sandH = (int)((midY - y) * (1.0 - fraction));
    for (int row = 0; row < sandH; row++) {
      float t = (float)row / (midY - y);
      int rowW = (int)(w * t);
      int rowX = cx - rowW / 2;
      tft.drawFastHLine(rowX, y + h - row, rowW, color);
    }
  }

  // Neck dot
  tft.fillCircle(cx, midY, 2, color);
}

void updateHourglass() {
  if (activePlayer < 0) return;

  float fraction = (float)playerTime[activePlayer] / maxTime;
  fraction = constrain(fraction, 0.0, 1.0);

  uint16_t col = running ? ST77XX_CYAN : ST77XX_YELLOW;
  drawHourglass(88, 14, 68, 52, fraction, col);

  // Big time display centered below hourglass
  char buf[10];
  formatTime(playerTime[activePlayer], buf);
  tft.setTextSize(2);
  tft.setTextColor(col);
  // Clear old time
  tft.fillRect(88, 68, 72, 12, ST77XX_BLACK);
  tft.setCursor(92, 68);
  tft.print(buf);
  tft.setTextSize(1);
}

// ---- Display ----
void drawScreen() {
  tft.fillScreen(ST77XX_BLACK);

  // Top bar
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(2, 2);
  tft.print("Max:");
  char buf[10];
  formatTime(maxTime, buf);
  tft.print(buf);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(55, 2);
  tft.print("B0:rst B1:+30s");

  tft.drawFastHLine(0, 12, 160, ST77XX_WHITE);

  // Vertical divider between player list and hourglass
  tft.drawFastVLine(87, 12, 68, ST77XX_WHITE);

  // Player rows on left half
  for (int i = 0; i < 6; i++) {
    drawPlayer(i);
  }

  // Hourglass on right
  if (activePlayer >= 0) {
    updateHourglass();
  }
}

void drawPlayer(int i) {
  int y = 15 + (i * 10);

  uint16_t bg = ST77XX_BLACK;
  if (i == activePlayer) {
    bg = running ? ST77XX_GREEN : ST77XX_YELLOW;
  }
  tft.fillRect(0, y, 86, 9, bg);

  uint16_t fg = (i == activePlayer) ? ST77XX_BLACK : ST77XX_WHITE;
  if (playerTime[i] <= 0) fg = ST77XX_RED;

  tft.setTextColor(fg);
  tft.setTextSize(1);

  tft.setCursor(2, y + 1);
  tft.print("P");
  tft.print(i + 1);

  char buf[10];
  formatTime(playerTime[i], buf);
  tft.setCursor(22, y + 1);
  tft.print(buf);

  // Small status indicator
  if (i == activePlayer) {
    tft.setCursor(58, y + 1);
    tft.print(running ? "RUN" : "PAU");
  }
}

// ---- Reset ----
void resetAll() {
  for (int i = 0; i < 6; i++) {
    playerTime[i] = maxTime;
  }
  activePlayer = -1;
  running = false;
  digitalWrite(LED_PIN, LOW);
  drawScreen();
}

// ---- Main ----
void setup() {
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT);
  }

  tft.initR(INITR_MINI160x80_PLUGIN);
  tft.setRotation(1);

  resetAll();
}

void loop() {
  readButtons();

  // Button 0: reset
  if (justPressed(0)) {
    resetAll();
  }

  // Button 1: +30s to max, loop at 10 min
  if (justPressed(1)) {
    maxTime += 30;
    if (maxTime > 600) maxTime = 30;
    resetAll();
  }

  // Buttons 2-7: players
  for (int i = 0; i < 6; i++) {
    if (justPressed(i + 2)) {
      if (i == activePlayer) {
        // Toggle pause/run your own timer
        running = !running;
        if (running) lastTick = millis();
        drawPlayer(i);
        updateHourglass();
      } else {
        // Switch to this player, don't auto-start
        int old = activePlayer;
        activePlayer = i;
        running = false;
        if (old >= 0) drawPlayer(old);
        drawPlayer(i);
        updateHourglass();
      }
    }
  }

  // Tick timer
  if (running && activePlayer >= 0) {
    unsigned long now = millis();
    if (now - lastTick >= 1000) {
      lastTick = now;
      playerTime[activePlayer]--;

      if (playerTime[activePlayer] <= 0) {
        playerTime[activePlayer] = 0;
        drawPlayer(activePlayer);
        updateHourglass();
        flashTimeout();
      } else {
        drawPlayer(activePlayer);
        updateHourglass();
      }
    }
  }

  // Animate hourglass while running
  if (running && activePlayer >= 0) {
    unsigned long now = millis();
    if (now - lastHourglassUpdate >= 2000) {
      lastHourglassUpdate = now;
      updateHourglass();
    }
  }

  delay(20);
}