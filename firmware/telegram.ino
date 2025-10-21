/****************************************************
 * ESP32 + PZEM-004T v3.0 + Blynk (UNCHANGED UX)
 * + Telegram (group-ready): /status, /on1, /off1, /on2, /off2, /on, /off
 * + Night alerts per lamp (22:00–07:00 MYT) + "unknown load" alert
 * Notes:
 * - Set BotFather Group Privacy = OFF (recommended)
 * - TELEGRAM_CHAT_ID must match your group's negative ID exactly
 ****************************************************/

// ---- Blynk (unchanged) ----
#define BLYNK_TEMPLATE_ID   "BLYNK TEMPLATE ID"
#define BLYNK_TEMPLATE_NAME "BLYNK TEMPLATE NAME"
#define BLYNK_AUTH_TOKEN    "BLYNK TOKEN"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <PZEM004Tv30.h>
#include <time.h>

// ---- Wi-Fi ----
char ssid[] = "WIFI NAME";
char pass[] = "WIFI PASSWORD";

// ---- Telegram ----
// Group chat IDs are negative (e.g. "-1001234567890")
#define TELEGRAM_BOT_TOKEN "XXXXXXXX:YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY"
#define TELEGRAM_CHAT_ID   "-1001234567890"

// ---- Pins (unchanged) ----
#define RX_PIN   16   // ESP32 RX2  <- PZEM TX
#define TX_PIN   17   // ESP32 TX2  -> PZEM RX
#define RELAY1   23   // Blynk V10 (Lamp 1)
#define RELAY2   22   // Blynk V11 (Lamp 2)

const bool RELAY_ACTIVE = LOW;
const bool RELAY_IDLE   = HIGH;

// ---- Objects ----
HardwareSerial PZEMSerial(2);
PZEM004Tv30 pzem(PZEMSerial, RX_PIN, TX_PIN);
BlynkTimer timer;

// ---- Blynk handlers (unchanged) ----
BLYNK_WRITE(V10) { digitalWrite(RELAY1, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }
BLYNK_WRITE(V11) { digitalWrite(RELAY2, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }

// ---- Night alert settings ----
const uint8_t NIGHT_START_H = 22;   // 22:00
const uint8_t NIGHT_END_H   = 7;    // 07:00
unsigned long NIGHT_ON_MINUTES = 5; // minutes ON before alert
const unsigned long NOTIF_COOLDOWN_MS = 30UL * 60UL * 1000UL; // 30min cooldown

// Power threshold for "unknown load" (when both relays OFF)
float ON_W_THRESHOLD = 5.0; // W

// ---- State per lamp ----
struct LampState {
  uint8_t pin;
  const char* name;
  unsigned long onSinceMs = 0;
  unsigned long lastNotifMs = 0;
};
LampState lamps[2] = {
  { RELAY1, "Lamp 1" },
  { RELAY2, "Lamp 2" }
};

// Unknown load state (relays OFF but power high)
unsigned long unknownOnSinceMs = 0;
unsigned long unknownLastNotifMs = 0;

// Telegram polling
long telegramUpdateOffset = 0;

// ---- NTP Time (Asia/Kuala_Lumpur) ----
const long GMT_OFFSET_SEC = 8 * 3600;
const int  DST_OFFSET_SEC = 0;

static inline bool relayIsOn(uint8_t pin) { return digitalRead(pin) == RELAY_ACTIVE; }

bool getLocalHour(uint8_t &hourOut) {
  struct tm t;
  if (!getLocalTime(&t, 1000)) return false;
  hourOut = (uint8_t)t.tm_hour;
  return true;
}
bool inNightWindow(uint8_t hr) {
  // 22→07 wraps midnight
  if (NIGHT_START_H < NIGHT_END_H) return (hr >= NIGHT_START_H && hr < NIGHT_END_H);
  return (hr >= NIGHT_START_H || hr < NIGHT_END_H);
}

// ---------- Telegram helpers & normalization ----------
String normalizeCmd(String s) {
  s.trim();
  int sp = s.indexOf(' ');            // keep first token
  if (sp > 0) s = s.substring(0, sp);
  int at = s.indexOf('@');            // strip @botname suffix
  if (at > 0) s = s.substring(0, at);
  s.toLowerCase();
  return s;
}

bool tgSend(const String &text) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage";
  String payload = String("{\"chat_id\":\"") + TELEGRAM_CHAT_ID + "\",\"text\":\"" + text + "\"}";
  if (!https.begin(client, url)) { Serial.println("[TG] begin() failed"); return false; }
  https.addHeader("Content-Type", "application/json");
  int code = https.POST(payload);
  https.end();
  if (code < 200 || code >= 300) Serial.printf("[TG] send HTTP %d\n", code);
  return (code >= 200 && code < 300);
}

void setRelay(uint8_t pin, bool on) { digitalWrite(pin, on ? RELAY_ACTIVE : RELAY_IDLE); }

// Commands: /status, /on1, /off1, /on2, /off2, /on, /off
void tgHandleCmd(const String &raw) {
  String cmd = normalizeCmd(raw);

  if (cmd == "/status" || cmd == "/start") {
    float p = pzem.power();
    bool l1 = relayIsOn(RELAY1);
    bool l2 = relayIsOn(RELAY2);
    uint8_t hr=0; bool ok=getLocalHour(hr);
    tgSend(String("Status:\n")
      + "Lamp 1: " + (l1 ? "ON" : "OFF") + "\n"
      + "Lamp 2: " + (l2 ? "ON" : "OFF") + "\n"
      + "Line Power: " + String(p,1) + " W\n"
      + "Night window: 22:00–07:00"
      + (ok ? String("\nLocal hour: ") + hr : "\nLocal hour: (Syncing NTP)")
    );
    return;
  }

  if (cmd == "/on1")  { setRelay(RELAY1, true);  tgSend("Lamp 1 turned ON.");  return; }
  if (cmd == "/off1") { setRelay(RELAY1, false); tgSend("Lamp 1 turned OFF."); return; }
  if (cmd == "/on2")  { setRelay(RELAY2, true);  tgSend("Lamp 2 turned ON.");  return; }
  if (cmd == "/off2") { setRelay(RELAY2, false); tgSend("Lamp 2 turned OFF."); return; }

  if (cmd == "/on")   { setRelay(RELAY1, true);  setRelay(RELAY2, true);  tgSend("Both lamps ON.");  return; }
  if (cmd == "/off")  { setRelay(RELAY1, false); setRelay(RELAY2, false); tgSend("Both lamps OFF."); return; }

  // ignore others
}

void tgPoll() {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;

  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN +
               "/getUpdates?timeout=10&allowed_updates=%5B%22message%22,%22edited_message%22%5D&offset=" +
               String(telegramUpdateOffset);

  if (!https.begin(client, url)) {
    Serial.println("[TG] begin() failed");
    return;
  }

  int code = https.GET();
  if (code < 200 || code >= 300) {
    Serial.printf("[TG] HTTP %d\n", code); // -11 = timeout sometimes, ignore
    https.end();
    return;
  }

  String body = https.getString();
  https.end();

  // DEBUG (peek)
  Serial.print("[TG] getUpdates OK. Body: ");
  Serial.println(body.substring(0, 300));

  int pos = body.indexOf("\"result\":[");
  if (pos < 0) return;
  pos += 10;

  while (true) {
    int uidIdx = body.indexOf("\"update_id\":", pos);
    if (uidIdx < 0) break;
    int uidStart = body.indexOf(':', uidIdx) + 1;
    int uidEnd   = body.indexOf(',', uidStart);
    long uid     = body.substring(uidStart, uidEnd).toInt();

    // prefer "message", else "edited_message"
    int msgIdx = body.indexOf("\"message\":{", uidEnd);
    int emgIdx = body.indexOf("\"edited_message\":{", uidEnd);
    int useIdx = -1; String key;

    if (msgIdx >= 0 && (emgIdx < 0 || msgIdx < emgIdx)) { useIdx = msgIdx; key = "\"message\":"; }
    else if (emgIdx >= 0) { useIdx = emgIdx; key = "\"edited_message\":"; }
    if (useIdx < 0) { pos = uidEnd; telegramUpdateOffset = uid + 1; continue; }

    // extract full JSON object for the message
    int brace = body.indexOf('{', useIdx + key.length());
    if (brace < 0) { pos = uidEnd; telegramUpdateOffset = uid + 1; continue; }
    int depth = 1, i = brace + 1;
    for (; i < (int)body.length() && depth > 0; ++i) {
      if (body[i] == '{') depth++;
      else if (body[i] == '}') depth--;
    }
    if (depth != 0) { pos = uidEnd; telegramUpdateOffset = uid + 1; continue; }
    String msgBlock = body.substring(brace, i);

    // === STRING-BASED CHAT ID (avoid overflow) ===
    String chatIdStr = "";
    int chatIdx = msgBlock.indexOf("\"chat\":");
    if (chatIdx >= 0) {
      int idKey = msgBlock.indexOf("\"id\":", chatIdx);
      if (idKey >= 0) {
        int cs = msgBlock.indexOf(':', idKey) + 1;
        int ce = msgBlock.indexOf(',', cs);
        if (ce < 0) ce = msgBlock.indexOf('}', cs);
        if (cs > 0 && ce > cs) {
          chatIdStr = msgBlock.substring(cs, ce); chatIdStr.trim();
        }
      }
    }

    // text field
    String text = "";
    int textIdx = msgBlock.indexOf("\"text\":");
    if (textIdx >= 0) {
      int q1 = msgBlock.indexOf('\"', textIdx + 7);
      int q2 = msgBlock.indexOf('\"', q1 + 1);
      if (q1 > 0 && q2 > q1) text = msgBlock.substring(q1 + 1, q2);
    }

    // advance offset for next poll
    telegramUpdateOffset = uid + 1;
    pos = i;

    // must match target group
    if (chatIdStr.length() == 0) continue;
    if (chatIdStr != String(TELEGRAM_CHAT_ID)) {
      Serial.print("[TG] Skipping chatId "); Serial.println(chatIdStr);
      continue;
    }

    if (text.length() > 0) {
      Serial.print("[TG] CMD: "); Serial.println(text);
      tgHandleCmd(text);
    }
  }
}

// ---- Core: readings + night alerts (Blynk unchanged) ----
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

  // Push to Blynk (unchanged)
  Blynk.virtualWrite(V0, v);
  Blynk.virtualWrite(V1, i);
  Blynk.virtualWrite(V2, p);
  Blynk.virtualWrite(V3, e);

  // Determine night window
  uint8_t hr = 0; bool haveHr = getLocalHour(hr);
  bool night = haveHr ? inNightWindow(hr) : true; // allow alerts until NTP sync

  unsigned long now = millis();
  unsigned long targetMs = NIGHT_ON_MINUTES * 60UL * 1000UL;

  // Per-lamp night alert by RELAY state
  for (int k = 0; k < 2; ++k) {
    bool isOn = relayIsOn(lamps[k].pin);
    if (night && isOn) {
      if (lamps[k].onSinceMs == 0) lamps[k].onSinceMs = now;
      unsigned long elapsed = now - lamps[k].onSinceMs;
      if (elapsed >= targetMs) {
        bool cooldownOk = (now - lamps[k].lastNotifMs) >= NOTIF_COOLDOWN_MS;
        if (cooldownOk) {
          tgSend(String("⚠️ Night alert: ") + lamps[k].name +
                 " has been ON for " + NIGHT_ON_MINUTES + " min.");
          lamps[k].lastNotifMs = now;
        }
      }
    } else {
      lamps[k].onSinceMs = 0; // reset when off or not in night window
    }
  }

  // Unknown load alert: both relays OFF but line power high at night
  bool bothOff = !relayIsOn(RELAY1) && !relayIsOn(RELAY2);
  if (night && bothOff && p >= ON_W_THRESHOLD) {
    if (unknownOnSinceMs == 0) unknownOnSinceMs = now;
    unsigned long elapsed = now - unknownOnSinceMs;
    if (elapsed >= targetMs) {
      bool cooldownOk = (now - unknownLastNotifMs) >= NOTIF_COOLDOWN_MS;
      if (cooldownOk) {
        tgSend(String("⚠️ Night alert: Power ≈ ") + String(p,1) +
               " W while both Lamp 1 & Lamp 2 are OFF. Possible unknown load.");
        unknownLastNotifMs = now;
      }
    }
  } else {
    unknownOnSinceMs = 0;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, RELAY_IDLE);
  digitalWrite(RELAY2, RELAY_IDLE);

  // Blynk (kept as-is)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // NTP time (MYT)
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

  // Timers
  timer.setInterval(2000, pushReadings);  // readings + alerts
  timer.setInterval(3000, tgPoll);        // Telegram commands

  tgSend("ESP32 online. Commands: /status, /on1, /off1, /on2, /off2, /on, /off");
}

void loop() {
  Blynk.run();
  timer.run();
}
