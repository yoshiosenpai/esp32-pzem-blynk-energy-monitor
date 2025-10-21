/****************************************************
 * ESP32 + PZEM-004T v3.0 + Blynk + 2-Relay (UNCHANGED UX)
 * + Telegram: team control (commands + reply keyboard)
 * + Night-only alert window (default 22:00–07:00, MYT)
 *
 * Blynk widgets/logic remain EXACTLY as before.
 * Telegram is additive: alerts + chat commands.
 ****************************************************/


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

// -------- Wi-Fi --------
char ssid[] = "WIFI NAME";
char pass[] = "WIFI PASSWORD";

// -------- Telegram --------
#define TELEGRAM_BOT_TOKEN "XXXXXXXX:YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY"
#define TELEGRAM_CHAT_ID   "6535703218"

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

// -------- Left-on detection settings --------
float ON_W_THRESHOLD = 5.0;                 // W → treat ≥ this as “ON”
unsigned long LEFT_ON_MINUTES = 10;         // minutes continuously ON before alert
const unsigned long NOTIF_COOLDOWN_MS = 10UL * 60UL * 1000UL; // 10 min anti-spam

// Night-only window (MYT, via NTP)
bool nightOnlyEnabled = true;
uint8_t nightStartHour = 22;  // 22:00
uint8_t nightEndHour   = 7;   // 07:00

// State
unsigned long onSinceMs = 0;
unsigned long lastNotifMs = 0;
unsigned long snoozeUntilMs = 0;

// Telegram getUpdates offset
long telegramUpdateOffset = 0;

// -------- Time (NTP) --------
const long GMT_OFFSET_SEC = 8 * 3600; // MYT = UTC+8
const int  DST_OFFSET_SEC = 0;

bool getLocalHour(uint8_t &hourOut) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) return false;
  hourOut = (uint8_t)timeinfo.tm_hour;
  return true;
}

bool inNightWindow(uint8_t hr) {
  if (!nightOnlyEnabled) return false;          // if disabled → not restricting
  if (nightStartHour == nightEndHour) return false; // treated as disabled
  if (nightStartHour < nightEndHour) {          // e.g., 20 → 23 (no wrap)
    return (hr >= nightStartHour && hr < nightEndHour);
  } else {                                      // e.g., 22 → 7 (wrap)
    return (hr >= nightStartHour || hr < nightEndHour);
  }
}

// -------- Telegram helpers --------
bool sendTelegram(const String &text, bool withKeyboard = false) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;

  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage";
  String payload;
  if (withKeyboard) {
    // Reply keyboard with basic controls
    payload =
      String("{\"chat_id\":\"") + TELEGRAM_CHAT_ID + "\","
      "\"text\":\"" + text + "\","
      "\"reply_markup\":{\"keyboard\":["
        "[\"/status\",\"/off\",\"/on\"],"
        "[\"/snooze 30\",\"/help\"]"
      "],\"resize_keyboard\":true}"
      "}";
  } else {
    payload =
      String("{\"chat_id\":\"") + TELEGRAM_CHAT_ID + "\","
      "\"text\":\"" + text + "\"}";
  }

  if (!https.begin(client, url)) return false;
  https.addHeader("Content-Type", "application/json");
  int code = https.POST(payload);
  https.end();
  return (code >= 200 && code < 300);
}

void setRelay1(bool on) {
  digitalWrite(RELAY1, on ? RELAY_ACTIVE : RELAY_IDLE);
  // Blynk UI is left untouched; we’re not forcing the app widget here.
}

// Parse a tiny slice of JSON (no full lib) from getUpdates
void handleTelegramCommand(const String &cmdline) {
  String s = cmdline; s.trim();

  if (s.equalsIgnoreCase("/help")) {
    sendTelegram(
      "Commands:\n"
      "/status\n"
      "/off  (turn lamp off)\n"
      "/on   (turn lamp on)\n"
      "/snooze <min>\n"
      "/settimeout <min>\n"
      "/setthreshold <watts>\n"
      "/night on|off\n"
      "/nightwindow <startHr> <endHr>\n"
      "/buttons\n"
      "/help"
    , true);
    return;
  }

  if (s.equalsIgnoreCase("/buttons")) {
    sendTelegram("Buttons shown. Use them for quick control.", true);
    return;
  }

  if (s.equalsIgnoreCase("/status")) {
    float p = pzem.power();
    bool lampOn = (digitalRead(RELAY1) == RELAY_ACTIVE);
    uint8_t hr = 0; bool gotHr = getLocalHour(hr);
    String t = String("Lamp: ") + (lampOn ? "ON" : "OFF") +
               "\nPower: " + String(p,1) + " W" +
               "\nThreshold: " + String(ON_W_THRESHOLD,1) + " W" +
               "\nTimeout: " + LEFT_ON_MINUTES + " min" +
               "\nNight-only: " + String(nightOnlyEnabled ? "ON" : "OFF") +
               "\nWindow: " + nightStartHour + "→" + nightEndHour +
               (gotHr ? ("\nLocal hour: " + String(hr)) : "\nLocal hour: (pending NTP)") +
               (snoozeUntilMs > millis() ? ("\nSnoozed (min left): " + String((snoozeUntilMs - millis())/60000UL)) : "");
    sendTelegram(t);
    return;
  }

  if (s.equalsIgnoreCase("/off")) { setRelay1(false); sendTelegram("Lamp turned OFF."); return; }
  if (s.equalsIgnoreCase("/on"))  { setRelay1(true);  sendTelegram("Lamp turned ON.");  return; }

  if (s.startsWith("/snooze")) {
    int sp = s.indexOf(' ');
    if (sp > 0) {
      long m = s.substring(sp+1).toInt();
      if (m >= 1 && m <= 720) {
        snoozeUntilMs = millis() + (unsigned long)m * 60000UL;
        sendTelegram(String("Snoozed alerts for ") + m + " minutes.");
        return;
      }
    }
    sendTelegram("Usage: /snooze <minutes> (1..720)");
    return;
  }

  if (s.startsWith("/settimeout")) {
    int sp = s.indexOf(' ');
    if (sp > 0) {
      long m = s.substring(sp+1).toInt();
      if (m >= 1 && m <= 240) {
        LEFT_ON_MINUTES = (unsigned long)m;
        sendTelegram(String("Timeout updated: ") + LEFT_ON_MINUTES + " min.");
        return;
      }
    }
    sendTelegram("Usage: /settimeout <minutes> (1..240)");
    return;
  }

  if (s.startsWith("/setthreshold")) {
    int sp = s.indexOf(' ');
    if (sp > 0) {
      float w = s.substring(sp+1).toFloat();
      if (w >= 0.5 && w <= 2000.0) {
        ON_W_THRESHOLD = w;
        sendTelegram(String("Threshold updated: ") + String(ON_W_THRESHOLD,1) + " W.");
        return;
      }
    }
    sendTelegram("Usage: /setthreshold <watts> (0.5..2000)");
    return;
  }

  if (s.equalsIgnoreCase("/night on") || s.equalsIgnoreCase("/night off")) {
    nightOnlyEnabled = s.endsWith("on");
    sendTelegram(String("Night-only alert window: ") + (nightOnlyEnabled ? "ON" : "OFF"));
    return;
  }

  if (s.startsWith("/nightwindow")) {
    // /nightwindow 22 7
    int sp1 = s.indexOf(' ');
    int sp2 = s.indexOf(' ', sp1 + 1);
    if (sp1 > 0 && sp2 > sp1) {
      int sH = s.substring(sp1+1, sp2).toInt();
      int eH = s.substring(sp2+1).toInt();
      if (sH >= 0 && sH <= 23 && eH >= 0 && eH <= 23) {
        nightStartHour = (uint8_t)sH;
        nightEndHour   = (uint8_t)eH;
        sendTelegram(String("Night window set: ") + nightStartHour + "→" + nightEndHour);
        return;
      }
    }
    sendTelegram("Usage: /nightwindow <startHour 0..23> <endHour 0..23>");
    return;
  }

  sendTelegram("Unknown. Try /help", true);
}

void pollTelegram() {
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

      if (chatIdFound == TELEGRAM_CHAT_ID) {
        handleTelegramCommand(text);
      }

      pos = q2 + 1;
      telegramUpdateOffset = uid + 1;
    }
  }
  https.end();
}

void pushReadings() {
  static bool first = true;

  float v  = pzem.voltage();
  float i  = pzem.current();
  float p  = pzem.power();
  float e  = pzem.energy();

  if (first) { first = false; return; }
  if (isnan(v) || isnan(i) || isnan(p) || isnan(e)) {
    Serial.println("PZEM read error (check AC L/N, CT S1/S2, TX/RX).");
    return;
  }

  Serial.printf("V=%.1f V | I=%.3f A | P=%.1f W | E=%.3f kWh\n", v, i, p, e);

  // Push to Blynk (same virtuals)
  Blynk.virtualWrite(V0, v);
  Blynk.virtualWrite(V1, i);
  Blynk.virtualWrite(V2, p);
  Blynk.virtualWrite(V3, e);

  // --- "Left-on" detection with optional night-only window ---
  unsigned long now = millis();
  bool lampAppearsOn = (p >= ON_W_THRESHOLD);

  // Resolve current local hour for night window
  uint8_t hr = 0; bool haveHr = getLocalHour(hr);
  bool shouldCheck = true;
  if (nightOnlyEnabled) {
    if (haveHr) shouldCheck = inNightWindow(hr);
    else        shouldCheck = true; // if time not synced yet, don't block alerts
  }

  bool snoozed = (snoozeUntilMs > now);

  if (lampAppearsOn && shouldCheck && !snoozed) {
    if (onSinceMs == 0) { onSinceMs = now; }
    else {
      unsigned long elapsed = now - onSinceMs;
      unsigned long target  = LEFT_ON_MINUTES * 60UL * 1000UL;
      if (elapsed >= target) {
        bool cooldownOk = (now - lastNotifMs) >= NOTIF_COOLDOWN_MS;
        if (cooldownOk) {
          String msg = String("⚠️ Lamp likely LEFT ON for ")
                     + LEFT_ON_MINUTES + " min. Power ~" + String(p,1) + " W."
                     + (nightOnlyEnabled ? " (Night window)" : "");
          if (sendTelegram(msg, true)) {
            lastNotifMs = now;
            Serial.println("Telegram sent: Left-on alert.");
          } else {
            Serial.println("Telegram send failed.");
          }
        }
      }
    }
  } else {
    // Reset if power fell below threshold OR outside window OR snoozed
    onSinceMs = 0;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, RELAY_IDLE);
  digitalWrite(RELAY2, RELAY_IDLE);

  // WiFi via Blynk (UNCHANGED for your UX)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // NTP time (MYT)
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

  // Timers
  timer.setInterval(2000, pushReadings);  // PZEM + “left-on” monitor
  timer.setInterval(3000, pollTelegram);  // Telegram command polling

  // Optional: show buttons once on boot
  sendTelegram("ESP32 is online. Use buttons or /help.", true);
}

void loop() {
  Blynk.run();
  timer.run();
}
