/****************************************************
 * ESP32 + PZEM-004T v3.0 + Blynk (unchanged)
 * + Telegram: /status, /off, /on
 * + Night mode alert (22:00–07:00 MYT): notify if lamp stays ON
 ****************************************************/

// -------- Blynk Template / Auth (UNCHANGED) --------
#define BLYNK_TEMPLATE_ID   "BLYNK TEMPLATE ID"
#define BLYNK_TEMPLATE_NAME "BLYNK TEMPLATE NAME"
#define BLYNK_AUTH_TOKEN    "BLYNK TOKEN"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <PZEM004Tv30.h>
#include <time.h>  // NTP time

// -------- Wi-Fi --------
char ssid[] = "WIFI NAME";
char pass[] = "WIFI PASSWORD";

// -------- Telegram (minimal) --------
// Group IDs are negative (e.g. "-1001234567890"); 1-1 chat is positive.
#define TELEGRAM_BOT_TOKEN "XXXXXXXX:YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY"
#define TELEGRAM_CHAT_ID   "-1001234567890"

// -------- Pins (UNCHANGED) --------
#define RX_PIN   16   // ESP32 RX2  <- PZEM TX
#define TX_PIN   17   // ESP32 TX2  -> PZEM RX
#define RELAY1   23   // Blynk V10 (Lamp)
#define RELAY2   22   // Blynk V11

const bool RELAY_ACTIVE = LOW;
const bool RELAY_IDLE   = HIGH;

// -------- Objects --------
HardwareSerial PZEMSerial(2);
PZEM004Tv30 pzem(PZEMSerial, RX_PIN, TX_PIN);
BlynkTimer timer;

// -------- Blynk handlers (UNCHANGED) --------
BLYNK_WRITE(V10) { digitalWrite(RELAY1, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }
BLYNK_WRITE(V11) { digitalWrite(RELAY2, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }

// -------- Simple settings --------
float ON_W_THRESHOLD        = 5.0;    // W: treat >= as "lamp ON" (tune to your lamp)
unsigned long NIGHT_ON_MINUTES = 5;   // continuous minutes before alert at night
const unsigned long NOTIF_COOLDOWN_MS = 30UL * 60UL * 1000UL; // alert at most every 30 min

// Night window: 22:00 → 07:00 (MYT)
const uint8_t NIGHT_START_H = 22;
const uint8_t NIGHT_END_H   = 7;

// -------- State --------
unsigned long onSinceMs = 0;
unsigned long lastNotifMs = 0;

// Telegram polling
long telegramUpdateOffset = 0;

// -------- Time (NTP) --------
const long GMT_OFFSET_SEC = 8 * 3600; // Asia/Kuala_Lumpur
const int  DST_OFFSET_SEC = 0;

bool getLocalHour(uint8_t &hourOut) {
  struct tm t;
  if (!getLocalTime(&t, 1000)) return false;
  hourOut = (uint8_t)t.tm_hour;
  return true;
}
bool inNightWindow(uint8_t hr) {
  // 22→07 wraps midnight: (hr>=22 || hr<7)
  if (NIGHT_START_H < NIGHT_END_H) return (hr >= NIGHT_START_H && hr < NIGHT_END_H);
  return (hr >= NIGHT_START_H || hr < NIGHT_END_H);
}

// -------- Telegram helpers (minimal) --------
bool tgSend(const String &text) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage";
  String payload = String("{\"chat_id\":\"") + TELEGRAM_CHAT_ID + "\",\"text\":\"" + text + "\"}";
  if (!https.begin(client, url)) return false;
  https.addHeader("Content-Type", "application/json");
  int code = https.POST(payload);
  https.end();
  return (code >= 200 && code < 300);
}

void setRelay1(bool on) {
  digitalWrite(RELAY1, on ? RELAY_ACTIVE : RELAY_IDLE);
  // Blynk UI left as-is (no forced sync), so your app remains unchanged.
}

void tgHandleCmd(const String &s) {
  String cmd = s; cmd.trim();
  if (cmd.equalsIgnoreCase("/status")) {
    float p = pzem.power();
    bool lamp = (digitalRead(RELAY1) == RELAY_ACTIVE);
    uint8_t hr=0; bool ok=getLocalHour(hr);
    tgSend(String("Lamp: ") + (lamp?"ON":"OFF") +
           "\nPower: " + String(p,1) + " W" +
           "\nNight window: 22:00–07:00" +
           (ok ? ("\nLocal hour: " + String(hr)) : "\nLocal hour: (Syncing NTP)") );
    return;
  }
  if (cmd.equalsIgnoreCase("/off")) { setRelay1(false); tgSend("Lamp turned OFF."); return; }
  if (cmd.equalsIgnoreCase("/on"))  { setRelay1(true);  tgSend("Lamp turned ON.");  return; }
  // Ignore others
}

void tgPoll() {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN +
               "/getUpdates?timeout=10&allowed_updates=%5B%22message%22%5D&offset=" + String(telegramUpdateOffset);
  if (!https.begin(client, url)) return;
  int code = https.GET();
  if (code >= 200 && code < 300) {
    String body = https.getString();
    int pos = 0;
    while (true) {
      int uidIdx = body.indexOf("\"update_id\":", pos);
      if (uidIdx < 0) break;
      int uidStart = body.indexOf(':', uidIdx) + 1;
      int uidEnd   = body.indexOf(',', uidStart);
      long uid     = body.substring(uidStart, uidEnd).toInt();

      int textKey  = body.indexOf("\"text\":", uidEnd);
      if (textKey < 0) { pos = uidEnd; telegramUpdateOffset = uid + 1; continue; }
      int q1 = body.indexOf('\"', textKey + 7);
      int q2 = body.indexOf('\"', q1 + 1);
      String text = body.substring(q1 + 1, q2);

      // chat id check
      int chatKey = body.indexOf("\"chat\":", textKey);
      int idKey   = body.indexOf("\"id\":", chatKey);
      int idStart = body.indexOf(':', idKey) + 1;
      int idEnd   = body.indexOf(',', idStart);
      String chatIdFound = body.substring(idStart, idEnd); chatIdFound.trim();

      if (chatIdFound == TELEGRAM_CHAT_ID) tgHandleCmd(text);

      pos = q2 + 1;
      telegramUpdateOffset = uid + 1;
    }
  }
  https.end();
}

// -------- Core reading + night alert (Blynk unchanged) --------
void pushReadings() {
  static bool first = true;

  float v  = pzem.voltage();
  float i  = pzem.current();
  float p  = pzem.power();
  float e  = pzem.energy();

  if (first) { first = false; return; }
  if (isnan(v) || isnan(i) || isnan(p) || isnan(e)) {
    Serial.println("PZEM read error (check wiring).");
    return;
  }

  Serial.printf("V=%.1f V | I=%.3f A | P=%.1f W | E=%.3f kWh\n", v, i, p, e);

  // Push to Blynk (same virtual pins)
  Blynk.virtualWrite(V0, v);
  Blynk.virtualWrite(V1, i);
  Blynk.virtualWrite(V2, p);
  Blynk.virtualWrite(V3, e);

  // Night-mode alert
  uint8_t hr = 0; bool haveHr = getLocalHour(hr);
  bool night = haveHr ? inNightWindow(hr) : true; // if time not yet synced, allow alerts

  unsigned long now = millis();
  bool lampOnByPower = (p >= ON_W_THRESHOLD);

  if (night && lampOnByPower) {
    if (onSinceMs == 0) onSinceMs = now;
    unsigned long elapsed = now - onSinceMs;
    unsigned long target  = NIGHT_ON_MINUTES * 60UL * 1000UL;

    if (elapsed >= target) {
      bool cooldownOk = (now - lastNotifMs) >= NOTIF_COOLDOWN_MS;
      if (cooldownOk) {
        tgSend(String("⚠️ Night alert: lamp has been ON for ")
               + NIGHT_ON_MINUTES + " min (≈" + String(p,1) + " W). "
               "If unintended, please turn it OFF.");
        lastNotifMs = now;
      }
    }
  } else {
    onSinceMs = 0; // reset if not in night window or lamp off
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, RELAY_IDLE);
  digitalWrite(RELAY2, RELAY_IDLE);

  // Blynk handles Wi-Fi too (UX unchanged)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // NTP time (Asia/Kuala_Lumpur)
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

  // Timers
  timer.setInterval(2000, pushReadings); // PZEM + night alert
  timer.setInterval(3000, tgPoll);       // Telegram command polling

  tgSend("ESP32 online. Commands: /status, /off, /on");
}

void loop() {
  Blynk.run();
  timer.run();
}
