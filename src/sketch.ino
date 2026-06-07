#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#define POT_PIN 34

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

Preferences prefs;

// =====================================
// MEDICINSKI PRAGOVI
// =====================================

#define FLOW_MAX      1400
#define THRESHOLD_1   600
#define THRESHOLD_2   900
#define THRESHOLD_3   1200

#define SUCCESS_TIME  5000
#define SAMPLE_MS     50
#define EMA_ALPHA     0.20

// =====================================

enum State {
  IDLE,
  RUNNING,
  SUCCESS,
  PENALTY
};

State currentState = IDLE;

// =====================================

float emaFlow = 0;
float currentFlow = 0;

float stability = 100;
float prevFlow = 0;

float volume = 0;
float bestVolume = 0;

unsigned long lastSample = 0;
unsigned long successStart = 0;
unsigned long successTime = 0;
unsigned long penaltyStart = 0;

// =====================================
// NVS
// =====================================

float loadBestVolume() {

  prefs.begin("balloon", true);

  float value = prefs.getFloat("bestVol", 0);

  prefs.end();

  return value;
}

void saveBestVolume(float value) {

  prefs.begin("balloon", false);

  prefs.putFloat("bestVol", value);

  prefs.end();

  Serial.print("[NVS] Novi rekord: ");

  Serial.println(value);
}

// =====================================
// FLOW
// =====================================

float readFlow() {

  int raw = analogRead(POT_PIN);

  float flow =
    map(
      raw,
      0,
      4095,
      0,
      FLOW_MAX
    );

  emaFlow = EMA_ALPHA * flow + (1.0 - EMA_ALPHA) * emaFlow;

  Serial.print("FLOW = ");
  Serial.print((int)emaFlow);
  Serial.println(" ml/s");

  return emaFlow;
}

// =====================================
// STABILITY
// =====================================

void calculateStability() {

  float diff =
    abs(
      currentFlow -
      prevFlow
    );

  prevFlow =
    currentFlow;

  stability =
    constrain(
      100 - diff / 4,
      0,
      100
    );
}

// =====================================
// BALLOON
// =====================================

void drawBalloon(
  int x,
  int y
) {

  display.drawCircle(
    x,
    y,
    9,
    WHITE
  );

  display.drawCircle(
    x,
    y,
    7,
    WHITE
  );

  display.drawLine(
    x - 4,
    y + 5,
    x - 2,
    y + 14,
    WHITE
  );

  display.drawLine(
    x + 4,
    y + 5,
    x + 2,
    y + 14,
    WHITE
  );

  display.drawRect(
    x - 3,
    y + 14,
    6,
    4,
    WHITE
  );
}

// =====================================
// OLED
// =====================================

void drawDisplay() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  // =====================
  // NASLOV
  // =====================

  display.setCursor(18,0);
  display.print("BalloonBreath");

  // =====================
  // RECORD
  // =====================

  display.setCursor(0,10);
  display.print("R:");
  display.print((int)bestVolume);

  // =====================
  // FLOW
  // =====================

  display.setCursor(70,10);
  display.print("F:");
  display.print((int)currentFlow);

  // =====================
  // SIGURNA ZONA
  // =====================

  display.drawRect(
    0,
    20,
    100,
    28,
    WHITE
  );

  // zona 900-1200

  display.fillRect(
    0,
    26,
    100,
    8,
    WHITE
  );

  // =====================
  // BALON
  // =====================

  int balloonY =
    map(
      constrain(
        currentFlow,
        0,
        FLOW_MAX
      ),
      0,
      FLOW_MAX,
      44,
      20
    );

  drawBalloon(
    50,
    balloonY
  );

  // =====================
  // COACH INDICATOR
  // =====================

  display.setCursor(108,8);
  display.print("C");

  // okvir

  display.drawRect(
    112,
    20,
    12,
    36,
    WHITE
  );

  // target zona

  display.drawRect(
    112,
    31,
    12,
    8,
    WHITE
  );

  // marker

  int markerY =
    map(
      (int)stability,
      0,
      100,
      52,
      22
    );

  display.fillRect(
    114,
    markerY,
    8,
    3,
    WHITE
  );

  // =====================
  // VOLUMEN
  // =====================

  display.setCursor(0,54);

  display.print("V:");
  display.print((int)volume);

  // =====================
  // STATUS
  // =====================

  display.setCursor(55,54);

  switch(currentState) {

    case IDLE:

      display.print("READY");
      break;

    case RUNNING:

      display.print("H:");
      display.print(successTime / 1000.0, 1);
      break;

    case SUCCESS:

      display.print("SUCCESS");
      break;

    case PENALTY:

      display.print("FAST!");
      break;
  }

  display.display();
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  analogReadResolution(12);

  if(!display.begin(
       SSD1306_SWITCHCAPVCC,
       0x3C))
  {
    Serial.println("OLED ERROR");

    while(true);
  }

  bestVolume = loadBestVolume();

  display.clearDisplay();
  display.display();


  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20,20);

  Serial.println();
  Serial.println(
    "===== BALLOONBREATH ====="
  );

  Serial.print("[NVS] Rekord=");

  Serial.println(bestVolume);
}

// =====================================
// LOOP
// =====================================

void loop() {

  unsigned long now = millis();

  if(now - lastSample < SAMPLE_MS)
  {
    return;
  }

  float dt = (now - lastSample) / 1000.0;

  lastSample = now;

  currentFlow = readFlow();

  calculateStability();

  volume += currentFlow * dt;

  switch(currentState) {

    case IDLE:

      successTime = 0;
      successStart = 0;

      if(currentFlow > THRESHOLD_1)
      {
        currentState = RUNNING;
        Serial.println("[START]");
      }

      break;

    case RUNNING:

      if(currentFlow >= THRESHOLD_3){
        currentState = PENALTY;

        penaltyStart = now;

        successTime = 0;

        Serial.println("[PENALTY] >1200 ml/s");

        break;
      }

      if(currentFlow >= THRESHOLD_2 && currentFlow < THRESHOLD_3 && stability > 80)
      {
        if(successStart == 0)
        {
          successStart = now;
        }

        successTime = now - successStart;

        if(successTime >= SUCCESS_TIME)
        {
          currentState = SUCCESS;

          Serial.println("[SUCCESS]");

          if(volume > bestVolume)
          {
            bestVolume = volume;

            saveBestVolume(bestVolume);
          }
        }
      }
      else
      {
        successStart = 0;
        successTime = 0;
      }

      break;

    case SUCCESS:

      if(currentFlow < 100)
      {
        currentState = IDLE;
        volume = 0;

        Serial.println("[RESET]");
      }

      break;

    case PENALTY:

      if(now - penaltyStart > 2000)
      {
        currentState = IDLE;
        volume = 0;

        Serial.println("[TRY AGAIN]");
      }

      break;
  }

  drawDisplay();
}