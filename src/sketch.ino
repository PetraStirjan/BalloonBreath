#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

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
// NETWORK CONFIG
// =====================================

#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""
#define SERVER_URL      "https://httpbin.org/post"
#define DEVICE_ID       "BalloonBreath-ESP32"

#define WIFI_RETRY_MS       10000UL
#define REPORT_RETRY_MS      8000UL
#define HTTP_TIMEOUT_MS       800
#define REPORT_DELAY_MS       800UL
#define REMINDER_INTERVAL_MS 60000UL
#define DEBUG_FLOW              0

// =====================================
// MEDICAL THRESHOLDS AND TIMING
// =====================================

#define FLOW_MAX       1400
#define THRESHOLD_1     600
#define THRESHOLD_2     900
#define THRESHOLD_3    1200

#define ADC_CENTER     2048
#define DEADZONE         80

#define SUCCESS_TIME       5000UL
#define SAMPLE_MS            35UL
#define DISPLAY_MS           70UL
#define RESULT_DISPLAY_MS  2500UL
#define BREATH_END_MS      1500UL

#define EMA_ALPHA        0.35
#define STABILITY_MIN      80
#define MAX_FLOW_STEP     350

// =====================================

enum State {
  IDLE,
  RUNNING,
  SUCCESS,
  PENALTY,
  ABORTED
};

enum BreathDirection {
  REST,
  INHALE,
  EXHALE
};

State currentState = IDLE;
BreathDirection direction = REST;

// =====================================
// SIGNAL
// =====================================

float rawFlow = 0;
float filteredFlow = 0;
float currentFlow = 0;

float stability = 100;
float prevFlow = 0;
float prevRawFlow = 0;
float rawFlowStep = 0;
bool jerkDetected = false;

float volume = 0;
float bestVolume = 0;
float bestPeakFlow = 0;

float balloonY = 39;
float balloonVelocity = 0;

float cyclePeakFlow = 0;
float cycleStabilitySum = 0;
unsigned int cycleStabilitySamples = 0;
unsigned long cycleStart = 0;
unsigned long breathInactiveStart = 0;

unsigned long lastSample = 0;
unsigned long lastDisplay = 0;
unsigned long successStart = 0;
unsigned long successTime = 0;
unsigned long stateEnteredAt = 0;
unsigned long lastSuccessfulExercise = 0;

// =====================================
// NETWORK / REPORT QUEUE
// =====================================

bool wifiStarted = false;
unsigned long lastWifiAttempt = 0;
int lastHttpCode = 0;
bool lastSendOk = false;
unsigned long lastReportAttempt = 0;

bool reportPending = false;
char reportResult[12] = "";
float reportVolume = 0;
float reportPeakFlow = 0;
float reportAvgStability = 0;
float reportBestVolume = 0;
float reportBestPeakFlow = 0;
unsigned long reportDuration = 0;
bool reportPenalty = false;

// =====================================
// NVS
// =====================================

void loadHistory() {

  prefs.begin("balloon", true);

  bestVolume = prefs.getFloat("bestVol", 0);
  bestPeakFlow = prefs.getFloat("bestPeak", 0);

  prefs.end();
}

void saveHistory() {

  prefs.begin("balloon", false);

  prefs.putFloat("bestVol", bestVolume);
  prefs.putFloat("bestPeak", bestPeakFlow);

  prefs.end();

  Serial.print("[NVS] Best volume=");
  Serial.print(bestVolume);
  Serial.print(" ml, best peak=");
  Serial.println(bestPeakFlow);
}

// =====================================
// HELPERS
// =====================================

const char* directionText() {

  switch(direction) {
    case INHALE:
      return "INHALE";

    case EXHALE:
      return "EXHALE";

    case REST:
    default:
      return "REST";
  }
}

float mapFlowMagnitude(int magnitude, int maxMagnitude) {

  long mapped = map(
    magnitude,
    DEADZONE,
    maxMagnitude,
    0,
    FLOW_MAX
  );

  return constrain((float)mapped, 0, FLOW_MAX);
}

bool reminderDue() {

  return
    currentState == IDLE &&
    millis() - lastSuccessfulExercise > REMINDER_INTERVAL_MS;
}

// =====================================
// WIFI AND SERVER COMMUNICATION
// =====================================

void updateWifi(unsigned long now) {

  if(WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  if(!wifiStarted || now - lastWifiAttempt >= WIFI_RETRY_MS)
  {
    wifiStarted = true;
    lastWifiAttempt = now;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);
  }
}

String buildReportJson() {

  String payload = "{";
  payload += "\"device_id\":\"" DEVICE_ID "\",";
  payload += "\"result\":\"" + String(reportResult) + "\",";
  payload += "\"volume_ml\":" + String(reportVolume, 1) + ",";
  payload += "\"peak_flow_ml_s\":" + String(reportPeakFlow, 1) + ",";
  payload += "\"avg_stability\":" + String(reportAvgStability, 1) + ",";
  payload += "\"best_volume_ml\":" + String(reportBestVolume, 1) + ",";
  payload += "\"best_peak_flow_ml_s\":" + String(reportBestPeakFlow, 1) + ",";
  payload += "\"duration_ms\":" + String(reportDuration) + ",";
  payload += "\"penalty\":";
  payload += reportPenalty ? "true" : "false";
  payload += "}";

  return payload;
}

void queueCycleReport(
  const char* result,
  bool penalty
) {

  strncpy(reportResult, result, sizeof(reportResult) - 1);
  reportResult[sizeof(reportResult) - 1] = '\0';

  reportVolume = volume;
  reportPeakFlow = cyclePeakFlow;
  reportAvgStability =
    cycleStabilitySamples > 0
      ? cycleStabilitySum / cycleStabilitySamples
      : stability;
  reportBestVolume = bestVolume;
  reportBestPeakFlow = bestPeakFlow;
  reportDuration = cycleStart > 0 ? millis() - cycleStart : 0;
  reportPenalty = penalty;
  reportPending = true;
  lastReportAttempt = 0;

  Serial.print("[REPORT] Queued ");
  Serial.print(reportResult);
  Serial.print(" volume=");
  Serial.print(reportVolume);
  Serial.print(" avgStability=");
  Serial.println(reportAvgStability);
}

void processReportQueue(unsigned long now) {

  if(!reportPending)
  {
    return;
  }

  if(currentState == RUNNING)
  {
    return;
  }

  if(
    (currentState == SUCCESS || currentState == PENALTY || currentState == ABORTED) &&
    now - stateEnteredAt < REPORT_DELAY_MS
  )
  {
    return;
  }

  if(WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if(lastReportAttempt != 0 && now - lastReportAttempt < REPORT_RETRY_MS)
  {
    return;
  }

  lastReportAttempt = now;

  HTTPClient http;
  WiFiClientSecure secureClient;
  http.setTimeout(HTTP_TIMEOUT_MS);

  String serverUrl = SERVER_URL;
  bool httpStarted = false;

  if(serverUrl.startsWith("https://"))
  {
    secureClient.setInsecure();
    httpStarted = http.begin(secureClient, serverUrl);
  }
  else
  {
    httpStarted = http.begin(serverUrl);
  }

  if(!httpStarted)
  {
    lastSendOk = false;
    lastHttpCode = -1;
    Serial.println("[HTTP] begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  String payload = buildReportJson();
  int code = http.POST(payload);

  lastHttpCode = code;
  lastSendOk = code >= 200 && code < 300;

  Serial.print("[HTTP] POST ");
  Serial.print(SERVER_URL);
  Serial.print(" code=");
  Serial.println(code);
  Serial.println(payload);

  if(code > 0)
  {
    String response = http.getString();

    Serial.print("[HTTP] Response preview: ");
    Serial.println(response.substring(0, 220));
  }

  http.end();

  if(code > 0)
  {
    reportPending = false;
  }
}

// =====================================
// FLOW AND STABILITY
// =====================================

float readBreathFlow() {

  int raw = analogRead(POT_PIN);
  int delta = raw - ADC_CENTER;
  int magnitude = abs(delta);

  if(magnitude <= DEADZONE)
  {
    direction = REST;
    rawFlow = 0;
  }
  else if(delta > 0)
  {
    direction = INHALE;
    rawFlow = mapFlowMagnitude(magnitude, 4095 - ADC_CENTER);
  }
  else
  {
    direction = EXHALE;
    rawFlow = mapFlowMagnitude(magnitude, ADC_CENTER);
  }

  rawFlowStep = fabs(rawFlow - prevRawFlow);
  prevRawFlow = rawFlow;

  filteredFlow =
    EMA_ALPHA * rawFlow +
    (1.0 - EMA_ALPHA) * filteredFlow;

  if(direction == REST && filteredFlow < 5)
  {
    filteredFlow = 0;
  }

  if(DEBUG_FLOW)
  {
    Serial.print("ADC=");
    Serial.print(raw);
    Serial.print(" DIR=");
    Serial.print(directionText());
    Serial.print(" FLOW=");
    Serial.print((int)filteredFlow);
    Serial.println(" ml/s");
  }

  return filteredFlow;
}

void calculateStability() {

  float diff = fabs(currentFlow - prevFlow);

  jerkDetected = rawFlowStep > MAX_FLOW_STEP;

  stability = constrain(
    100 - diff / 4,
    0,
    100
  );

  if(jerkDetected)
  {
    stability = 0;
  }

  prevFlow = currentFlow;
}

void updateCycleStats() {

  if(currentState != RUNNING)
  {
    return;
  }

  if(direction == INHALE)
  {
    cyclePeakFlow = max(cyclePeakFlow, currentFlow);
    cycleStabilitySum += stability;
    cycleStabilitySamples++;
  }
}

// =====================================
// BALLOON PHYSICS
// =====================================

void resetBalloonPhysics() {

  balloonY = 39;
  balloonVelocity = 0;
}

int flowToBalloonY(float flow) {

  flow = constrain(flow, 0, FLOW_MAX);

  if(flow <= THRESHOLD_1)
  {
    return map((int)flow, 0, THRESHOLD_1, 39, 35);
  }

  if(flow <= THRESHOLD_2)
  {
    return map((int)flow, THRESHOLD_1, THRESHOLD_2, 35, 28);
  }

  if(flow <= THRESHOLD_3)
  {
    return map((int)flow, THRESHOLD_2, THRESHOLD_3, 28, 20);
  }

  return map((int)flow, THRESHOLD_3, FLOW_MAX, 20, 17);
}

void updateBalloonPhysics(float dt) {

  float targetY = 39;

  if(direction == INHALE)
  {
    targetY = flowToBalloonY(currentFlow);
  }

  float force = (targetY - balloonY) * 34.0;

  balloonVelocity += force * dt;
  balloonVelocity *= 0.70;
  balloonY += balloonVelocity * dt;

  balloonY = constrain(balloonY, 17, 39);
}

// =====================================
// OLED DRAWING
// =====================================

void drawTinyWifi(
  int x,
  int y
) {

  if(WiFi.status() == WL_CONNECTED)
  {
    display.drawPixel(x + 2, y + 4, WHITE);
    display.drawFastHLine(x + 1, y + 2, 3, WHITE);
    display.drawFastHLine(x, y, 5, WHITE);
  }
  else
  {
    display.drawLine(x, y, x + 4, y + 4, WHITE);
    display.drawLine(x + 4, y, x, y + 4, WHITE);
  }

  if(reportPending)
  {
    display.drawPixel(x + 6, y + 4, WHITE);
  }
}

const char* patientMessage() {

  if(reminderDue())
  {
    return "TIME";
  }

  if(jerkDetected && currentState == RUNNING)
  {
    return "STEADY";
  }

  switch(currentState) {

    case IDLE:
      if(direction == EXHALE)
      {
        return "BREATHE";
      }

      if(direction == INHALE && currentFlow < THRESHOLD_1)
      {
        return "STRONGER";
      }

      return "READY";

    case RUNNING:
      if(direction == EXHALE || direction == REST)
      {
        return "BREATHE";
      }

      if(currentFlow < THRESHOLD_2)
      {
        return "ALMOST";
      }

      return "HOLD";

    case SUCCESS:
      return "GREAT";

    case PENALTY:
      return "TOO FAST";

    case ABORTED:
      return "TRY AGAIN";
  }

  return "READY";
}

void drawHeader() {

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(24, 0);
  display.print("BalloonBreath");

  drawTinyWifi(119, 1);
}

void drawDashedLine(
  int x1,
  int x2,
  int y
) {

  for(int x = x1; x < x2; x += 6)
  {
    display.drawFastHLine(
      x,
      y,
      4,
      WHITE
    );
  }
}

void drawFriendlyBalloon(
  int x,
  int y
) {

  display.drawCircle(x, y, 9, WHITE);

  // Center line and panels make the balloon readable on a 1-bit OLED.
  display.drawLine(x, y - 9, x, y + 10, WHITE);
  display.drawLine(x - 5, y - 7, x - 5, y + 6, WHITE);
  display.drawLine(x + 5, y - 7, x + 5, y + 6, WHITE);
  display.drawFastHLine(x - 5, y, 11, WHITE);

  display.drawLine(x - 7, y + 5, x - 4, y + 11, WHITE);
  display.drawLine(x + 7, y + 5, x + 4, y + 11, WHITE);
  display.drawRect(x - 4, y + 11, 8, 4, WHITE);
}

void drawSafeZone() {

  const int x1 = 26;
  const int x2 = 92;
  const int yTop = 14;
  const int yBottom = 50;
  const int y600 = flowToBalloonY(THRESHOLD_1);
  const int y900 = flowToBalloonY(THRESHOLD_2);
  const int y1200 = flowToBalloonY(THRESHOLD_3);

  display.drawLine(x1, yTop, x1, yBottom, WHITE);
  display.drawFastHLine(x1, yBottom, x2 - x1, WHITE);

  drawDashedLine(x1, x2, y1200);
  drawDashedLine(x1, x2, y900);
  drawDashedLine(x1, x2, y600);

  display.setCursor(0, 14);
  display.print("1200");
  display.setCursor(4, 28);
  display.print("900");
  display.setCursor(4, 42);
  display.print("600");

  drawDashedLine(
    52,
    73,
    (int)balloonY
  );

  drawFriendlyBalloon(
    62,
    (int)balloonY
  );
}

void drawCoachIndicator() {

  const int x = 108;
  const int y = 15;
  const int w = 13;
  const int h = 37;

  display.drawRect(x, y, w, h, WHITE);

  int targetTop = map(100, 0, 100, y + h - 3, y + 2);
  int targetBottom = map(STABILITY_MIN, 0, 100, y + h - 3, y + 2);

  display.drawRect(x + 2, targetTop, w - 4, targetBottom - targetTop + 2, WHITE);

  int markerY = map(
    (int)stability,
    0,
    100,
    y + h - 3,
    y + 2
  );

  display.fillRect(x - 4, markerY, 4, 2, WHITE);
  display.fillRect(x + w, markerY, 4, 2, WHITE);
}

void drawFooter() {

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.drawFastHLine(0, 54, 128, WHITE);

  if(currentState == SUCCESS)
  {
    display.setCursor(14, 56);
    display.print("VOL ");
    display.print((int)volume);
    display.print(" ml");
    return;
  }

  if(currentState == PENALTY)
  {
    display.setCursor(38, 56);
    display.print("TOO FAST");
    return;
  }

  if(currentState == ABORTED)
  {
    display.setCursor(20, 56);
    display.print("TRY AGAIN");
    return;
  }

  if(currentState == RUNNING && successTime > 0)
  {
    display.setCursor(28, 56);
    display.print("HOLD ");
    display.print(successTime / 1000.0, 1);
    display.print("/5s");
    return;
  }

  const char* message = patientMessage();

  if(strcmp(message, "READY") == 0)
  {
    display.setCursor(4, 56);
    display.print("GOAL: STAY IN ZONE");
  }
  else
  {
    int x = (128 - (int)strlen(message) * 6) / 2;
    display.setCursor(max(0, x), 56);
    display.print(message);
  }
}

void drawReminderBanner() {

  if(!reminderDue())
  {
    return;
  }

  if((millis() / 500) % 2 == 0)
  {
    display.drawRect(22, 16, 74, 36, WHITE);
  }
}

void drawDisplay() {

  display.clearDisplay();

  drawHeader();

  drawSafeZone();
  drawCoachIndicator();
  drawReminderBanner();
  drawFooter();

  display.display();
}

// =====================================
// EXERCISE STATE
// =====================================

void resetCycleStats(unsigned long now) {

  volume = 0;
  cyclePeakFlow = 0;
  cycleStabilitySum = 0;
  cycleStabilitySamples = 0;
  cycleStart = now;
  breathInactiveStart = 0;
}

void enterState(
  State nextState,
  unsigned long now
) {

  currentState = nextState;
  stateEnteredAt = now;
}

void finishCycle(
  State finalState,
  const char* report,
  bool penalty,
  unsigned long now
) {

  bool historyChanged = false;

  if(finalState == SUCCESS)
  {
    if(cyclePeakFlow > bestPeakFlow)
    {
      bestPeakFlow = cyclePeakFlow;
      historyChanged = true;
    }

    if(volume > bestVolume)
    {
      bestVolume = volume;
      historyChanged = true;
    }
  }

  if(historyChanged)
  {
    saveHistory();
  }

  queueCycleReport(report, penalty);
  enterState(finalState, now);

  if(finalState == SUCCESS)
  {
    lastSuccessfulExercise = now;
  }
}

void updateExerciseState(
  unsigned long now,
  float dt
) {

  switch(currentState) {

    case IDLE:

      successStart = 0;
      successTime = 0;

      if(direction == INHALE && currentFlow > THRESHOLD_1)
      {
        resetCycleStats(now);
        resetBalloonPhysics();
        enterState(RUNNING, now);
        Serial.println("[START]");
      }

      break;

    case RUNNING:

      updateCycleStats();

      if(direction == INHALE && currentFlow > 100)
      {
        breathInactiveStart = 0;
      }
      else if(breathInactiveStart == 0)
      {
        breathInactiveStart = now;
      }

      if(direction == INHALE)
      {
        volume += currentFlow * dt;
      }

      if(direction == INHALE && currentFlow >= THRESHOLD_3)
      {
        successStart = 0;
        successTime = 0;
        Serial.println("[PENALTY] >1200 ml/s");
        finishCycle(PENALTY, "PENALTY", true, now);
        break;
      }

      if(
        direction == INHALE &&
        currentFlow >= THRESHOLD_2 &&
        currentFlow < THRESHOLD_3 &&
        stability >= STABILITY_MIN &&
        !jerkDetected
      )
      {
        if(successStart == 0)
        {
          successStart = now;
        }

        successTime = now - successStart;

        if(successTime >= SUCCESS_TIME)
        {
          Serial.println("[SUCCESS]");
          finishCycle(SUCCESS, "SUCCESS", false, now);
        }
      }
      else
      {
        successStart = 0;
        successTime = 0;
      }

      if(
        currentState == RUNNING &&
        breathInactiveStart != 0 &&
        now - breathInactiveStart > BREATH_END_MS
      )
      {
        Serial.println("[ABORTED] Breath ended");
        finishCycle(ABORTED, "ABORTED", false, now);
      }

      break;

    case SUCCESS:
    case PENALTY:
    case ABORTED:

      if(now - stateEnteredAt > RESULT_DISPLAY_MS)
      {
        volume = 0;
        successStart = 0;
        successTime = 0;
        breathInactiveStart = 0;
        enterState(IDLE, now);
        Serial.println("[READY]");
      }

      break;
  }
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

  loadHistory();
  lastSuccessfulExercise = millis();

  WiFi.mode(WIFI_STA);

  display.clearDisplay();
  display.display();

  Serial.println();
  Serial.println("===== BALLOONBREATH =====");
  Serial.println("Offset ADC=2048, right=inhale, left=exhale");
  Serial.print("[NVS] Best volume=");
  Serial.print(bestVolume);
  Serial.print(" ml, best peak=");
  Serial.println(bestPeakFlow);
}

// =====================================
// LOOP
// =====================================

void loop() {

  unsigned long now = millis();

  updateWifi(now);
  processReportQueue(now);

  if(now - lastSample < SAMPLE_MS)
  {
    return;
  }

  float dt = (now - lastSample) / 1000.0;
  lastSample = now;

  currentFlow = readBreathFlow();

  calculateStability();
  updateBalloonPhysics(dt);
  updateExerciseState(now, dt);

  if(now - lastDisplay >= DISPLAY_MS)
  {
    lastDisplay = now;
    drawDisplay();
  }
}
