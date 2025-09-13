/****************************************************
 * ESP32 + PZEM-004T v3.0 + Blynk + 2-Relay
 * Code by Solehin Rizal
 *  - Library: "PZEM004Tv30" by Jakub Mandula
 *  - UART2: RX2=GPIO16 (from PZEM TX), TX2=GPIO17 (to PZEM RX)
 *  - Blynk V0..V3 = V/I/P/E
 *  - Blynk V10 = Relay1, V11 = Relay2
 ****************************************************/

 // -------- Blynk Template / Auth --------
#define BLYNK_TEMPLATE_ID "BLYNK TEMPLATE ID"
#define BLYNK_TEMPLATE_NAME "BLYNK TEMPLATE NAME"
#define BLYNK_AUTH_TOKEN "BLYNK TOKEN"


#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <PZEM004Tv30.h>           // v3.0 library


// -------- Wi-Fi --------
char ssid[] = "WIFI NAME";
char pass[] = "WIFI PASSWORD";

// -------- Pins --------
#define RX_PIN   16   // ESP32 RX2  <- PZEM TX
#define TX_PIN   17   // ESP32 TX2  -> PZEM RX
#define RELAY1   23   // Blynk V10
#define RELAY2   22   // Blynk V11

// Many 5V relay boards are active-LOW:
const bool RELAY_ACTIVE = LOW;
const bool RELAY_IDLE   = HIGH;

// -------- Objects --------
HardwareSerial PZEMSerial(2);      // UART2
PZEM004Tv30 pzem(PZEMSerial, RX_PIN, TX_PIN);
BlynkTimer timer;

// -- Blynk button handlers --
BLYNK_WRITE(V10) { digitalWrite(RELAY1, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }
BLYNK_WRITE(V11) { digitalWrite(RELAY2, param.asInt() ? RELAY_ACTIVE : RELAY_IDLE); }

void pushReadings() {
  static bool first = true;

  // Read all metrics
  float v  = pzem.voltage();
  float i  = pzem.current();
  float p  = pzem.power();
  float e  = pzem.energy();

  // Discard the very first (sometimes garbage) frame
  if (first) { first = false; return; }

  // Guard NaNs
  if (isnan(v) || isnan(i) || isnan(p) || isnan(e)) {
    Serial.println("PZEM read error (check AC L/N, CT S1/S2, and TX/RX).");
    return;
  }

  // Print to Serial
  Serial.printf("V=%.1f V | I=%.3f A | P=%.1f W | E=%.3f kWh\n", v, i, p, e);

  // Push to Blynk
  Blynk.virtualWrite(V0, v);
  Blynk.virtualWrite(V1, i);
  Blynk.virtualWrite(V2, p);
  Blynk.virtualWrite(V3, e);
}

void setup() {
  Serial.begin(115200);

  // Relays
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, RELAY_IDLE);
  digitalWrite(RELAY2, RELAY_IDLE);

  // Start Blynk (handles Wi-Fi too)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Push readings every 2 seconds
  timer.setInterval(2000, pushReadings);
}

void loop() {
  Blynk.run();
  timer.run();
}
